#pragma once
#include "engine/formats/nif_animation.h"
#include "engine/formats/nif_effect.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace KnC {

// Gamebryo nif stream header first block body version gates tables
struct NifHeader {
    uint32_t version       = 0;      // user version added at 10 1 0 8 little endian added at 20 0 0 3 else implied true before
    uint32_t user_version  = 0;
    bool     little_endian = true;
    uint32_t object_count  = 0;

    std::vector<std::string> type_names;     // RTTI table
    std::vector<uint16_t>    object_types;
    std::vector<bool>        object_skippable;  // index into type names one entry per block 20 2 0 5 and later else all false
    std::vector<uint32_t>    object_sizes;
    // sizes 20 2 0 5 and later else empty then table offset writer copies header verbatim on resize
    uint64_t                 object_sizes_offset = 0;
    std::vector<std::string> strings;        // strings table 20 1 0 1 and later else empty then max length writer copies header verbatim
    uint32_t                 max_string_length = 0;
    // Table begins ends rename from 20 1 0 1 append two words splice
    uint64_t                 strings_offset = 0;
    uint64_t                 strings_end_offset = 0;

    // Offset first block body below 20 2 0 5 no sizes decode seek
    uint64_t body_offset = 0;
};

// NiTexturingProperty Map texture matrix NiTextureTransformController component
struct NifTextureMatrix {
    // False map no transform members spell identity not read method
    bool     is_present     = false;
    float    translation[2] = {0.f, 0.f};
    float    scale[2]       = {1.f, 1.f};
    float    rotation       = 0.f;   // rotation radians about centre method is TransformMethod 1 in Max 762 maps HBO tree
    uint32_t method         = 0;
    float    centre[2]      = {0.f, 0.f};
};

// Light block is enabled dimmer colors attenuation range 30 3 0 5
struct NifLight {
    uint8_t is_enabled      = 1;                 // is enabled is NiBool from 10 1 0 102 unaffected nodes list NiDynamicEffect spares NiDeferredPointLight writes
    std::vector<uint32_t> unaffected_nodes;
    float   range[3]        = {0.f, 0.f, 0.f};   // Range max range falloff
    uint8_t group_mask      = 0;
    float   dimmer          = 1.f;
    float   ambient[3]      = {0.f, 0.f, 0.f};
    float   diffuse[3]      = {0.f, 0.f, 0.f};
    float   specular[3]     = {0.f, 0.f, 0.f};
    bool    has_attenuation = false;
    float   attenuation[3]  = {0.f, 0.f, 0.f};   // Constant linear quadratic
};

// Local transform scene node translation 3 by 3 rotation uniform
struct NifTransform {
    float translation[3] = {0.f, 0.f, 0.f};
    float rotation[9]    = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};
    float scale          = 1.f;
};

// One bone NiSkinData bind transform vertex skin space parallel
struct NifSkinBone {
    NifTransform          bind;
    std::vector<uint16_t> vertex_indices;
    std::vector<float>    weights;
};

// One NiSkinPartition record mesh chunk bone set hardware palette
struct NifSkinPartitionRecord {
    uint32_t              bones_per_vertex = 0;
    std::vector<uint16_t> bones;
    std::vector<uint16_t> vertex_map;   // Partition vertex to mesh vertex
    std::vector<float>    weights;
    std::vector<uint8_t>  bone_indices;
    std::vector<uint16_t> triangles;    // Partition local strips expanded
};

// Block contributes skin NiSkinInstance NiSkinData NiSkinPartition filled
struct NifSkin {
    // NiSkinInstance bones drive geometry data location
    uint32_t data_link          = kNoLink;
    uint32_t partition_link     = kNoLink;
    uint32_t skeleton_root_link = kNoLink;
    std::vector<uint32_t> bones;
    // NiSkinData bind pose entry per NiSkinInstance bone
    NifTransform             skin_to_root;
    std::vector<NifSkinBone> bone_binds;
    std::vector<NifSkinPartitionRecord> partitions;
};

// Vertex array begin inside span width zero offset zero vertex
struct NifVertexSpans {
    uint64_t vertices = 0;   uint8_t vertex_format = 0;
    uint64_t normals  = 0;   uint8_t normal_format = 0;
    uint64_t uvs      = 0;   uint8_t uv_format     = 0;   // the first set only
    uint64_t colours  = 0;   uint8_t colour_format = 0;
};

// One NiGeometry material list entry name index string table 20 1 0 1
struct NifGeometryMaterial {
    std::string name;
    uint32_t    name_index = kNoLink;
    uint32_t    extra_data = 0;
};

// NiGeometry NiGeometryData beyond renderer writes block parse decode
struct NifGeometrySource {
    // NiGeometry from 20 2 0 5 named material list active entry
    std::vector<NifGeometryMaterial> materials;
    uint32_t    active_material = 0;
    // Below 20 2 0 5 optional shader raw byte not bool
    uint8_t     has_shader = 0;
    std::string shader_name;
    uint32_t    shader_name_index = kNoLink;
    uint32_t    shader_implementation = 0;
    uint8_t     is_dirty = 0;             // From 20 2 0 7

    uint32_t group_id       = 0;          // vertex count held not derived array empty group id from 10 1 0 114
    uint16_t vertex_count   = 0;
    uint8_t  keep_flags     = 0;
    uint8_t  compress_flags = 0;
    uint16_t data_flags     = 0;          // data flags texture set low 6 bits basis top 4 raw formats below 20 3 1 0 stream floats
    uint8_t  raw_vertex_format = 0;
    uint8_t  raw_normal_format = 0;
    uint8_t  raw_colour_format = 0;
    // Tangents bitangents normal basis appends normals width declared
    std::vector<float> tangents_bitangents;
    std::vector<float> extra_uv_sets;
    uint16_t consistency_flags = 0;
    uint32_t additional_data_link = kNoLink;   // additional data link from 10 3 0 7 triangle data NiTriShapeData index array count streamed
    uint16_t triangle_count = 0;
    uint32_t index_count    = 0;
    uint8_t  has_indices    = 0;
    // NiTriShapeData shared normal groups one vertex list per group
    std::vector<std::vector<uint16_t>> shared_normal_groups;
    // NiTriStripsData triangles strips expanded degenerate corners strip array
    std::vector<uint16_t> strip_lengths;
    uint8_t               has_strip_indices = 0;
    std::vector<uint16_t> strip_indices;
};

// Property value inside block span stream offset zero name
struct NifPropertySpan {
    uint64_t offset = 0;
    uint8_t  width  = 0;   // width 1 2 or 4 bytes shift bits below value mask applies after shift 0 means whole word
    uint8_t  shift  = 0;
    uint8_t  mask   = 0;
};

// NiTextureEffect the map it lays over the geometry it affects and how its coordinates are made
struct NifTextureEffect {
    // 0 projected light 1 projected shadow 2 environment map 3 fog map
    uint32_t texture_type = 0;
    // 0 world parallel 1 world perspective 2 sphere map 3 specular cube map 4 diffuse cube map
    uint32_t coordinate_type = 0;
    uint32_t texture_link = kNoLink;
};

// Four property families glTF carries extras values pointer NiMaterialProperty
struct NifPropertySpans {
    NifPropertySpan alpha_flags;       // NiAlphaProperty
    NifPropertySpan alpha_threshold;
    NifPropertySpan depth_flags;       // depth flags is NiZBufferProperty lighting mode is NiVertexColorProperty
    NifPropertySpan lighting_mode;
    NifPropertySpan vertex_mode;
    NifPropertySpan texture_apply;     // texture apply is NiTexturingProperty texture clamp is inside it on the base map
    NifPropertySpan texture_clamp;
};

// One decoded block name transform filled families type names
struct NifBlock {
    std::string  type;
    std::string  name;
    // NiObjectNET descendant carries name field empty name not string
    bool         has_name = false;
    // Fixed string table entry body 20 1 0 1 kNoLink no name
    uint32_t     name_index = kNoLink;
    // NiObject LoadBinary streams group id below 10 1 0 114 NiGeometryData
    uint32_t     object_group_id = 0;
    // NiObjectNET NiAVObject state written back parse
    std::vector<uint32_t> extra_data;
    uint32_t     controller_link = kNoLink;
    uint16_t     object_flags = 0;
    uint32_t     collision_link = kNoLink;
    bool         has_transform = false;
    NifTransform transform;
    std::vector<uint32_t> children;   // link id arrays children NiNode only properties every NiAVObject descendant
    std::vector<uint32_t> properties;
    // NiNode effect list the dynamic effects that light its subtree
    std::vector<uint32_t> effects;
    uint32_t     data_link = kNoLink; // data link points to geometry data block holding positions triples corner indices strips expanded
    std::vector<float>    vertices;
    std::vector<uint16_t> triangles;
    // Parallel vertices normals triples texture set pairs empty
    std::vector<float>    normals;
    std::vector<float>    uvs;
    // Parallel vertices authored vertex color effect tree paint draw
    std::vector<float>    vertex_colours;
    // Geometry data blocks bounding sphere block space NiNode merges
    float bound_center[3] = {0.f, 0.f, 0.f};
    float bound_radius    = 0.f;
    // NiSourceTexture name artist image resolving path caller job
    std::string  texture_file_name;
    // NiStringPalette NUL separated buffer pre 20 1 0 1 IDTag index
    std::string  string_palette;
    // NiTexturingProperty link NiSourceTexture base map texture coordinate
    uint32_t         base_texture_link = kNoLink;
    NifTextureMatrix base_texture_matrix;
    // The detail map of slot 2 the device modulates it twice over the base its own matrix
    uint32_t         detail_texture_link = kNoLink;
    NifTextureMatrix detail_texture_matrix;
    // Non base texture maps detail decal glow NiTexturingProperty slot
    std::vector<uint32_t> extra_texture_links;
    // The slot of each extra map 1 dark 2 detail 3 gloss 4 glow 5 bump 6 the first decal
    std::vector<uint32_t> extra_texture_slots;
    // Animation payload pointer null controller pose key groups
    std::shared_ptr<NifAnimation> animation;
    // NiGeometry NiSkinInstance binds skeleton
    uint32_t skin_instance_link = kNoLink;
    // Skin payload pointer three types carry one
    std::shared_ptr<NifSkin> skin;
    // Vertex arrays stream geometry data pointer half million carry
    std::shared_ptr<NifVertexSpans> vertex_spans;
    // Device state payload three property blocks
    std::shared_ptr<NifRenderState> render_state;
    // Payload values four property families glTF extras null blocks
    std::shared_ptr<NifPropertySpans> property_spans;
    // NiGeometry NiGeometryData renderer not read null other
    std::shared_ptr<NifGeometrySource> geometry_source;
    // Light payload light blocks pointer tree tens lights
    std::shared_ptr<NifLight> light;
    // Particle payload NiParticleSystem NiPSysData
    std::shared_ptr<NifParticleSystem> particles;
    // Particle modifier payload NiPSysModifier descendant
    std::shared_ptr<NifPSysModifier> psys_modifier;
    // Texture effect payload NiTextureEffect only
    std::shared_ptr<NifTextureEffect> texture_effect;
    // Block body file offset next block below 20 2 0 5 no parse
    uint64_t byte_offset = 0;
    uint64_t byte_length = 0;
    // Name transform inside span bytes decoder read values edit
    uint64_t name_offset      = 0;
    uint64_t transform_offset = 0;
    // NiMaterialProperty fourteen floats ambient diffuse specular emissive glTF
    uint64_t material_offset  = 0;
    // NiStencilProperty draw mode 0 default 1 ccw 2 cw 3 both sides
    uint32_t stencil_draw_mode = 0;
    bool     has_stencil = false;
    // NiBillboardNode mode 0 face camera 1 rotate about up 2 rigid face camera
    uint16_t billboard_mode = 0;
    bool     has_billboard = false;
    // No decoder unknown type header step over size 20 2 0 5
    bool     skipped = false;
};

struct NifScene {
    NifHeader header;
    std::vector<NifBlock> blocks;
    // Scene roots footer declares file blocks nothing links
    std::vector<uint32_t> roots;
    // Bytes after last block stream footer root count parse exact
    uint64_t trailing_bytes = 0;
};

// Blocks stream order below 20 2 0 5 no sizes error block
bool read_nif_scene(const std::string& path, NifScene& out, std::string& error);

// Reads header only false file missing not NIF parse
bool read_nif_header(const std::string& path, NifHeader& out);

// Device state block Gamebryo property inherited NiAVObject 2478 203232
std::vector<NifSurfaceState> resolve_surface_states(const NifScene& scene);

// Geometry to properties to NiTexturingProperty to base NiSourceTexture
std::string find_base_texture_file_name(const NifScene& scene, const NifBlock& geometry);
// The detail map file of the geometry own NiTexturingProperty empty when it carries none
std::string find_detail_texture_file_name(const NifScene& scene, const NifBlock& geometry);

// Names for extra texture links in slot order
std::vector<std::string> find_extra_texture_file_names(const NifScene& scene, const NifBlock& geometry);

// Animation payload block link null range controller interpolator
const NifAnimation* find_animation(const NifScene& scene, uint32_t link);

} // namespace KnC
