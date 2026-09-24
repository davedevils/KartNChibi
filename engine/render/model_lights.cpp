#include "engine/render/model_lights.h"

#include "engine/formats/nif_scene_graph.h"

#include <cmath>
#include <utility>

namespace KnC::Render {

namespace {

// NiTextureEffect texture type 2 environment map coordinate type 2 sphere map
constexpr uint32_t kEnvironmentMapEffect = 2;
constexpr uint32_t kSphereMapCoordinates = 2;

// Rest placement of every block the roots at identity
std::vector<NifPlacement> place_blocks(const NifScene& scene) {
    std::vector<NifPlacement> placed(scene.blocks.size());
    std::vector<bool> visited(scene.blocks.size(), false);
    std::vector<std::pair<uint32_t, NifPlacement>> pending;
    for (const uint32_t root : find_root_block_indices(scene))
        if (root < scene.blocks.size()) pending.emplace_back(root, NifPlacement());
    while (!pending.empty()) {
        const auto [index, parent] = pending.back();
        pending.pop_back();
        if (visited[index]) continue;
        visited[index] = true;
        const NifBlock& block = scene.blocks[index];
        placed[index] = block.has_transform ? compose_placement(parent, block.transform) : parent;
        for (const uint32_t child : block.children)
            if (child < scene.blocks.size()) pending.emplace_back(child, placed[index]);
    }
    return placed;
}

// Gamebryo lights a directional light along its world X axis
void travel_direction(const NifPlacement& placement, float out[3]) {
    const float local_x[3] = {1.f, 0.f, 0.f};
    place_direction(placement, local_x, out);
    const float length = std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    if (length <= 0.f) return;
    for (int axis = 0; axis < 3; ++axis) out[axis] /= length;
}

bool is_sphere_environment(const NifBlock& block) {
    return block.texture_effect && block.texture_effect->texture_type == kEnvironmentMapEffect &&
           block.texture_effect->coordinate_type == kSphereMapCoordinates;
}

} // namespace

ModelLights collect_model_lights(const NifScene& scene) {
    ModelLights lights;
    const std::vector<NifPlacement> placed = place_blocks(scene);
    for (const uint32_t root : find_root_block_indices(scene)) {
        if (root >= scene.blocks.size()) continue;
        for (const uint32_t link : scene.blocks[root].effects) {
            if (link >= scene.blocks.size() || !scene.blocks[link].light) continue;
            const NifBlock& block = scene.blocks[link];
            const NifLight& light = *block.light;
            if (light.is_enabled == 0) continue;
            if (block.type == "NiAmbientLight") {
                for (int channel = 0; channel < 3; ++channel)
                    lights.ambient[channel] += light.ambient[channel] * light.dimmer;
                continue;
            }
            if (block.type != "NiDirectionalLight" || lights.count >= kModelLights) continue;
            travel_direction(placed[link], lights.direction[lights.count]);
            for (int channel = 0; channel < 3; ++channel)
                lights.colour[lights.count][channel] = light.diffuse[channel] * light.dimmer;
            ++lights.count;
        }
    }
    return lights;
}

EnvironmentMaps::EnvironmentMaps(const NifScene& scene) : map_of_block_(scene.blocks.size(), -1) {
    const std::vector<NifPlacement> placed = place_blocks(scene);
    std::vector<bool> visited(scene.blocks.size(), false);
    std::vector<std::pair<uint32_t, int>> pending;
    for (const uint32_t root : find_root_block_indices(scene))
        if (root < scene.blocks.size()) pending.emplace_back(root, -1);
    while (!pending.empty()) {
        auto [index, map] = pending.back();
        pending.pop_back();
        if (visited[index]) continue;
        visited[index] = true;
        for (const uint32_t link : scene.blocks[index].effects) {
            if (link >= scene.blocks.size() || !is_sphere_environment(scene.blocks[link])) continue;
            const uint32_t source = scene.blocks[link].texture_effect->texture_link;
            if (source >= scene.blocks.size() || scene.blocks[source].texture_file_name.empty()) continue;
            EnvironmentMap found;
            found.texture = scene.blocks[source].texture_file_name;
            for (int entry = 0; entry < 9; ++entry) found.rotation[entry] = placed[link].rotation[entry];
            maps_.push_back(std::move(found));
            map = static_cast<int>(maps_.size() - 1);
            break;
        }
        map_of_block_[index] = map;
        for (const uint32_t child : scene.blocks[index].children)
            if (child < scene.blocks.size()) pending.emplace_back(child, map);
    }
}

const EnvironmentMap& EnvironmentMaps::of(uint32_t block) const {
    static const EnvironmentMap none;
    if (block >= map_of_block_.size() || map_of_block_[block] < 0) return none;
    return maps_[static_cast<size_t>(map_of_block_[block])];
}

}
