#include "engine/formats/nif_internal.h"

#include <memory>

namespace KnC::nif {

namespace {

// One word packs texture sets low six bits normal basis top four
constexpr uint16_t kTextureSetCountMask   = 0x003F;
constexpr uint16_t kNormalBasisMethodMask = 0xF000;

bool skip_floats(Cursor& cursor, size_t float_count) {
    return cursor.skip(float_count * sizeof(float));
}

// Named normal basis appends tangents bitangents bump shader reads kept round trip
bool read_normal_arrays(Cursor& cursor, const NifHeader& header, uint16_t vertex_count,
                        uint16_t data_flags, NifBlock& block) {
    NifComponentFormat format = NifComponentFormat::Absent;
    NifGeometrySource& source = geometry_source_of(block);
    if (!take_component_format(cursor, header, format, source.raw_normal_format)) return false;
    if (format == NifComponentFormat::Absent) return true;
    const size_t components = static_cast<size_t>(vertex_count) * 3u;
    block.vertex_spans->normals = cursor.position() - block.byte_offset;
    block.vertex_spans->normal_format = static_cast<uint8_t>(format);
    if (!read_component_array(cursor, format, components, block.normals)) return false;
    if ((data_flags & kNormalBasisMethodMask) == 0) return true;
    return read_component_array(cursor, format, components * 2u, source.tangents_bitangents);
}

// Sets stored one whole after another first leads no format 343516 AK
bool read_uv_sets(Cursor& cursor, uint16_t vertex_count, uint16_t data_flags,
                  NifComponentFormat format, NifBlock& block) {
    const size_t set_count = data_flags & kTextureSetCountMask;
    if (set_count == 0) return true;
    const size_t components = static_cast<size_t>(vertex_count) * 2u;
    block.vertex_spans->uvs = cursor.position() - block.byte_offset;
    block.vertex_spans->uv_format = static_cast<uint8_t>(format);
    if (!read_component_array(cursor, format, components, block.uvs)) return false;
    return read_component_array(cursor, format, components * (set_count - 1u),
                                geometry_source_of(block).extra_uv_sets);
}

// Every array has own flag one misread loses whole stream
bool read_geometry_data(Cursor& cursor, const NifHeader& header, NifBlock& block,
                        uint16_t& vertex_count_out) {
    NifGeometrySource& source = geometry_source_of(block);
    if (header.version >= kGeometryGroupIdFrom && !cursor.take_u32(source.group_id))
        return false;

    if (!cursor.take_u16(source.vertex_count)) return false;
    if (!cursor.take_u8(source.keep_flags) || !cursor.take_u8(source.compress_flags))
        return false;
    const uint16_t vertex_count = source.vertex_count;

    block.vertex_spans = std::make_shared<NifVertexSpans>();
    NifComponentFormat vertex_format = NifComponentFormat::Absent;
    if (!take_component_format(cursor, header, vertex_format, source.raw_vertex_format))
        return false;
    block.vertex_spans->vertex_format = static_cast<uint8_t>(vertex_format);
    if (vertex_format != NifComponentFormat::Absent)
        block.vertex_spans->vertices = cursor.position() - block.byte_offset;
    if (!read_component_array(cursor, vertex_format,
                              static_cast<size_t>(vertex_count) * 3u, block.vertices))
        return false;

    if (!cursor.take_u16(source.data_flags)) return false;
    if (!read_normal_arrays(cursor, header, vertex_count, source.data_flags, block))
        return false;

    // Bounding sphere centre then radius
    if (!cursor.take(block.bound_center, sizeof(block.bound_center)) ||
        !cursor.take(&block.bound_radius, sizeof(block.bound_radius)))
        return false;

    NifComponentFormat colour_format = NifComponentFormat::Absent;
    if (!take_component_format(cursor, header, colour_format, source.raw_colour_format))
        return false;
    block.vertex_spans->colour_format = static_cast<uint8_t>(colour_format);
    if (colour_format != NifComponentFormat::Absent)
        block.vertex_spans->colours = cursor.position() - block.byte_offset;
    if (!read_component_array(cursor, colour_format,
                              static_cast<size_t>(vertex_count) * 4u, block.vertex_colours))
        return false;

    if (!read_uv_sets(cursor, vertex_count, source.data_flags, vertex_format, block))
        return false;

    vertex_count_out = vertex_count;
    if (!cursor.take_u16(source.consistency_flags)) return false;
    if (header.version < kAdditionalGeometryDataFrom) return true;
    return cursor.take_u32(source.additional_data_link);
}

// NiTriBasedGeomData adds only triangle count
bool read_tri_based_geom_data(Cursor& cursor, const NifHeader& header,
                              NifBlock& block, uint16_t& triangle_count) {
    uint16_t vertex_count = 0;
    if (!read_geometry_data(cursor, header, block, vertex_count)) return false;
    NifGeometrySource& source = geometry_source_of(block);
    if (!cursor.take_u16(source.triangle_count)) return false;
    triangle_count = source.triangle_count;
    return true;
}

// Each group names vertices that share one normal
bool read_shared_normal_groups(Cursor& cursor, NifBlock& block) {
    uint16_t group_count = 0;
    if (!cursor.take_u16(group_count)) return false;
    std::vector<std::vector<uint16_t>>& groups = geometry_source_of(block).shared_normal_groups;
    groups.resize(group_count);
    for (std::vector<uint16_t>& group : groups) {
        uint16_t vertex_count = 0;
        if (!cursor.take_u16(vertex_count)) return false;
        if (!take_u16_array(cursor, vertex_count, group)) return false;
    }
    return true;
}

} // namespace

NifGeometrySource& geometry_source_of(NifBlock& block) {
    if (!block.geometry_source) block.geometry_source = std::make_shared<NifGeometrySource>();
    return *block.geometry_source;
}

void expand_strip(const uint16_t* strip, size_t length, std::vector<uint16_t>& out) {
    for (size_t corner = 2; corner < length; ++corner) {
        const uint16_t a = strip[corner - 2], b = strip[corner - 1], c = strip[corner];
        if (a == b || b == c || a == c) continue;
        const bool flipped = (corner % 2) != 0;
        out.push_back(a);
        out.push_back(flipped ? c : b);
        out.push_back(flipped ? b : c);
    }
}

// Blocks descend from NiObject not NiObjectNET carry no name

bool read_tri_shape_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    uint16_t triangle_count = 0;
    if (!read_tri_based_geom_data(cursor, header, block, triangle_count)) return false;
    NifGeometrySource& source = geometry_source_of(block);

    // Index count streamed in own uint32 not derived
    if (!cursor.take_u32(source.index_count)) return false;
    if (source.index_count > 3u * static_cast<uint32_t>(triangle_count)) return false;

    if (!cursor.take_u8(source.has_indices)) return false;
    if (source.has_indices != 0 && !take_u16_array(cursor, source.index_count, block.triangles))
        return false;

    return read_shared_normal_groups(cursor, block);
}

bool read_tri_strips_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    uint16_t triangle_count = 0;
    if (!read_tri_based_geom_data(cursor, header, block, triangle_count)) return false;
    NifGeometrySource& source = geometry_source_of(block);

    uint16_t strip_count = 0;
    if (!cursor.take_u16(strip_count)) return false;
    // Without strips lengths flag indices not written
    if (strip_count == 0) return true;

    if (!take_u16_array(cursor, strip_count, source.strip_lengths)) return false;

    if (!cursor.take_u8(source.has_strip_indices)) return false;
    if (source.has_strip_indices == 0) return true;

    // Reference derives total in 16 bits not sum of lengths
    const uint16_t strip_index_count =
        static_cast<uint16_t>(triangle_count + 2u * strip_count);
    if (!take_u16_array(cursor, strip_index_count, source.strip_indices)) return false;

    size_t offset = 0;
    for (const uint16_t length : source.strip_lengths) {
        if (offset + length > source.strip_indices.size()) break;
        expand_strip(source.strip_indices.data() + offset, length, block.triangles);
        offset += length;
    }
    return true;
}

bool read_range_lod_data(Cursor& cursor, const NifHeader&, NifBlock&) {
    float lod_centre[3] = {};
    if (!cursor.take(lod_centre, sizeof(lod_centre))) return false;

    uint32_t range_count = 0;
    if (!cursor.take_u32(range_count)) return false;
    // Count this large is lost stream not LOD chain
    if (range_count > kMaxLinks) return false;
    return skip_floats(cursor, static_cast<size_t>(range_count) * 2u);
}

// NiLinesData is NiGeometryData one byte per vertex if joined next
bool read_lines_data(Cursor& cursor, const NifHeader& header, NifBlock& block) {
    uint16_t vertex_count = 0;
    return read_geometry_data(cursor, header, block, vertex_count) &&
           cursor.skip(vertex_count);
}

}
