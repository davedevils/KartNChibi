#pragma once
// glTF 2 0 document in JSON and binary buffer every accessor reads from one view per accessor
#include <cstdint>
#include <string>
#include <vector>

namespace KnC {

// accessor componentType on the wire as the GL constant
enum class GltfComponent : uint32_t {
    UnsignedByte  = 5121,
    UnsignedShort = 5123,
    UnsignedInt   = 5125,
    Float         = 5126,
};

// accessor type count of components each one spans is gltf element width
enum class GltfElement : uint8_t { Scalar, Vec2, Vec3, Vec4, Mat4 };

uint32_t gltf_element_width(GltfElement element);
const char* gltf_element_name(GltfElement element);

// bufferView target array buffer element array buffer or neither
enum class GltfTarget : uint32_t { None = 0, ArrayBuffer = 34962, ElementArrayBuffer = 34963 };

// One accessor and tightly packed bufferView byte offset aligned to component size
struct GltfAccessor {
    GltfComponent component   = GltfComponent::Float;
    GltfElement   element     = GltfElement::Scalar;
    GltfTarget    target      = GltfTarget::None;
    uint32_t      count       = 0;

    uint64_t      byte_offset = 0;
    uint64_t      byte_length = 0;
    // Component wise bounds required on POSITION accessor written on every accessor
    double minimum[16] = {0};
    double maximum[16] = {0};
};

// One primitive of a mesh minus 1 is an attribute the geometry does not carry
struct GltfPrimitive {
    int position = -1;
    int normal   = -1;
    int texcoord = -1;
    int colour   = -1;
    int joints   = -1;
    int weights  = -1;
    int indices  = -1;
    int material = -1;
};

struct GltfMesh {
    std::string                name;
    std::vector<GltfPrimitive> primitives;
};

// Node as translation rotation and scale glTF forbids animating a node with a matrix
struct GltfNode {
    std::string           name;
    float                 translation[3] = {0.f, 0.f, 0.f};
    float                 rotation[4]    = {0.f, 0.f, 0.f, 1.f};
    float                 scale[3]       = {1.f, 1.f, 1.f};
    int                   mesh           = -1;
    int                   skin           = -1;
    std::vector<uint32_t> children;
};

struct GltfSkin {
    std::string           name;
    int                   inverse_bind_matrices = -1;
    int                   skeleton              = -1;
    std::vector<uint32_t> joints;
};

// sampler wrapS wrapT as the GL constants glTF names
enum class GltfWrap : uint32_t { Repeat = 10497, ClampToEdge = 33071 };

struct GltfSampler {
    GltfWrap wrap_u = GltfWrap::Repeat;
    GltfWrap wrap_v = GltfWrap::Repeat;
};

struct GltfTexture {
    int image   = -1;
    int sampler = -1;
};

struct GltfImage {
    std::string uri;
};

enum class GltfAlphaMode : uint8_t { Opaque, Mask, Blend };

// One glTF extras object entry free form JSON the spec allows on any object
struct GltfExtra {
    std::string        name;
    std::vector<float> values;
};

struct GltfMaterial {
    std::string   name;
    float         base_colour[4] = {1.f, 1.f, 1.f, 1.f};
    int           base_texture   = -1;
    float         metallic       = 0.f;
    float         roughness      = 1.f;
    float         emissive[3]    = {0.f, 0.f, 0.f};
    GltfAlphaMode alpha_mode     = GltfAlphaMode::Opaque;
    float         alpha_cutoff   = 0.5f;
    bool          double_sided   = false;
    // KHR materials unlit the honest class for a surface drawn with no lighting term
    bool          unlit          = false;
    // The source format own material state untouched for a reader that wants it back
    std::vector<GltfExtra> extras;
};

enum class GltfPath : uint8_t { Translation, Rotation, Scale };

// glTF has LINEAR STEP and CUBICSPLINE no B spline so a curve resampled onto LINEAR
enum class GltfInterpolation : uint8_t { Linear, Step };

struct GltfAnimationSampler {
    // input accessor of the sample times output accessor of the sampled values
    uint32_t          input         = 0;
    uint32_t          output        = 0;
    GltfInterpolation interpolation = GltfInterpolation::Linear;
};

struct GltfAnimationChannel {
    uint32_t sampler = 0;
    uint32_t node    = 0;
    GltfPath path    = GltfPath::Translation;
};

struct GltfAnimation {
    std::string                       name;
    std::vector<GltfAnimationSampler> samplers;
    std::vector<GltfAnimationChannel> channels;
};

// A whole glTF asset buffer is the payload the accessors index
struct GltfDocument {
    std::string               generator = "xlegend-map";
    std::string               buffer_uri;
    std::vector<uint8_t>      buffer;
    std::vector<GltfAccessor> accessors;
    std::vector<GltfMesh>     meshes;
    std::vector<GltfNode>     nodes;
    std::vector<GltfSkin>     skins;
    std::vector<GltfMaterial> materials;
    std::vector<GltfTexture>  textures;
    std::vector<GltfImage>    images;
    std::vector<GltfSampler>  samplers;
    std::vector<GltfAnimation> animations;
    std::vector<uint32_t>     scene_roots;
};

// Appends count elements to buffer and returns the accessor that reads them
uint32_t gltf_add_floats(GltfDocument& document, const std::vector<float>& values,
                         GltfElement element, GltfTarget target);
uint32_t gltf_add_joints(GltfDocument& document, const std::vector<uint16_t>& slots);
uint32_t gltf_add_indices(GltfDocument& document, const std::vector<uint32_t>& indices);

// Writes path gltf and bin file false when file cannot be opened or float is NaN
bool write_gltf(const GltfDocument& document, const std::string& gltf_path,
                const std::string& bin_path, std::string& error);

// The JSON on its own for a caller that wants to check it before it lands
bool build_gltf_json(const GltfDocument& document, std::string& out, std::string& error);

} // namespace KnC
