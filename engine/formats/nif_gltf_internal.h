#pragma once
// Shared between geometry animation halves NIF exporter not public interface
#include "engine/formats/nif_gltf.h"
#include "engine/formats/nif_skeleton.h"

namespace KnC {

// Row major 3x3 rotation unit quaternion glTF x y z w
void gltf_quaternion_of(const float rotation[9], float out[4]);

// NiControllerSequence every clip stream transform controllers model carries
void append_nif_gltf_animations(const NifGltfRequest& request, const NifSkeleton& skeleton,
                                NifGltfResult& out);

} // namespace KnC
