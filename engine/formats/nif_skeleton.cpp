#include "engine/formats/nif_skeleton.h"

#include "engine/formats/nif_animation_eval.h"

#include <algorithm>
#include <cfloat>
#include <unordered_map>
#include <utility>

namespace KnC {

namespace {

// Clip repeats over window like controller single mapping stated
NifController clip_controller(const NifSkeletonClip& clip) {
    NifController controller;
    controller.frequency = clip.frequency;
    controller.start_time = clip.start_time;
    controller.stop_time = clip.stop_time;
    controller.flags = static_cast<uint16_t>(
        kControllerActiveBit | (static_cast<uint16_t>(clip.cycle) << kCycleTypeShift));
    return controller;
}

// Compressed control point is a signed 16 bit fraction bias plus multiplier times point over 32767 HBOnline FUN 004bfbe0
constexpr float kCompactControlPointScale = 1.f / 32767.f;

// Channel control points dequantised undriven channel uses rest pose
bool dequantise_bspline_channel(const NifAnimation& points, const NifBSplineChannel& channel,
                                std::size_t count, std::vector<float>& out) {
    out.clear();
    if (!channel.is_driven()) return true;
    if (static_cast<std::size_t>(channel.offset) + count >
        points.compact_control_points.size())
        return false;
    out.resize(count);
    for (std::size_t index = 0; index < count; ++index)
        out[index] = static_cast<float>(points.compact_control_points[channel.offset + index]) *
                         kCompactControlPointScale * channel.multiplier +
                     channel.bias;
    return true;
}

// Transform channel holds fall back to rest for undriven
NifTransform posed_local(const NifSkeletonChannel& channel, const NifTransform& rest,
                         float key_time) {
    const NifTransformSample sample = sample_nif_channel(channel, key_time);
    NifTransform posed = rest;
    if (sample.has_translation)
        for (int axis = 0; axis < 3; ++axis)
            posed.translation[axis] = sample.transform.translation[axis];
    if (sample.has_rotation)
        for (int cell = 0; cell < 9; ++cell) posed.rotation[cell] = sample.transform.rotation[cell];
    if (sample.has_scale) posed.scale = sample.transform.scale;
    return posed;
}

// Node placement or identity at root parents precede children
NifPlacement parent_placement(const NifSkeleton& skeleton, std::size_t index,
                              const std::vector<NifPlacement>& placed) {
    const int parent = skeleton.nodes[index].parent;
    if (parent < 0) return NifPlacement();
    return placed[static_cast<std::size_t>(parent)];
}

// Geometry data blocks properties not nodes tree order matters
void push_children(const NifScene& model, uint32_t block, int node, NifSkeleton& out,
                   std::vector<std::pair<uint32_t, int>>& pending) {
    for (uint32_t child : model.blocks[block].children) {
        if (child >= model.blocks.size() || out.node_of_block[child] >= 0) continue;
        NifSkeletonNode added;
        added.name = model.blocks[child].name;
        added.parent = node;
        added.rest = model.blocks[child].transform;
        out.node_of_block[child] = static_cast<int>(out.nodes.size());
        pending.emplace_back(child, static_cast<int>(out.nodes.size()));
        out.nodes.push_back(std::move(added));
    }
}

// Spline interpolator drives curve basis data empty drives none
bool drives_a_bspline_channel(const NifBSplineTransform& spline) {
    return spline.translation.is_driven() || spline.rotation.is_driven() ||
           spline.scale.is_driven();
}

// Legacy controller poses nothing NiQuatTransform did not exist
NifInterpolator unposed_interpolator() {
    NifInterpolator pose;
    pose.pose_translation[0] = -FLT_MAX;
    pose.pose_rotation[0]    = -FLT_MAX;
    pose.pose_scale          = -FLT_MAX;
    return pose;
}

// Sequence entry curve from version 10 1 0 104
bool bind_entry_curve(const NifScene& stream, const NifSequenceEntry& entry,
                      NifSkeletonChannel& channel) {
    const bool through_controller = stream.header.version < kInterpolatorsFrom;
    const NifAnimation* const played = find_animation(
        stream, through_controller ? entry.controller_link : entry.interpolator_link);
    if (played == nullptr) return false;
    channel.interpolator = through_controller ? unposed_interpolator() : played->interpolator;
    const NifAnimation* const keys = find_animation(
        stream, through_controller ? played->controller.interpolator_link
                                   : played->interpolator.data_link);
    if (keys != nullptr) channel.keys = *keys;
    return !drives_a_bspline_channel(played->bspline) ||
           resolve_nif_bspline(stream, *played, channel.spline);
}

// A transform controller in the chain of one block NiKeyframeController below 10 1 0 104
bool owns_transform_controller(const NifScene& model, uint32_t block) {
    uint32_t link = model.blocks[block].controller_link;
    for (std::size_t hop = 0; hop < model.blocks.size() && link < model.blocks.size(); ++hop) {
        const NifBlock& controller = model.blocks[link];
        if (controller.type == "NiTransformController" || controller.type == "NiKeyframeController")
            return true;
        if (controller.animation == nullptr) break;
        link = controller.animation->controller.next_link;
    }
    return false;
}

// The stock manager drives the accumulation root only through a controller of its own
void mark_controlled(const NifScene& model, NifSkeleton& out) {
    for (uint32_t block = 0; block < model.blocks.size(); ++block) {
        const int node = out.node_of_block[block];
        if (node >= 0 && owns_transform_controller(model, block))
            out.nodes[static_cast<std::size_t>(node)].controlled = true;
    }
}

} // namespace

NifTransformSample sample_nif_channel(const NifSkeletonChannel& channel, float key_time) {
    if (channel.spline.is_empty())
        return evaluate_transform(channel.keys, channel.interpolator, key_time);
    return evaluate_bspline(channel.spline, channel.interpolator, key_time);
}

int NifSkeleton::node_of(uint32_t block) const {
    return block < node_of_block.size() ? node_of_block[block] : -1;
}

// Breadth first from roots node pushed after parent forward pass
NifSkeleton build_nif_skeleton(const NifScene& model) {
    NifSkeleton skeleton;
    skeleton.node_of_block.assign(model.blocks.size(), -1);
    std::vector<std::pair<uint32_t, int>> pending;
    for (uint32_t root : find_root_block_indices(model)) {
        if (skeleton.node_of_block[root] >= 0) continue;
        NifSkeletonNode added;
        added.name = model.blocks[root].name;
        added.rest = model.blocks[root].transform;
        skeleton.node_of_block[root] = static_cast<int>(skeleton.nodes.size());
        pending.emplace_back(root, static_cast<int>(skeleton.nodes.size()));
        skeleton.nodes.push_back(std::move(added));
    }
    for (std::size_t step = 0; step < pending.size(); ++step)
        push_children(model, pending[step].first, pending[step].second, skeleton, pending);
    mark_controlled(model, skeleton);
    return skeleton;
}

bool resolve_nif_bspline(const NifScene& clip_stream, const NifAnimation& interpolator,
                         NifBSplineSampler& out) {
    const NifBSplineTransform& spline = interpolator.bspline;
    const NifAnimation* const points = find_animation(clip_stream, spline.data_link);
    const NifAnimation* const basis = find_animation(clip_stream, spline.basis_link);
    if (points == nullptr || basis == nullptr) return false;
    // Each channel spans basis control point count interleaved
    const std::size_t spanned = basis->basis_control_points;
    out.start_time = spline.start_time;
    out.stop_time = spline.stop_time;
    return dequantise_bspline_channel(*points, spline.translation, spanned * 3,
                                      out.translations) &&
           dequantise_bspline_channel(*points, spline.rotation, spanned * 4, out.rotations) &&
           dequantise_bspline_channel(*points, spline.scale, spanned, out.scales);
}

std::vector<uint32_t> find_nif_sequences(const NifScene& clip_stream) {
    std::vector<uint32_t> sequences;
    for (uint32_t index = 0; index < clip_stream.blocks.size(); ++index)
        if (clip_stream.blocks[index].type == "NiControllerSequence") sequences.push_back(index);
    return sequences;
}

bool bind_nif_sequence(const NifScene& clip_stream, uint32_t sequence_block,
                       const NifSkeleton& skeleton, NifSkeletonClip& out,
                       std::size_t& unbound) {
    if (sequence_block >= clip_stream.blocks.size()) return false;
    const NifBlock& block = clip_stream.blocks[sequence_block];
    if (block.animation == nullptr) return false;
    const NifSequence& sequence = block.animation->sequence;
    out.name = block.name;
    out.start_time = sequence.start_time;
    out.stop_time = sequence.stop_time;
    out.cycle = sequence.cycle;
    out.frequency = sequence.frequency != 0.f ? sequence.frequency : 1.f;
    // Text key moments carried onto clip consumer no walk back
    const NifAnimation* text = find_animation(clip_stream, sequence.text_keys_link);
    out.text_keys.clear();
    if (text != nullptr) out.text_keys = text->text_keys;
    out.channels.clear();
    // One map per clip widest binds 79 nodes hundreds entries
    std::unordered_map<std::string, int> node_of_name;
    for (std::size_t index = 0; index < skeleton.nodes.size(); ++index)
        node_of_name.emplace(skeleton.nodes[index].name, static_cast<int>(index));
    // Every named node binds through the multi target controller of the stock manager
    for (const NifSequenceEntry& entry : sequence.entries) {
        const auto found = node_of_name.find(entry.target_name);
        if (found == node_of_name.end()) {
            ++unbound;
            continue;
        }
        // The accumulation root keeps its rest without a controller the main of Princess and Yuk
        if (entry.target_name == sequence.accumulation_root_name &&
            !skeleton.nodes[static_cast<std::size_t>(found->second)].controlled) {
            ++unbound;
            continue;
        }
        NifSkeletonChannel channel;
        channel.node = found->second;
        if (!bind_entry_curve(clip_stream, entry, channel)) {
            ++unbound;
            continue;
        }
        out.channels.push_back(std::move(channel));
    }
    return true;
}

void rest_nif_skeleton(const NifSkeleton& skeleton, std::vector<NifPlacement>& out) {
    out.assign(skeleton.nodes.size(), NifPlacement());
    for (std::size_t index = 0; index < skeleton.nodes.size(); ++index)
        out[index] = compose_placement(parent_placement(skeleton, index, out),
                                       skeleton.nodes[index].rest);
}

void pose_nif_skeleton(const NifSkeleton& skeleton, const NifSkeletonClip& clip, float seconds,
                       std::vector<NifPlacement>& out) {
    std::vector<NifTransform> local(skeleton.nodes.size());
    for (std::size_t index = 0; index < skeleton.nodes.size(); ++index)
        local[index] = skeleton.nodes[index].rest;
    float key_time = clip.start_time;
    map_controller_time(clip_controller(clip), seconds, key_time);
    for (const NifSkeletonChannel& channel : clip.channels) {
        if (channel.node < 0 || static_cast<std::size_t>(channel.node) >= local.size()) continue;
        const std::size_t node = static_cast<std::size_t>(channel.node);
        local[node] = posed_local(channel, skeleton.nodes[node].rest, key_time);
    }
    out.assign(skeleton.nodes.size(), NifPlacement());
    for (std::size_t index = 0; index < skeleton.nodes.size(); ++index)
        out[index] = compose_placement(parent_placement(skeleton, index, out), local[index]);
}

} // namespace KnC
