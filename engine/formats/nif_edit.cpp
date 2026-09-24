#include "engine/formats/nif_edit.h"

#include <cstring>

namespace KnC {

namespace {

// translation 3x3 rotation in row order the scale contiguous in that order
constexpr size_t kTransformFloats = 13;
constexpr size_t kTransformBytes = kTransformFloats * sizeof(float);

// From this version an object name is an index into the header fixed string table
constexpr uint32_t kStringTableFrom = 0x14010001u;

bool transform_span_fits(const NifBlock& block, const std::string& body) {
    return block.has_transform && block.transform_offset + kTransformBytes <= body.size();
}

// The floats a transform is in the order the stream holds them
void gather(const NifTransform& transform, float out[kTransformFloats]) {
    std::memcpy(out, transform.translation, sizeof(transform.translation));
    std::memcpy(out + 3, transform.rotation, sizeof(transform.rotation));
    out[12] = transform.scale;
}

void scatter(const float values[kTransformFloats], NifTransform& out) {
    std::memcpy(out.translation, values, sizeof(out.translation));
    std::memcpy(out.rotation, values + 3, sizeof(out.rotation));
    out.scale = values[12];
}

// ambient diffuse specular and emissive as rgb triples then glossiness and alpha
constexpr size_t kMaterialFloats = 14;
constexpr size_t kMaterialBytes = kMaterialFloats * sizeof(float);

bool material_span_fits(const NifBlock& block, const std::string& body) {
    return block.material_offset != 0 &&
           block.material_offset + kMaterialBytes <= body.size();
}

void gather(const NifMaterialState& material, float out[kMaterialFloats]) {
    std::memcpy(out, material.ambient, sizeof(material.ambient));
    std::memcpy(out + 3, material.diffuse, sizeof(material.diffuse));
    std::memcpy(out + 6, material.specular, sizeof(material.specular));
    std::memcpy(out + 9, material.emissive, sizeof(material.emissive));
    out[12] = material.glossiness;
    out[13] = material.alpha;
}

void scatter(const float values[kMaterialFloats], NifMaterialState& out) {
    std::memcpy(out.ambient, values, sizeof(out.ambient));
    std::memcpy(out.diffuse, values + 3, sizeof(out.diffuse));
    std::memcpy(out.specular, values + 6, sizeof(out.specular));
    std::memcpy(out.emissive, values + 9, sizeof(out.emissive));
    out.glossiness = values[12];
    out.alpha = values[13];
}

// The four byte string table index a 20 1 0 1 and above name is
bool read_name_index(const NifBlock& block, const std::string& body, uint32_t& out) {
    if (!block.has_name || block.name_offset + sizeof(uint32_t) > body.size()) return false;
    std::memcpy(&out, body.data() + block.name_offset, sizeof(out));
    return true;
}

// Below 20 1 0 1 the name is a length and that many bytes at the head of NiObjectNET body
bool read_counted_name(const NifBlock& block, const std::string& body, std::string& out) {
    uint32_t length = 0;
    if (!read_name_index(block, body, length)) return false;
    const size_t text_at = static_cast<size_t>(block.name_offset) + sizeof(uint32_t);
    if (text_at + length > body.size()) return false;
    out.assign(body, text_at, length);
    return true;
}

// The index the name will stand at once the edit is written reusing an entry
uint32_t index_for_name(const NifHeader& header, const std::string& name,
                        std::vector<std::string>& added_strings) {
    for (uint32_t index = 0; index < header.strings.size(); ++index)
        if (header.strings[index] == name) return index;
    const uint32_t first_added = static_cast<uint32_t>(header.strings.size());
    for (uint32_t added = 0; added < added_strings.size(); ++added)
        if (added_strings[added] == name) return first_added + added;
    added_strings.push_back(name);
    return first_added + static_cast<uint32_t>(added_strings.size()) - 1;
}

// Which member of NifPropertySpans one NifPropertyValue names
const NifPropertySpan* property_span(const NifBlock& block, NifPropertyValue which) {
    if (!block.property_spans) return nullptr;
    const NifPropertySpans& spans = *block.property_spans;
    switch (which) {
        case NifPropertyValue::AlphaFlags:       return &spans.alpha_flags;
        case NifPropertyValue::AlphaThreshold:   return &spans.alpha_threshold;
        case NifPropertyValue::DepthFlags:       return &spans.depth_flags;
        case NifPropertyValue::LightingMode:     return &spans.lighting_mode;
        case NifPropertyValue::VertexMode:       return &spans.vertex_mode;
        case NifPropertyValue::TextureApplyMode: return &spans.texture_apply;
        case NifPropertyValue::TextureClampMode: return &spans.texture_clamp;
    }
    return nullptr;
}

// The span own word as wide as the stream wrote it false when block body is shorter
bool read_word(const NifPropertySpan& span, const std::string& body, uint32_t& out) {
    if (span.offset == 0 || span.offset + span.width > body.size()) return false;
    out = 0;
    std::memcpy(&out, body.data() + span.offset, span.width);
    return true;
}

// NifComponentFormat Float32 the only width a splice can copy into
constexpr uint8_t kFloat32Format = 0x01;

// Where the named array stands and what width it was written at plus values the parse already holds
struct ArraySpan {
    uint64_t                  offset = 0;
    uint8_t                   format = 0;
    const std::vector<float>* held   = nullptr;
};

ArraySpan array_span(const NifBlock& block, NifVertexArray which) {
    const NifVertexSpans& spans = *block.vertex_spans;
    if (which == NifVertexArray::Positions) return {spans.vertices, spans.vertex_format,
                                                    &block.vertices};
    if (which == NifVertexArray::Normals) return {spans.normals, spans.normal_format,
                                                  &block.normals};
    if (which == NifVertexArray::TextureCoordinates)
        return {spans.uvs, spans.uv_format, &block.uvs};
    return {spans.colours, spans.colour_format, &block.vertex_colours};
}

const char* array_name(NifVertexArray which) {
    if (which == NifVertexArray::Positions) return "positions";
    if (which == NifVertexArray::Normals) return "normals";
    if (which == NifVertexArray::TextureCoordinates) return "texture coordinates";
    return "vertex colours";
}

} // namespace

bool read_nif_property_value(const NifBlock& block, NifPropertyValue which,
                             const std::string& body, uint32_t& out) {
    const NifPropertySpan* span = property_span(block, which);
    uint32_t word = 0;
    if (span == nullptr || !read_word(*span, body, word)) return false;
    out = span->mask == 0 ? word : (word >> span->shift) & span->mask;
    return true;
}

bool splice_nif_property_value(const NifBlock& block, NifPropertyValue which, uint32_t value,
                               std::string& body) {
    const NifPropertySpan* span = property_span(block, which);
    uint32_t word = 0;
    if (span == nullptr || !read_word(*span, body, word)) return false;
    if (span->mask == 0) {
        // The value must fit the width it was written at
        const uint32_t widest = span->width >= 4 ? 0xFFFFFFFFu : (1u << (span->width * 8)) - 1u;
        if (value > widest) return false;
        word = value;
    } else {
        if (value > span->mask) return false;
        word = (word & ~(static_cast<uint32_t>(span->mask) << span->shift)) |
               (value << span->shift);
    }
    std::memcpy(body.data() + span->offset, &word, span->width);
    return true;
}

const char* nif_property_value_name(NifPropertyValue which) {
    switch (which) {
        case NifPropertyValue::AlphaFlags:       return "alpha flags";
        case NifPropertyValue::AlphaThreshold:   return "alpha test reference";
        case NifPropertyValue::DepthFlags:       return "depth flags";
        case NifPropertyValue::LightingMode:     return "lighting mode";
        case NifPropertyValue::VertexMode:       return "vertex mode";
        case NifPropertyValue::TextureApplyMode: return "texture apply mode";
        case NifPropertyValue::TextureClampMode: return "texture clamp mode";
    }
    return "unknown property value";
}

std::string nif_block_body(const std::string& source_bytes, const NifBlock& block) {
    if (block.byte_offset + block.byte_length > source_bytes.size()) return std::string();
    return source_bytes.substr(static_cast<size_t>(block.byte_offset),
                               static_cast<size_t>(block.byte_length));
}

bool read_nif_transform(const NifBlock& block, const std::string& body, NifTransform& out) {
    if (!transform_span_fits(block, body)) return false;
    float values[kTransformFloats];
    std::memcpy(values, body.data() + block.transform_offset, kTransformBytes);
    scatter(values, out);
    return true;
}

bool splice_nif_transform(const NifBlock& block, const NifTransform& transform,
                          std::string& body) {
    if (!transform_span_fits(block, body)) return false;
    float values[kTransformFloats];
    gather(transform, values);
    std::memcpy(body.data() + block.transform_offset, values, kTransformBytes);
    return true;
}

bool read_nif_material(const NifBlock& block, const std::string& body, NifMaterialState& out) {
    if (!material_span_fits(block, body)) return false;
    float values[kMaterialFloats];
    std::memcpy(values, body.data() + block.material_offset, kMaterialBytes);
    scatter(values, out);
    return true;
}

bool splice_nif_material(const NifBlock& block, const NifMaterialState& material,
                         std::string& body) {
    if (!material_span_fits(block, body)) return false;
    float values[kMaterialFloats];
    gather(material, values);
    std::memcpy(body.data() + block.material_offset, values, kMaterialBytes);
    return true;
}

bool read_nif_name(const NifHeader& header, const NifBlock& block, const std::string& body,
                   std::string& out) {
    if (header.version < kStringTableFrom) return read_counted_name(block, body, out);
    uint32_t index = 0;
    if (!read_name_index(block, body, index)) return false;
    if (index == kNoLink) {   // the sentinel for a block that carries no name
        out.clear();
        return true;
    }
    if (index >= header.strings.size()) return false;
    out = header.strings[index];
    return true;
}

bool splice_nif_name(const NifHeader& header, const NifBlock& block, const std::string& name,
                     std::string& body, std::vector<std::string>& added_strings) {
    uint32_t held = 0;
    if (!read_name_index(block, body, held)) return false;
    if (header.version >= kStringTableFrom) {
        const uint32_t index = index_for_name(header, name, added_strings);
        std::memcpy(body.data() + block.name_offset, &index, sizeof(index));
        return true;
    }
    const size_t text_at = static_cast<size_t>(block.name_offset) + sizeof(uint32_t);
    if (text_at + held > body.size()) return false;
    const uint32_t length = static_cast<uint32_t>(name.size());
    std::memcpy(body.data() + block.name_offset, &length, sizeof(length));
    body.replace(text_at, held, name);
    return true;
}

bool splice_nif_vertex_array(const NifBlock& block, NifVertexArray which,
                             const std::vector<float>& values, std::string& body,
                             std::string& refusal) {
    if (!block.vertex_spans) {
        refusal = "the block is not a geometry data block";
        return false;
    }
    const ArraySpan span = array_span(block, which);
    if (span.offset == 0) {
        refusal = std::string("the block carries no ") + array_name(which);
        return false;
    }
    if (span.format != kFloat32Format) {
        refusal = std::string("the ") + array_name(which) + " are packed narrower than a float";
        return false;
    }
    if (values.size() != span.held->size()) {
        refusal = std::string("the ") + array_name(which) + " are " +
                  std::to_string(values.size()) + " floats, the block holds " +
                  std::to_string(span.held->size());
        return false;
    }
    const size_t bytes = values.size() * sizeof(float);
    if (span.offset + bytes > body.size()) {
        refusal = std::string("the ") + array_name(which) + " run past the block";
        return false;
    }
    std::memcpy(body.data() + span.offset, values.data(), bytes);
    return true;
}

} // namespace KnC
