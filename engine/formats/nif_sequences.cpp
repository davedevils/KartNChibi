#include "engine/formats/nif_internal.h"

#include <algorithm>

namespace KnC::nif {

namespace {

// Sequence interpolators and IDTag arrays from version 10 1 0 113
constexpr uint32_t kSequenceIdTagStringsFrom = make_version(10, 1, 0, 113);
// Phase field stopped at version 10 3 0 1
constexpr uint32_t kSequencePhaseUntil = make_version(10, 3, 0, 1);
constexpr uint32_t kSequenceFlagsFrom = make_version(20, 3, 0, 8);

constexpr uint32_t kIdTagStringCount = 5;
// Text key is time plus name string table index
constexpr uint64_t kSmallestTextKeyBytes = 2 * sizeof(uint32_t);
// Translation rotation scale each index own control points
constexpr uint32_t kTransformHandleCount = 3;

// IDTag names object property controller and interpolator node first
bool read_id_tag(Cursor& cursor, const NifHeader& header, NifSequenceEntry& entry) {
    if (header.version < kStringTableFrom) {
        // Five strings are byte offsets into NiStringPalette first node used
        constexpr size_t kUnreadOffsetBytes = (kIdTagStringCount - 1) * sizeof(uint32_t);
        return cursor.take_u32(entry.palette_link) &&
               cursor.take_u32(entry.target_name_offset) && cursor.skip(kUnreadOffsetBytes);
    }
    if (!read_object_name(cursor, header, entry.target_name)) return false;
    std::string id_string;
    for (uint32_t field = 1; field < kIdTagStringCount; ++field)
        if (!read_object_name(cursor, header, id_string)) return false;
    return true;
}

// Interpolator controller then IDTag naming target
bool read_sequence_entry(Cursor& cursor, const NifHeader& header,
                         NifSequenceEntry& entry) {
    return cursor.take_u32(entry.interpolator_link) &&
           cursor.take_u32(entry.controller_link) &&
           read_id_tag(cursor, header, entry);
}

// Entry array weight text keys and playback timing
bool read_sequence_playback(Cursor& cursor, const NifHeader& header,
                            NifSequence& sequence) {
    uint32_t raw_cycle = 0;
    if (!cursor.take_f32(sequence.weight) ||
        !cursor.take_u32(sequence.text_keys_link) ||
        !cursor.take_u32(raw_cycle) || raw_cycle > static_cast<uint32_t>(NifCycleType::Clamp) ||
        !cursor.take_f32(sequence.frequency))
        return false;
    sequence.cycle = static_cast<NifCycleType>(raw_cycle);
    if (header.version < kSequencePhaseUntil) {
        float phase = 0.f;
        if (!cursor.take_f32(phase)) return false;
    }
    if (!cursor.take_f32(sequence.start_time) ||
        !cursor.take_f32(sequence.stop_time) ||
        !skip_link(cursor) ||   // owning controller manager
        !read_object_name(cursor, header, sequence.accumulation_root_name))
        return false;
    if (header.version >= kSequenceFlagsFrom) {
        uint32_t sequence_flags = 0;
        if (!cursor.take_u32(sequence_flags)) return false;
    }
    // Below fixed string table shared IDTag palette named last
    if (header.version >= kStringTableFrom) return true;
    return skip_link(cursor);
}

// Legacy sequences list target names controllers no interpolators no timing
bool read_legacy_sequence(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    NifSequence& sequence = animation_of(block).sequence;
    uint32_t target_count = 0;
    if (!read_object_name(cursor, header, block.name) ||
        !read_object_name(cursor, header, sequence.accumulation_root_name) ||
        !cursor.take_u32(sequence.text_keys_link) ||
        !cursor.take_u32(target_count) || target_count > kMaxLinks)
        return false;
    sequence.entries.resize(target_count);
    for (NifSequenceEntry& entry : sequence.entries)
        if (!read_object_name(cursor, header, entry.target_name) ||
            !cursor.take_u32(entry.controller_link))
            return false;
    return true;
}

// Legacy sequence window taken from controllers once stream decoded
void time_sequence_from_controllers(const NifScene& scene, NifSequence& sequence) {
    bool timed = false;
    for (const NifSequenceEntry& entry : sequence.entries) {
        const NifAnimation* const driver = find_animation(scene, entry.controller_link);
        if (driver == nullptr) continue;
        const NifController& controller = driver->controller;
        sequence.start_time = timed ? std::min(sequence.start_time, controller.start_time)
                                    : controller.start_time;
        sequence.stop_time = timed ? std::max(sequence.stop_time, controller.stop_time)
                                   : controller.stop_time;
        // Cycle and rate per controller clips state same pair
        if (!timed) {
            sequence.cycle = controller.cycle_type();
            sequence.frequency = controller.frequency;
        }
        timed = true;
    }
}

// NiBSplineInterpolator time range control points and basis blocks
bool read_bspline_interpolator(Cursor& cursor, NifBSplineTransform& spline) {
    return cursor.take_f32(spline.start_time) && cursor.take_f32(spline.stop_time) &&
           cursor.take_u32(spline.data_link) && cursor.take_u32(spline.basis_link);
}

// Static transform spline holds for undriven channels NiQuatTransform layout
bool read_bspline_pose(Cursor& cursor, NifInterpolator& interpolator) {
    return cursor.take(interpolator.pose_translation,
                       sizeof(interpolator.pose_translation)) &&
           cursor.take(interpolator.pose_rotation, sizeof(interpolator.pose_rotation)) &&
           cursor.take_f32(interpolator.pose_scale);
}

} // namespace

void resolve_legacy_sequence_timing(NifScene& scene) {
    if (scene.header.version >= kInterpolatorsFrom) return;
    for (NifBlock& block : scene.blocks) {
        if (!block.animation || block.type != "NiControllerSequence") continue;
        time_sequence_from_controllers(scene, block.animation->sequence);
    }
}

bool read_controller_sequence(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (header.version < kInterpolatorsFrom)
        return read_legacy_sequence(cursor, header, block);
    if (header.version < kSequenceIdTagStringsFrom) return false;

    uint32_t entry_count = 0;
    uint32_t entry_array_grow_by = 0;
    if (!read_object_name(cursor, header, block.name) ||
        !cursor.take_u32(entry_count) || entry_count > kMaxLinks ||
        !cursor.take_u32(entry_array_grow_by))
        return false;
    NifSequence& sequence = animation_of(block).sequence;
    sequence.entries.resize(entry_count);
    for (NifSequenceEntry& entry : sequence.entries)
        if (!read_sequence_entry(cursor, header, entry)) return false;
    return read_sequence_playback(cursor, header, sequence);
}

bool read_text_key_extra_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    uint32_t key_count = 0;
    if (!read_object_name(cursor, header, block.name) ||
        !cursor.take_u32(key_count) ||
        key_count > cursor.remaining() / kSmallestTextKeyBytes)
        return false;
    std::vector<NifTextKey>& keys = animation_of(block).text_keys;
    keys.resize(key_count);
    for (NifTextKey& key : keys)
        if (!cursor.take_f32(key.time) || !read_object_name(cursor, header, key.text))
            return false;
    return true;
}

bool read_bool_interpolator(Cursor& cursor, const NifHeader&, NifBlock& block) {
    // Pose held when data block has no keys
    NifInterpolator& interpolator = animation_of(block).interpolator;
    uint8_t pose_is_set = 0;   // NiBool is one byte
    if (!cursor.take_u8(pose_is_set)) return false;
    interpolator.pose_value = pose_is_set != 0 ? 1.f : 0.f;
    return cursor.take_u32(interpolator.data_link);
}

bool read_bool_data(Cursor& cursor, const NifHeader&, NifBlock& block) {
    return read_bool_key_group(cursor, animation_of(block).channel);
}

bool read_bspline_comp_transform_interpolator(Cursor& cursor, const NifHeader&,
                                              NifBlock& block) {
    NifAnimation& animation = animation_of(block);
    NifBSplineTransform& spline = animation.bspline;
    if (!read_bspline_interpolator(cursor, spline) ||
        !read_bspline_pose(cursor, animation.interpolator))
        return false;
    NifBSplineChannel* const channels[kTransformHandleCount] = {
        &spline.translation, &spline.rotation, &spline.scale};
    for (NifBSplineChannel* channel : channels)
        if (!cursor.take_u32(channel->offset)) return false;
    for (NifBSplineChannel* channel : channels)
        if (!cursor.take_f32(channel->bias) || !cursor.take_f32(channel->multiplier))
            return false;
    return true;
}

bool read_bspline_data(Cursor& cursor, const NifHeader&, NifBlock& block) {
    NifAnimation& animation = animation_of(block);
    uint32_t control_point_count = 0;
    if (!cursor.take_u32(control_point_count) ||
        !take_f32_array(cursor, control_point_count, animation.control_points))
        return false;
    uint32_t compact_count = 0;
    if (!cursor.take_u32(compact_count) ||
        compact_count > cursor.remaining() / sizeof(int16_t))
        return false;
    animation.compact_control_points.resize(compact_count);
    return compact_count == 0 ||
           cursor.take(animation.compact_control_points.data(),
                       static_cast<size_t>(compact_count) * sizeof(int16_t));
}

bool read_bspline_basis_data(Cursor& cursor, const NifHeader&, NifBlock& block) {
    return cursor.take_u32(animation_of(block).basis_control_points);
}

}
