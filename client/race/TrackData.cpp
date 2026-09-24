#include "TrackData.h"

#include "engine/formats/nif_reader.h"
#include "games/kart/physics/client/world_collision.h"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace KnC::Client {

namespace fs = std::filesystem;

namespace {

std::string lowerAscii(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

uint32_t readU32(const std::vector<uint8_t>& b, size_t at) {
    if (at + 4 > b.size()) return 0;
    return static_cast<uint32_t>(b[at]) | (static_cast<uint32_t>(b[at + 1]) << 8) |
           (static_cast<uint32_t>(b[at + 2]) << 16) | (static_cast<uint32_t>(b[at + 3]) << 24);
}

float readF32(const std::vector<uint8_t>& b, size_t at) {
    const uint32_t bits = readU32(b, at);
    float f = 0.f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

// COL head is four dwords then checkpoint points as server reads them
bool readColHead(const std::string& path, int& count, std::vector<CheckpointPoint3>& points) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    const std::streamoff size = file.tellg();
    if (size < 16) return false;
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    const uint32_t a = readU32(bytes, 0);
    if (a > 100 || 16 + 12 * a > bytes.size()) return false;
    count = static_cast<int>(a);
    points.clear();
    for (uint32_t i = 0; i < a; ++i) {
        CheckpointPoint3 p;
        p.x = readF32(bytes, 16 + 12 * i);
        p.y = readF32(bytes, 16 + 12 * i + 4);
        p.z = readF32(bytes, 16 + 12 * i + 8);
        points.push_back(p);
    }
    return true;
}

// node transform in nif tree translation rotation and scale
struct NodeXf {
    float t[3] = {0.f, 0.f, 0.f};
    float r[9] = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};
    float s = 1.f;
};

NodeXf fromNif(const KnC::NifTransform& n) {
    NodeXf x;
    for (int i = 0; i < 3; ++i) x.t[i] = n.translation[i];
    for (int i = 0; i < 9; ++i) x.r[i] = n.rotation[i];
    x.s = n.scale;
    return x;
}

NodeXf composeXf(const NodeXf& a, const NodeXf& b) {
    NodeXf o;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            float sum = 0.f;
            for (int k = 0; k < 3; ++k) sum += a.r[i * 3 + k] * b.r[k * 3 + j];
            o.r[i * 3 + j] = sum;
        }
    for (int i = 0; i < 3; ++i) {
        const float bt = a.r[i * 3] * b.t[0] + a.r[i * 3 + 1] * b.t[1] + a.r[i * 3 + 2] * b.t[2];
        o.t[i] = a.t[i] + a.s * bt;
    }
    o.s = a.s * b.s;
    return o;
}

void walkXf(const KnC::NifScene& scene, uint32_t idx, const NodeXf& parent, std::vector<NodeXf>& world,
            std::vector<char>& seen) {
    if (idx >= scene.blocks.size() || seen[idx]) return;
    seen[idx] = 1;
    const NodeXf local = scene.blocks[idx].has_transform ? fromNif(scene.blocks[idx].transform) : NodeXf{};
    world[idx] = composeXf(parent, local);
    for (uint32_t child : scene.blocks[idx].children) walkXf(scene, child, world[idx], world, seen);
}

// one line per world step showing its span
struct WorldClock {
    std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
    void mark(const char* step) {
        const auto now = std::chrono::steady_clock::now();
        std::printf("[time] race world %s %.0f ms\n", step, std::chrono::duration<double, std::milli>(now - last).count());
        last = now;
    }
};

}

// quad is first tri shape of file corners land in world space through node tree
bool loadMinimap(const std::string& gameDir, const TrackFiles& files, MinimapData& out) {
    out = MinimapData();
    const std::string nifPath = findEntryCi(files.trackDir, "minimap.nif");
    const std::string iniPath = findEntryCi(files.trackDir, "minimap.ini");
    if (nifPath.empty() || iniPath.empty()) return false;
    std::ifstream ini(iniPath);
    char comma = 0;
    if (!(ini >> out.ini[0] >> comma >> out.ini[1] >> comma >> out.ini[2])) return false;
    KnC::NifScene scene;
    std::string error;
    if (!KnC::read_nif_scene(nifPath, scene, error)) {
        std::printf("[track] minimap nif %s\n", error.c_str());
        return false;
    }
    std::vector<NodeXf> world(scene.blocks.size());
    std::vector<char> seen(scene.blocks.size(), 0);
    for (uint32_t root : scene.roots) walkXf(scene, root, NodeXf{}, world, seen);
    for (uint32_t si = 0; si < scene.blocks.size(); ++si) {
        const KnC::NifBlock& shape = scene.blocks[si];
        if ((shape.type != "NiTriShape" && shape.type != "NiTriStrips") || shape.data_link == KnC::kNoLink) continue;
        if (shape.data_link >= scene.blocks.size()) continue;
        const KnC::NifBlock& data = scene.blocks[shape.data_link];
        if (data.vertices.size() < 12 || data.uvs.size() < 8) continue;
        const NodeXf& xf = world[si];
        for (int v = 0; v < 4; ++v) {
            const float* in = &data.vertices[static_cast<size_t>(v) * 3];
            float p[3];
            for (int i = 0; i < 3; ++i) {
                const float rv = xf.r[i * 3] * in[0] + xf.r[i * 3 + 1] * in[1] + xf.r[i * 3 + 2] * in[2];
                p[i] = xf.t[i] + xf.s * rv;
            }
            out.cornerX[v] = p[0];
            out.cornerY[v] = p[1];
            out.quadZ = p[2];
            out.cornerU[v] = data.uvs[static_cast<size_t>(v) * 2];
            out.cornerV[v] = data.uvs[static_cast<size_t>(v) * 2 + 1];
        }
        out.valid = true;
        break;
    }
    if (!out.valid) return false;
    std::string rel = files.trackDir;
    const std::string root = gameDir;
    if (rel.rfind(root, 0) == 0) rel = rel.substr(root.size());
    while (!rel.empty() && (rel.front() == '/' || rel.front() == '\\')) rel.erase(rel.begin());
    out.texture = rel + "/minimap.dds";
    std::printf("[track] minimap quad %.0f %.0f | %.0f %.0f | %.0f %.0f | %.0f %.0f z %.0f ini %.0f %.0f %.0f\n", out.cornerX[0],
                out.cornerY[0], out.cornerX[1], out.cornerY[1], out.cornerX[2], out.cornerY[2], out.cornerX[3], out.cornerY[3],
                out.quadZ, out.ini[0], out.ini[1], out.ini[2]);
    return true;
}

std::string findEntryCi(const std::string& dir, const std::string& name) {
    std::error_code ignored;
    if (!fs::is_directory(dir, ignored)) return std::string();
    const std::string wanted = lowerAscii(name);
    for (const auto& entry : fs::directory_iterator(dir, ignored)) {
        if (lowerAscii(entry.path().filename().string()) == wanted) return entry.path().string();
    }
    return std::string();
}

std::string kartBodyNif(const std::string& gameDir, const std::string& model) {
    // a factory token names its chassis before the hash the body of the CHASSIS tree
    const size_t hash = model.find('#');
    if (hash != std::string::npos)
        return findEntryCi(findEntryCi(gameDir + "/Data/Public/Car/FactoryCar/CHASSIS", model.substr(0, hash)), "BODY.nif");
    const std::string folder = findEntryCi(gameDir + "/Data/Public/Car/Body/High", model);
    if (folder.empty()) return std::string();
    return findEntryCi(folder, "BODY.nif");
}

std::string kartCarFile(const std::string& gameDir, const std::string& model) {
    return findEntryCi(gameDir + "/Data/Car", model + ".car");
}

std::string driverBodyNif(const std::string& gameDir, const std::string& asset) {
    const std::string folder = findEntryCi(gameDir + "/Data/Public/Driver/Body/High", asset);
    if (folder.empty()) return std::string();
    return findEntryCi(folder, "body.nif");
}

bool resolveTrackFiles(const Catalog& catalog, int trackId, const std::string& gameDir, TrackFiles& out,
                       std::string& error) {
    const TrackRow* row = catalog.track(static_cast<uint32_t>(trackId));
    if (!row) { error = "track " + std::to_string(trackId) + " has no 0x00C3 row"; return false; }
    const ThemeRow* theme = catalog.theme(row->themeId);
    if (!theme) { error = "theme " + std::to_string(row->themeId) + " has no 0x00C4 row"; return false; }
    out.trackId = trackId;
    out.themeFolder = theme->folder;
    out.trackFolder = row->folder;
    out.laps = row->lapCount;
    for (int i = 0; i < 3; ++i) out.tuning[i] = row->tuning[i];
    out.nameKey = row->nameKey;
    const std::string world = gameDir + "/Data/Public/World";
    out.mapDir = findEntryCi(world, theme->folder);
    if (out.mapDir.empty()) { error = "no World folder " + theme->folder + " under " + world; return false; }
    out.trackDir = findEntryCi(out.mapDir, row->folder);
    if (out.trackDir.empty()) { error = "no track folder " + row->folder + " under " + out.mapDir; return false; }
    for (char& c : out.mapDir) if (c == '\\') c = '/';
    for (char& c : out.trackDir) if (c == '\\') c = '/';
    return true;
}

// Mission theme keeps its dds files right under Texture loader knows only High and Low
void fixTexturePaths(const TrackFiles& files, KnC::Render::PropModel& model) {
    std::error_code ignored;
    auto fix = [&](std::string& path) {
        if (path.empty() || fs::exists(path, ignored)) return;
        const std::string name = fs::path(path).filename().string();
        for (const std::string dir : {files.mapDir + "/Texture", files.trackDir, files.mapDir + "/Texture/High"}) {
            const std::string found = findEntryCi(dir, name);
            if (!found.empty()) { path = found; return; }
        }
    };
    for (KnC::Render::PropPart& part : model.parts) {
        fix(part.texture_path);
        fix(part.environment.texture);
    }
    for (KnC::Render::ParticleSystemDefinition& system : model.particle_systems) fix(system.texture_path);
}

bool loadRaceWorld(const TrackFiles& files, RaceWorld& out, std::string& error, bool items) {
    out.files = files;
    KnC::Tools::TrackSceneRequest request;
    request.track_dir = files.trackDir;
    request.load_sun = true;
    request.load_collision = false;
    request.load_markers = false;
    request.load_geometry = false;
    request.load_item_boxes = items;
    request.load_item_drums = items;
    // app points texture cache at its warm pak index second pak scan cost 23 s
    request.point_textures_at_pak = false;
    WorldClock clock;
    if (!KnC::Tools::load_track_scene(request, out.scene, error)) return false;
    clock.mark("scene");
    for (KnC::Render::PropModel& model : out.scene.scene.prop_models) fixTexturePaths(files, model);
    for (KnC::Render::SkyPhase& phase : out.scene.scene.sky_phases) fixTexturePaths(files, phase.model);
    // scene loader reads COL only for its overlay physics needs pieces without it
    if (!KnC::Kart::Client::world_load_track_pieces(out.scene.collision, files.trackDir, error)) return false;
    clock.mark("col pieces");
    std::string ignored;
    if (!KnC::Kart::Client::gimmick_load_boost(files.trackDir, out.boostRows, ignored)) out.boostRows.clear();
    if (!items || !KnC::Kart::Client::gimmick_load_itembox(files.trackDir, out.itemBoxes, ignored)) out.itemBoxes.clear();
    if (!items || !KnC::Kart::Client::gimmick_load_itemdrum(files.trackDir, out.itemDrums, ignored)) out.itemDrums.clear();
    clock.mark("ini rows");
    const std::string col = findEntryCi(files.trackDir, "track.COL");
    if (col.empty() || !readColHead(col, out.checkpointCount, out.checkpointPoints)) {
        out.checkpointCount = 0;
        out.checkpointPoints.clear();
        std::printf("[track] no COL head under %s the laps stay uncounted\n", files.trackDir.c_str());
    }
    clock.mark("col head");
    const size_t worldAt = files.trackDir.find("/Data/Public/World");
    if (!loadMinimap(worldAt == std::string::npos ? std::string() : files.trackDir.substr(0, worldAt), files, out.minimap))
        std::printf("[track] no minimap under %s\n", files.trackDir.c_str());
    clock.mark("minimap");
    std::printf("[track] %s %zu prop models %zu start rows %d checkpoints %zu boxes %zu drums %zu boost rows %zu col pieces laps %u\n",
                files.trackDir.c_str(), out.scene.scene.prop_models.size(), out.scene.start_rows.size(),
                out.checkpointCount, out.itemBoxes.size(), out.itemDrums.size(), out.boostRows.size(), out.scene.collision.pieces.size(),
                files.laps);
    return true;
}

}
