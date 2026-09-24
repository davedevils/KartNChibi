#include "engine/formats/nif_encode.h"

#include "engine/formats/nif_internal.h"

#include <cmath>

namespace KnC {

using namespace nif;

namespace {

// Append only body writer no bounds check multi byte put is memcpy
class BodyWriter {
public:
    void put(const void* bytes, size_t count) {
        out_.append(static_cast<const char*>(bytes), count);
    }
    void put_u8(uint8_t value) { out_.push_back(static_cast<char>(value)); }
    void put_u16(uint16_t value) { put(&value, sizeof(value)); }
    void put_u32(uint32_t value) { put(&value, sizeof(value)); }
    void put_f32(float value) { put(&value, sizeof(value)); }
    void put_u16_array(const std::vector<uint16_t>& values) {
        if (!values.empty()) put(values.data(), values.size() * sizeof(uint16_t));
    }
    void put_links(const std::vector<uint32_t>& links) {
        put_u32(static_cast<uint32_t>(links.size()));
        for (const uint32_t link : links) put_u32(link);
    }
    // Name is counted string below 20 1 0 1 table index from there up
    void put_name(const NifHeader& header, const std::string& text, uint32_t index) {
        if (header.version >= kStringTableFrom) {
            put_u32(index);
            return;
        }
        put_u32(static_cast<uint32_t>(text.size()));
        put(text.data(), text.size());
    }
    void put_components(NifComponentFormat format, const std::vector<float>& values) {
        write_component_array(format, values, out_);
    }
    std::string& bytes() { return out_; }

private:
    std::string out_;
};

// Below 20 3 1 0 presence byte is bool non zero means float array
NifComponentFormat format_of(const NifHeader& header, uint8_t raw_format) {
    if (header.version < kComponentFormatFrom)
        return raw_format != 0 ? NifComponentFormat::Float32 : NifComponentFormat::Absent;
    return static_cast<NifComponentFormat>(raw_format);
}

bool refuse(std::string& refusal, std::string what) {
    refusal = std::move(what);
    return false;
}

// NiObjectNET has name extra data links controller link
void encode_object_net(const NifHeader& header, const NifBlock& block, BodyWriter& writer) {
    writer.put_name(header, block.name, block.name_index);
    writer.put_links(block.extra_data);
    writer.put_u32(block.controller_link);
}

// NiAVObject has 16 bits flags thirteen floats transform property collision
void encode_av_object(const NifHeader& header, const NifBlock& block, BodyWriter& writer) {
    encode_object_net(header, block, writer);
    writer.put_u16(block.object_flags);
    writer.put(block.transform.translation, sizeof(block.transform.translation));
    writer.put(block.transform.rotation, sizeof(block.transform.rotation));
    writer.put_f32(block.transform.scale);
    writer.put_links(block.properties);
    writer.put_u32(block.collision_link);
}

// NiGeometry is base of NiTriShape and NiTriStrips body
void encode_geometry(const NifHeader& header, const NifBlock& block,
                     const NifGeometrySource& source, BodyWriter& writer) {
    encode_av_object(header, block, writer);
    writer.put_u32(block.data_link);
    writer.put_u32(block.skin_instance_link);
    if (header.version >= kSizeTableFrom) {
        writer.put_u32(static_cast<uint32_t>(source.materials.size()));
        for (const NifGeometryMaterial& material : source.materials) {
            writer.put_name(header, material.name, material.name_index);
            writer.put_u32(material.extra_data);
        }
        writer.put_u32(source.active_material);
    } else {
        writer.put_u8(source.has_shader);
        if (source.has_shader != 0) {
            writer.put_name(header, source.shader_name, source.shader_name_index);
            writer.put_u32(source.shader_implementation);
        }
    }
    if (header.version >= kGeometryDirtyFlagFrom) writer.put_u8(source.is_dirty);
}

// NiGeometryData has vertex arrays behind presence byte naming packing sphere
void encode_geometry_data(const NifHeader& header, const NifBlock& block,
                          const NifGeometrySource& source, BodyWriter& writer) {
    if (header.version >= kGeometryGroupIdFrom) writer.put_u32(source.group_id);
    writer.put_u16(source.vertex_count);
    writer.put_u8(source.keep_flags);
    writer.put_u8(source.compress_flags);

    const NifComponentFormat vertex_format = format_of(header, source.raw_vertex_format);
    writer.put_u8(source.raw_vertex_format);
    writer.put_components(vertex_format, block.vertices);

    writer.put_u16(source.data_flags);
    const NifComponentFormat normal_format = format_of(header, source.raw_normal_format);
    writer.put_u8(source.raw_normal_format);
    if (normal_format != NifComponentFormat::Absent) {
        writer.put_components(normal_format, block.normals);
        writer.put_components(normal_format, source.tangents_bitangents);
    }

    writer.put(block.bound_center, sizeof(block.bound_center));
    writer.put_f32(block.bound_radius);

    const NifComponentFormat colour_format = format_of(header, source.raw_colour_format);
    writer.put_u8(source.raw_colour_format);
    writer.put_components(colour_format, block.vertex_colours);

    writer.put_components(vertex_format, block.uvs);
    writer.put_components(vertex_format, source.extra_uv_sets);

    writer.put_u16(source.consistency_flags);
    if (header.version >= kAdditionalGeometryDataFrom)
        writer.put_u32(source.additional_data_link);
}

// NiTriStripsData keeps strips not expanded triangles reader drops stitching
void encode_tri_strips_data(const NifHeader& header, const NifBlock& block,
                            const NifGeometrySource& source, BodyWriter& writer) {
    encode_geometry_data(header, block, source, writer);
    writer.put_u16(source.triangle_count);
    writer.put_u16(static_cast<uint16_t>(source.strip_lengths.size()));
    if (source.strip_lengths.empty()) return;
    writer.put_u16_array(source.strip_lengths);
    writer.put_u8(source.has_strip_indices);
    if (source.has_strip_indices != 0) writer.put_u16_array(source.strip_indices);
}

void encode_tri_shape_data(const NifHeader& header, const NifBlock& block,
                           const NifGeometrySource& source, BodyWriter& writer) {
    encode_geometry_data(header, block, source, writer);
    writer.put_u16(source.triangle_count);
    writer.put_u32(source.index_count);
    writer.put_u8(source.has_indices);
    if (source.has_indices != 0) writer.put_u16_array(block.triangles);
    writer.put_u16(static_cast<uint16_t>(source.shared_normal_groups.size()));
    for (const std::vector<uint16_t>& group : source.shared_normal_groups) {
        writer.put_u16(static_cast<uint16_t>(group.size()));
        writer.put_u16_array(group);
    }
}

bool is_geometry(const NifBlock& block) {
    return block.type == "NiTriShape" || block.type == "NiTriStrips";
}

bool is_geometry_data(const NifBlock& block) {
    return block.type == "NiTriShapeData" || block.type == "NiTriStripsData";
}

// Vertex count and triangle index uint16 why mesh must be split
constexpr size_t kMaxVerticesPerBlock = 65535;

// Bounding sphere centre is box centre not vertex centre puts all in
void bound_of(const std::vector<float>& positions, float centre[3], float& radius) {
    float lowest[3] = {positions[0], positions[1], positions[2]};
    float highest[3] = {positions[0], positions[1], positions[2]};
    for (size_t at = 0; at + 2 < positions.size(); at += 3)
        for (int axis = 0; axis < 3; ++axis) {
            if (positions[at + axis] < lowest[axis]) lowest[axis] = positions[at + axis];
            if (positions[at + axis] > highest[axis]) highest[axis] = positions[at + axis];
        }
    for (int axis = 0; axis < 3; ++axis) centre[axis] = (lowest[axis] + highest[axis]) * 0.5f;
    float widest_square = 0.f;
    for (size_t at = 0; at + 2 < positions.size(); at += 3) {
        float square = 0.f;
        for (int axis = 0; axis < 3; ++axis) {
            const float gap = positions[at + axis] - centre[axis];
            square += gap * gap;
        }
        if (square > widest_square) widest_square = square;
    }
    radius = std::sqrt(widest_square);
}

// Skipped array not written filled array must be right length by count
bool array_length_fits(const std::vector<float>& values, size_t vertices, size_t stride,
                       const char* named, std::string& refusal) {
    if (values.empty() || values.size() == vertices * stride) return true;
    return refuse(refusal, std::string("the ") + named + " are " +
                               std::to_string(values.size()) + " floats, " +
                               std::to_string(vertices) + " vertices need " +
                               std::to_string(vertices * stride));
}

// Validate geometry vertex bound is format not policy uint16 limit splits
bool geometry_is_writable(const NifTriShapeGeometry& geometry, size_t& vertices,
                          std::string& refusal) {
    if (geometry.positions.empty() || geometry.positions.size() % 3 != 0)
        return refuse(refusal, "a position array is three floats a vertex and cannot be empty");
    vertices = geometry.positions.size() / 3;
    if (vertices > kMaxVerticesPerBlock)
        return refuse(refusal, "the mesh holds " + std::to_string(vertices) +
                                   " vertices; a data block states its count in a uint16, so "
                                   "it has to be split at " +
                                   std::to_string(kMaxVerticesPerBlock));
    if (geometry.triangles.size() % 3 != 0)
        return refuse(refusal, "the index array is three corners a triangle");
    for (const uint16_t corner : geometry.triangles)
        if (corner >= vertices)
            return refuse(refusal, "index " + std::to_string(corner) + " names no vertex of " +
                                       std::to_string(vertices));
    return array_length_fits(geometry.normals, vertices, 3, "normals", refusal) &&
           array_length_fits(geometry.uvs, vertices, 2, "texture coordinates", refusal) &&
           array_length_fits(geometry.colours, vertices, 4, "vertex colours", refusal);
}

} // namespace

bool can_encode_nif_block(const NifBlock& block) {
    if (block.skipped || !block.geometry_source) return false;
    return is_geometry(block) || is_geometry_data(block);
}

bool encode_nif_block(const NifHeader& header, const NifBlock& block, std::string& out,
                      std::string& refusal) {
    if (block.skipped)
        return refuse(refusal, "the reader stepped over this block and read nothing in it");
    if (!block.geometry_source)
        return refuse(refusal, block.type + " carries no decoded source to write back");
    if (!is_geometry(block) && !is_geometry_data(block))
        return refuse(refusal, "no encoder for " + block.type);

    BodyWriter writer;
    // Below 10 1 0 114 NiObject group id stands ahead of block body
    if (header.version < kGeometryGroupIdFrom) writer.put_u32(block.object_group_id);
    const NifGeometrySource& source = *block.geometry_source;
    if (is_geometry(block)) encode_geometry(header, block, source, writer);
    else if (block.type == "NiTriShapeData") encode_tri_shape_data(header, block, source, writer);
    else encode_tri_strips_data(header, block, source, writer);
    out = std::move(writer.bytes());
    return true;
}

bool build_nif_tri_shape_data(const NifTriShapeGeometry& geometry, NifBlock& out,
                              std::string& refusal) {
    size_t vertices = 0;
    if (!geometry_is_writable(geometry, vertices, refusal)) return false;

    out = NifBlock{};
    out.type = "NiTriShapeData";
    out.vertices = geometry.positions;
    out.normals = geometry.normals;
    out.uvs = geometry.uvs;
    out.vertex_colours = geometry.colours;
    out.triangles = geometry.triangles;
    bound_of(out.vertices, out.bound_center, out.bound_radius);

    NifGeometrySource& source = *(out.geometry_source = std::make_shared<NifGeometrySource>());
    source.vertex_count = static_cast<uint16_t>(vertices);
    // Float32 is 0x01 also plain array there byte below 20 3 1 0
    source.raw_vertex_format = static_cast<uint8_t>(NifComponentFormat::Float32);
    source.raw_normal_format = geometry.normals.empty()
                                   ? 0
                                   : static_cast<uint8_t>(NifComponentFormat::Float32);
    source.raw_colour_format = geometry.colours.empty()
                                   ? 0
                                   : static_cast<uint8_t>(NifComponentFormat::Float32);
    source.data_flags = geometry.uvs.empty() ? 0 : 1;   // One texture set no normal basis
    source.triangle_count = static_cast<uint16_t>(geometry.triangles.size() / 3);
    source.index_count = static_cast<uint32_t>(geometry.triangles.size());
    source.has_indices = geometry.triangles.empty() ? 0 : 1;
    return true;
}

} // namespace KnC
