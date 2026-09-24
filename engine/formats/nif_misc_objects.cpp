#include "engine/formats/nif_internal.h"

namespace KnC::nif {

namespace {

// NiPixelFormat traded colour masks for per channel component specs
constexpr uint32_t kComponentPixelFormatFrom = make_version(10, 3, 0, 3);
// Colour space flag pixel format carries
constexpr uint32_t kPixelFormatSrgbFrom = make_version(20, 3, 0, 4);
// Below this texture payload one face count not streamed
constexpr uint32_t kPixelFaceCountFrom = make_version(10, 3, 0, 6);
// NiPersistentSrcTextureRendererData streaming pad offset here
constexpr uint32_t kPixelPadOffsetFrom = make_version(20, 2, 0, 6);

// Width height offset into payload per level
constexpr uint32_t kMipmapLevelBytes = 3 * sizeof(uint32_t);
// SDK reads mipmap table into 16 entry stack array
constexpr uint32_t kMaxMipmapLevels = 16;

// Below 10 3 0 3 pixel format meta format plus DirectDraw masks
bool read_legacy_pixel_format(Cursor& cursor) {
    uint32_t colour_masks[4] = {};
    uint32_t bits_per_pixel = 0;
    uint32_t obsolete_fast_compare[2] = {};
    return skip_enum(cursor) &&                        // format
           cursor.take(colour_masks, sizeof(colour_masks)) &&
           cursor.take_u32(bits_per_pixel) &&
           cursor.take(obsolete_fast_compare, sizeof(obsolete_fast_compare)) &&
           skip_enum(cursor);                          // tiling
}

// One channel what holds encoded width sign byte streamed version
bool read_pixel_component(Cursor& cursor) {
    uint8_t bits_per_component = 0;
    uint8_t is_signed = 0;
    return skip_enum(cursor) && skip_enum(cursor) &&   // NiBool one byte type and representation enums skipped
           cursor.take_u8(bits_per_component) && cursor.take_u8(is_signed);
}

bool read_component_pixel_format(Cursor& cursor, const NifHeader& header) {
    constexpr int kComponentCount = 4;
    uint8_t bits_per_pixel = 0;
    uint32_t renderer_hint = 0;
    uint32_t extra_data = 0;
    uint8_t endian_flags = 0;
    if (!skip_enum(cursor) ||                          // format
        !cursor.take_u8(bits_per_pixel) || !cursor.take_u32(renderer_hint) ||
        !cursor.take_u32(extra_data) || !cursor.take_u8(endian_flags) ||
        !skip_enum(cursor))                            // tiling
        return false;
    if (header.version >= kPixelFormatSrgbFrom) {
        uint8_t is_srgb_space = 0;
        if (!cursor.take_u8(is_srgb_space)) return false;
    }
    for (int component = 0; component < kComponentCount; ++component)
        if (!read_pixel_component(cursor)) return false;
    return true;
}

bool read_pixel_format(Cursor& cursor, const NifHeader& header) {
    if (header.version < kComponentPixelFormatFrom)
        return read_legacy_pixel_format(cursor);
    return read_component_pixel_format(cursor, header);
}

// Both texture payload classes stream before diverge trailing offset
bool read_texture_payload_header(Cursor& cursor, const NifHeader& header,
                                 uint32_t& payload_bytes) {
    uint32_t mipmap_levels = 0;
    uint32_t pixel_stride = 0;
    if (!read_pixel_format(cursor, header) ||
        !skip_link(cursor) ||                          // palette
        !cursor.take_u32(mipmap_levels) || !cursor.take_u32(pixel_stride))
        return false;
    if (mipmap_levels > kMaxMipmapLevels) return false;
    if (!cursor.skip(static_cast<size_t>(mipmap_levels) * kMipmapLevelBytes))
        return false;
    return cursor.take_u32(payload_bytes);
}

} // namespace

bool read_ambient_light(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    // NiAmbientLight adds nothing own NiLight verbatim
    return read_light(cursor, header, block, true);
}

bool read_pixel_data(Cursor& cursor, const NifHeader& header, NifBlock&) {
    uint32_t payload_bytes = 0;
    if (!read_texture_payload_header(cursor, header, payload_bytes)) return false;
    uint32_t faces = 1;   // Cube faces became explicit at 10 3 0 6
    if (header.version >= kPixelFaceCountFrom && !cursor.take_u32(faces))
        return false;
    return cursor.skip(static_cast<size_t>(payload_bytes) * faces);
}

bool read_point3_interpolator(Cursor& cursor, const NifHeader&, NifBlock& block) {
    // Point held case where data block has no keys
    NifInterpolator& interpolator = animation_of(block).interpolator;
    return cursor.take(interpolator.pose_translation,
                       sizeof(interpolator.pose_translation)) &&
           cursor.take_u32(interpolator.data_link);
}

bool read_persistent_src_texture_renderer_data(Cursor& cursor, const NifHeader& header,
                                               NifBlock&) {
    uint32_t payload_bytes = 0;
    if (!read_texture_payload_header(cursor, header, payload_bytes)) return false;
    if (header.version >= kPixelPadOffsetFrom) {
        uint32_t pad_offset_in_bytes = 0;
        if (!cursor.take_u32(pad_offset_in_bytes)) return false;
    }
    // Unlike NiPixelData this one always streams face count
    uint32_t faces = 0;
    return cursor.take_u32(faces) &&
           skip_enum(cursor) &&                        // target renderer
           cursor.skip(static_cast<size_t>(payload_bytes) * faces);
}

bool read_colour_data(Cursor& cursor, const NifHeader&, NifBlock& block) {
    return read_key_group(cursor, KeyContent::Colour, animation_of(block).channel);
}

}
