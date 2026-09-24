#include "tools/track_scene/track_scene.h"

#include "engine/render/light_rig.h"
#include "engine/render/nif_prop_model.h"
#include "engine/render/texture_cache.h"
#include "shared/src/PakReader.h"

#include <bx/math.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>

namespace KnC::Tools {

namespace fs = std::filesystem;
using namespace KnC::Render;
using namespace KnC::Kart::Client;

namespace {

// Packs one colour into the vertex abgr word bgfx expects
uint32_t pack_abgr(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
}

std::string lower_ascii(std::string text);

// Indexes World Texture folders by lower case name toy circuit borrows a snow theme texture this way
const std::map<std::string, std::string>& world_texture_index(const fs::path& world_dir) {
    static std::map<std::string, std::map<std::string, std::string>> cached;
    const std::string key = world_dir.string();
    const auto known = cached.find(key);
    if (known != cached.end()) return known->second;
    std::map<std::string, std::string>& index = cached[key];
    std::error_code ignored;
    for (const auto& theme : fs::directory_iterator(world_dir, ignored)) {
        if (!theme.is_directory(ignored)) continue;
        const fs::path textures = theme.path() / "Texture";
        if (!fs::exists(textures, ignored)) continue;
        for (const auto& file : fs::recursive_directory_iterator(textures, ignored)) {
            if (!file.is_regular_file(ignored)) continue;
            index.emplace(lower_ascii(file.path().filename().string()), file.path().string());
        }
    }
    std::cout << "[track] " << index.size() << " textures indexed under " << key << "\n";
    return index;
}

// Pak answers texture names no folder holds reading its table costs a second so it waits for first miss
void point_texture_cache_at_pak(const fs::path& game_dir) {
    static std::string opened;
    const std::string wanted = game_dir.string();
    if (opened == wanted) return;
    opened = wanted;
    KnC::Render::set_texture_bytes_source(
        [wanted](const std::string& path, std::vector<uint8_t>& out) {
            static std::shared_ptr<KnC::PakReader> pak;
            static bool tried = false;
            if (!tried) {
                tried = true;
                auto reader = std::make_shared<KnC::PakReader>();
                if (reader->OpenGameDir(wanted)) pak = reader;
                else std::cout << "[track] no pak under " << wanted
                               << ", a missing texture stays missing\n";
            }
            if (!pak) return false;
            out = pak->ReadFile(fs::path(path).filename().string());
            return !out.empty();
        });
}

// Beside the nif then the map Texture folders then every World Texture folder
std::string resolve_texture(const fs::path& nif_dir, const fs::path& map_dir,
                            const std::string& raw_path) {
    if (raw_path.empty()) return raw_path;
    const std::string name = fs::path(raw_path).filename().string();
    if (name.empty()) return raw_path;
    std::error_code ignored;
    const fs::path beside = nif_dir / name;
    if (fs::exists(beside, ignored)) return beside.string();
    const fs::path high = map_dir / "Texture" / "High" / name;
    if (fs::exists(high, ignored)) return high.string();
    const fs::path low = map_dir / "Texture" / "Low" / name;
    if (fs::exists(low, ignored)) return low.string();
    const std::map<std::string, std::string>& index = world_texture_index(map_dir.parent_path());
    const auto found = index.find(lower_ascii(name));
    if (found != index.end()) return found->second;
    return raw_path;
}

void resolve_model_textures(const fs::path& nif_dir, const fs::path& map_dir, PropModel& model) {
    for (PropPart& part : model.parts) {
        part.texture_path = resolve_texture(nif_dir, map_dir, part.texture_path);
        if (!part.environment.texture.empty())
            part.environment.texture = resolve_texture(nif_dir, map_dir, part.environment.texture);
    }
    for (ParticleSystemDefinition& system : model.particle_systems)
        system.texture_path = resolve_texture(nif_dir, map_dir, system.texture_path);
}

// Loads one nif beside its own folder and resolves its textures then appends the model
bool add_prop_model(const std::string& nif_path, const fs::path& map_dir, MapScene& scene,
                    size_t& model_index, std::string& error) {
    NifModelRequest request;
    request.nif_path = nif_path;
    request.texture_dir = fs::path(nif_path).parent_path().string();
    PropModel model;
    if (!load_prop_model(request, model, error)) return false;
    resolve_model_textures(fs::path(request.texture_dir), map_dir, model);
    model_index = scene.prop_models.size();
    scene.prop_models.push_back(std::move(model));
    return true;
}

// Loads one nif beside its own folder resolves textures and appends it at identity
bool add_prop_at_identity(const std::string& nif_path, const fs::path& map_dir, SceneLayer layer,
                          MapScene& scene, std::string& error) {
    size_t index = 0;
    if (!add_prop_model(nif_path, map_dir, scene, index, error)) return false;
    PropInstance instance;
    instance.model_index = index;
    instance.layer = layer;
    scene.prop_instances.push_back(instance);
    return true;
}

std::string lower_ascii(std::string text) {
    for (char& letter : text) letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
    return text;
}

// The client asset store ignores case and the disk names differ from the exe names
bool find_entry_ci(const fs::path& dir, const std::string& wanted, fs::path& out) {
    std::error_code ignored;
    const std::string lowered = lower_ascii(wanted);
    for (const auto& entry : fs::directory_iterator(dir, ignored)) {
        if (lower_ascii(entry.path().filename().string()) != lowered) continue;
        out = entry.path();
        return true;
    }
    return false;
}

// Track id is theme id plus the folder suffix minus one as the client catalogue uses it
int track_id_from_folder(const fs::path& track_dir) {
    struct Theme { const char* map; int id; };
    static const Theme themes[] = {{"forest", 10}, {"cookie", 20}, {"desert", 30}, {"toy", 40},
                                   {"devil", 50},  {"snow", 60},   {"palace", 70}, {"swamp", 80},
                                   {"race", 90}};
    const std::string map_name = lower_ascii(track_dir.parent_path().filename().string());
    const std::string track_name = lower_ascii(track_dir.filename().string());
    int theme = -1;
    for (const Theme& entry : themes)
        if (map_name == entry.map) theme = entry.id;
    if (theme < 0) return -1;
    const size_t split = track_name.rfind('_');
    if (split == std::string::npos) return -1;
    const int suffix = std::atoi(track_name.c_str() + split + 1);
    if (suffix <= 0) return -1;
    return theme + suffix - 1;
}

// Client loader 0x4D4180 names gimmick nifs per track id at identity index format starts at one
struct GimmickNifLoad {
    int track_id;
    const char* format;
    int count;
    // The GimmickClass of gimmicks h the hit test of that name carries
    int gimmick_class;
};

const GimmickNifLoad kGimmickNifLoads[] = {
    {12, "tree_fairy_%02d", 1, 8}, {13, "tree_door_%02d", 1, 9}, {13, "mole_%02d", 1, 10},
    {20, "Ant_%02d", 1, 0},        {20, "Pierrot_%02d", 2, 3},   {21, "CookieMan_%02d", 1, 6},
    {22, "Chef_%02d", 1, 7},       {31, "scorpion_%02d", 1, 4},  {40, "toybox_%02d", 2, 5},
    {56, "lavaman_%02d", 1, 1},    {60, "Sheep_%02d", 1, 11},    {70, "cobra", 1, 14},
    {71, "Glass_%02d", 2, 17},     {71, "Turnstile_%02d", 1, 18}, {71, "Door_%02d", 1, 19},
    {72, "Turnstile_%02d", 1, 18}, {72, "Frame_%02d", 1, 21},     {72, "fountain_%02d", 1, 20},
};

// Two gimmicks take their rest pose from tables baked in the exe and chase the car from there
struct GimmickPlacedLoad {
    int track_id;
    const char* name;
    float scale;
    int count;
    const float (*positions)[3];
    const float* yaws;
    // The GimmickClass of gimmicks h mushman is 2 spider is 0xD
    int gimmick_class;
};

// Mushman rows at 0x5F1AC0 yaw 180 scale from gimmick mushman update 0x4DB500
const float kMushmanPositions[3][3] = {{467.2f, -195.4f, 15.4f}, {325.7f, -283.9f, 16.8f}, {541.3f, -83.9f, 14.4f}};
const float kMushmanYaws[3] = {180.f, 180.f, 180.f};
// Spider rows at 0x5F1B18 and 0x5F1B60 with the yaws at 0x5F1AE8 and 0x5F1B00
const float kSpiderPositionsSwamp01[6][3] = {{-212.066f, 21.749f, -3.593f}, {-212.066f, -3.423f, -3.593f},
                                             {47.062f, 734.788f, -25.763f}, {38.979f, 720.67f, -28.466f},
                                             {255.135f, 522.37f, -1.424f},  {315.821f, 366.892f, -9.837f}};
const float kSpiderYawsSwamp01[6] = {270.f, 270.f, 320.f, 320.f, 330.f, 330.f};
const float kSpiderPositionsSwamp03[6][3] = {{3.912f, 284.553f, 23.427f},    {-75.395f, 314.832f, 23.427f},
                                             {-107.612f, 353.701f, 21.376f}, {-253.934f, 422.374f, 20.079f},
                                             {-217.621f, 284.892f, 14.86f},  {-165.952f, 152.441f, 19.242f}};
const float kSpiderYawsSwamp03[6] = {270.f, 270.f, 320.f, 250.f, 330.f, 220.f};

const GimmickPlacedLoad kGimmickPlacedLoads[] = {
    {11, "mushman", 0.2f, 3, kMushmanPositions, kMushmanYaws, 2},
    {80, "swa_spider", 1.f, 6, kSpiderPositionsSwamp01, kSpiderYawsSwamp01, 13},
    {82, "swa_spider", 1.f, 6, kSpiderPositionsSwamp03, kSpiderYawsSwamp03, 13},
};

// The gimmick and drum updates lower the node by this before the set translate
constexpr float kPlacedGimmickDrop = 0.8f;
constexpr float kDrumDrop = 0.2f;
constexpr float kDegToRad = 3.14159265f / 180.f;

// Scale rotate translate order bx RotateZ of plus A equals client RotationZ of minus A so yaw is negated
void client_yaw_world(float x, float y, float z, float yaw_degrees, float scale, float out[16]) {
    float scaling[16];
    bx::mtxScale(scaling, scale);
    float rotate[16];
    bx::mtxRotateZ(rotate, yaw_degrees * kDegToRad);
    float translate[16];
    bx::mtxTranslate(translate, x, y, z);
    float scaled[16];
    bx::mtxMul(scaled, scaling, rotate);
    bx::mtxMul(out, scaled, translate);
}

// Formats one client gimmick name with its one based index
std::string gimmick_file_name(const char* format, int index) {
    char name[64];
    std::snprintf(name, sizeof(name), format, index);
    return std::string(name) + ".nif";
}

// Loads the gimmick nifs the client names for this track
void load_track_gimmicks(const fs::path& track_dir, const fs::path& map_dir, MapScene& scene,
                         std::vector<TrackGimmick>& gimmicks) {
    std::error_code ignored;
    const fs::path gimmick_dir = track_dir / "Gimmick";
    if (!fs::exists(gimmick_dir, ignored)) return;
    const int track_id = track_id_from_folder(track_dir);
    std::vector<std::string> named;
    for (const GimmickNifLoad& load : kGimmickNifLoads) {
        if (load.track_id != track_id) continue;
        for (int index = 1; index <= load.count; ++index) {
            const std::string file_name = gimmick_file_name(load.format, index);
            named.push_back(lower_ascii(file_name));
            fs::path nif_path;
            if (!find_entry_ci(gimmick_dir, file_name, nif_path)) {
                std::cout << "[track] Gimmick/" << file_name << " named by the client is missing\n";
                continue;
            }
            std::string error;
            if (!add_prop_at_identity(nif_path.string(), map_dir, SceneLayer::Props, scene, error)) {
                std::cout << "[track] " << file_name << " " << error << "\n";
                continue;
            }
            const ModelBound& bound = scene.prop_models.back().bound;
            // The identity loads stand where the nif bakes them so the bound centre is the world spot
            TrackGimmick row;
            row.position[0] = bound.center[0];
            row.position[1] = bound.center[1];
            row.position[2] = bound.center[2];
            row.gimmick_class = load.gimmick_class;
            gimmicks.push_back(row);
            std::cout << "[track] gimmick " << nif_path.filename().string()
                      << " at its baked transform, rest bound centre " << bound.center[0] << " "
                      << bound.center[1] << " " << bound.center[2] << "\n";
        }
    }
    for (const GimmickPlacedLoad& load : kGimmickPlacedLoads) {
        if (load.track_id != track_id) continue;
        const std::string file_name = std::string(load.name) + ".nif";
        named.push_back(lower_ascii(file_name));
        fs::path nif_path;
        if (!find_entry_ci(gimmick_dir, file_name, nif_path)) {
            std::cout << "[track] Gimmick/" << file_name << " named by the client is missing\n";
            continue;
        }
        size_t model_index = 0;
        std::string error;
        if (!add_prop_model(nif_path.string(), map_dir, scene, model_index, error)) {
            std::cout << "[track] " << file_name << " " << error << "\n";
            continue;
        }
        for (int index = 0; index < load.count; ++index) {
            PropInstance instance;
            instance.model_index = model_index;
            instance.layer = SceneLayer::Props;
            const float* position = load.positions[index];
            client_yaw_world(position[0], position[1], position[2] - kPlacedGimmickDrop, load.yaws[index],
                             load.scale, instance.world);
            scene.prop_instances.push_back(instance);
            TrackGimmick row;
            row.position[0] = position[0];
            row.position[1] = position[1];
            row.position[2] = position[2];
            row.gimmick_class = load.gimmick_class;
            gimmicks.push_back(row);
            std::cout << "[track] gimmick " << nif_path.filename().string() << " at " << position[0] << " "
                      << position[1] << " " << position[2] << " yaw " << load.yaws[index] << "\n";
        }
    }
    for (const auto& entry : fs::directory_iterator(gimmick_dir, ignored)) {
        if (!entry.is_regular_file(ignored)) continue;
        const std::string file_name = lower_ascii(entry.path().filename().string());
        if (file_name.size() < 4 || file_name.compare(file_name.size() - 4, 4, ".nif") != 0) continue;
        if (std::find(named.begin(), named.end(), file_name) != named.end()) continue;
        std::cout << "[track] Gimmick/" << entry.path().filename().string()
                  << " is named by no client loader, skipped\n";
    }
}

// The intact drum model per track id and its node scale from itemdrum models load 0x4BF3F0
struct DrumTheme {
    const char* model;
    float scale;
};

DrumTheme drum_theme_for_track(int track_id) {
    switch (track_id) {
    case 10: case 11: case 12: return {"FOR_Gimmick_02_1", 1.f};
    case 20: case 21: case 22: return {"cheese_01", 1.f};
    case 30: case 31: case 32: return {"de_Gimmick_01", 2.5f};
    case 40: case 41: return {"TOY_Gimmick_01", 1.5f};
    case 50: case 51: case 52: return {"bone_head_01", 1.f};
    case 60: case 61: return {"SN_Gimmick_02_1", 1.f};
    default: return {"de_Gimmick_01", 2.5f};
    }
}

// Client fscanf 0x48AF20 leaves yaw unset on a three column row fresh process zero gives yaw zero
bool read_itemdrum_rows(const fs::path& track_dir, std::vector<GimmickItemdrumRow>& out) {
    fs::path ini_path;
    if (!find_entry_ci(track_dir, "itemdrum.ini", ini_path)) return false;
    std::ifstream file(ini_path);
    if (!file) return false;
    std::string line;
    while (out.size() < GIMMICK_ITEMDRUM_MAX_LINES && std::getline(file, line)) {
        GimmickItemdrumRow row;
        const int fields = std::sscanf(line.c_str(), "%f,%f,%f,%f", &row.x, &row.y, &row.z, &row.field_3);
        if (fields < 3) continue;
        if (fields == 3) row.field_3 = 0.f;
        out.push_back(row);
    }
    return true;
}

// Every itemdrum row stands one theme drum from Item ItemDrum at the row with its yaw column
void load_item_drums(const fs::path& track_dir, const fs::path& map_dir, const ColTrack* collision,
                     MapScene& scene) {
    // The client skips the drums when no track record is up so missions and rooms get none
    const int track_id = track_id_from_folder(track_dir);
    if (track_id < 0) return;
    std::vector<GimmickItemdrumRow> rows;
    if (!read_itemdrum_rows(track_dir, rows) || rows.empty()) return;
    const fs::path public_dir = map_dir.parent_path().parent_path();
    fs::path item_dir;
    fs::path drum_dir;
    if (!find_entry_ci(public_dir, "Item", item_dir) || !find_entry_ci(item_dir, "ItemDrum", drum_dir)) {
        std::cout << "[track] no Item/ItemDrum folder above " << track_dir.string() << "\n";
        return;
    }
    const DrumTheme theme = drum_theme_for_track(track_id);
    fs::path nif_path;
    if (!find_entry_ci(drum_dir, std::string(theme.model) + ".nif", nif_path)) {
        std::cout << "[track] drum model " << theme.model << " is missing\n";
        return;
    }
    size_t model_index = 0;
    std::string error;
    if (!add_prop_model(nif_path.string(), map_dir, scene, model_index, error)) {
        std::cout << "[track] " << theme.model << " " << error << "\n";
        return;
    }
    for (const GimmickItemdrumRow& row : rows) {
        float point[3] = {row.x, row.y, row.z};
        if (collision != nullptr) {
            BspQuery probe;
            if (world_place_probe_local(probe, *collision, row.x, row.y, row.z, nullptr, false))
                world_ground_height_at(probe, point);
        }
        PropInstance instance;
        instance.model_index = model_index;
        instance.layer = SceneLayer::Props;
        client_yaw_world(point[0], point[1], point[2] - kDrumDrop, row.field_3 + 270.f, theme.scale,
                         instance.world);
        scene.prop_instances.push_back(instance);
    }
    std::cout << "[track] " << rows.size() << " drums of " << theme.model << " on the itemdrum rows\n";
}

// Item box model one per itembox ini row nif's own NiTransformControllers spin and pulse the glow no extra code needed
void load_item_boxes(const fs::path& track_dir, const fs::path& map_dir, TrackScene& out) {
    std::vector<GimmickItemboxRow> rows;
    std::string error;
    if (!gimmick_load_itembox(track_dir.string(), rows, error) || rows.empty()) return;
    const fs::path public_dir = map_dir.parent_path().parent_path();
    fs::path item_dir, box_dir, type_dir, nif_path;
    if (!find_entry_ci(public_dir, "Item", item_dir) ||
        !find_entry_ci(item_dir, "ItemBox", box_dir) ||
        !find_entry_ci(box_dir, "Type_01", type_dir) ||
        !find_entry_ci(type_dir, "itembox_01.nif", nif_path)) {
        std::cout << "[track] no Item ItemBox Type 01 itembox 01 nif above " << track_dir.string()
                  << "\n";
        return;
    }
    NifModelRequest request;
    request.nif_path = nif_path.string();
    request.texture_dir = type_dir.string();
    if (!load_prop_model(request, out.item_box_model, error)) {
        std::cout << "[track] itembox_01.nif " << error << "\n";
        return;
    }
    resolve_model_textures(type_dir, map_dir, out.item_box_model);
    out.has_item_box = true;
    out.item_boxes.reserve(rows.size());
    for (const GimmickItemboxRow& row : rows) {
        TrackItemBox box;
        box.position[0] = row.x;
        box.position[1] = row.y;
        box.position[2] = row.z;
        out.item_boxes.push_back(box);
    }
    std::cout << "[track] " << out.item_boxes.size() << " item boxes on the itembox rows\n";
}

// A face normal from two edges zero length falls back to up
void face_normal(const float p0[3], const float p1[3], const float p2[3], float out[3]) {
    const float e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
    const float e2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
    out[0] = e1[1] * e2[2] - e1[2] * e2[1];
    out[1] = e1[2] * e2[0] - e1[0] * e2[2];
    out[2] = e1[0] * e2[1] - e1[1] * e2[0];
    const float length = std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    if (length < 1e-8f) { out[0] = 0.f; out[1] = 0.f; out[2] = 1.f; return; }
    out[0] /= length; out[1] /= length; out[2] /= length;
}

// Emissive vertex colour ignores scene light depth test always passes never writes so it draws through the track mesh
void make_unlit_vertex_coloured(PropPart& part, bool translucent) {
    part.has_vertex_colours = true;
    part.surface.vertex_colour.lighting = NifLightingMode::EmissiveOnly;
    part.surface.vertex_colour.vertex = NifVertexMode::Emissive;
    part.surface.depth.flags = 0x1u;
    // 0xED is the usual NiAlphaProperty flags word blend on source alpha over inverse source alpha
    if (translucent) part.surface.alpha.flags = 0xEDu;
}

// One triangle added both ways so a fixed backface cull never hides it
void add_triangle_both_sides(PropPart& part, const float p0[3], const float p1[3],
                             const float p2[3], uint32_t colour) {
    float normal[3];
    face_normal(p0, p1, p2, normal);
    const float points[3][3] = {{p0[0], p0[1], p0[2]}, {p1[0], p1[1], p1[2]}, {p2[0], p2[1], p2[2]}};
    for (int side = 0; side < 2; ++side) {
        const uint32_t base = static_cast<uint32_t>(part.vertices.size());
        for (int corner = 0; corner < 3; ++corner) {
            SceneVertex vertex;
            vertex.x = points[corner][0];
            vertex.y = points[corner][1];
            vertex.z = points[corner][2];
            const float sign = side == 0 ? 1.f : -1.f;
            vertex.normal_x = normal[0] * sign;
            vertex.normal_y = normal[1] * sign;
            vertex.normal_z = normal[2] * sign;
            vertex.abgr = colour;
            part.vertices.push_back(vertex);
        }
        if (side == 0) {
            part.indices.push_back(base + 0);
            part.indices.push_back(base + 1);
            part.indices.push_back(base + 2);
        } else {
            part.indices.push_back(base + 0);
            part.indices.push_back(base + 2);
            part.indices.push_back(base + 1);
        }
    }
}

// One box centred on its own local origin a marker world matrix places it
PropModel build_box_model(const std::string& name, float min_x, float max_x, float min_y,
                          float max_y, float min_z, float max_z, uint32_t colour) {
    PropModel model;
    model.name = name;
    model.lit_by_map_ambient = false;
    PropPart part;
    make_unlit_vertex_coloured(part, false);
    const float c[8][3] = {
        {min_x, min_y, min_z}, {max_x, min_y, min_z}, {max_x, max_y, min_z}, {min_x, max_y, min_z},
        {min_x, min_y, max_z}, {max_x, min_y, max_z}, {max_x, max_y, max_z}, {min_x, max_y, max_z},
    };
    const int faces[6][4] = {
        {0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4}, {3, 2, 6, 7}, {0, 3, 7, 4}, {1, 2, 6, 5},
    };
    for (const auto& face : faces) {
        add_triangle_both_sides(part, c[face[0]], c[face[1]], c[face[2]], colour);
        add_triangle_both_sides(part, c[face[0]], c[face[2]], c[face[3]], colour);
    }
    part.bound.center[0] = (min_x + max_x) * 0.5f;
    part.bound.center[1] = (min_y + max_y) * 0.5f;
    part.bound.center[2] = (min_z + max_z) * 0.5f;
    model.parts.push_back(std::move(part));
    return model;
}

// Heading zero drives toward minus x clockwise RageZone ghost Race 01 motion angle is 180 minus yaw byte angle
void marker_world(float x, float y, float z, float heading_degrees, float out[16]) {
    float rotate[16];
    bx::mtxRotateZ(rotate, heading_degrees * (3.14159265f / 180.f));
    float translate[16];
    bx::mtxTranslate(translate, x, y, z);
    bx::mtxMul(out, rotate, translate);
}

// One row of a gimmick csv turned into an unoriented marker instance
template <typename Row>
void append_position_markers(const std::vector<Row>& rows, size_t model_index,
                             std::vector<PropInstance>& out) {
    for (const Row& row : rows) {
        PropInstance instance;
        instance.model_index = model_index;
        instance.layer = SceneLayer::Props;
        marker_world(row.x, row.y, row.z, 0.f, instance.world);
        out.push_back(instance);
    }
}

// Two boundary lines of the col ground plane crossed to get one cell corner
bool intersect_edges(const ColEdge& first, const ColEdge& second, float& x, float& z) {
    const float det = first.a * second.b - second.a * first.b;
    if (std::fabs(det) < 1e-8f) return false;
    x = (first.b * second.c - second.b * first.c) / det;
    z = (second.a * first.c - first.a * second.c) / det;
    return true;
}

// One col piece cell array turned into flat coloured triangles on the ground plane
void append_collision_cells(const ColPiece& piece, PropPart& part, uint32_t colour) {
    for (const ColCell& cell : piece.cells) {
        uint16_t real_index[3];
        bool valid = true;
        for (int i = 0; i < 3; ++i) {
            if (cell.edge[i] == 0xffffu) { valid = false; break; }
            real_index[i] = cell.edge[i] & 0x7fffu;
            if (real_index[i] >= piece.edges.size()) { valid = false; break; }
        }
        if (!valid) continue;
        float corner_x[3], corner_z[3];
        bool ok = true;
        for (int i = 0; i < 3 && ok; ++i) {
            const ColEdge& a = piece.edges[real_index[i]];
            const ColEdge& b = piece.edges[real_index[(i + 1) % 3]];
            ok = intersect_edges(a, b, corner_x[i], corner_z[i]);
        }
        if (!ok) continue;
        float points[3][3];
        bool finite = true;
        for (int i = 0; i < 3; ++i) {
            const float height = -(cell.heightA * corner_x[i] + cell.heightB * corner_z[i] + cell.heightD);
            if (!std::isfinite(height) || !std::isfinite(corner_x[i]) || !std::isfinite(corner_z[i])) {
                finite = false;
                break;
            }
            // The col ground plane is x and z render world keeps z up so z becomes height
            points[i][0] = corner_x[i];
            points[i][1] = corner_z[i];
            points[i][2] = height;
        }
        if (!finite) continue;
        add_triangle_both_sides(part, points[0], points[1], points[2], colour);
    }
}

PropModel build_collision_model(const ColTrack& track, uint32_t colour) {
    PropModel model;
    model.name = "collision";
    model.lit_by_map_ambient = false;
    PropPart part;
    make_unlit_vertex_coloured(part, true);
    for (const ColPiece& piece : track.pieces) append_collision_cells(piece, part, colour);
    model.parts.push_back(std::move(part));
    return model;
}

// Day six to eighteen night the rest only used when a night nif ships too
bool sky_hour_is_day(int hour) { return hour >= 6 && hour < 19; }

void add_sky_phase(const std::string& nif_path, const fs::path& map_dir, SkyTime time,
                   const std::string& phase_name, MapScene& scene, std::string& error) {
    NifModelRequest request;
    request.nif_path = nif_path;
    request.texture_dir = fs::path(nif_path).parent_path().string();
    SkyPhase phase;
    if (!load_prop_model(request, phase.model, error)) return;
    resolve_model_textures(fs::path(request.texture_dir), map_dir, phase.model);
    phase.name = phase_name;
    phase.time = time;
    scene.sky_phases.push_back(std::move(phase));
}

// One line per load step the span of the step so a slow race entry tells where its seconds went
struct StepClock {
    std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
    void mark(const char* step) {
        const auto now = std::chrono::steady_clock::now();
        std::printf("[time] track %s %.0f ms\n", step, std::chrono::duration<double, std::milli>(now - last).count());
        last = now;
    }
};

} // namespace

bool load_track_scene(const TrackSceneRequest& request, TrackScene& out, std::string& error) {
    out = TrackScene{};
    const fs::path track_dir(request.track_dir);
    std::error_code ignored;
    if (!fs::exists(track_dir, ignored)) {
        error = "no such track folder " + request.track_dir;
        return false;
    }
    const fs::path map_dir = track_dir.parent_path();
    const fs::path world_dir = map_dir.parent_path();
    // Data Public World theme track is five folders under the game the pak sits there
    if (request.point_textures_at_pak) point_texture_cache_at_pak(world_dir.parent_path().parent_path().parent_path());
    out.scene.tile_id = track_dir.filename().string();
    reset_scene_bounds(out.scene);
    StepClock clock;

    const fs::path track_nif = track_dir / "track.nif";
    if (!fs::exists(track_nif, ignored)) {
        error = "no track.nif in " + request.track_dir;
        return false;
    }
    if (!add_prop_at_identity(track_nif.string(), map_dir, SceneLayer::Terrain, out.scene, error))
        return false;
    clock.mark("nif");

    const fs::path geometry_nif = track_dir / "geometry.nif";
    if (request.load_geometry && fs::exists(geometry_nif, ignored)) {
        std::string geometry_error;
        if (!add_prop_at_identity(geometry_nif.string(), map_dir, SceneLayer::Terrain, out.scene,
                                  geometry_error))
            std::cout << "[track] geometry.nif " << geometry_error << "\n";
    }

    for (const PropModel& model : out.scene.prop_models) {
        for (const PropPart& part : model.parts) {
            for (const SceneVertex& vertex : part.vertices) {
                const float point[3] = {vertex.x, vertex.y, vertex.z};
                expand_scene_bounds(point, out.scene);
            }
        }
    }

    const fs::path sky_nif = track_dir / "sky.nif";
    const fs::path sky_night_nif = track_dir / "sky_night.nif";
    const bool has_day_sky = fs::exists(sky_nif, ignored);
    const bool has_night_sky = fs::exists(sky_night_nif, ignored);
    if (has_day_sky) {
        std::string sky_error;
        add_sky_phase(sky_nif.string(), map_dir, SkyTime::Day, "day", out.scene, sky_error);
        if (!sky_error.empty()) std::cout << "[track] sky.nif " << sky_error << "\n";
    }
    if (has_night_sky) {
        std::string sky_error;
        add_sky_phase(sky_night_nif.string(), map_dir, SkyTime::Night, "night", out.scene, sky_error);
        if (!sky_error.empty()) std::cout << "[track] sky_night.nif " << sky_error << "\n";
    }
    if (!out.scene.sky_phases.empty()) {
        const uint8_t day_index = 0;
        const uint8_t night_index = has_day_sky && has_night_sky ? 1 : 0;
        for (int hour = 0; hour < kHoursPerDay; ++hour)
            out.scene.sky_phase_of_hour[hour] =
                has_day_sky && (!has_night_sky || sky_hour_is_day(hour)) ? day_index : night_index;
    }
    clock.mark("sky");

    load_track_gimmicks(track_dir, map_dir, out.scene, out.gimmicks);
    clock.mark("gimmicks");

    if (request.load_sun) {
        const fs::path light_nif = world_dir / "Light.nif";
        if (fs::exists(light_nif, ignored)) {
            KnC::Render::LightRig rig;
            std::string light_error;
            if (load_light_rig(light_nif.string(), rig, light_error)) {
                apply_light_rig(rig, out.scene);
            } else {
                std::cout << "[track] Light.nif " << light_error << "\n";
            }
        } else {
            std::cout << "[track] no Light.nif above " << request.track_dir << "\n";
        }
    }

    if (request.load_collision) {
        std::string collision_error;
        if (world_load_track_pieces(out.collision, request.track_dir, collision_error)) {
            PropModel collision_model = build_collision_model(out.collision, pack_abgr(70, 160, 255, 110));
            const size_t index = out.scene.prop_models.size();
            out.scene.prop_models.push_back(std::move(collision_model));
            PropInstance instance;
            instance.model_index = index;
            instance.layer = SceneLayer::Collision;
            out.scene.prop_instances.push_back(instance);
        } else {
            std::cout << "[track] " << collision_error << "\n";
        }
    }

    clock.mark("light");
    // The drums snap to the col ground like the client when the col is loaded
    if (request.load_item_drums)
        load_item_drums(track_dir, map_dir, out.collision.pieces.empty() ? nullptr : &out.collision, out.scene);
    clock.mark("drums");

    if (request.load_item_boxes) load_item_boxes(track_dir, map_dir, out);
    clock.mark("item boxes");

    std::string start_error;
    gimmick_load_start(request.track_dir, out.start_rows, start_error);
    if (!out.start_rows.empty())
        std::cout << "[track] " << out.start_rows.size() << " start rows, row 0 heading "
                  << out.start_rows[0].heading << " degrees, zero drives toward minus x\n";

    if (request.load_markers) {
        const uint32_t start_colour = pack_abgr(40, 230, 70, 255);
        const uint32_t gimmick_colour = pack_abgr(230, 40, 190, 255);
        out.marker_models.push_back(
            build_box_model("start marker", -2.4f, 0.3f, -0.6f, 0.6f, -0.6f, 0.6f, start_colour));
        out.marker_models.push_back(
            build_box_model("gimmick marker", -1.f, 1.f, -1.f, 1.f, -1.f, 1.f, gimmick_colour));

        for (const GimmickStartRow& row : out.start_rows) {
            PropInstance instance;
            instance.model_index = 0;
            instance.layer = SceneLayer::Props;
            marker_world(row.x, row.y, row.z, row.heading, instance.world);
            out.marker_instances.push_back(instance);
        }

        std::vector<GimmickBoostRow> boost_rows;
        std::string boost_error;
        if (gimmick_load_boost(request.track_dir, boost_rows, boost_error)) {
            for (const GimmickBoostRow& row : boost_rows) {
                PropInstance instance;
                instance.model_index = 1;
                instance.layer = SceneLayer::Props;
                marker_world(row.field_1, row.field_2, row.field_3, 0.f, instance.world);
                out.marker_instances.push_back(instance);
            }
        }

        std::vector<GimmickItemboxRow> itembox_rows;
        std::string itembox_error;
        if (gimmick_load_itembox(request.track_dir, itembox_rows, itembox_error))
            append_position_markers(itembox_rows, 1, out.marker_instances);

        std::vector<GimmickItembiteRow> itembite_rows;
        std::string itembite_error;
        if (gimmick_load_itembite(request.track_dir, itembite_rows, itembite_error))
            append_position_markers(itembite_rows, 1, out.marker_instances);

        std::vector<GimmickItemdrumRow> itemdrum_rows;
        std::string itemdrum_error;
        if (gimmick_load_itemdrum(request.track_dir, itemdrum_rows, itemdrum_error))
            append_position_markers(itemdrum_rows, 1, out.marker_instances);

        for (int32_t follow_index = GIMMICK_FOLLOW_FILE_MIN; follow_index <= GIMMICK_FOLLOW_FILE_MAX;
            ++follow_index) {
            std::vector<GimmickFollowRow> follow_rows;
            std::string follow_error;
            if (!gimmick_load_follow(request.track_dir, follow_index, follow_rows, follow_error)) continue;
            for (const GimmickFollowRow& row : follow_rows) {
                PropInstance instance;
                instance.model_index = 1;
                instance.layer = SceneLayer::Props;
                marker_world(row.field_0, row.field_1, row.field_2, 0.f, instance.world);
                out.marker_instances.push_back(instance);
            }
        }
    }

    return true;
}

}
