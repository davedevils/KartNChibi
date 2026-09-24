#include "engine/formats/nif_scene_graph.h"

namespace KnC {

namespace {

void rotate(const float rotation[9], const float source[3], float out[3]) {
    for (int row = 0; row < 3; ++row) {
        out[row] = rotation[row * 3 + 0] * source[0] +
                   rotation[row * 3 + 1] * source[1] +
                   rotation[row * 3 + 2] * source[2];
    }
}

} // namespace

NifPlacement compose_placement(const NifPlacement& parent, const NifTransform& local) {
    NifPlacement result;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            float sum = 0.f;
            for (int step = 0; step < 3; ++step)
                sum += parent.rotation[row * 3 + step] * local.rotation[step * 3 + column];
            result.rotation[row * 3 + column] = sum;
        }
    }
    float rotated_translation[3];
    rotate(parent.rotation, local.translation, rotated_translation);
    for (int axis = 0; axis < 3; ++axis) {
        result.translation[axis] =
            parent.translation[axis] + parent.scale * rotated_translation[axis];
    }
    result.scale = parent.scale * local.scale;
    return result;
}

void place_point(const NifPlacement& placement, const float local[3], float out[3]) {
    float rotated[3];
    rotate(placement.rotation, local, rotated);
    for (int axis = 0; axis < 3; ++axis)
        out[axis] = placement.translation[axis] + placement.scale * rotated[axis];
}

void place_direction(const NifPlacement& placement, const float local[3], float out[3]) {
    rotate(placement.rotation, local, out);
}

// Prop file hundreds blocks exporter unreferenced footer roots authoritative
std::vector<uint32_t> find_root_block_indices(const NifScene& scene) {
    if (!scene.roots.empty()) {
        std::vector<uint32_t> declared;
        for (uint32_t root : scene.roots)
            if (root < scene.blocks.size()) declared.push_back(root);
        return declared;
    }

    std::vector<bool> is_child(scene.blocks.size(), false);
    for (const NifBlock& block : scene.blocks)
        for (uint32_t child : block.children)
            if (child < is_child.size()) is_child[child] = true;

    std::vector<uint32_t> roots;
    for (uint32_t index = 0; index < scene.blocks.size(); ++index)
        if (!is_child[index]) roots.push_back(index);
    return roots;
}

} // namespace KnC
