#include "engine/render/light_rig.h"

#include "engine/formats/nif_reader.h"
#include "engine/formats/nif_scene_graph.h"

#include <cmath>
#include <vector>

namespace KnC::Render {

namespace {

// World placement of every block parents composed onto children from the roots
void place_blocks(const NifScene& scene, std::vector<NifPlacement>& placed) {
    placed.assign(scene.blocks.size(), NifPlacement());
    std::vector<bool> visited(scene.blocks.size(), false);
    struct Frame { uint32_t block; NifPlacement placement; };
    std::vector<Frame> pending;
    for (const uint32_t root : find_root_block_indices(scene)) {
        if (root < scene.blocks.size()) pending.push_back({root, NifPlacement()});
    }
    while (!pending.empty()) {
        const Frame frame = pending.back();
        pending.pop_back();
        if (visited[frame.block]) continue;
        visited[frame.block] = true;
        const NifBlock& block = scene.blocks[frame.block];
        NifPlacement here = frame.placement;
        if (block.has_transform) here = compose_placement(here, block.transform);
        placed[frame.block] = here;
        for (const uint32_t child : block.children) {
            if (child < scene.blocks.size()) pending.push_back({child, here});
        }
    }
}

HourColour colour_of(const float rgb[3]) {
    return HourColour{rgb[0], rgb[1], rgb[2]};
}

} // namespace

bool load_light_rig(const std::string& path, LightRig& out, std::string& error) {
    NifScene scene;
    if (!read_nif_scene(path, scene, error)) return false;
    std::vector<NifPlacement> placed;
    place_blocks(scene, placed);
    bool found_ambient = false;
    bool found_sun = false;
    for (size_t index = 0; index < scene.blocks.size(); ++index) {
        const NifBlock& block = scene.blocks[index];
        if (!block.light) continue;
        if (block.type == "NiAmbientLight" && !found_ambient) {
            out.ambient = colour_of(block.light->ambient);
            found_ambient = true;
        }
        if (block.type == "NiDirectionalLight" && !found_sun) {
            out.sun.colour = colour_of(block.light->diffuse);
            // Gamebryo shines a directional light along its world X axis
            const float local_x[3] = {1.f, 0.f, 0.f};
            float direction[3];
            place_direction(placed[index], local_x, direction);
            const float length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] +
                                           direction[2] * direction[2]);
            if (length > 0.f) {
                for (float& axis : direction) axis /= length;
            }
            // The sign is not settled by the data a sun comes from above so the light travels down
            if (direction[2] > 0.f) {
                for (float& axis : direction) axis = -axis;
            }
            for (int axis = 0; axis < 3; ++axis) out.sun.direction[axis] = direction[axis];
            out.sun.enabled = true;
            found_sun = true;
        }
    }
    if (!found_ambient && !found_sun) {
        error = path + " holds no NiAmbientLight and no NiDirectionalLight";
        return false;
    }
    return true;
}

void apply_light_rig(const LightRig& rig, MapScene& scene) {
    scene.day_night.ambient.fill(rig.ambient);
    scene.sun = rig.sun;
}

}
