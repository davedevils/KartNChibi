#pragma once
// NIF surface device state translated to bgfx used surface particle
#include "engine/formats/nif_effect.h"
#include "engine/render/day_night.h"
#include "engine/render/model_animation.h"

#include <bgfx/bgfx.h>

#include <iostream>
#include <string>

namespace KnC::Render {

// Masks stretch over layer edge texels must not wrap around
constexpr uint32_t kCoverageSampler = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
// Ground at grazing angle trilinear alone smears it need anisotropic
constexpr uint32_t kDiffuseSampler =
    BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_ANISOTROPIC;

// How surface base map addresses coordinates outside 0 to 1 and which device filter it asks
inline uint32_t diffuse_sampler(NifTextureClamp clamp,
                                NifTextureFilter filter = NifTextureFilter::Trilerp) {
    uint32_t flags = filters_nearest(filter) ? (BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT)
                                             : kDiffuseSampler;
    if (!filters_mipmaps(filter)) flags |= BGFX_SAMPLER_MIP_POINT;
    if (!wraps_u(clamp)) flags |= BGFX_SAMPLER_U_CLAMP;
    if (!wraps_v(clamp)) flags |= BGFX_SAMPLER_V_CLAMP;
    return flags;
}

constexpr float kNoAlphaCutout = 0.f;
// One step of an eight bit alpha a clear texel of a blended part that writes depth
constexpr float kClearTexelCutout = 1.f / 255.f;
// Layer closed surface no property list draws with default state
constexpr uint64_t kPropState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z |
                                BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW;

// Surface without NiTextureTransformController samples through raw coordinates
constexpr UvMatrix kRawTextureCoordinates;

// Material alpha of surface with neither material nor controller
constexpr float kOpaqueMaterial = 1.f;

// One depth comparison per NIF test function 2434 of 2478 LESS EQUAL
inline uint64_t depth_comparison(NifTestFunction function) {
    switch (function) {
        case NifTestFunction::Always:       return BGFX_STATE_DEPTH_TEST_ALWAYS;
        case NifTestFunction::Less:         return BGFX_STATE_DEPTH_TEST_LESS;
        case NifTestFunction::Equal:        return BGFX_STATE_DEPTH_TEST_EQUAL;
        case NifTestFunction::Greater:      return BGFX_STATE_DEPTH_TEST_GREATER;
        case NifTestFunction::NotEqual:     return BGFX_STATE_DEPTH_TEST_NOTEQUAL;
        case NifTestFunction::GreaterEqual: return BGFX_STATE_DEPTH_TEST_GEQUAL;
        case NifTestFunction::Never:        return BGFX_STATE_DEPTH_TEST_NEVER;
        case NifTestFunction::LessEqual:    break;
    }
    return BGFX_STATE_DEPTH_TEST_LEQUAL;
}

inline uint64_t blend_factor(NifBlendFunction function) {
    switch (function) {
        case NifBlendFunction::Zero:            return BGFX_STATE_BLEND_ZERO;
        case NifBlendFunction::SourceColour:    return BGFX_STATE_BLEND_SRC_COLOR;
        case NifBlendFunction::InvSourceColour: return BGFX_STATE_BLEND_INV_SRC_COLOR;
        case NifBlendFunction::DestColour:      return BGFX_STATE_BLEND_DST_COLOR;
        case NifBlendFunction::InvDestColour:   return BGFX_STATE_BLEND_INV_DST_COLOR;
        case NifBlendFunction::SourceAlpha:     return BGFX_STATE_BLEND_SRC_ALPHA;
        case NifBlendFunction::InvSourceAlpha:  return BGFX_STATE_BLEND_INV_SRC_ALPHA;
        case NifBlendFunction::DestAlpha:       return BGFX_STATE_BLEND_DST_ALPHA;
        case NifBlendFunction::InvDestAlpha:    return BGFX_STATE_BLEND_INV_DST_ALPHA;
        case NifBlendFunction::SourceAlphaSat:  return BGFX_STATE_BLEND_SRC_ALPHA_SAT;
        case NifBlendFunction::One:             break;
    }
    return BGFX_STATE_BLEND_ONE;
}

// Every pair of the NiAlphaProperty flags reaches the device the fragment hands it straight alpha
inline uint64_t blend_state(const NifAlphaState& alpha) {
    if (!alpha.blend_enabled()) return 0;
    return BGFX_STATE_BLEND_FUNC(blend_factor(alpha.source_blend()),
                                 blend_factor(alpha.destination_blend()));
}

// Surface property list asks device props closed back faces culled
inline uint64_t surface_draw_state(const NifSurfaceState& surface) {
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_CULL_CW;
    if (surface.depth.test_enabled()) state |= depth_comparison(surface.depth.test_function());
    if (surface.depth.write_enabled()) state |= BGFX_STATE_WRITE_Z;
    return state | blend_state(surface.alpha);
}

// A texel the shader drops nothing above one so a never test drops every texel
constexpr float kDropEveryTexel = 2.f;

// threshold sign carries test function above zero drops under below zero drops over reference 0 still drops clear texels
inline float surface_alpha_cutout(const NifAlphaState& alpha) {
    if (!alpha.test_enabled()) return kNoAlphaCutout;
    const float reference = static_cast<float>(alpha.test_threshold) / 255.f;
    const float half_step = 0.5f / 255.f;
    switch (alpha.test_function()) {
        case NifTestFunction::Never:        return kDropEveryTexel;
        case NifTestFunction::Less:         return -reference;
        case NifTestFunction::LessEqual:    return -(reference + half_step);
        case NifTestFunction::Always:       return kNoAlphaCutout;
        case NifTestFunction::GreaterEqual: return reference;
        default:                            break;
    }
    // Greater equal and not equal all keep the texels over the reference itself
    return reference + half_step;
}

// A blended part that writes depth drops its clear texels so what stands behind them still draws
inline float surface_alpha_cutout(const NifSurfaceState& surface) {
    const float cutout = surface_alpha_cutout(surface.alpha);
    if (cutout < 0.f || cutout > kClearTexelCutout) return cutout;
    if (!surface.alpha.blend_enabled() || !surface.depth.write_enabled()) return cutout;
    return kClearTexelCutout;
}

// Fixed function lighting feeds surface vertex colour three vectors shaders
struct ShadeState {
    // Material emissive colour rgb w 1 when vertex colour is
    float emissive[4] = {0.f, 0.f, 0.f, 0.f};
    // Material ambient colour rgb w 1 when vertex colour is
    float ambient[4] = {0.f, 0.f, 0.f, 0.f};
    // Vertex alpha reaches fragment x material alpha y surface takes
    float alpha[4] = {0.f, 1.f, 0.f, 0.f};
    // Material diffuse colour rgb w 1 when vertex colour is the sun term reads it
    float diffuse[4] = {0.f, 0.f, 0.f, 0.f};
};

// Viewer overlays markers hulls collision grid not client vertex colour
constexpr ShadeState kUnlitShade{{0.f, 0.f, 0.f, 1.f}, {0.f, 0.f, 0.f, 0.f},
                                 {1.f, 0.f, 0.f, 0.f}};
// TerrainShader fx multiplies baked cell colour hour ambient alpha vertex
constexpr ShadeState kTerrainShade{{0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 1.f},
                                   {1.f, 0.f, 0.f, 0.f}};
// Particle quad ramp colour vertex material emissive tint ambient slot
constexpr ShadeState kParticleShade{{0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 1.f},
                                    {1.f, 1.f, 0.f, 0.f}};

// NIF surface properties vertex colour has vertex colours mesh carries
inline ShadeState surface_shade_state(const NifSurfaceState& surface, bool has_base_texture,
                                      bool has_vertex_colours) {
    ShadeState shade;
    const NifShadedBy shading = surface_shading(surface, has_base_texture);
    if (shading == NifShadedBy::Unlit) return kUnlitShade;
    const float from_vertex = has_vertex_colours ? 1.f : 0.f;
    if (shading == NifShadedBy::VertexEmissive) shade.emissive[3] = from_vertex;
    if (shading == NifShadedBy::VertexAmbientDiffuse) {
        shade.ambient[3] = from_vertex;
        shade.diffuse[3] = from_vertex;
        shade.alpha[0] = from_vertex;
        shade.alpha[1] = 1.f - from_vertex;
    }
    for (int channel = 0; channel < 3; ++channel) {
        shade.emissive[channel] = surface.material.emissive[channel];
        shade.ambient[channel] = surface.material.ambient[channel];
        shade.diffuse[channel] = surface.material.diffuse[channel];
    }
    return shade;
}

// Draw tells shaders surface splat cutout vertex colour lighting ambient
struct SurfaceUniforms {
    // Diffuse repeats xy alpha cutout z material alpha w
    float      splat[4] = {1.f, 1.f, kNoAlphaCutout, kOpaqueMaterial};
    ShadeState shade = kUnlitShade;
    HourColour ambient;
};

}
