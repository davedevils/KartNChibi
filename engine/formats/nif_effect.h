#pragma once
// What a NIF stream says about how a surface is drawn and particle systems hanging off it
#include "engine/formats/nif_animation.h"

#include <cstdint>
#include <string>
#include <vector>

namespace KnC {

// Wire values of NiAlphaProperty blend function enum which also what source and destination fields hold
enum class NifBlendFunction : uint32_t {
    One             = 0,
    Zero            = 1,
    SourceColour    = 2,
    InvSourceColour = 3,
    DestColour      = 4,
    InvDestColour   = 5,
    SourceAlpha     = 6,
    InvSourceAlpha  = 7,
    DestAlpha       = 8,
    InvDestAlpha    = 9,
    SourceAlphaSat  = 10,
};

// Wire values of NiAlphaProperty alpha test function enum
enum class NifTestFunction : uint32_t {
    Always = 0, Less = 1, Equal = 2, LessEqual = 3,
    Greater = 4, NotEqual = 5, GreaterEqual = 6, Never = 7,
};

// NiAlphaProperty flags word packs the whole blend and alpha test state test threshold is reference
struct NifAlphaState {
    uint16_t flags          = 0;
    uint8_t  test_threshold = 0;

    bool blend_enabled() const { return (flags & 0x0001u) != 0; }
    NifBlendFunction source_blend() const {
        return static_cast<NifBlendFunction>((flags >> 1) & 0xFu);
    }
    NifBlendFunction destination_blend() const {
        return static_cast<NifBlendFunction>((flags >> 5) & 0xFu);
    }
    bool test_enabled() const { return (flags & 0x0200u) != 0; }
    NifTestFunction test_function() const {
        return static_cast<NifTestFunction>((flags >> 10) & 0x7u);
    }
    // Bit 13 asks the accumulator to leave this surface where the tree put it
    bool no_sorting() const { return (flags & 0x2000u) != 0; }
};

// NiZBufferProperty a surface that names none draws with the renderer own default property
struct NifDepthState {
    uint16_t flags = 0x000Fu;

    bool test_enabled() const { return (flags & 0x0001u) != 0; }
    bool write_enabled() const { return (flags & 0x0002u) != 0; }
    NifTestFunction test_function() const {
        return static_cast<NifTestFunction>((flags >> 2) & 0x7u);
    }
};

// NiMaterialProperty emissive is what makes an effect surface glow without a light alpha is the rest value
struct NifMaterialState {
    float ambient[3]    = {0.5f, 0.5f, 0.5f};
    float diffuse[3]    = {0.5f, 0.5f, 0.5f};
    float specular[3]   = {0.f, 0.f, 0.f};
    float emissive[3]   = {0.f, 0.f, 0.f};
    float glossiness    = 4.f;
    float alpha         = 1.f;
};

// NiTexturingProperty ApplyMode texture stage colour operation client treats Replace as reason to switch lighting
enum class NifTextureApplyMode : uint32_t {
    Replace    = 0,
    Decal      = 1,
    Modulate   = 2,
    Highlight  = 3,
    Highlight2 = 4,
};

// NiTexturingProperty Map ClampMode bits 12 13 of map flags word
enum class NifTextureClamp : uint32_t {
    ClampUClampV = 0,
    ClampUWrapV  = 1,
    WrapUClampV  = 2,
    WrapUWrapV   = 3,
};

inline bool wraps_u(NifTextureClamp clamp) {
    return (static_cast<uint32_t>(clamp) & 0x2u) != 0;
}

inline bool wraps_v(NifTextureClamp clamp) {
    return (static_cast<uint32_t>(clamp) & 0x1u) != 0;
}

// NiTexturingProperty Map FilterMode the enum after the clamp bits 0 to 3 of the flags word
enum class NifTextureFilter : uint32_t {
    Nearest          = 0,
    Bilerp           = 1,
    Trilerp          = 2,
    NearestMipNearest = 3,
    NearestMipLerp   = 4,
    BilerpMipNearest = 5,
    Anisotropic      = 6,
};

// Nearest and bilerp name no mip level so the device samples the authored image alone
inline bool filters_mipmaps(NifTextureFilter filter) {
    return filter != NifTextureFilter::Nearest && filter != NifTextureFilter::Bilerp;
}

// Nearest picks one texel the rest interpolate the four
inline bool filters_nearest(NifTextureFilter filter) {
    return filter == NifTextureFilter::Nearest || filter == NifTextureFilter::NearestMipNearest ||
           filter == NifTextureFilter::NearestMipLerp;
}

// NiVertexColorProperty LightingMode which terms of fixed function lighting equation reach a lit vertex
enum class NifLightingMode : uint32_t {
    EmissiveOnly           = 0,
    EmissiveAmbientDiffuse = 1,
};

// NiVertexColorProperty VertexMode which material term the vertex colour stands in for
enum class NifVertexMode : uint32_t {
    Ignore         = 0,
    Emissive       = 1,
    AmbientDiffuse = 2,
};

// NiVertexColorProperty from 20 1 0 2 both modes packed into flags word below streamed as enum
struct NifVertexColourState {
    NifLightingMode lighting = NifLightingMode::EmissiveAmbientDiffuse;
    NifVertexMode   vertex   = NifVertexMode::Ignore;
};

// One property block contribution to the device state a block is exactly one of four families
struct NifRenderState {
    NifAlphaState        alpha;
    NifDepthState        depth;
    NifMaterialState     material;
    NifVertexColourState vertex_colour;
    // NiTexturingProperty client own default property is flags 0 so surface names none applies Replace
    NifTextureApplyMode  texture_apply = NifTextureApplyMode::Replace;
    // How the base map own coordinates are addressed outside 0 1
    NifTextureClamp      base_texture_clamp = NifTextureClamp::WrapUWrapV;
    // The device filter of the base map a non mip one keeps the authored image
    NifTextureFilter     base_texture_filter = NifTextureFilter::Trilerp;
};

// The device state one geometry own property list asks for a property names leaves member at default
struct NifSurfaceState {
    NifAlphaState        alpha;
    NifDepthState        depth;
    NifMaterialState     material;
    NifVertexColourState vertex_colour;
    NifTextureApplyMode  texture_apply = NifTextureApplyMode::Replace;
    NifTextureClamp      base_texture_clamp = NifTextureClamp::WrapUWrapV;
    NifTextureFilter     base_texture_filter = NifTextureFilter::Trilerp;
    bool has_alpha         = false;
    bool has_depth         = false;
    bool has_material      = false;
    bool has_vertex_colour = false;
    // NiStencilProperty draw mode both sides the renderer culls nothing on the part
    bool two_sided         = false;
    // The blocks the state came from a controller names one of these as its target
    uint32_t alpha_link         = kNoLink;
    uint32_t depth_link         = kNoLink;
    uint32_t material_link      = kNoLink;
    uint32_t vertex_colour_link = kNoLink;
    uint32_t texturing_link     = kNoLink;
};

// Which NiPSysModifier family a block belongs to only the families the client own data uses are named
enum class NifPSysModifierKind : uint8_t {
    Unknown,
    AgeDeath,
    Position,
    BoundUpdate,
    Spawn,
    GrowFade,
    Colour,
    Rotation,
    Gravity,
    BoxEmitter,
    MeshEmitter,
    SphereEmitter,
    CylinderEmitter,
};

// NiPSysEmitter and two volume shapes the client data ships three launch angles are radians
struct NifPSysEmitter {
    float speed                  = 0.f;
    float speed_variation        = 0.f;
    float declination            = 0.f;
    float declination_variation  = 0.f;
    float planar_angle           = 0.f;
    float planar_angle_variation = 0.f;
    float initial_colour[4]      = {1.f, 1.f, 1.f, 1.f};
    float initial_radius         = 0.f;
    float radius_variation       = 0.f;
    float life_span              = 0.f;
    float life_span_variation    = 0.f;
    // NiPSysVolumeEmitter the object whose local space the volume is in
    uint32_t volume_object_link = kNoLink;
    // NiPSysBoxEmitter the full width height and depth of the box
    float box_extent[3] = {0.f, 0.f, 0.f};
    // NiPSysSphereEmitter and NiPSysCylinderEmitter the radius of volume and cylinder axis its height
    float volume_radius = 0.f;
    float volume_height = 0.f;
    // NiPSysMeshEmitter the meshes whose surfaces particles are born on
    std::vector<uint32_t> emitter_meshes;
    uint32_t velocity_type = 0;
    uint32_t emission_type = 0;
    float    emit_axis[3]  = {0.f, 0.f, 1.f};
};

// NiPSysGrowFadeModifier the seconds a particle spends growing to size and shrinking back before dies
struct NifPSysGrowFade {
    float    grow_time       = 0.f;
    float    fade_time       = 0.f;
    uint16_t grow_generation = 0;
    uint16_t fade_generation = 0;
};

// NiPSysRotationModifier
struct NifPSysRotation {
    float speed              = 0.f;
    float speed_variation    = 0.f;
    float angle              = 0.f;
    float angle_variation    = 0.f;
    bool  random_sign        = false;
    bool  random_axis        = false;
    float axis[3]            = {0.f, 0.f, 1.f};
};

// NiPSysGravityModifier decay scales the pull down with distance from object axis is relative to
struct NifPSysGravity {
    uint32_t object_link      = kNoLink;
    float    axis[3]          = {0.f, 0.f, -1.f};
    float    decay            = 0.f;
    float    strength         = 0.f;
    uint32_t force_type       = 0;
    float    turbulence       = 0.f;
    float    turbulence_scale = 1.f;
};

// NiPSysSpawnModifier what a particle leaves behind when it dies
struct NifPSysSpawn {
    uint16_t generation_count  = 0;
    float    spawn_fraction    = 0.f;
    uint16_t minimum_to_spawn  = 0;
    uint16_t maximum_to_spawn  = 0;
    float    speed_chaos       = 0.f;
    float    direction_chaos   = 0.f;
    float    life_span         = 0.f;
    float    life_span_variation = 0.f;
};

// One NiPSysModifier block a modifier is exactly one family so only family member is filled
struct NifPSysModifier {
    NifPSysModifierKind kind      = NifPSysModifierKind::Unknown;
    uint32_t            order     = 0;
    bool                is_active = false;
    uint32_t            target_system_link = kNoLink;

    NifPSysEmitter  emitter;
    NifPSysGrowFade grow_fade;
    NifPSysRotation rotation;
    NifPSysGravity  gravity;
    NifPSysSpawn    spawn;
    // NiPSysColorModifier the NiColorData ramp it samples by particle age
    uint32_t colour_data_link = kNoLink;
    // NiPSysAgeDeathModifier
    bool     spawn_on_death     = false;
    uint32_t spawn_modifier_link = kNoLink;
    // NiPSysMeshUpdateModifier the mesh templates drawn one per particle
    std::vector<uint32_t> mesh_links;
};

// What one block contributes to a particle system a NiParticleSystem fills first group
struct NifParticleSystem {
    // NiParticleSystem whether the simulation runs in world space and modifiers that run over it
    bool                  world_space = false;
    std::vector<uint32_t> modifiers;
    // NiPSysData how many particles the pool holds at once and how many exporter left alive
    uint16_t pool_size    = 0;
    uint16_t active_count = 0;
    // NiPSysData authored per particle arrays when it carries them empty is the usual case
    std::vector<float> sizes;
    std::vector<float> radii;
    bool has_rotation_speeds = false;
    // NiMeshPSysData the node whose meshes are drawn one per particle
    uint32_t particle_meshes_link = kNoLink;
};

// Which source feeds the lit vertex colour of one surface this is whole of client Renderer SetupVertexColorLighting
enum class NifShadedBy : uint8_t {
    // D3DRS LIGHTING is switched off so vertex colour reaches texture stage untouched material not read
    Unlit,
    // Every material source stays D3DMCS MATERIAL emissive plus ambient times device global ambient
    Material,
    // D3DRS EMISSIVEMATERIALSOURCE is D3DMCS COLOR1 vertex colour is emissive term material gives ambient
    VertexEmissive,
    // Ambient and diffuse are D3DMCS COLOR1 emissive stays the material own
    VertexAmbientDiffuse,
};

// What the surface own properties make of it has base texture is whether NiTexturingProperty base map reaches image
NifShadedBy surface_shading(const NifSurfaceState& surface, bool has_base_texture);
const char* shaded_by_name(NifShadedBy shading);

// The names the wire values read as and the way back for the one an editor lets reader change
const char* blend_function_name(NifBlendFunction function);
bool parse_blend_function(const std::string& text, NifBlendFunction& out);
const char* test_function_name(NifTestFunction function);
const char* psys_modifier_kind_name(NifPSysModifierKind kind);

} // namespace KnC
