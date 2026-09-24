#include "tools/track_scene/ghost_car.h"

#include "engine/formats/kfm_reader.h"
#include "engine/formats/nif_reader.h"
#include "engine/formats/nif_scene_graph.h"
#include "engine/formats/nif_skeleton.h"

#include <bx/math.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace KnC::Tools {

namespace fs = std::filesystem;
using namespace KnC::Render;

namespace {

constexpr float kDegToRad = 3.14159265f / 180.f;
constexpr float kRadToDeg = 180.f / 3.14159265f;
// The lean side counts over 3 units a second in car visual update the 0x5A32B8 constant
constexpr float kGhostLeanSpeedGate = 3.f;
// A remote car eases car 0x32E4 to plus or minus 0x5A6ACC 30 degrees by the lean side
constexpr float kRemoteSteerRadians = 0.5236f;
// An eighth of the way per 20 ms tick the 0x5A32AC blend of the remote mover
constexpr float kRemoteSteerBlend = 0.125f;
constexpr float kRemoteTickSeconds = 0.02f;
// car visual update adds speed times minus 0x5A69A4 to every spin each frame at 60 frames a second
constexpr float kRemoteSpinPerUnit = 0.01f * 60.f;
// The ground shake gate 0x59F404 10 the cap 0x5A1648 100 and the scale 0x5A69A0 4e-6
constexpr float kGhostShakeSpeedGate = 10.f;
constexpr float kGhostShakeSpeedCap = 100.f;
constexpr float kGhostShakePerUnit = 4.0e-6f;

std::string lower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// Composed node transform translation and a row major 3x3 rotation
struct NodeXf {
    float translation[3] = {0.f, 0.f, 0.f};
    float rotation[9]    = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};
    float scale          = 1.f;
};

NodeXf from_nif_transform(const KnC::NifTransform& transform) {
    NodeXf xf;
    for (int i = 0; i < 3; ++i) xf.translation[i] = transform.translation[i];
    for (int i = 0; i < 9; ++i) xf.rotation[i] = transform.rotation[i];
    xf.scale = transform.scale;
    return xf;
}

// Parent applied first then local same order the nif model builder bakes nodes with
NodeXf compose(const NodeXf& parent, const NodeXf& local) {
    NodeXf result;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col) {
            float sum = 0.f;
            for (int k = 0; k < 3; ++k) sum += parent.rotation[row * 3 + k] * local.rotation[k * 3 + col];
            result.rotation[row * 3 + col] = sum;
        }
    for (int row = 0; row < 3; ++row) {
        const float rotated = parent.rotation[row * 3 + 0] * local.translation[0] +
                              parent.rotation[row * 3 + 1] * local.translation[1] +
                              parent.rotation[row * 3 + 2] * local.translation[2];
        result.translation[row] = parent.translation[row] + parent.scale * rotated;
    }
    result.scale = parent.scale * local.scale;
    return result;
}

// Depth first walk collects the world transform of every named node by name
void collect_named_nodes(const KnC::NifScene& scene, uint32_t index, const NodeXf& parent,
                         std::vector<std::pair<std::string, NodeXf>>& out) {
    if (index >= scene.blocks.size()) return;
    const KnC::NifBlock& block = scene.blocks[index];
    const NodeXf local = block.has_transform ? from_nif_transform(block.transform) : NodeXf{};
    const NodeXf world = compose(parent, local);
    if (!block.name.empty()) out.emplace_back(block.name, world);
    for (uint32_t child : block.children) collect_named_nodes(scene, child, world, out);
}

// Row 3 holds the translation same bx mtxTranslate layout row vector times matrix
void node_xf_to_matrix(const NodeXf& xf, float out[16]) {
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col) out[row * 4 + col] = xf.rotation[col * 3 + row] * xf.scale;
    out[3] = 0.f; out[7] = 0.f; out[11] = 0.f;
    out[12] = xf.translation[0];
    out[13] = xf.translation[1];
    out[14] = xf.translation[2];
    out[15] = 1.f;
}

// Finds the world transform of a named dummy node returns false when the body has none
bool find_named_dummy(const KnC::NifScene& scene, const char* name, float out[16]) {
    const std::string wanted = lower(name);
    std::vector<std::pair<std::string, NodeXf>> named;
    for (uint32_t root : scene.roots) collect_named_nodes(scene, root, NodeXf{}, named);
    for (const auto& entry : named) {
        if (lower(entry.first) == wanted) {
            node_xf_to_matrix(entry.second, out);
            return true;
        }
    }
    return false;
}

// Finds the world transform of the wheel dummy node returns false when the body has none
bool find_wheel_dummy(const KnC::NifScene& scene, int wheelNumber, float out[16]) {
    char name[16];
    std::snprintf(name, sizeof(name), "O_WHEEL%02d", wheelNumber);
    return find_named_dummy(scene, name, out);
}

// The BODYCOLOR paint folder beside the body nif any case of the two names empty when missing
std::string find_paint_dir(const fs::path& body_dir, const std::string& paint) {
    if (paint.empty()) return std::string();
    std::error_code ignored;
    for (const auto& folder : fs::directory_iterator(body_dir, ignored)) {
        if (!folder.is_directory(ignored) || lower(folder.path().filename().string()) != "bodycolor") continue;
        for (const auto& entry : fs::directory_iterator(folder.path(), ignored))
            if (entry.is_directory(ignored) && lower(entry.path().filename().string()) == lower(paint))
                return entry.path().string();
    }
    return std::string();
}

// Trims the blanks GetPrivateProfileString ignores around a section a key or a value
std::string trim(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return std::string();
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

// Every section of a plain ini keyed lower case the way the client reads driver pos
std::map<std::string, std::map<std::string, std::string>> read_ini(const std::string& path) {
    std::map<std::string, std::map<std::string, std::string>> sections;
    std::ifstream file(path);
    std::string line, section;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = lower(trim(line.substr(1, line.size() - 2)));
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        sections[section][lower(trim(line.substr(0, equals)))] = trim(line.substr(equals + 1));
    }
    return sections;
}

// Define Driver driver pos ini of the driver folder name walking up from the body nif
std::string find_driver_pos_ini(const fs::path& body_nif) {
    const std::string driver = lower(body_nif.parent_path().filename().string());
    std::error_code ignored;
    fs::path dir = fs::absolute(body_nif, ignored).parent_path();
    for (int depth = 0; depth < 8 && !dir.empty(); ++depth) {
        const fs::path candidate = dir / "Define" / "Driver" / ("driver_pos_" + driver + ".ini");
        if (fs::exists(candidate, ignored)) return candidate.string();
        if (dir == dir.parent_path()) break;
        dir = dir.parent_path();
    }
    return std::string();
}

// Every clip of the body its KFM beside the nif lists them by sequence id and KF position
void find_body_clips(const fs::path& body_nif, std::vector<CharacterClipRequest>& out) {
    const fs::path body_dir = body_nif.parent_path();
    const fs::path kfm_path = body_dir / (body_nif.stem().string() + ".kfm");
    KnC::KfmFile kfm;
    std::string kfm_error;
    if (!KnC::read_kfm(kfm_path.string(), kfm, kfm_error)) return;
    for (const KnC::KfmSequence& sequence : kfm.sequences) {
        CharacterClipRequest clip;
        clip.kf_path = (body_dir / KnC::kfm_relative_path(sequence.kf_path)).string();
        clip.name = sequence.name;
        clip.sequence_id = static_cast<int32_t>(sequence.sequence_id);
        // The right drift loop KF holds two sequences the KFM points at the second one
        clip.sequence_index = sequence.animation_index;
        std::error_code ignored;
        if (fs::exists(clip.kf_path, ignored)) out.push_back(clip);
    }
}

// The five accessory attach nodes of the exe table 0x5EA910 index 2 to 6
const char* const kSlotNodes[7] = {"O_PAINT", "O_NAME", "O_BODY", "O_FACE", "O_HEAD", "O_GLASS", "O_BACK"};

// The parts worn by a driver asset one list per asset for the whole process
std::map<std::string, std::vector<std::string>>& driver_part_registry() {
    static std::map<std::string, std::vector<std::string>> registry;
    return registry;
}

// The attach node the part file names itself its root child carries one of the five names
std::string part_attach_node(const KnC::NifScene& scene) {
    std::vector<std::pair<std::string, NodeXf>> named;
    for (uint32_t root : scene.roots) collect_named_nodes(scene, root, NodeXf{}, named);
    for (const auto& entry : named)
        for (int slot = 2; slot <= 6; ++slot)
            if (lower(entry.first) == lower(kSlotNodes[slot])) return kSlotNodes[slot];
    return std::string();
}

// A composed placement back as a local transform the palette bind takes one
KnC::NifTransform transform_of_placement(const KnC::NifPlacement& placement) {
    KnC::NifTransform out;
    for (int i = 0; i < 3; ++i) out.translation[i] = placement.translation[i];
    for (int i = 0; i < 9; ++i) out.rotation[i] = placement.rotation[i];
    out.scale = placement.scale;
    return out;
}

// Parent applied first then local same order as compose placement of the scene graph
KnC::NifTransform compose_transform(const KnC::NifTransform& parent, const KnC::NifTransform& local) {
    KnC::NifPlacement as_parent;
    for (int i = 0; i < 3; ++i) as_parent.translation[i] = parent.translation[i];
    for (int i = 0; i < 9; ++i) as_parent.rotation[i] = parent.rotation[i];
    as_parent.scale = parent.scale;
    return transform_of_placement(KnC::compose_placement(as_parent, local));
}

// Half the z extent of the mesh the wheel disc bakes around its own origin
float wheel_radius_of(const PropModel& wheel) {
    float low = 0.f, high = 0.f;
    bool any = false;
    for (const PropPart& part : wheel.parts)
        for (const SceneVertex& vertex : part.vertices) {
            if (!any) { low = high = vertex.z; any = true; }
            low = std::min(low, vertex.z);
            high = std::max(high, vertex.z);
        }
    const float radius = 0.5f * (high - low);
    return radius > 1e-3f ? radius : 0.5f;
}

} // namespace

std::string find_texture(const std::string& root, const std::string& wanted) {
    std::error_code ignored;
    if (wanted.empty() || fs::exists(wanted, ignored)) return wanted;
    const std::string name = lower(fs::path(wanted).filename().string());
    const fs::path root_path(root);
    std::string found;
    for (const fs::path& base : {root_path, root_path.parent_path()}) {
        for (const auto& entry : fs::recursive_directory_iterator(base, ignored)) {
            if (!entry.is_regular_file(ignored)) continue;
            if (lower(entry.path().filename().string()) != name) continue;
            const std::string path = entry.path().string();
            if (found.empty() || lower(path).find("high") != std::string::npos) found = path;
        }
        if (!found.empty()) return found;
    }
    return wanted;
}

void resolve_textures(const std::string& root, PropModel& model) {
    for (PropPart& part : model.parts) {
        part.texture_path = find_texture(root, part.texture_path);
        if (!part.detail_texture_path.empty())
            part.detail_texture_path = find_texture(root, part.detail_texture_path);
    }
    // The flip files of a mesh part sit beside its base map
    for (KnC::Render::FlipAnimation& flip : model.animation.flip_channels)
        for (std::string& path : flip.textures) path = find_texture(root, path);
    for (ParticleSystemDefinition& system : model.particle_systems) {
        system.texture_path = find_texture(root, system.texture_path);
        // The flip files of a sprite sit beside the base map
        for (std::string& path : system.flip_textures) path = find_texture(root, path);
    }
}

void resolve_textures(const std::string& root, CharacterModel& model) {
    for (SkinnedPart& part : model.parts) part.texture_path = find_texture(root, part.texture_path);
}

namespace {

// Every kart and driver the process built keyed by its files the room built them before the race
struct ParsedModels {
    std::mutex lock;
    std::map<std::string, std::shared_ptr<const GhostCar>> cars;
    std::map<std::string, std::shared_ptr<const GhostDriver>> drivers;
};

ParsedModels& parsed_models() {
    static ParsedModels shared;
    return shared;
}

std::string model_key(const std::string& nif, const std::string& more) {
    std::string key = nif;
    for (char& letter : key) {
        letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
        if (letter == '\\') letter = '/';
    }
    return key + "|" + more;
}

bool load_ghost_car_fresh(const std::string& body_nif, GhostCar& out, std::string& error,
                          const std::string& paint);
bool load_ghost_driver_fresh(const std::string& body_nif, const std::string& chassis, GhostDriver& out,
                             std::string& error);

}

bool load_ghost_car(const std::string& body_nif, GhostCar& out, std::string& error,
                    const std::string& paint) {
    const std::string key = model_key(body_nif, paint);
    {
        ParsedModels& models = parsed_models();
        std::lock_guard<std::mutex> lock(models.lock);
        const auto found = models.cars.find(key);
        if (found != models.cars.end()) {
            out = *found->second;
            return true;
        }
    }
    if (!load_ghost_car_fresh(body_nif, out, error, paint)) return false;
    ParsedModels& models = parsed_models();
    std::lock_guard<std::mutex> lock(models.lock);
    models.cars[key] = std::make_shared<const GhostCar>(out);
    return true;
}

namespace {

bool load_ghost_car_fresh(const std::string& body_nif, GhostCar& out, std::string& error,
                          const std::string& paint) {
    out = GhostCar{};
    const fs::path body_dir = fs::path(body_nif).parent_path();

    NifModelRequest body_request;
    body_request.nif_path = body_nif;
    // The paint folder first the plain body textures stand in for what it lacks
    const std::string paint_dir = find_paint_dir(body_dir, paint);
    body_request.texture_dir = paint_dir.empty() ? body_dir.string() : paint_dir;
    if (!load_prop_model(body_request, out.body, error)) return false;
    resolve_textures(body_request.texture_dir, out.body);
    if (!paint_dir.empty()) resolve_textures(body_dir.string(), out.body);

    KnC::NifScene body_scene;
    std::string scene_error;
    if (!KnC::read_nif_scene(body_nif, body_scene, scene_error)) return true;  // body alone still usable

    for (int smokeNumber = 1; smokeNumber <= 2; ++smokeNumber) {
        char name[16];
        std::snprintf(name, sizeof(name), "O_SM%02d", smokeNumber);
        float dummy_world[16];
        if (!find_named_dummy(body_scene, name, dummy_world)) break;
        std::array<float, 16> local;
        for (int i = 0; i < 16; ++i) local[i] = dummy_world[i];
        out.smoke_local.push_back(local);
    }
    out.has_ant = find_named_dummy(body_scene, "O_ANT", out.ant_local.data());
    out.has_name = find_named_dummy(body_scene, "O_NAME", out.name_local.data());

    for (int wheelNumber = 1; wheelNumber <= 4; ++wheelNumber) {
        float dummy_world[16];
        if (!find_wheel_dummy(body_scene, wheelNumber, dummy_world)) continue;
        char file_name[32];
        std::snprintf(file_name, sizeof(file_name), "WHEEL%d.nif", wheelNumber);
        const fs::path wheel_path = body_dir / file_name;
        std::error_code ignored;
        if (!fs::exists(wheel_path, ignored)) continue;

        NifModelRequest wheel_request;
        wheel_request.nif_path = wheel_path.string();
        wheel_request.texture_dir = body_dir.string();
        PropModel wheel_model;
        std::string wheel_error;
        if (!load_prop_model(wheel_request, wheel_model, wheel_error)) continue;
        resolve_textures(wheel_request.texture_dir, wheel_model);

        std::array<float, 16> local;
        for (int i = 0; i < 16; ++i) local[i] = dummy_world[i];
        out.wheel_radius.push_back(wheel_radius_of(wheel_model));
        out.wheels.push_back(std::move(wheel_model));
        out.wheel_local.push_back(local);
    }
    return true;
}

}

bool load_kart_part_model(const std::string& nif, const std::string& texture_dir, PropModel& out,
                          std::string& error) {
    out = PropModel{};
    KnC::NifScene scene;
    if (!KnC::read_nif_scene(nif, scene, error)) return false;
    NifModelRequest request;
    request.nif_path = nif;
    request.texture_dir = texture_dir;
    if (!nif_has_skin(scene)) {
        build_prop_model(scene, request, out);
        return true;
    }
    // The skinned pieces ride their bones so the model is baked at the rest pose of the stream
    CharacterModelRequest skinned;
    skinned.nif_path = nif;
    skinned.texture_dir = texture_dir;
    CharacterModel model;
    if (!build_character_model(scene, skinned, model, error)) return false;
    SkinnedPose pose;
    evaluate_skinned_pose(model.rig, -1, 0.f, pose);
    out.name = model.name;
    for (size_t index = 0; index < model.parts.size(); ++index) {
        const SkinnedPart& part = model.parts[index];
        PropPart baked;
        baked.texture_path = part.texture_path;
        baked.surface = part.surface;
        baked.has_vertex_colours = part.has_vertex_colours;
        baked.indices = part.indices;
        // part offset counts floats each slot takes three rows of four the shader layout
        const std::size_t base = pose.part_offset.size() > index ? pose.part_offset[index] : 0;
        for (const SkinnedVertex& vertex : part.vertices) {
            SceneVertex placed = vertex.surface;
            float point[3] = {0.f, 0.f, 0.f};
            float normal[3] = {0.f, 0.f, 0.f};
            for (int slot = 0; slot < kWeightsPerVertex; ++slot) {
                const float weight = vertex.bone_weight[slot];
                if (weight <= 0.f) continue;
                const std::size_t row = base + static_cast<std::size_t>(vertex.bone_slot[slot]) * kBoneMatrixRows * 4;
                if (row + kBoneMatrixRows * 4 > pose.rows.size()) continue;
                for (int axis = 0; axis < 3; ++axis) {
                    const float* line = &pose.rows[row + static_cast<std::size_t>(axis) * 4];
                    point[axis] += weight * (line[0] * vertex.surface.x + line[1] * vertex.surface.y +
                                             line[2] * vertex.surface.z + line[3]);
                    normal[axis] += weight * (line[0] * vertex.surface.normal_x + line[1] * vertex.surface.normal_y +
                                              line[2] * vertex.surface.normal_z);
                }
            }
            placed.x = point[0]; placed.y = point[1]; placed.z = point[2];
            const float length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
            if (length > 1e-5f) {
                placed.normal_x = normal[0] / length;
                placed.normal_y = normal[1] / length;
                placed.normal_z = normal[2] / length;
            }
            baked.vertices.push_back(placed);
        }
        out.parts.push_back(std::move(baked));
    }
    return !out.parts.empty();
}

bool ghost_car_dummy(const std::string& body_nif, const char* name, float out[16]) {
    KnC::NifScene scene;
    std::string error;
    if (!KnC::read_nif_scene(body_nif, scene, error)) return false;
    return find_named_dummy(scene, name, out);
}

void ghost_car_instances(const GhostCar& car, size_t first_model_index, const float car_world[16],
                         const GhostWheelState& wheels, std::vector<PropInstance>& out) {
    PropInstance body;
    body.model_index = first_model_index;
    body.layer = SceneLayer::Props;
    for (int i = 0; i < 16; ++i) body.world[i] = car_world[i];
    out.push_back(body);

    // bx rotate angles run the other way round than the right hand rule hence the minus signs
    float steer[16];
    bx::mtxRotateZ(steer, -wheels.steer_degrees * kDegToRad);
    for (size_t i = 0; i < car.wheels.size(); ++i) {
        PropInstance wheel;
        wheel.model_index = first_model_index + 1 + i;
        wheel.layer = SceneLayer::Props;
        float spin[16];
        bx::mtxRotateY(spin, i < wheels.spin_radians.size() ? -wheels.spin_radians[i] : 0.f);
        // D3DX RotationX of minus 8 degrees on the even wheels plus 8 on the odd ones the camber
        float camber[16];
        bx::mtxRotateX(camber, (i % 2 == 0 ? 1.f : -1.f) * kGhostWheelCamberDeg * kDegToRad);
        // Spin about the axle first then the camber then the steer then the corner then the car
        float leaned[16];
        bx::mtxMul(leaned, spin, camber);
        float turned[16];
        if (i < 2) bx::mtxMul(turned, leaned, steer);
        else for (int k = 0; k < 16; ++k) turned[k] = leaned[k];
        // The corner lifted by the ground shake of the frame
        float corner[16];
        for (int k = 0; k < 16; ++k) corner[k] = car.wheel_local[i][k];
        if (i < wheels.lift.size()) corner[14] += wheels.lift[i];
        float placed[16];
        bx::mtxMul(placed, turned, corner);
        bx::mtxMul(wheel.world, placed, car_world);
        out.push_back(wheel);
    }
}

void ghost_wheels_shake(float speed, const float grip[4], bool on_ground, GhostWheelState& wheels) {
    wheels.lift.assign(4, 0.f);
    // car visual update 0x48ED6C only on the ground over 10 units a second the speed caps at 100
    if (!on_ground || speed <= kGhostShakeSpeedGate) return;
    const float capped = std::min(speed, kGhostShakeSpeedCap);
    for (size_t i = 0; i < 4; ++i) {
        const float roll = static_cast<float>(std::rand() % 600 - 300);
        wheels.lift[i] = roll * capped * grip[i] * kGhostShakePerUnit;
    }
}

void ghost_wheels_advance(const GhostCar& car, const float car_world[16], const float moved[3],
                          float dt, int turn_state, GhostWheelState& wheels) {
    // The body nose points down its own minus x row 0 of the matrix is that axis in the world
    const float forward[3] = {-car_world[0], -car_world[1], -car_world[2]};
    const float along = moved[0] * forward[0] + moved[1] * forward[1] + moved[2] * forward[2];
    wheels.spin_radians.resize(car.wheels.size(), 0.f);
    for (size_t i = 0; i < car.wheels.size(); ++i) {
        // D3DX RotationY of a negative angle rolls the top of the wheel toward the nose
        float& spin = wheels.spin_radians[i];
        spin -= along * kRemoteSpinPerUnit;
        if (spin > 1000.f || spin < -1000.f) spin = std::fmod(spin, 2.f * 3.14159265f);
    }
    // Lean side 1 is the left steer key 2 the right one the recording holds no steer angle
    const float target = turn_state == 1 ? kRemoteSteerRadians
                       : turn_state == 2 ? -kRemoteSteerRadians : 0.f;
    const float ticks = dt > 0.f ? dt / kRemoteTickSeconds : 1.f;
    const float blend = 1.f - std::pow(1.f - kRemoteSteerBlend, ticks);
    const float steer = wheels.steer_degrees * kDegToRad + (target - wheels.steer_degrees * kDegToRad) * blend;
    wheels.steer_degrees = steer * kRadToDeg;
}

void ghost_wheels_from_physics(const GhostCar& car, const float spin_radians[4], float steer_radians,
                               GhostWheelState& wheels) {
    wheels.spin_radians.resize(car.wheels.size(), 0.f);
    for (size_t i = 0; i < car.wheels.size() && i < 4; ++i) wheels.spin_radians[i] = spin_radians[i];
    wheels.steer_degrees = steer_radians * kRadToDeg;
}

bool find_driver_seat(const std::string& body_nif, const std::string& chassis, float out_seat[3]) {
    const std::string ini = find_driver_pos_ini(fs::path(body_nif));
    if (ini.empty()) return false;
    const auto sections = read_ini(ini);
    const auto section = sections.find(lower(chassis));
    if (section == sections.end()) return false;
    const char* keys[3] = {"x", "y", "z"};
    for (int axis = 0; axis < 3; ++axis) {
        const auto value = section->second.find(keys[axis]);
        if (value == section->second.end()) return false;
        out_seat[axis] = static_cast<float>(std::atof(value->second.c_str()));
    }
    return true;
}

int GhostDriver::clip_of(int sequence_id) const {
    if (sequence_id < 0 || static_cast<size_t>(sequence_id) >= clip_of_sequence.size()) return -1;
    return clip_of_sequence[static_cast<size_t>(sequence_id)];
}

const char* ghost_driver_slot_node(int equip_slot) {
    if (equip_slot < 2 || equip_slot > 6) return "";
    return kSlotNodes[equip_slot];
}

void ghost_driver_set_parts(const std::string& asset, const std::vector<std::string>& part_nifs) {
    driver_part_registry()[lower(asset)] = part_nifs;
}

const std::vector<std::string>& ghost_driver_parts(const std::string& asset) {
    static const std::vector<std::string> none;
    const auto it = driver_part_registry().find(lower(asset));
    return it == driver_part_registry().end() ? none : it->second;
}

std::string ghost_driver_parts_token(const std::string& asset) {
    std::string token;
    for (const std::string& path : ghost_driver_parts(asset)) {
        token += fs::path(path).stem().string();
        token += ' ';
    }
    return token;
}

namespace {

// BODYSET nif merged into body attach node rigid pieces drop
bool merge_driver_part(const std::string& part_nif, int attach, CharacterModel& body) {
    KnC::NifScene scene;
    std::string error;
    if (!KnC::read_nif_scene(part_nif, scene, error)) return false;
    CharacterModelRequest request;
    request.nif_path = part_nif;
    request.texture_dir = fs::path(part_nif).parent_path().string();
    CharacterModel part;
    if (!build_character_model(scene, request, part, error)) return false;
    resolve_textures(request.texture_dir, part);

    // The rest world of every node of the part file a rigid piece bakes it into its bind
    std::vector<KnC::NifPlacement> part_rest;
    rest_nif_skeleton(part.rig.skeleton, part_rest);
    // only node skin binds bone body shares by name
    std::vector<char> is_bone(part.rig.skeleton.nodes.size(), 0);
    for (const KnC::NifBlock& block : scene.blocks) {
        if (!block.skin) continue;
        for (uint32_t bone : block.skin->bones) {
            const int node = part.rig.skeleton.node_of(bone);
            if (node >= 0 && static_cast<size_t>(node) < is_bone.size()) is_bone[static_cast<size_t>(node)] = 1;
        }
    }
    for (size_t index = 0; index < part.parts.size(); ++index) {
        BonePalette palette = part.rig.palettes[index];
        bool rigid = false;
        for (size_t slot = 0; slot < palette.nodes.size(); ++slot) {
            const int node = palette.nodes[slot];
            const bool bone = node >= 0 && static_cast<size_t>(node) < is_bone.size() &&
                              is_bone[static_cast<size_t>(node)] != 0;
            const std::string name = bone ? lower(part.rig.skeleton.nodes[static_cast<size_t>(node)].name)
                                          : std::string();
            int same = -1;
            if (!name.empty())
                for (size_t look = 0; look < body.rig.skeleton.nodes.size(); ++look)
                    if (lower(body.rig.skeleton.nodes[look].name) == name) { same = static_cast<int>(look); break; }
            if (same >= 0) {
                // A skinned piece keeps its bind the body carries the same bone
                palette.nodes[slot] = same;
                continue;
            }
            // A rigid piece takes the rest world of its own node then rides the attach node
            const KnC::NifTransform baked =
                node >= 0 && static_cast<size_t>(node) < part_rest.size()
                    ? transform_of_placement(part_rest[static_cast<size_t>(node)])
                    : KnC::NifTransform();
            palette.binds[slot] = compose_transform(baked, palette.binds[slot]);
            palette.nodes[slot] = attach;
            rigid = true;
        }
        if (rigid && attach < 0) continue;
        body.parts.push_back(std::move(part.parts[index]));
        body.rig.palettes.push_back(std::move(palette));
    }
    return true;
}

// The rest pose box of the merged model the renderer fits its sphere on it
void measure_driver_bounds(CharacterModel& model) {
    std::vector<KnC::NifPlacement> rest;
    rest_nif_skeleton(model.rig.skeleton, rest);
    float low[3] = {1e30f, 1e30f, 1e30f}, high[3] = {-1e30f, -1e30f, -1e30f};
    bool any = false;
    for (size_t index = 0; index < model.parts.size(); ++index) {
        const BonePalette& palette = model.rig.palettes[index];
        for (const SkinnedVertex& vertex : model.parts[index].vertices) {
            const float local[3] = {vertex.surface.x, vertex.surface.y, vertex.surface.z};
            float moved[3] = {0.f, 0.f, 0.f};
            for (int slot = 0; slot < kWeightsPerVertex; ++slot) {
                const size_t entry = vertex.bone_slot[slot];
                if (vertex.bone_weight[slot] <= 0.f || entry >= palette.nodes.size()) continue;
                const int node = palette.nodes[entry];
                const KnC::NifPlacement placed = KnC::compose_placement(
                    node < 0 ? KnC::NifPlacement() : rest[static_cast<size_t>(node)], palette.binds[entry]);
                float point[3];
                KnC::place_point(placed, local, point);
                for (int axis = 0; axis < 3; ++axis) moved[axis] += vertex.bone_weight[slot] * point[axis];
            }
            for (int axis = 0; axis < 3; ++axis) {
                low[axis] = std::min(low[axis], moved[axis]);
                high[axis] = std::max(high[axis], moved[axis]);
            }
            any = true;
        }
    }
    if (!any) return;
    for (int axis = 0; axis < 3; ++axis) {
        model.bounds_min[axis] = low[axis];
        model.bounds_max[axis] = high[axis];
    }
}

}

bool load_ghost_driver(const std::string& body_nif, const std::string& chassis, GhostDriver& out,
                       std::string& error) {
    // the worn parts change the build so they join the key
    const std::string asset = fs::path(body_nif).parent_path().filename().string();
    const std::string key = model_key(body_nif, chassis + "|" + ghost_driver_parts_token(asset));
    {
        ParsedModels& models = parsed_models();
        std::lock_guard<std::mutex> lock(models.lock);
        const auto found = models.drivers.find(key);
        if (found != models.drivers.end()) {
            out = *found->second;
            return true;
        }
    }
    if (!load_ghost_driver_fresh(body_nif, chassis, out, error)) return false;
    ParsedModels& models = parsed_models();
    std::lock_guard<std::mutex> lock(models.lock);
    models.drivers[key] = std::make_shared<const GhostDriver>(out);
    return true;
}

namespace {

bool load_ghost_driver_fresh(const std::string& body_nif, const std::string& chassis, GhostDriver& out,
                             std::string& error) {
    out = GhostDriver{};
    const fs::path body_path(body_nif);
    CharacterModelRequest request;
    request.nif_path = body_nif;
    request.texture_dir = body_path.parent_path().string();
    find_body_clips(body_path, request.clips);

    // The worn BODYSET parts of this asset the folder name is the asset the server names
    const std::string asset = body_path.parent_path().filename().string();
    const std::vector<std::string>& worn = ghost_driver_parts(asset);
    out.parts_token = ghost_driver_parts_token(asset);
    // The body keeps all its own geometry the exe only sets the child of the slot node
    std::vector<std::pair<std::string, std::string>> attach;
    {
        std::error_code ignored;
        for (const std::string& part : worn) {
            if (part.empty() || !fs::exists(part, ignored)) continue;
            KnC::NifScene part_scene;
            std::string part_error;
            if (!KnC::read_nif_scene(part, part_scene, part_error)) continue;
            const std::string node = part_attach_node(part_scene);
            if (node.empty()) continue;
            attach.emplace_back(part, node);
        }
    }
    KnC::NifScene body_scene;
    if (!KnC::read_nif_scene(body_nif, body_scene, error)) return false;
    if (!build_character_model(body_scene, request, out.model, error)) return false;
    resolve_textures(request.texture_dir, out.model);
    for (const auto& part : attach) {
        // A mesh named like a slot takes no child so a rigid part of that slot never shows
        int node = -1;
        for (uint32_t block = 0; block < body_scene.blocks.size(); ++block) {
            if (lower(body_scene.blocks[block].name) != lower(part.second)) continue;
            if (body_scene.blocks[block].data_link < body_scene.blocks.size()) continue;
            const int found = out.model.rig.skeleton.node_of(block);
            if (found >= 0) { node = found; break; }
        }
        if (!merge_driver_part(part.first, node, out.model))
            std::printf("[driver] %s did not merge on %s\n", part.first.c_str(), part.second.c_str());
    }
    if (!attach.empty()) measure_driver_bounds(out.model);
    for (size_t clip = 0; clip < out.model.rig.clips.size(); ++clip) {
        const CharacterClip& bound = out.model.rig.clips[clip];
        if (out.idle_clip < 0 || bound.motion.name == "MT_IDLE") out.idle_clip = static_cast<int>(clip);
        if (bound.sequence_id < 0) continue;
        const size_t id = static_cast<size_t>(bound.sequence_id);
        if (id >= out.clip_of_sequence.size()) out.clip_of_sequence.resize(id + 1, -1);
        // First clip of an id wins the KFM lists each id once
        if (out.clip_of_sequence[id] < 0) out.clip_of_sequence[id] = static_cast<int>(clip);
    }
    out.seat_found = find_driver_seat(body_nif, chassis, out.seat);
    bx::mtxTranslate(out.seat_local.data(), out.seat[0], out.seat[1], out.seat[2]);
    return true;
}

}

int ghost_driver_sequence(const GhostPose& pose, float speed) {
    // car visual update 0x48F515 reverse flag first then a running boost then the lean side
    if (pose.reversing) return kDriverSeqBack;
    if (pose.boosting) return kDriverSeqTurbo;
    if (speed > kGhostLeanSpeedGate && (pose.turnState == 1 || pose.turnState == 2))
        return pose.turnState == 1 ? kDriverSeqDriftLeft : kDriverSeqDriftRight;
    return kDriverSeqIdle;
}

int ghost_driver_clip(const GhostDriver& driver, const GhostPose& pose, float speed) {
    const int clip = driver.clip_of(ghost_driver_sequence(pose, speed));
    return clip >= 0 ? clip : driver.idle_clip;
}

void ghost_driver_instance(const GhostDriver& driver, size_t model_index,
                           const float car_world[16], int clip, float clip_seconds,
                           CharacterInstance& out) {
    out.model_index = model_index;
    out.clip = clip;
    out.clip_seconds = clip_seconds;
    bx::mtxMul(out.world, driver.seat_local.data(), car_world);
}

}
