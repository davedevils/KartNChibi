#pragma once
// Named node tree character animated through clips bind on it
#include "engine/formats/nif_animation.h"
#include "engine/formats/nif_animation_eval.h"
#include "engine/formats/nif_reader.h"
#include "engine/formats/nif_scene_graph.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace KnC {

// One skeleton node name parent rest transform when idle
struct NifSkeletonNode {
    std::string  name;
    int          parent = -1;   // index into NifSkeleton nodes -1 at root
    NifTransform rest;
    // Owns a transform controller in the stream the accumulation root binds only then
    bool controlled = false;
};

// Every node from roots parents before children forward pass
struct NifSkeleton {
    std::vector<NifSkeletonNode> nodes;
    std::vector<int> node_of_block;

    // Node one block became -1 for unreached block
    int node_of(uint32_t block) const;
};

NifSkeleton build_nif_skeleton(const NifScene& model);

// One clip channel curve 103 of 139 streams B-spline
struct NifSkeletonChannel {
    int               node = -1;
    NifInterpolator   interpolator;
    NifAnimation      keys;
    NifBSplineSampler spline;
};

// Resolve NiBSplineTransformInterpolator dequantise control points all channels
bool resolve_nif_bspline(const NifScene& clip_stream, const NifAnimation& interpolator,
                         NifBSplineSampler& out);

// One named animation bound to skeleton
struct NifSkeletonClip {
    std::string  name;
    float        start_time = 0.f;
    float        stop_time  = 0.f;
    NifCycleType cycle      = NifCycleType::Loop;
    float        frequency  = 1.f;
    // Clip named moments stream order same time axis
    std::vector<NifTextKey> text_keys;
    std::vector<NifSkeletonChannel> channels;

    float duration() const { return stop_time - start_time; }
};

// Block indices every NiControllerSequence stream order counted
std::vector<uint32_t> find_nif_sequences(const NifScene& clip_stream);

// Bind sequence onto skeleton by name the accumulation root only through its own controller
bool bind_nif_sequence(const NifScene& clip_stream, uint32_t sequence_block,
                       const NifSkeleton& skeleton, NifSkeletonClip& out,
                       std::size_t& unbound);

// Channel at key time on clip keys or B-spline
NifTransformSample sample_nif_channel(const NifSkeletonChannel& channel, float key_time);

// Pose every node seconds skeleton root space sized
void pose_nif_skeleton(const NifSkeleton& skeleton, const NifSkeletonClip& clip,
                       float seconds, std::vector<NifPlacement>& out);

// Skeleton at rest transforms no clip bound
void rest_nif_skeleton(const NifSkeleton& skeleton, std::vector<NifPlacement>& out);

} // namespace KnC
