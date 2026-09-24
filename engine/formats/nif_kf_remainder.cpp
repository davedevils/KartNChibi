#include "engine/formats/nif_internal.h"

namespace KnC::nif {

namespace {

// kInterpolatorsFrom gate below transform controller links data bool key

constexpr uint32_t kBoolKeyBytes = sizeof(float) + 1;
// One compressed channel carries offset and half range
constexpr uint32_t kFloatCompressionScalarCount = 2;

// One key array interpolation enum version gated below 10 1 0 104
bool read_emitter_active_keys(Cursor& cursor, const NifHeader& header) {
    uint32_t count = 0;
    if (!cursor.take_u32(count)) return false;
    if (count == 0) return true;
    if (header.version >= kInterpolatorsFrom && !skip_enum(cursor)) return false;
    return cursor.skip(static_cast<size_t>(count) * kBoolKeyBytes);
}

} // namespace

bool read_bspline_comp_float_interpolator(Cursor& cursor, const NifHeader&,
                                          NifBlock&) {
    float time_range[2] = {};   // Start end
    if (!cursor.take(time_range, sizeof(time_range)) ||
        !skip_link(cursor) ||   // control points then basis
        !skip_link(cursor))
        return false;
    // Value held case where spline cannot be sampled
    float pose_value = 0.f;
    uint32_t control_point_handle = 0;
    float compression_scalars[kFloatCompressionScalarCount] = {};
    return cursor.take(&pose_value, sizeof(pose_value)) &&
           cursor.take_u32(control_point_handle) &&
           cursor.take(compression_scalars, sizeof(compression_scalars));
}

bool read_colour_interpolator(Cursor& cursor, const NifHeader&, NifBlock& block) {
    // Colour held case where data block has no keys
    NifInterpolator& interpolator = animation_of(block).interpolator;
    return cursor.take(interpolator.pose_colour, sizeof(interpolator.pose_colour)) &&
           cursor.take_u32(interpolator.data_link);
}

bool read_keyframe_controller(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    // Above gate type no longer exists written NiTransformController links
    if (header.version >= kInterpolatorsFrom) return false;
    NifAnimation& animation = animation_of(block);
    return read_time_controller(cursor, block) &&
           cursor.take_u32(animation.controller.interpolator_link);
}

bool read_keyframe_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    NifAnimation& animation = animation_of(block);
    return read_rotation_key_group(cursor, header, animation) &&
           read_key_group(cursor, KeyContent::Point, animation.translations) &&
           read_key_group(cursor, KeyContent::Float, animation.scales);
}

// Two channels one block only birth rate retained payload one
bool read_psys_emitter_ctlr_data(Cursor& cursor, const NifHeader& header,
                                 NifBlock& block) {
    return read_key_group(cursor, KeyContent::Float, animation_of(block).channel) &&
           read_emitter_active_keys(cursor, header);
}

}
