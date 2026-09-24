#include "engine/formats/nif_internal.h"

#include <memory>

namespace KnC::nif {

namespace {

// Anisotropy joined NiTexturingProperty Map
constexpr uint32_t kMapAnisotropyFrom = make_version(20, 5, 0, 4);
// Two console era shorts every map before this version
constexpr uint32_t kMapLegacyShortsUntil = make_version(10, 3, 0, 4);
// Map transform flag 10 1 0 0 property flags 10 0 1 2 TestWheel
constexpr uint32_t kMapTransformFrom = make_version(10, 1, 0, 0);
constexpr uint32_t kTexturingFlagsUntil = make_version(10, 0, 1, 2);
// Parallax slot only holds ParallaxMap once format knows type
constexpr uint32_t kParallaxMapFrom = make_version(20, 2, 0, 5);
// NiSourceTexture gained two hints each own version
constexpr uint32_t kDirectRendererHintFrom = make_version(10, 1, 0, 103);
constexpr uint32_t kPersistentRendererDataFrom = make_version(20, 2, 0, 4);
// NiStencilProperty split mask into compare write masks
constexpr uint32_t kStencilWriteMaskFrom = make_version(30, 3, 0, 5);
// From 20 3 1 0 two bytes between flags test 15 bytes 17 5155 AK
constexpr uint32_t kAlphaExtraPairFrom = make_version(20, 3, 1, 0);

// Slot renderer draws with others tint glow perturb
constexpr uint32_t kBaseMapSlot = 0;
constexpr uint32_t kDetailMapSlot = 2;
// Slots map carries members beyond common ones
constexpr uint32_t kBumpMapSlot = 5;
constexpr uint32_t kParallaxMapSlot = 7;

// Map slot image matrix clamp span coordinates outside 0 1
struct TextureMap {
    uint32_t         texture_link = kNoLink;
    NifTextureMatrix matrix;
    NifTextureClamp  clamp = NifTextureClamp::WrapUWrapV;
    NifTextureFilter filter = NifTextureFilter::Trilerp;
    NifPropertySpan  clamp_span;
};

// NiTextureTransform translate scale rotation angle method centre
bool read_texture_transform(Cursor& cursor, NifTextureMatrix& matrix) {
    float translate_scale_rotation[5] = {};
    if (!cursor.take(translate_scale_rotation, sizeof(translate_scale_rotation)) ||
        !cursor.take_u32(matrix.method) || !cursor.take(matrix.centre, sizeof(matrix.centre)))
        return false;
    matrix.translation[0] = translate_scale_rotation[0];
    matrix.translation[1] = translate_scale_rotation[1];
    matrix.scale[0] = translate_scale_rotation[2];
    matrix.scale[1] = translate_scale_rotation[3];
    matrix.rotation = translate_scale_rotation[4];
    matrix.is_present = true;
    return true;
}

// NiTexturingProperty Map body shared every map class no transform
bool read_texture_map(Cursor& cursor, const NifHeader& header, uint64_t block_offset,
                      TextureMap& map) {
    if (!cursor.take_u32(map.texture_link)) return false;
    if (header.version < kPropertyEnumsUntil) {
        uint32_t clamp_mode = 0;
        uint32_t texture_coordinate_set = 0;
        // Clamp mode filter mode later flags
        uint32_t filter_mode = 0;
        map.clamp_span = {cursor.position() - block_offset, 4, 0, 0x3};
        if (!cursor.take_u32(clamp_mode) || !cursor.take_u32(filter_mode) ||
            !cursor.take_u32(texture_coordinate_set)) return false;
        map.clamp = static_cast<NifTextureClamp>(clamp_mode & 0x3u);
        map.filter = static_cast<NifTextureFilter>(filter_mode & 0x7u);
    } else {
        uint16_t flags = 0;
        map.clamp_span = {cursor.position() - block_offset, 2, 12, 0x3};
        if (!cursor.take_u16(flags)) return false;
        map.clamp = static_cast<NifTextureClamp>((flags >> 12) & 0x3u);
        map.filter = static_cast<NifTextureFilter>(flags & 0x7u);
    }
    if (header.version >= kMapAnisotropyFrom) {
        uint16_t max_anisotropy = 0;
        if (!cursor.take_u16(max_anisotropy)) return false;
    }
    if (header.version < kMapLegacyShortsUntil) {
        uint16_t legacy_l = 0;
        uint16_t legacy_k = 0;
        if (!cursor.take_u16(legacy_l) || !cursor.take_u16(legacy_k)) return false;
    }
    if (header.version < kMapTransformFrom) return true;
    uint8_t has_texture_transform = 0;
    if (!cursor.take_u8(has_texture_transform)) return false;
    if (has_texture_transform != 1) return true;
    return read_texture_transform(cursor, map.matrix);
}

// One map list entry slot index picks map class
bool read_map_slot(Cursor& cursor, const NifHeader& header, uint32_t slot,
                   NifBlock& block) {
    uint8_t has_map = 0;
    if (!cursor.take_u8(has_map)) return false;
    if (has_map == 0) return true;
    TextureMap map;
    if (!read_texture_map(cursor, header, block.byte_offset, map)) return false;
    if (slot == kBaseMapSlot) {
        block.base_texture_link = map.texture_link;
        block.base_texture_matrix = map.matrix;
        render_state_of(block).base_texture_clamp = map.clamp;
        render_state_of(block).base_texture_filter = map.filter;
        property_spans_of(block).texture_clamp = map.clamp_span;
    } else if (map.texture_link != kNoLink) {
        block.extra_texture_links.push_back(map.texture_link);
        block.extra_texture_slots.push_back(slot);
        if (slot == kDetailMapSlot) {
            block.detail_texture_link = map.texture_link;
            block.detail_texture_matrix = map.matrix;
        }
    }

    if (slot == kBumpMapSlot) {
        float luma_and_bump_matrix[6] = {};   // Luma scale offset 2 by 2 matrix
        return cursor.take(luma_and_bump_matrix, sizeof(luma_and_bump_matrix));
    }
    if (slot == kParallaxMapSlot && header.version >= kParallaxMapFrom) {
        float parallax_offset = 0.f;
        return cursor.take(&parallax_offset, sizeof(parallax_offset));
    }
    return true;
}

// Shader maps separate counted list maps each id
bool read_shader_map_list(Cursor& cursor, const NifHeader& header, uint64_t block_offset) {
    uint32_t shader_map_count = 0;
    if (!cursor.take_u32(shader_map_count) || shader_map_count > kMaxLinks) return false;
    for (uint32_t i = 0; i < shader_map_count; ++i) {
        uint8_t has_map = 0;
        if (!cursor.take_u8(has_map)) return false;
        if (has_map == 0) continue;
        TextureMap map;
        uint32_t shader_id = 0;
        if (!read_texture_map(cursor, header, block_offset, map) ||
            !cursor.take_u32(shader_id)) return false;
    }
    return true;
}

// Below 20 1 0 2 stencil state field by field the draw mode is the last enum
bool read_legacy_stencil_state(Cursor& cursor, NifBlock& block) {
    uint8_t stencil_enabled = 0;
    uint32_t reference = 0;
    uint32_t mask = 0;
    if (!cursor.take_u8(stencil_enabled) || !skip_enum(cursor)) return false;
    if (!cursor.take_u32(reference) || !cursor.take_u32(mask)) return false;
    // Fail action pass depth fail pass then the draw mode
    if (!skip_enum(cursor) || !skip_enum(cursor) || !skip_enum(cursor)) return false;
    if (!cursor.take_u32(block.stencil_draw_mode)) return false;
    block.has_stencil = true;
    return true;
}

} // namespace

// TexturingFlags apply mode bits 1 to 3 client 0x00648E9C
bool read_texturing_property(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_net(cursor, header, block)) return false;
    if (header.version <= kTexturingFlagsUntil) {
        uint16_t legacy_flags = 0;   // Flags word own up to 10 0 1 2
        if (!cursor.take_u16(legacy_flags)) return false;
    }
    uint32_t apply_mode = 0;
    const uint64_t apply_at = cursor.position() - block.byte_offset;
    if (header.version < kPropertyEnumsUntil) {
        property_spans_of(block).texture_apply = {apply_at, 4, 0, 0};
        if (!cursor.take_u32(apply_mode)) return false;
    } else {
        uint16_t flags = 0;
        property_spans_of(block).texture_apply = {apply_at, 2, 1, 0x7};
        if (!cursor.take_u16(flags)) return false;
        apply_mode = (flags >> 1) & 0x7u;
    }
    render_state_of(block).texture_apply = static_cast<NifTextureApplyMode>(apply_mode);
    uint32_t map_count = 0;
    if (!cursor.take_u32(map_count) || map_count > kMaxLinks) return false;
    for (uint32_t slot = 0; slot < map_count; ++slot)
        if (!read_map_slot(cursor, header, slot, block)) return false;
    return read_shader_map_list(cursor, header, block.byte_offset);
}

bool read_source_texture(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_net(cursor, header, block)) return false;   // NiTexture adds nothing
    uint8_t is_external = 0;
    if (!cursor.take_u8(is_external)) return false;
    // Filename obeys string table gate as object name
    if (!read_object_name(cursor, header, block.texture_file_name)) return false;
    if (!skip_link(cursor)) return false;
    // skips pixel data link then reads layout mipmap alpha format enums
    if (!skip_enum(cursor) || !skip_enum(cursor) || !skip_enum(cursor)) return false;
    uint8_t is_static = 0;
    if (!cursor.take_u8(is_static)) return false;
    if (header.version >= kDirectRendererHintFrom) {
        uint8_t load_direct_to_renderer = 0;
        if (!cursor.take_u8(load_direct_to_renderer)) return false;
    }
    if (header.version < kPersistentRendererDataFrom) return true;
    uint8_t renderer_data_is_persistent = 0;
    return cursor.take_u8(renderer_data_is_persistent);
}

NifRenderState& render_state_of(NifBlock& block) {
    if (!block.render_state) block.render_state = std::make_shared<NifRenderState>();
    return *block.render_state;
}

NifPropertySpans& property_spans_of(NifBlock& block) {
    if (!block.property_spans) block.property_spans = std::make_shared<NifPropertySpans>();
    return *block.property_spans;
}

bool read_z_buffer_property(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_net(cursor, header, block)) return false;
    property_spans_of(block).depth_flags = {cursor.position() - block.byte_offset, 2, 0, 0};
    NifDepthState& depth = render_state_of(block).depth;
    if (!cursor.take_u16(depth.flags)) return false;
    if (header.version >= kPropertyEnumsUntil) return true;
    // Below 20 1 0 2 the flags word holds test and write only the function is its own enum
    uint32_t test_function = 0;
    if (!cursor.take_u32(test_function)) return false;
    depth.flags = static_cast<uint16_t>((depth.flags & 0x0003u) | ((test_function & 0x7u) << 2));
    return true;
}

// From 20 1 0 2 modes flags bit 3 bits 4 5 32 bit enum vertex
bool read_vertex_colour_property(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_net(cursor, header, block)) return false;
    uint16_t flags = 0;
    NifPropertySpans& spans = property_spans_of(block);
    const uint64_t flags_at = cursor.position() - block.byte_offset;
    if (!cursor.take_u16(flags)) return false;
    NifVertexColourState& state = render_state_of(block).vertex_colour;
    if (header.version >= kPropertyEnumsUntil) {
        spans.lighting_mode = {flags_at, 2, 3, 0x1};
        spans.vertex_mode = {flags_at, 2, 4, 0x3};
        state.lighting = static_cast<NifLightingMode>((flags >> 3) & 0x1u);
        state.vertex = static_cast<NifVertexMode>((flags >> 4) & 0x3u);
        return true;
    }
    uint32_t vertex_mode = 0;
    uint32_t lighting_mode = 0;
    spans.vertex_mode = {cursor.position() - block.byte_offset, 4, 0, 0};
    spans.lighting_mode = {cursor.position() - block.byte_offset + 4, 4, 0, 0};
    if (!cursor.take_u32(vertex_mode) || !cursor.take_u32(lighting_mode)) return false;
    state.vertex = static_cast<NifVertexMode>(vertex_mode);
    state.lighting = static_cast<NifLightingMode>(lighting_mode);
    return true;
}

bool read_alpha_property(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_net(cursor, header, block)) return false;
    NifAlphaState& alpha = render_state_of(block).alpha;
    NifPropertySpans& spans = property_spans_of(block);
    spans.alpha_flags = {cursor.position() - block.byte_offset, 2, 0, 0};
    if (!cursor.take_u16(alpha.flags)) return false;
    if (header.version >= kAlphaExtraPairFrom) {
        // 0x00 then 0x01 all 14184 blocks AK ships two bytes
        uint8_t first = 0;
        uint8_t second = 0;
        if (!cursor.take_u8(first) || !cursor.take_u8(second)) return false;
    }
    spans.alpha_threshold = {cursor.position() - block.byte_offset, 1, 0, 0};
    return cursor.take_u8(alpha.test_threshold);
}

// Four RGB colors glossiness alpha fourteen floats ambient diffuse specular
bool read_material_property(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_net(cursor, header, block)) return false;
    NifMaterialState& material = render_state_of(block).material;
    block.material_offset = cursor.position() - block.byte_offset;
    return cursor.take(material.ambient, sizeof(material.ambient)) &&
           cursor.take(material.diffuse, sizeof(material.diffuse)) &&
           cursor.take(material.specular, sizeof(material.specular)) &&
           cursor.take(material.emissive, sizeof(material.emissive)) &&
           cursor.take_f32(material.glossiness) && cursor.take_f32(material.alpha);
}

bool read_stencil_property(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_net(cursor, header, block)) return false;
    if (header.version < kPropertyEnumsUntil) return read_legacy_stencil_state(cursor, block);

    uint16_t flags = 0;
    uint32_t reference = 0;
    uint32_t compare_mask = 0;
    if (!cursor.take_u16(flags) || !cursor.take_u32(reference) ||
        !cursor.take_u32(compare_mask)) return false;
    // Bits 13 and 14 of the flags word carry the draw mode
    block.stencil_draw_mode = (flags >> 13) & 3;
    block.has_stencil = true;
    if (header.version < kStencilWriteMaskFrom) return true;
    uint32_t write_mask = 0;
    return cursor.take_u32(write_mask);
}

bool read_specular_property(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_net(cursor, header, block)) return false;
    uint16_t flags = 0;
    return cursor.take_u16(flags);
}

// Flat or smooth shading flags word same specular dither AK three bytes
bool read_shade_property(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_net(cursor, header, block)) return false;
    uint16_t flags = 0;
    return cursor.take_u16(flags);
}

bool read_dither_property(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    if (!read_object_net(cursor, header, block)) return false;
    uint16_t flags = 0;
    return cursor.take_u16(flags);
}

}
