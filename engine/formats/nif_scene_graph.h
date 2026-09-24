#pragma once
#include "engine/formats/nif_reader.h"

#include <cstdint>
#include <vector>

namespace KnC {

// Transform composed from root as translation scale rotation
struct NifPlacement {
    float rotation[9]    = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};
    float translation[3] = {0.f, 0.f, 0.f};
    float scale          = 1.f;
};

NifPlacement compose_placement(const NifPlacement& parent, const NifTransform& local);

// Map point from node space to composed placement space
void place_point(const NifPlacement& placement, const float local[3], float out[3]);

// Direction rotated only no translation no scale
void place_direction(const NifPlacement& placement, const float local[3], float out[3]);

// Root blocks never listed as children recovered from links
std::vector<uint32_t> find_root_block_indices(const NifScene& scene);

} // namespace KnC
