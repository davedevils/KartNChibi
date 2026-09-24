#include "engine/formats/nif_internal.h"

namespace KnC::nif {

namespace {

// NiMultiTargetTransformController target list stopped at version
constexpr uint32_t kPoseVersion = make_version(20, 5, 0, 1);

} // namespace

bool read_camera(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_av_object(cursor, header, block)) return false;
    uint16_t obsolete_flags = 0;
    float view_frustum[6] = {};   // View frustum left right top bottom near far
    uint8_t is_orthographic = 0;
    float view_port[4] = {};      // View port left right top bottom
    float level_of_detail_adjust = 0.f;
    if (!cursor.take_u16(obsolete_flags) ||
        !cursor.take(view_frustum, sizeof(view_frustum)) ||
        !cursor.take_u8(is_orthographic) ||
        !cursor.take(view_port, sizeof(view_port)) ||
        !cursor.take(&level_of_detail_adjust, sizeof(level_of_detail_adjust)) ||
        !skip_link(cursor))       // scene
        return false;
    std::vector<uint32_t> screen_element_links;
    return read_links(cursor, screen_element_links) &&   // reads polygons then textures link lists
           read_links(cursor, screen_element_links);
}

bool read_directional_light(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    return read_light(cursor, header, block, true);
}

// NiDeferredPointLight lacks affected node list 542 blocks exact
bool read_deferred_point_light(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_light(cursor, header, block, false)) return false;
    block.light->has_attenuation = true;
    return cursor.take(block.light->attenuation, sizeof(block.light->attenuation));
}

// NiPointLight with affected nodes three attenuation constants 157
bool read_point_light(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_light(cursor, header, block, true)) return false;
    block.light->has_attenuation = true;
    return cursor.take(block.light->attenuation, sizeof(block.light->attenuation));
}

// NiSpotLight adds outer inner angles and exponent
bool read_spot_light(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_point_light(cursor, header, block)) return false;
    float outer_angle = 0.f;
    float inner_angle = 0.f;
    float exponent = 0.f;
    if (!cursor.take(&outer_angle, sizeof(outer_angle))) return false;
    if (header.version >= make_version(20, 2, 0, 5) &&
        !cursor.take(&inner_angle, sizeof(inner_angle))) return false;
    return cursor.take(&exponent, sizeof(exponent));
}

// NiDeferredLightDimmerController targets NiDeferredPointLight 53 blocks
bool read_deferred_light_dimmer_controller(Cursor& cursor, const NifHeader&,
                                           NifBlock& block) {
    return read_single_interp_controller(cursor, block);
}

bool read_material_colour_controller(Cursor& cursor, const NifHeader&, NifBlock& block) {
    if (!read_single_interp_controller(cursor, block)) return false;
    uint16_t target_colour_flags = 0;
    return cursor.take_u16(target_colour_flags);
}

bool read_multi_target_transform_controller(Cursor& cursor, const NifHeader& header,
                                            NifBlock& block) {
    if (!read_time_controller(cursor, block)) return false;
    if (header.version >= kPoseVersion) return true;
    uint16_t target_count = 0;
    if (!cursor.take_u16(target_count)) return false;
    for (uint16_t target = 0; target < target_count; ++target)
        if (!skip_link(cursor)) return false;
    return true;
}

bool read_boolean_extra_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_name(cursor, header, block.name)) return false;
    uint8_t boolean_value = 0;   // NiBool is one byte
    return cursor.take_u8(boolean_value);
}

bool read_integers_extra_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_name(cursor, header, block.name)) return false;
    uint32_t value_count = 0;
    if (!cursor.take_u32(value_count)) return false;
    return cursor.skip(static_cast<size_t>(value_count) * sizeof(int32_t));
}

}
