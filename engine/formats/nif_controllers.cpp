#include "engine/formats/nif_internal.h"

namespace KnC::nif {

namespace {

// kInterpolatorsFrom is the gate here below it NiGeomMorpherController keeps morph weights as keys
constexpr uint32_t kInlineMorphWeightsFrom = make_version(20, 1, 0, 3);
// NiLookAtInterpolator stopped streaming its cached pose here
constexpr uint32_t kLookAtPoseUntil = make_version(20, 4, 0, 13);
// Highest NiTextureTransform EOperation any SDK generation registers
constexpr uint32_t kLastTextureTransform =
    static_cast<uint32_t>(NifTextureTransformOperation::ScaleV);

constexpr uint32_t kFloatBytes = sizeof(float);
constexpr uint32_t kPointBytes = 3 * kFloatBytes;
// Translation quaternion and scale as NiQuatTransform writes them
constexpr uint32_t kQuatTransformBytes = kPointBytes + 5 * kFloatBytes;

// From 20 1 0 3 each link carries a start weight beside it the link is what plays
bool read_weighted_interpolators(Cursor& cursor, NifGeomMorpher& morpher) {
    uint32_t count = 0;
    if (!cursor.take_u32(count) || count > kMaxLinks) return false;
    morpher.weight_interpolator_links.assign(count, kNoLink);
    for (uint32_t i = 0; i < count; ++i) {
        float weight = 0.f;
        if (!cursor.take_u32(morpher.weight_interpolator_links[i]) ||
            !cursor.take(&weight, sizeof(weight)))
            return false;
    }
    return true;
}

// At 10 2 0 0 a frame is its name then one spare word then three floats a vertex
bool read_morph_target(Cursor& cursor, const NifHeader& header,
                       uint32_t vertex_count, NifMorphTarget& out) {
    if (!read_object_name(cursor, header, out.name)) return false;
    if (header.version < kInlineMorphWeightsFrom) {
        uint32_t spare = 0;
        if (!cursor.take_u32(spare)) return false;
    }
    return take_f32_array(cursor, static_cast<size_t>(vertex_count) * 3, out.vectors);
}

} // namespace

bool read_transform_controller(Cursor& cursor, const NifHeader&, NifBlock& block) {
    return read_single_interp_controller(cursor, block);
}

bool read_transform_interpolator(Cursor& cursor, const NifHeader&, NifBlock& block) {
    // The pose held for the case where the data block has no keys
    NifInterpolator& interpolator = animation_of(block).interpolator;
    return cursor.take(interpolator.pose_translation,
                       sizeof(interpolator.pose_translation)) &&
           cursor.take(interpolator.pose_rotation,
                       sizeof(interpolator.pose_rotation)) &&
           cursor.take_f32(interpolator.pose_scale) &&
           cursor.take_u32(interpolator.data_link);
}

bool read_transform_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    NifAnimation& animation = animation_of(block);
    return read_rotation_key_group(cursor, header, animation) &&
           read_key_group(cursor, KeyContent::Point, animation.translations) &&
           read_key_group(cursor, KeyContent::Float, animation.scales);
}

bool read_float_interpolator(Cursor& cursor, const NifHeader&, NifBlock& block) {
    NifInterpolator& interpolator = animation_of(block).interpolator;
    return cursor.take_f32(interpolator.pose_value) &&
           cursor.take_u32(interpolator.data_link);
}

bool read_float_data(Cursor& cursor, const NifHeader&, NifBlock& block) {
    return read_key_group(cursor, KeyContent::Float, animation_of(block).channel);
}

bool read_pos_data(Cursor& cursor, const NifHeader&, NifBlock& block) {
    return read_key_group(cursor, KeyContent::Point, animation_of(block).channel);
}

bool read_alpha_controller(Cursor& cursor, const NifHeader&, NifBlock& block) {
    return read_single_interp_controller(cursor, block);
}

bool read_texture_transform_controller(Cursor& cursor, const NifHeader& header,
                                       NifBlock& block) {
    if (!read_single_interp_controller(cursor, block)) return false;
    uint8_t is_shader_map = 0;
    uint32_t map_index = 0;
    uint32_t raw_operation = 0;
    if (!cursor.take_u8(is_shader_map) || !cursor.take_u32(map_index) ||
        !cursor.take_u32(raw_operation))
        return false;
    // Below 10 1 0 104 the same thirteen bytes carry the data link as well in an order
    if (header.version < kInterpolatorsFrom) return true;
    if (raw_operation > kLastTextureTransform) return false;
    NifTextureTransform& transform = animation_of(block).texture_transform;
    transform.is_shader_map = is_shader_map != 0;
    transform.map_index = map_index;
    transform.operation = static_cast<NifTextureTransformOperation>(raw_operation);
    return true;
}

bool read_flip_controller(Cursor& cursor, const NifHeader&, NifBlock& block) {
    if (!read_single_interp_controller(cursor, block)) return false;
    // The links stay the renderer swaps the base map through them
    NifTextureFlip& flip = animation_of(block).texture_flip;
    return cursor.take_u32(flip.affected_map) && read_links(cursor, flip.textures);
}

bool read_geom_morpher_controller(Cursor& cursor, const NifHeader& header,
                                  NifBlock& block) {
    if (!read_time_controller(cursor, block)) return false;
    NifGeomMorpher& morpher = animation_of(block).morpher;
    uint16_t flags = 0;
    uint8_t always_update = 0;
    if (!cursor.take_u16(flags) || !cursor.take_u32(morpher.data_link) ||
        !cursor.take_u8(always_update))
        return false;
    morpher.always_update = always_update != 0;
    if (header.version < kInterpolatorsFrom) return true;
    if (header.version >= kInlineMorphWeightsFrom)
        return read_weighted_interpolators(cursor, morpher);
    // At 10 2 0 0 one plain NiFloatInterpolator link per frame of the NiMorphData
    return read_links(cursor, morpher.weight_interpolator_links);
}

bool read_morph_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    // Older streams store per target key arrays here a layout this decoder does not read
    if (header.version < kInterpolatorsFrom) return false;
    NifMorphData& morph = animation_of(block).morph;
    uint32_t target_count = 0;
    uint8_t uses_relative_targets = 0;
    if (!cursor.take_u32(target_count) || !cursor.take_u32(morph.vertex_count) ||
        !cursor.take_u8(uses_relative_targets) || target_count > kMaxLinks)
        return false;
    morph.relative_targets = uses_relative_targets != 0;
    morph.targets.resize(target_count);
    for (NifMorphTarget& target : morph.targets)
        if (!read_morph_target(cursor, header, morph.vertex_count, target)) return false;
    return true;
}

bool read_look_at_interpolator(Cursor& cursor, const NifHeader& header, NifBlock&) {
    uint16_t flags = 0;
    std::string look_at_name;
    if (!cursor.take_u16(flags) || !skip_link(cursor) ||
        !read_object_name(cursor, header, look_at_name))
        return false;
    if (header.version < kLookAtPoseUntil) {
        char obsolete_pose[kQuatTransformBytes] = {};
        if (!cursor.take(obsolete_pose, sizeof(obsolete_pose))) return false;
    }
    // Translate roll and scale interpolators in that order
    return skip_link(cursor) && skip_link(cursor) && skip_link(cursor);
}

bool read_path_interpolator(Cursor& cursor, const NifHeader&, NifBlock&) {
    uint16_t flags = 0;
    float bank_angle_and_smoothing[2] = {};
    int16_t follow_axis = 0;
    return cursor.take_u16(flags) && skip_enum(cursor) &&   // bank direction
           cursor.take(bank_angle_and_smoothing, sizeof(bank_angle_and_smoothing)) &&
           cursor.take(&follow_axis, sizeof(follow_axis)) &&
           skip_link(cursor) && skip_link(cursor);
}

}
