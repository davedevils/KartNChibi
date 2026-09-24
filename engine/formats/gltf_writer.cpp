#include "engine/formats/gltf_writer.h"

#include <charconv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace KnC {

namespace {

// Every accessor starts on a four byte boundary so validators stay quiet
constexpr uint64_t kAccessorAlignment = 4;

void pad_to_alignment(std::vector<uint8_t>& buffer) {
    while (buffer.size() % kAccessorAlignment != 0) buffer.push_back(0);
}

// Shortest text that reads back as the same float NaN and infinity are refused
bool append_float(std::string& out, float value) {
    if (!std::isfinite(value)) return false;
    char text[64];
    const std::to_chars_result written = std::to_chars(text, text + sizeof(text), value);
    if (written.ec != std::errc()) return false;
    out.append(text, static_cast<size_t>(written.ptr - text));
    return true;
}

void append_unsigned(std::string& out, uint64_t value) {
    char text[32];
    const std::to_chars_result written = std::to_chars(text, text + sizeof(text), value);
    out.append(text, static_cast<size_t>(written.ptr - text));
}

// Check byte run for well formed UTF 8 75 of 4048 client streams are invalid
bool is_utf8(const std::string& text) {
    for (size_t index = 0; index < text.size();) {
        const auto lead = static_cast<unsigned char>(text[index]);
        int trailing = 0;
        if (lead < 0x80) trailing = 0;
        else if ((lead & 0xE0) == 0xC0) trailing = 1;
        else if ((lead & 0xF0) == 0xE0) trailing = 2;
        else if ((lead & 0xF8) == 0xF0) trailing = 3;
        else return false;
        if (index + trailing >= text.size() && trailing != 0) return false;
        for (int step = 1; step <= trailing; ++step)
            if ((static_cast<unsigned char>(text[index + step]) & 0xC0) != 0x80) return false;
        index += static_cast<size_t>(trailing) + 1;
    }
    return true;
}

void append_hex_escape(std::string& out, unsigned char byte) {
    static const char kDigits[] = "0123456789abcdef";
    out.append("\\u00");
    out.push_back(kDigits[byte >> 4]);
    out.push_back(kDigits[byte & 0xF]);
}

// Name from NIF node reaches JSON six escapes applied control bytes dropped
void append_string(std::string& out, const std::string& text) {
    const bool decodable = is_utf8(text);
    out.push_back('"');
    for (const char letter : text) {
        switch (letter) {
            case '"':  out.append("\\\""); continue;
            case '\\': out.append("\\\\"); continue;
            case '\n': out.append("\\n");  continue;
            case '\r': out.append("\\r");  continue;
            case '\t': out.append("\\t");  continue;
            default: break;
        }
        const auto byte = static_cast<unsigned char>(letter);
        if (byte < 0x20) continue;
        if (byte < 0x80 || decodable) out.push_back(letter);
        else append_hex_escape(out, byte);
    }
    out.push_back('"');
}

// One JSON document under construction tracks comma state only
class JsonBuilder {
public:
    explicit JsonBuilder(std::string& out) : out_(out) {}

    void open_object() { punctuate(); out_.push_back('{'); fresh_ = true; }
    void open_array() { punctuate(); out_.push_back('['); fresh_ = true; }
    void close_object() { out_.push_back('}'); fresh_ = false; }
    void close_array() { out_.push_back(']'); fresh_ = false; }

    void key(const char* name) {
        punctuate();
        append_string(out_, name);
        out_.push_back(':');
        fresh_ = true;
    }

    void number(uint64_t value) { punctuate(); append_unsigned(out_, value); fresh_ = false; }
    void text(const std::string& value) { punctuate(); append_string(out_, value); fresh_ = false; }
    void boolean(bool value) { punctuate(); out_.append(value ? "true" : "false"); fresh_ = false; }

    bool real(float value) {
        punctuate();
        fresh_ = false;
        return append_float(out_, value);
    }

private:
    void punctuate() {
        if (!fresh_) out_.push_back(',');
        fresh_ = false;
    }

    std::string& out_;
    bool fresh_ = true;
};

const char* path_name(GltfPath path) {
    switch (path) {
        case GltfPath::Rotation: return "rotation";
        case GltfPath::Scale:    return "scale";
        case GltfPath::Translation: break;
    }
    return "translation";
}

const char* interpolation_name(GltfInterpolation interpolation) {
    return interpolation == GltfInterpolation::Step ? "STEP" : "LINEAR";
}

const char* alpha_mode_name(GltfAlphaMode mode) {
    switch (mode) {
        case GltfAlphaMode::Mask:  return "MASK";
        case GltfAlphaMode::Blend: return "BLEND";
        case GltfAlphaMode::Opaque: break;
    }
    return "OPAQUE";
}

bool uses_unlit(const GltfDocument& document) {
    for (const GltfMaterial& material : document.materials)
        if (material.unlit) return true;
    return false;
}

// Accessor bounds in the component type they were measured in
bool write_bounds(JsonBuilder& json, const GltfAccessor& accessor, const double* values) {
    json.open_array();
    for (uint32_t index = 0; index < gltf_element_width(accessor.element); ++index) {
        if (accessor.component != GltfComponent::Float) {
            json.number(static_cast<uint64_t>(values[index]));
            continue;
        }
        if (!json.real(static_cast<float>(values[index]))) return false;
    }
    json.close_array();
    return true;
}

bool write_accessors(JsonBuilder& json, const GltfDocument& document) {
    if (document.accessors.empty()) return true;
    json.key("accessors");
    json.open_array();
    for (uint32_t index = 0; index < document.accessors.size(); ++index) {
        const GltfAccessor& accessor = document.accessors[index];
        json.open_object();
        json.key("bufferView"); json.number(index);
        json.key("componentType"); json.number(static_cast<uint32_t>(accessor.component));
        json.key("count"); json.number(accessor.count);
        json.key("type"); json.text(gltf_element_name(accessor.element));
        json.key("min");
        if (!write_bounds(json, accessor, accessor.minimum)) return false;
        json.key("max");
        if (!write_bounds(json, accessor, accessor.maximum)) return false;
        json.close_object();
    }
    json.close_array();
    return true;
}

void write_buffer_views(JsonBuilder& json, const GltfDocument& document) {
    if (document.accessors.empty()) return;
    json.key("bufferViews");
    json.open_array();
    for (const GltfAccessor& accessor : document.accessors) {
        json.open_object();
        json.key("buffer"); json.number(0);
        json.key("byteOffset"); json.number(accessor.byte_offset);
        json.key("byteLength"); json.number(accessor.byte_length);
        if (accessor.target != GltfTarget::None) {
            json.key("target");
            json.number(static_cast<uint32_t>(accessor.target));
        }
        json.close_object();
    }
    json.close_array();
}

void write_indices(JsonBuilder& json, const char* name, const std::vector<uint32_t>& values) {
    json.key(name);
    json.open_array();
    for (const uint32_t value : values) json.number(value);
    json.close_array();
}

bool write_vector(JsonBuilder& json, const char* name, const float* values, uint32_t count) {
    json.key(name);
    json.open_array();
    for (uint32_t index = 0; index < count; ++index)
        if (!json.real(values[index])) return false;
    json.close_array();
    return true;
}

void write_attribute(JsonBuilder& json, const char* name, int accessor) {
    if (accessor < 0) return;
    json.key(name);
    json.number(static_cast<uint32_t>(accessor));
}

void write_primitive(JsonBuilder& json, const GltfPrimitive& primitive) {
    json.open_object();
    json.key("attributes");
    json.open_object();
    write_attribute(json, "POSITION", primitive.position);
    write_attribute(json, "NORMAL", primitive.normal);
    write_attribute(json, "TEXCOORD_0", primitive.texcoord);
    write_attribute(json, "COLOR_0", primitive.colour);
    write_attribute(json, "JOINTS_0", primitive.joints);
    write_attribute(json, "WEIGHTS_0", primitive.weights);
    json.close_object();
    write_attribute(json, "indices", primitive.indices);
    write_attribute(json, "material", primitive.material);
    json.close_object();
}

void write_meshes(JsonBuilder& json, const GltfDocument& document) {
    if (document.meshes.empty()) return;
    json.key("meshes");
    json.open_array();
    for (const GltfMesh& mesh : document.meshes) {
        json.open_object();
        if (!mesh.name.empty()) { json.key("name"); json.text(mesh.name); }
        json.key("primitives");
        json.open_array();
        for (const GltfPrimitive& primitive : mesh.primitives) write_primitive(json, primitive);
        json.close_array();
        json.close_object();
    }
    json.close_array();
}

bool write_nodes(JsonBuilder& json, const GltfDocument& document) {
    json.key("nodes");
    json.open_array();
    for (const GltfNode& node : document.nodes) {
        json.open_object();
        if (!node.name.empty()) { json.key("name"); json.text(node.name); }
        if (!write_vector(json, "translation", node.translation, 3)) return false;
        if (!write_vector(json, "rotation", node.rotation, 4)) return false;
        if (!write_vector(json, "scale", node.scale, 3)) return false;
        write_attribute(json, "mesh", node.mesh);
        write_attribute(json, "skin", node.skin);
        if (!node.children.empty()) write_indices(json, "children", node.children);
        json.close_object();
    }
    json.close_array();
    return true;
}

bool write_pbr(JsonBuilder& json, const GltfMaterial& material) {
    json.key("pbrMetallicRoughness");
    json.open_object();
    if (!write_vector(json, "baseColorFactor", material.base_colour, 4)) return false;
    if (material.base_texture >= 0) {
        json.key("baseColorTexture");
        json.open_object();
        json.key("index"); json.number(static_cast<uint32_t>(material.base_texture));
        json.close_object();
    }
    json.key("metallicFactor");
    if (!json.real(material.metallic)) return false;
    json.key("roughnessFactor");
    if (!json.real(material.roughness)) return false;
    json.close_object();
    return true;
}

// The extras object spec puts no schema on every entry is an array
bool write_extras(JsonBuilder& json, const std::vector<GltfExtra>& extras) {
    if (extras.empty()) return true;
    json.key("extras");
    json.open_object();
    for (const GltfExtra& extra : extras) {
        json.key(extra.name.c_str());
        json.open_array();
        for (const float value : extra.values)
            if (!json.real(value)) return false;
        json.close_array();
    }
    json.close_object();
    return true;
}

bool write_material(JsonBuilder& json, const GltfMaterial& material) {
    json.open_object();
    if (!material.name.empty()) { json.key("name"); json.text(material.name); }
    if (!write_pbr(json, material)) return false;
    if (!write_vector(json, "emissiveFactor", material.emissive, 3)) return false;
    json.key("alphaMode"); json.text(alpha_mode_name(material.alpha_mode));
    if (material.alpha_mode == GltfAlphaMode::Mask) {
        json.key("alphaCutoff");
        if (!json.real(material.alpha_cutoff)) return false;
    }
    if (material.double_sided) { json.key("doubleSided"); json.boolean(true); }
    if (material.unlit) {
        json.key("extensions");
        json.open_object();
        json.key("KHR_materials_unlit");
        json.open_object();
        json.close_object();
        json.close_object();
    }
    if (!write_extras(json, material.extras)) return false;
    json.close_object();
    return true;
}

bool write_materials(JsonBuilder& json, const GltfDocument& document) {
    if (document.materials.empty()) return true;
    json.key("materials");
    json.open_array();
    for (const GltfMaterial& material : document.materials)
        if (!write_material(json, material)) return false;
    json.close_array();
    return true;
}

void write_textures(JsonBuilder& json, const GltfDocument& document) {
    if (!document.images.empty()) {
        json.key("images");
        json.open_array();
        for (const GltfImage& image : document.images) {
            json.open_object();
            json.key("uri"); json.text(image.uri);
            json.close_object();
        }
        json.close_array();
    }
    if (!document.samplers.empty()) {
        json.key("samplers");
        json.open_array();
        for (const GltfSampler& sampler : document.samplers) {
            json.open_object();
            json.key("wrapS"); json.number(static_cast<uint32_t>(sampler.wrap_u));
            json.key("wrapT"); json.number(static_cast<uint32_t>(sampler.wrap_v));
            json.close_object();
        }
        json.close_array();
    }
    if (document.textures.empty()) return;
    json.key("textures");
    json.open_array();
    for (const GltfTexture& texture : document.textures) {
        json.open_object();
        write_attribute(json, "source", texture.image);
        write_attribute(json, "sampler", texture.sampler);
        json.close_object();
    }
    json.close_array();
}

void write_skins(JsonBuilder& json, const GltfDocument& document) {
    if (document.skins.empty()) return;
    json.key("skins");
    json.open_array();
    for (const GltfSkin& skin : document.skins) {
        json.open_object();
        if (!skin.name.empty()) { json.key("name"); json.text(skin.name); }
        write_attribute(json, "inverseBindMatrices", skin.inverse_bind_matrices);
        write_attribute(json, "skeleton", skin.skeleton);
        write_indices(json, "joints", skin.joints);
        json.close_object();
    }
    json.close_array();
}

void write_animations(JsonBuilder& json, const GltfDocument& document) {
    if (document.animations.empty()) return;
    json.key("animations");
    json.open_array();
    for (const GltfAnimation& animation : document.animations) {
        json.open_object();
        if (!animation.name.empty()) { json.key("name"); json.text(animation.name); }
        json.key("samplers");
        json.open_array();
        for (const GltfAnimationSampler& sampler : animation.samplers) {
            json.open_object();
            json.key("input"); json.number(sampler.input);
            json.key("output"); json.number(sampler.output);
            json.key("interpolation"); json.text(interpolation_name(sampler.interpolation));
            json.close_object();
        }
        json.close_array();
        json.key("channels");
        json.open_array();
        for (const GltfAnimationChannel& channel : animation.channels) {
            json.open_object();
            json.key("sampler"); json.number(channel.sampler);
            json.key("target");
            json.open_object();
            json.key("node"); json.number(channel.node);
            json.key("path"); json.text(path_name(channel.path));
            json.close_object();
            json.close_object();
        }
        json.close_array();
        json.close_object();
    }
    json.close_array();
}

// Bounds over one interleaved array every accessor carries them
void measure_bounds(const std::vector<float>& values, uint32_t width, GltfAccessor& out) {
    for (uint32_t component = 0; component < width; ++component) {
        out.minimum[component] = std::numeric_limits<double>::max();
        out.maximum[component] = -std::numeric_limits<double>::max();
    }
    for (size_t index = 0; index < values.size(); ++index) {
        const uint32_t component = static_cast<uint32_t>(index % width);
        const double value = values[index];
        if (value < out.minimum[component]) out.minimum[component] = value;
        if (value > out.maximum[component]) out.maximum[component] = value;
    }
    if (!values.empty()) return;
    for (uint32_t component = 0; component < width; ++component) {
        out.minimum[component] = 0.0;
        out.maximum[component] = 0.0;
    }
}

template <typename Element>
void append_raw(std::vector<uint8_t>& buffer, const std::vector<Element>& values) {
    const size_t bytes = values.size() * sizeof(Element);
    const size_t start = buffer.size();
    buffer.resize(start + bytes);
    if (bytes != 0) std::memcpy(buffer.data() + start, values.data(), bytes);
}

} // namespace

uint32_t gltf_element_width(GltfElement element) {
    switch (element) {
        case GltfElement::Vec2: return 2;
        case GltfElement::Vec3: return 3;
        case GltfElement::Vec4: return 4;
        case GltfElement::Mat4: return 16;
        case GltfElement::Scalar: break;
    }
    return 1;
}

const char* gltf_element_name(GltfElement element) {
    switch (element) {
        case GltfElement::Vec2: return "VEC2";
        case GltfElement::Vec3: return "VEC3";
        case GltfElement::Vec4: return "VEC4";
        case GltfElement::Mat4: return "MAT4";
        case GltfElement::Scalar: break;
    }
    return "SCALAR";
}

uint32_t gltf_add_floats(GltfDocument& document, const std::vector<float>& values,
                         GltfElement element, GltfTarget target) {
    const uint32_t width = gltf_element_width(element);
    pad_to_alignment(document.buffer);
    GltfAccessor accessor;
    accessor.component = GltfComponent::Float;
    accessor.element = element;
    accessor.target = target;
    accessor.count = static_cast<uint32_t>(values.size() / width);
    accessor.byte_offset = document.buffer.size();
    accessor.byte_length = values.size() * sizeof(float);
    measure_bounds(values, width, accessor);
    append_raw(document.buffer, values);
    document.accessors.push_back(accessor);
    return static_cast<uint32_t>(document.accessors.size() - 1);
}

uint32_t gltf_add_joints(GltfDocument& document, const std::vector<uint16_t>& slots) {
    pad_to_alignment(document.buffer);
    GltfAccessor accessor;
    accessor.component = GltfComponent::UnsignedShort;
    accessor.element = GltfElement::Vec4;
    accessor.target = GltfTarget::ArrayBuffer;
    accessor.count = static_cast<uint32_t>(slots.size() / 4);
    accessor.byte_offset = document.buffer.size();
    accessor.byte_length = slots.size() * sizeof(uint16_t);
    for (uint32_t component = 0; component < 4; ++component) {
        accessor.minimum[component] = slots.empty() ? 0.0 : 65535.0;
        accessor.maximum[component] = 0.0;
    }
    for (size_t index = 0; index < slots.size(); ++index) {
        const uint32_t component = static_cast<uint32_t>(index % 4);
        const double value = slots[index];
        if (value < accessor.minimum[component]) accessor.minimum[component] = value;
        if (value > accessor.maximum[component]) accessor.maximum[component] = value;
    }
    append_raw(document.buffer, slots);
    document.accessors.push_back(accessor);
    return static_cast<uint32_t>(document.accessors.size() - 1);
}

uint32_t gltf_add_indices(GltfDocument& document, const std::vector<uint32_t>& indices) {
    pad_to_alignment(document.buffer);
    GltfAccessor accessor;
    accessor.component = GltfComponent::UnsignedInt;
    accessor.element = GltfElement::Scalar;
    accessor.target = GltfTarget::ElementArrayBuffer;
    accessor.count = static_cast<uint32_t>(indices.size());
    accessor.byte_offset = document.buffer.size();
    accessor.byte_length = indices.size() * sizeof(uint32_t);
    accessor.minimum[0] = indices.empty() ? 0.0 : static_cast<double>(indices[0]);
    accessor.maximum[0] = accessor.minimum[0];
    for (const uint32_t index : indices) {
        if (index < accessor.minimum[0]) accessor.minimum[0] = index;
        if (index > accessor.maximum[0]) accessor.maximum[0] = index;
    }
    append_raw(document.buffer, indices);
    document.accessors.push_back(accessor);
    return static_cast<uint32_t>(document.accessors.size() - 1);
}

namespace {

void write_preamble(JsonBuilder& json, const GltfDocument& document) {
    json.key("asset");
    json.open_object();
    json.key("version"); json.text("2.0");
    json.key("generator"); json.text(document.generator);
    json.close_object();
    if (uses_unlit(document)) {
        json.key("extensionsUsed");
        json.open_array();
        json.text("KHR_materials_unlit");
        json.close_array();
    }
    json.key("scene"); json.number(0);
    json.key("scenes");
    json.open_array();
    json.open_object();
    write_indices(json, "nodes", document.scene_roots);
    json.close_object();
    json.close_array();
}

void write_buffer(JsonBuilder& json, const GltfDocument& document) {
    // A buffer of no bytes is not allowed glTF arrays that name it are gone
    if (document.buffer.empty()) return;
    json.key("buffers");
    json.open_array();
    json.open_object();
    json.key("uri"); json.text(document.buffer_uri);
    json.key("byteLength"); json.number(document.buffer.size());
    json.close_object();
    json.close_array();
}

bool build_gltf_body(JsonBuilder& json, const GltfDocument& document, std::string& error) {
    if (!write_nodes(json, document)) { error = "a node transform is not finite"; return false; }
    write_meshes(json, document);
    if (!write_materials(json, document)) { error = "a material value is not finite"; return false; }
    write_textures(json, document);
    write_skins(json, document);
    write_animations(json, document);
    if (!write_accessors(json, document)) { error = "an accessor bound is not finite"; return false; }
    write_buffer_views(json, document);
    return true;
}

} // namespace

bool build_gltf_json(const GltfDocument& document, std::string& out, std::string& error) {
    out.clear();
    JsonBuilder json(out);
    json.open_object();
    write_preamble(json, document);
    if (!build_gltf_body(json, document, error)) return false;
    write_buffer(json, document);
    json.close_object();
    return true;
}

bool write_gltf(const GltfDocument& document, const std::string& gltf_path,
                const std::string& bin_path, std::string& error) {
    std::string json;
    if (!build_gltf_json(document, json, error)) return false;
    std::ofstream binary(bin_path, std::ios::binary | std::ios::trunc);
    if (!binary.is_open()) {
        error = "cannot open " + bin_path + " for writing";
        return false;
    }
    binary.write(reinterpret_cast<const char*>(document.buffer.data()),
                 static_cast<std::streamsize>(document.buffer.size()));
    if (!binary) {
        error = "cannot write " + bin_path;
        return false;
    }
    binary.close();
    std::ofstream scene(gltf_path, std::ios::binary | std::ios::trunc);
    if (!scene.is_open()) {
        error = "cannot open " + gltf_path + " for writing";
        return false;
    }
    scene.write(json.data(), static_cast<std::streamsize>(json.size()));
    if (!scene) {
        error = "cannot write " + gltf_path;
        return false;
    }
    return true;
}

} // namespace KnC
