#include "engine/formats/nif_gltf_internal.h"

#include "engine/formats/nif_effect.h"
#include "engine/formats/nif_scene_graph.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace KnC {

namespace {

// glTF is Y up NIF is Z up root node quarter turn about X sin and cos of -45 degrees
constexpr float kQuarterTurn = 0.70710678f;

// Global ambient lit surface constant colour folded into baseColorFactor
constexpr float kReferenceAmbient = 1.f;

// Vertex colour set which surface shading reads alpha separate from colour
enum class ColourUse { None, Rgb, Rgba };

bool carries_vertex_colours(const NifBlock& mesh) {
    return !mesh.vertex_colours.empty() &&
           mesh.vertex_colours.size() * 3 == mesh.vertices.size() * 4;
}

float saturate(float value) { return std::clamp(value, 0.f, 1.f); }

// Texture stem NIF names with glTF extension exporter writes png from dds
std::string png_uri_of(const std::string& texture_name) {
    size_t start = texture_name.find_last_of("/\\");
    start = start == std::string::npos ? 0 : start + 1;
    const size_t dot = texture_name.find_last_of('.');
    const size_t end = dot == std::string::npos || dot < start ? texture_name.size() : dot;
    return texture_name.substr(start, end - start) + ".png";
}

GltfAlphaMode alpha_mode_of(const NifAlphaState& alpha) {
    if (alpha.blend_enabled()) return GltfAlphaMode::Blend;
    if (alpha.test_enabled()) return GltfAlphaMode::Mask;
    return GltfAlphaMode::Opaque;
}

ColourUse colour_use_of(NifShadedBy shading, bool coloured) {
    if (!coloured) return ColourUse::None;
    switch (shading) {
        case NifShadedBy::Unlit:                return ColourUse::Rgba;
        case NifShadedBy::VertexAmbientDiffuse: return ColourUse::Rgba;
        case NifShadedBy::VertexEmissive:       return ColourUse::Rgb;
        case NifShadedBy::Material:             break;
    }
    return ColourUse::None;
}

// Constant client lighting before texture term adds not multiply COLOR 0
struct SurfaceColour {
    float base[4]     = {1.f, 1.f, 1.f, 1.f};
    float dropped     = 0.f;   // the largest channel of the term glTF cannot add
};

SurfaceColour surface_colour_of(const NifSurfaceState& surface, NifShadedBy shading,
                                bool coloured) {
    SurfaceColour colour;
    const NifMaterialState& material = surface.material;
    if (shading == NifShadedBy::Unlit) return colour;
    if (colour_use_of(shading, coloured) == ColourUse::None) {
        for (int channel = 0; channel < 3; ++channel)
            colour.base[channel] =
                saturate(material.emissive[channel] + material.ambient[channel] * kReferenceAmbient);
        colour.base[3] = material.alpha;
        return colour;
    }
    // Vertex feeds one term other dropped and counted
    const float* folded = shading == NifShadedBy::VertexEmissive ? material.ambient
                                                                 : material.emissive;
    for (int channel = 0; channel < 3; ++channel)
        colour.dropped = std::max(colour.dropped, folded[channel]);
    colour.base[3] = shading == NifShadedBy::VertexEmissive ? material.alpha : 1.f;
    return colour;
}

// Five property blocks surface chain resolves to as glTF extras fold
struct SurfaceExtraField {
    const char* name;
    int         width;
};

constexpr SurfaceExtraField kSurfaceExtraFields[] = {
    {"nifAmbient", 3},          {"nifDiffuse", 3},
    {"nifSpecular", 3},         {"nifEmissive", 3},
    {"nifGlossiness", 1},       {"nifMaterialAlpha", 1},
    {"nifAlphaFlags", 1},       {"nifAlphaThreshold", 1},
    {"nifDepthFlags", 1},       {"nifLightingMode", 1},
    {"nifVertexMode", 1},       {"nifTextureApplyMode", 1},
    {"nifTextureClampMode", 1}, {"nifHasMaterialProperty", 1},
    {"nifHasAlphaProperty", 1}, {"nifHasZBufferProperty", 1},
    {"nifHasVertexColourProperty", 1},
};

constexpr int surface_state_width() {
    int total = 0;
    for (const SurfaceExtraField& field : kSurfaceExtraFields) total += field.width;
    return total;
}

constexpr int kSurfaceStateWidth = surface_state_width();

std::vector<float> surface_state_numbers(const NifSurfaceState& surface) {
    const NifMaterialState& material = surface.material;
    std::vector<float> numbers;
    numbers.reserve(kSurfaceStateWidth);
    for (int channel = 0; channel < 3; ++channel) numbers.push_back(material.ambient[channel]);
    for (int channel = 0; channel < 3; ++channel) numbers.push_back(material.diffuse[channel]);
    for (int channel = 0; channel < 3; ++channel) numbers.push_back(material.specular[channel]);
    for (int channel = 0; channel < 3; ++channel) numbers.push_back(material.emissive[channel]);
    numbers.push_back(material.glossiness);
    numbers.push_back(material.alpha);
    numbers.push_back(static_cast<float>(surface.alpha.flags));
    numbers.push_back(static_cast<float>(surface.alpha.test_threshold));
    numbers.push_back(static_cast<float>(surface.depth.flags));
    numbers.push_back(static_cast<float>(surface.vertex_colour.lighting));
    numbers.push_back(static_cast<float>(surface.vertex_colour.vertex));
    numbers.push_back(static_cast<float>(surface.texture_apply));
    numbers.push_back(static_cast<float>(surface.base_texture_clamp));
    numbers.push_back(surface.has_material ? 1.f : 0.f);
    numbers.push_back(surface.has_alpha ? 1.f : 0.f);
    numbers.push_back(surface.has_depth ? 1.f : 0.f);
    numbers.push_back(surface.has_vertex_colour ? 1.f : 0.f);
    return numbers;
}

std::vector<GltfExtra> surface_extras_of(const std::vector<float>& numbers) {
    std::vector<GltfExtra> extras;
    int at = 0;
    for (const SurfaceExtraField& field : kSurfaceExtraFields) {
        GltfExtra extra;
        extra.name = field.name;
        extra.values.assign(numbers.begin() + at, numbers.begin() + at + field.width);
        extras.push_back(std::move(extra));
        at += field.width;
    }
    return extras;
}

// Surface device state key two surfaces share glTF material all values
struct MaterialKey {
    std::string        texture;
    uint8_t            shading    = 0;
    uint8_t            colour_use = 0;
    std::vector<float> state;

    bool operator<(const MaterialKey& other) const {
        if (texture != other.texture) return texture < other.texture;
        if (shading != other.shading) return shading < other.shading;
        if (colour_use != other.colour_use) return colour_use < other.colour_use;
        return state < other.state;
    }
};

MaterialKey material_key_of(const NifSurfaceState& surface, const std::string& texture,
                            NifShadedBy shading, ColourUse use) {
    MaterialKey key;
    key.texture = texture;
    key.shading = static_cast<uint8_t>(shading);
    key.colour_use = static_cast<uint8_t>(use);
    key.state = surface_state_numbers(surface);
    return key;
}

// Vertex influences widest weight first JOINTS 0 holds four decide kept
struct Influence {
    uint16_t joint  = 0;
    float    weight = 0.f;
};

// NiSkinData bone vertex list scattered into per vertex lists
std::vector<std::vector<Influence>> influences_of(const NifSkin& data, size_t vertex_count) {
    std::vector<std::vector<Influence>> influences(vertex_count);
    for (size_t bone = 0; bone < data.bone_binds.size(); ++bone) {
        const NifSkinBone& bind = data.bone_binds[bone];
        for (size_t entry = 0; entry < bind.vertex_indices.size(); ++entry) {
            const size_t vertex = bind.vertex_indices[entry];
            if (vertex >= vertex_count || entry >= bind.weights.size()) continue;
            influences[vertex].push_back({static_cast<uint16_t>(bone), bind.weights[entry]});
        }
    }
    for (std::vector<Influence>& list : influences)
        std::sort(list.begin(), list.end(),
                  [](const Influence& left, const Influence& right) {
                      return left.weight > right.weight;
                  });
    return influences;
}

// NIF transform as column major 4x4 glTF inverse bind matrix
void gltf_matrix_of(const NifTransform& transform, float out[16]) {
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            out[column * 4 + row] = transform.scale * transform.rotation[row * 3 + column];
    for (int row = 0; row < 3; ++row) {
        out[12 + row] = transform.translation[row];
        out[row * 4 + 3] = 0.f;
    }
    out[15] = 1.f;
}

// Rotation orthonormality error largest entry of R R minus I
float gltf_orthonormality_error(const float rotation[9]) {
    float worst = 0.f;
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column) {
            float product = 0.f;
            for (int step = 0; step < 3; ++step)
                product += rotation[step * 3 + row] * rotation[step * 3 + column];
            worst = std::max(worst, std::fabs(product - (row == column ? 1.f : 0.f)));
        }
    return worst;
}

// Quaternion error largest entry difference between two 3x3 matrices
float gltf_quaternion_error(const float rotation[9], const float quaternion[4]) {
    float rebuilt[9];
    gltf_rotation_of_quaternion(quaternion, rebuilt);
    float worst = 0.f;
    for (int cell = 0; cell < 9; ++cell)
        worst = std::max(worst, std::fabs(rebuilt[cell] - rotation[cell]));
    return worst;
}

// Export in progress holds reader resolved state skeleton property chains
class NifGltfBuilder {
public:
    NifGltfBuilder(const NifGltfRequest& request, NifGltfResult& out)
        : model_(*request.model), options_(request.options), request_(request), out_(out) {}

    bool build();

private:
    void build_nodes();
    void attach_conversion_root();
    void build_geometry();
    void emit_geometry(uint32_t block, int node);
    bool is_drawable(const NifBlock& mesh, const std::string& texture);
    void append_attributes(const NifBlock& mesh, ColourUse use, GltfPrimitive& primitive);
    int  material_for(uint32_t block, const std::string& texture, ColourUse use);
    int  texture_for(const std::string& texture, NifTextureClamp clamp);
    int  skin_for(uint32_t block, GltfPrimitive& primitive);
    void append_skin_attributes(const NifSkin& data, const NifBlock& mesh,
                                GltfPrimitive& primitive);
    void measure_bind_pose(uint32_t block, const NifSkin& instance, const NifSkin& data);

    const NifScene&       model_;
    const NifGltfOptions& options_;
    const NifGltfRequest& request_;
    NifGltfResult&        out_;
    NifSkeleton           skeleton_;
    std::vector<NifSurfaceState> surfaces_;
    std::vector<NifPlacement>    rest_;
    std::map<MaterialKey, int>   material_of_key_;
    std::map<std::string, int>   texture_of_key_;
    std::map<uint32_t, int>      skin_of_block_;
    // Parallel to geometry vertices bind pose checks weights written not
    std::vector<uint16_t> emitted_joints_;
    std::vector<float>    emitted_weights_;
};

void NifGltfBuilder::build_nodes() {
    out_.document.nodes.resize(skeleton_.nodes.size());
    for (size_t index = 0; index < skeleton_.nodes.size(); ++index) {
        const NifSkeletonNode& source = skeleton_.nodes[index];
        GltfNode& node = out_.document.nodes[index];
        node.name = source.name;
        for (int axis = 0; axis < 3; ++axis) {
            node.translation[axis] = source.rest.translation[axis];
            node.scale[axis] = source.rest.scale;
        }
        gltf_quaternion_of(source.rest.rotation, node.rotation);
        out_.report.worst_rotation_orthonormality =
            std::max(out_.report.worst_rotation_orthonormality,
                     gltf_orthonormality_error(source.rest.rotation));
        out_.report.worst_quaternion_error =
            std::max(out_.report.worst_quaternion_error,
                     gltf_quaternion_error(source.rest.rotation, node.rotation));
        if (source.parent < 0)
            out_.document.scene_roots.push_back(static_cast<uint32_t>(index));
        else
            out_.document.nodes[static_cast<size_t>(source.parent)].children.push_back(
                static_cast<uint32_t>(index));
    }
    out_.report.nodes = out_.document.nodes.size();
}

void NifGltfBuilder::attach_conversion_root() {
    if (!options_.convert_to_y_up) return;
    GltfNode converted;
    converted.name = "gltf_y_up";
    converted.rotation[0] = -kQuarterTurn;   // Minus 90 degrees about X so NIF Z up becomes glTF Y up
    converted.rotation[3] = kQuarterTurn;
    converted.children = out_.document.scene_roots;
    out_.document.scene_roots.assign(1, static_cast<uint32_t>(out_.document.nodes.size()));
    out_.document.nodes.push_back(std::move(converted));
}

int NifGltfBuilder::texture_for(const std::string& texture, NifTextureClamp clamp) {
    if (texture.empty()) return -1;
    const std::string key = texture + "|" + std::to_string(static_cast<uint32_t>(clamp));
    const auto cached = texture_of_key_.find(key);
    if (cached != texture_of_key_.end()) return cached->second;

    const std::string uri = png_uri_of(texture);
    int image = -1;
    for (size_t index = 0; index < out_.document.images.size(); ++index)
        if (out_.document.images[index].uri == uri) image = static_cast<int>(index);
    if (image < 0) {
        image = static_cast<int>(out_.document.images.size());
        out_.document.images.push_back({uri});
        out_.report.textures.push_back({texture, uri});
    }
    GltfSampler wanted;
    wanted.wrap_u = wraps_u(clamp) ? GltfWrap::Repeat : GltfWrap::ClampToEdge;
    wanted.wrap_v = wraps_v(clamp) ? GltfWrap::Repeat : GltfWrap::ClampToEdge;
    int sampler = -1;
    for (size_t index = 0; index < out_.document.samplers.size(); ++index)
        if (out_.document.samplers[index].wrap_u == wanted.wrap_u &&
            out_.document.samplers[index].wrap_v == wanted.wrap_v)
            sampler = static_cast<int>(index);
    if (sampler < 0) {
        sampler = static_cast<int>(out_.document.samplers.size());
        out_.document.samplers.push_back(wanted);
    }
    const int index = static_cast<int>(out_.document.textures.size());
    out_.document.textures.push_back({image, sampler});
    texture_of_key_.emplace(key, index);
    return index;
}

int NifGltfBuilder::material_for(uint32_t block, const std::string& texture, ColourUse use) {
    const NifSurfaceState& surface = surfaces_[block];
    const NifShadedBy shading = surface_shading(surface, !texture.empty());
    const MaterialKey key = material_key_of(surface, texture, shading, use);
    const auto cached = material_of_key_.find(key);
    if (cached != material_of_key_.end()) return cached->second;

    const SurfaceColour colour = surface_colour_of(surface, shading, use != ColourUse::None);
    GltfMaterial material;
    material.name = texture.empty() ? "untextured" : png_uri_of(texture);
    material.base_texture = texture_for(texture, surface.base_texture_clamp);
    // NiMaterialProperty alpha outside 0 1 glTF baseColorFactor component not
    for (int channel = 0; channel < 4; ++channel)
        material.base_colour[channel] = saturate(colour.base[channel]);
    material.alpha_mode = alpha_mode_of(surface.alpha);
    material.alpha_cutoff = static_cast<float>(surface.alpha.test_threshold) / 255.f;
    // Client nothing shaded by surface normal half D3DRS LIGHTING off unlit
    material.unlit = true;
    // Folded factor for outside viewer channels folded trip back unfold
    material.extras = surface_extras_of(key.state);
    if (colour.dropped > 0.f) {
        ++out_.report.materials_with_dropped_term;
        out_.report.worst_dropped_term = std::max(out_.report.worst_dropped_term, colour.dropped);
        if (!surface.has_material) ++out_.report.materials_without_a_material_property;
    }
    const int index = static_cast<int>(out_.document.materials.size());
    out_.document.materials.push_back(std::move(material));
    material_of_key_.emplace(key, index);
    ++out_.report.materials;
    ++out_.report.unlit_materials;
    return index;
}

void NifGltfBuilder::append_skin_attributes(const NifSkin& data, const NifBlock& mesh,
                                            GltfPrimitive& primitive) {
    const size_t count = mesh.vertices.size() / 3;
    const std::vector<std::vector<Influence>> influences = influences_of(data, count);
    emitted_joints_.assign(count * 4, 0);
    emitted_weights_.assign(count * 4, 0.f);
    for (size_t vertex = 0; vertex < count; ++vertex) {
        const std::vector<Influence>& list = influences[vertex];
        float authored = 0.f;
        for (const Influence& influence : list) authored += influence.weight;
        if (list.size() > 4) {
            ++out_.report.vertices_over_four_influences;
            for (size_t extra = 4; extra < list.size(); ++extra)
                out_.report.worst_dropped_weight =
                    std::max(out_.report.worst_dropped_weight, list[extra].weight);
        }
        if (!list.empty())
            out_.report.worst_weight_sum_error =
                std::max(out_.report.worst_weight_sum_error, std::fabs(authored - 1.f));
        float kept = 0.f;
        for (size_t slot = 0; slot < 4 && slot < list.size(); ++slot) kept += list[slot].weight;
        // Vertex zero weight rides first joint whole renderer partition fallback
        if (kept <= 0.f) {
            emitted_weights_[vertex * 4] = 1.f;
            continue;
        }
        // Slot glTF no weight must name joint 0 validator reads used
        for (size_t slot = 0; slot < 4 && slot < list.size(); ++slot) {
            if (list[slot].weight <= 0.f) continue;
            emitted_joints_[vertex * 4 + slot] = list[slot].joint;
            emitted_weights_[vertex * 4 + slot] = list[slot].weight / kept;
        }
    }
    primitive.joints = static_cast<int>(gltf_add_joints(out_.document, emitted_joints_));
    primitive.weights = static_cast<int>(
        gltf_add_floats(out_.document, emitted_weights_, GltfElement::Vec4, GltfTarget::ArrayBuffer));
}

// Sum vertex influences weight bone world bind measure bind pose
void NifGltfBuilder::measure_bind_pose(uint32_t block, const NifSkin& instance,
                                       const NifSkin& data) {
    const NifBlock& mesh = model_.blocks[model_.blocks[block].data_link];
    const size_t count = mesh.vertices.size() / 3;
    // Every bone known reach node skin for refused skin outright
    std::vector<NifPlacement> bind(instance.bones.size());
    for (size_t bone = 0; bone < instance.bones.size(); ++bone) {
        const size_t node = static_cast<size_t>(skeleton_.node_of(instance.bones[bone]));
        bind[bone] = compose_placement(rest_[node], data.bone_binds[bone].bind);
    }
    const NifPlacement& shape = rest_[static_cast<size_t>(skeleton_.node_of(block))];
    for (size_t vertex = 0; vertex < count; ++vertex) {
        float posed[3] = {0.f, 0.f, 0.f};
        for (int slot = 0; slot < 4; ++slot) {
            const float weight = emitted_weights_[vertex * 4 + slot];
            if (weight == 0.f) continue;
            const uint16_t joint = emitted_joints_[vertex * 4 + slot];
            if (joint >= bind.size()) continue;
            float moved[3];
            place_point(bind[joint], &mesh.vertices[vertex * 3], moved);
            for (int axis = 0; axis < 3; ++axis) posed[axis] += weight * moved[axis];
        }
        float expected[3];
        place_point(shape, &mesh.vertices[vertex * 3], expected);
        float distance = 0.f;
        for (int axis = 0; axis < 3; ++axis) {
            const float offset = posed[axis] - expected[axis];
            distance += offset * offset;
        }
        out_.report.worst_bind_pose_error =
            std::max(out_.report.worst_bind_pose_error, std::sqrt(distance));
    }
}

int NifGltfBuilder::skin_for(uint32_t block, GltfPrimitive& primitive) {
    const NifBlock& geometry = model_.blocks[block];
    if (geometry.skin_instance_link >= model_.blocks.size()) return -1;
    const NifSkin* instance = model_.blocks[geometry.skin_instance_link].skin.get();
    if (instance == nullptr || instance->data_link >= model_.blocks.size()) return -1;
    const NifSkin* data = model_.blocks[instance->data_link].skin.get();
    if (data == nullptr || data->bone_binds.size() != instance->bones.size()) return -1;

    GltfSkin skin;
    skin.name = geometry.name;
    std::vector<float> inverse_binds;
    inverse_binds.reserve(instance->bones.size() * 16);
    for (size_t bone = 0; bone < instance->bones.size(); ++bone) {
        const int node = skeleton_.node_of(instance->bones[bone]);
        if (node < 0) {
            ++out_.report.skins_with_unreachable_bones;
            return -1;
        }
        skin.joints.push_back(static_cast<uint32_t>(node));
        float matrix[16];
        gltf_matrix_of(data->bone_binds[bone].bind, matrix);
        inverse_binds.insert(inverse_binds.end(), matrix, matrix + 16);
    }
    skin.skeleton = skeleton_.node_of(instance->skeleton_root_link);
    skin.inverse_bind_matrices = static_cast<int>(
        gltf_add_floats(out_.document, inverse_binds, GltfElement::Mat4, GltfTarget::None));
    append_skin_attributes(*data, model_.blocks[geometry.data_link], primitive);
    measure_bind_pose(block, *instance, *data);
    const int index = static_cast<int>(out_.document.skins.size());
    out_.report.joints += skin.joints.size();
    out_.document.skins.push_back(std::move(skin));
    ++out_.report.skins;
    skin_of_block_.emplace(block, index);
    return index;
}

// Triangle list NIF holds corners past vertex list dropped collector
std::vector<uint32_t> triangles_of(const NifBlock& mesh) {
    std::vector<uint32_t> indices;
    const size_t count = mesh.vertices.size() / 3;
    for (size_t corner = 0; corner + 2 < mesh.triangles.size(); corner += 3) {
        if (mesh.triangles[corner] >= count || mesh.triangles[corner + 1] >= count ||
            mesh.triangles[corner + 2] >= count)
            continue;
        for (int step = 0; step < 3; ++step) indices.push_back(mesh.triangles[corner + step]);
    }
    return indices;
}

// Authored vertex colour clamped client packs byte 2781 accessors outside
std::vector<float> clamped_colours(const NifBlock& mesh, uint32_t width) {
    const size_t count = mesh.vertices.size() / 3;
    std::vector<float> colours(count * width);
    for (size_t vertex = 0; vertex < count; ++vertex)
        for (uint32_t channel = 0; channel < width; ++channel)
            colours[vertex * width + channel] =
                saturate(mesh.vertex_colours[vertex * 4 + channel]);
    return colours;
}

void NifGltfBuilder::append_attributes(const NifBlock& mesh, ColourUse use,
                                       GltfPrimitive& primitive) {
    primitive.position = static_cast<int>(
        gltf_add_floats(out_.document, mesh.vertices, GltfElement::Vec3, GltfTarget::ArrayBuffer));
    if (mesh.normals.size() == mesh.vertices.size())
        primitive.normal = static_cast<int>(gltf_add_floats(
            out_.document, mesh.normals, GltfElement::Vec3, GltfTarget::ArrayBuffer));
    if (mesh.uvs.size() * 3 == mesh.vertices.size() * 2)
        primitive.texcoord = static_cast<int>(
            gltf_add_floats(out_.document, mesh.uvs, GltfElement::Vec2, GltfTarget::ArrayBuffer));
    if (use == ColourUse::None) return;
    const uint32_t width = use == ColourUse::Rgb ? 3u : 4u;
    primitive.colour = static_cast<int>(
        gltf_add_floats(out_.document, clamped_colours(mesh, width),
                        use == ColourUse::Rgb ? GltfElement::Vec3 : GltfElement::Vec4,
                        GltfTarget::ArrayBuffer));
}

// Geometry client draws reaches base map carries coordinates sample
bool NifGltfBuilder::is_drawable(const NifBlock& mesh, const std::string& texture) {
    if (texture.empty() && !options_.include_untextured) {
        ++out_.report.skipped_untextured;
        return false;
    }
    if (mesh.uvs.size() * 3 != mesh.vertices.size() * 2 && !texture.empty()) {
        ++out_.report.skipped_without_uv;
        return false;
    }
    return true;
}

void NifGltfBuilder::emit_geometry(uint32_t block, int node) {
    const NifBlock& geometry = model_.blocks[block];
    const NifBlock& mesh = model_.blocks[geometry.data_link];
    const std::string texture = find_base_texture_file_name(model_, geometry);
    if (!is_drawable(mesh, texture)) return;
    const std::vector<uint32_t> indices = triangles_of(mesh);
    if (indices.empty()) {
        ++out_.report.skipped_without_triangles;
        return;
    }
    const NifShadedBy shading = surface_shading(surfaces_[block], !texture.empty());
    const ColourUse use = colour_use_of(shading, carries_vertex_colours(mesh));

    GltfPrimitive primitive;
    append_attributes(mesh, use, primitive);
    primitive.indices = static_cast<int>(gltf_add_indices(out_.document, indices));
    primitive.material = material_for(block, texture, use);
    GltfNode& holder = out_.document.nodes[static_cast<size_t>(node)];
    holder.skin = skin_for(block, primitive);

    GltfMesh built;
    built.name = geometry.name;
    built.primitives.push_back(primitive);
    holder.mesh = static_cast<int>(out_.document.meshes.size());
    out_.document.meshes.push_back(std::move(built));
    ++out_.report.meshes;
    ++out_.report.primitives;
    out_.report.triangles += indices.size() / 3;
}

void NifGltfBuilder::build_geometry() {
    for (uint32_t block = 0; block < model_.blocks.size(); ++block) {
        const NifBlock& geometry = model_.blocks[block];
        if (geometry.data_link >= model_.blocks.size()) continue;
        if (model_.blocks[geometry.data_link].vertices.empty()) {
            ++out_.report.skipped_without_vertices;
            continue;
        }
        const int node = skeleton_.node_of(block);
        if (node < 0) {
            ++out_.report.skipped_unreachable;
            continue;
        }
        emit_geometry(block, node);
    }
}

bool NifGltfBuilder::build() {
    skeleton_ = build_nif_skeleton(model_);
    if (skeleton_.nodes.empty()) {
        out_.error = "the stream declares no scene node";
        return false;
    }
    surfaces_ = resolve_surface_states(model_);
    rest_nif_skeleton(skeleton_, rest_);
    build_nodes();
    build_geometry();
    append_nif_gltf_animations(request_, skeleton_, out_);
    attach_conversion_root();
    return true;
}

} // namespace

// Shepperd method component divided by largest of four never near
void gltf_rotation_of_quaternion(const float quaternion[4], float out[9]) {
    const float x = quaternion[0], y = quaternion[1], z = quaternion[2], w = quaternion[3];
    const float rebuilt[9] = {
        1.f - 2.f * (y * y + z * z), 2.f * (x * y - z * w),       2.f * (x * z + y * w),
        2.f * (x * y + z * w),       1.f - 2.f * (x * x + z * z), 2.f * (y * z - x * w),
        2.f * (x * z - y * w),       2.f * (y * z + x * w),       1.f - 2.f * (x * x + y * y)};
    for (int cell = 0; cell < 9; ++cell) out[cell] = rebuilt[cell];
}

void gltf_quaternion_of(const float rotation[9], float out[4]) {
    const float trace = rotation[0] + rotation[4] + rotation[8];
    float wxyz[4] = {1.f, 0.f, 0.f, 0.f};   // w x y z while being built
    if (trace > 0.f) {
        const float scale = std::sqrt(trace + 1.f) * 2.f;
        wxyz[0] = 0.25f * scale;
        wxyz[1] = (rotation[7] - rotation[5]) / scale;
        wxyz[2] = (rotation[2] - rotation[6]) / scale;
        wxyz[3] = (rotation[3] - rotation[1]) / scale;
    } else if (rotation[0] > rotation[4] && rotation[0] > rotation[8]) {
        const float scale = std::sqrt(1.f + rotation[0] - rotation[4] - rotation[8]) * 2.f;
        wxyz[0] = (rotation[7] - rotation[5]) / scale;
        wxyz[1] = 0.25f * scale;
        wxyz[2] = (rotation[1] + rotation[3]) / scale;
        wxyz[3] = (rotation[2] + rotation[6]) / scale;
    } else if (rotation[4] > rotation[8]) {
        const float scale = std::sqrt(1.f + rotation[4] - rotation[0] - rotation[8]) * 2.f;
        wxyz[0] = (rotation[2] - rotation[6]) / scale;
        wxyz[1] = (rotation[1] + rotation[3]) / scale;
        wxyz[2] = 0.25f * scale;
        wxyz[3] = (rotation[5] + rotation[7]) / scale;
    } else {
        const float scale = std::sqrt(1.f + rotation[8] - rotation[0] - rotation[4]) * 2.f;
        wxyz[0] = (rotation[3] - rotation[1]) / scale;
        wxyz[1] = (rotation[2] + rotation[6]) / scale;
        wxyz[2] = (rotation[5] + rotation[7]) / scale;
        wxyz[3] = 0.25f * scale;
    }
    const float length = std::sqrt(wxyz[0] * wxyz[0] + wxyz[1] * wxyz[1] + wxyz[2] * wxyz[2] +
                                   wxyz[3] * wxyz[3]);
    const float inverse = length > 0.f ? 1.f / length : 1.f;
    out[0] = wxyz[1] * inverse;
    out[1] = wxyz[2] * inverse;
    out[2] = wxyz[3] * inverse;
    out[3] = wxyz[0] * inverse;
}

// Indices in kSurfaceExtraFields order gathered one list missing refused
bool read_nif_gltf_material_extras(const std::vector<GltfExtra>& extras,
                                   NifGltfMaterialExtras& out) {
    std::vector<float> numbers;
    for (const SurfaceExtraField& field : kSurfaceExtraFields) {
        const GltfExtra* found = nullptr;
        for (const GltfExtra& extra : extras)
            if (extra.name == field.name) found = &extra;
        if (found == nullptr || found->values.size() != static_cast<size_t>(field.width))
            return false;
        numbers.insert(numbers.end(), found->values.begin(), found->values.end());
    }
    if (numbers.size() != kSurfaceStateWidth) return false;
    for (int channel = 0; channel < 3; ++channel) {
        out.material.ambient[channel] = numbers[channel];
        out.material.diffuse[channel] = numbers[3 + channel];
        out.material.specular[channel] = numbers[6 + channel];
        out.material.emissive[channel] = numbers[9 + channel];
    }
    out.material.glossiness = numbers[12];
    out.material.alpha = numbers[13];
    out.alpha.flags = static_cast<uint16_t>(numbers[14]);
    out.alpha.test_threshold = static_cast<uint8_t>(numbers[15]);
    out.depth.flags = static_cast<uint16_t>(numbers[16]);
    out.vertex_colour.lighting = static_cast<NifLightingMode>(static_cast<uint32_t>(numbers[17]));
    out.vertex_colour.vertex = static_cast<NifVertexMode>(static_cast<uint32_t>(numbers[18]));
    out.texture_apply = static_cast<NifTextureApplyMode>(static_cast<uint32_t>(numbers[19]));
    out.base_texture_clamp = static_cast<NifTextureClamp>(static_cast<uint32_t>(numbers[20]));
    out.has_material = numbers[21] != 0.f;
    out.has_alpha = numbers[22] != 0.f;
    out.has_depth = numbers[23] != 0.f;
    out.has_vertex_colour = numbers[24] != 0.f;
    return true;
}

bool build_nif_gltf(const NifGltfRequest& request, NifGltfResult& out) {
    if (request.model == nullptr) {
        out.error = "no model stream was given";
        return false;
    }
    NifGltfBuilder builder(request, out);
    return builder.build();
}

} // namespace KnC
