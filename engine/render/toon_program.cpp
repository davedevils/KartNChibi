#include "engine/render/toon_program.h"

#include "engine/render/shaders/fs_outline.bin.h"
#include "engine/render/shaders/fs_toon.bin.h"
#include "engine/render/shaders/vs_outline.bin.h"
#include "engine/render/shaders/vs_skinned_outline.bin.h"
#include "engine/render/shaders/vs_skinned_toon.bin.h"
#include "engine/render/shaders/vs_toon.bin.h"

#include <bgfx/embedded_shader.h>

#include <cstdint>
#include <iostream>

namespace KnC::Render {

namespace {

// char texture toonramp bmp 256x1 grey client shadow floor 184 255
constexpr uint8_t kToonRamp[256] = {
    184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184,
    184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184,
    184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184,
    184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 185, 185,
    185, 185, 185, 186, 186, 186, 187, 187, 188, 189, 189, 190, 191, 192, 193, 194,
    196, 197, 199, 200, 202, 203, 205, 207, 209, 211, 213, 214, 216, 218, 221, 223,
    225, 226, 228, 230, 232, 234, 236, 237, 239, 240, 242, 243, 245, 246, 247, 248,
    249, 250, 250, 251, 252, 252, 253, 253, 253, 254, 254, 254, 254, 254, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
};

// const LightDir register c1 StaticToonShader view space camera right shoulder
constexpr float kToonLightDirection[4] = {-0.40825f, -0.54006f, 0.73598f, 0.f};
// LineThickness NSB default 0 point 02 model space units
constexpr float kLineThickness[4] = {0.02f, 0.f, 0.f, 0.f};
// character LineThickness 1382 of 1393 bodies override to 0 point 005
constexpr float kSkinnedLineThickness[4] = {0.005f, 0.f, 0.f, 0.f};
// LineColor opaque black NSB silhouette pass never reads alpha
constexpr float kLineColour[4] = {0.f, 0.f, 0.f, 1.f};
// ramp is lookup not surface wrap edge clamps shadow
constexpr uint32_t kRampSampler = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;

const bgfx::EmbeddedShader kToonShaders[] = {
    BGFX_EMBEDDED_SHADER(vs_toon), BGFX_EMBEDDED_SHADER(fs_toon),
    BGFX_EMBEDDED_SHADER(vs_outline), BGFX_EMBEDDED_SHADER(fs_outline),
    BGFX_EMBEDDED_SHADER(vs_skinned_toon), BGFX_EMBEDDED_SHADER(vs_skinned_outline),
    BGFX_EMBEDDED_SHADER_END()};

bgfx::ProgramHandle link(const char* vertex, const char* fragment) {
    const bgfx::RendererType::Enum backend = bgfx::getRendererType();
    return bgfx::createProgram(bgfx::createEmbeddedShader(kToonShaders, backend, vertex),
                               bgfx::createEmbeddedShader(kToonShaders, backend, fragment), true);
}

void destroy_uniform(bgfx::UniformHandle& uniform) {
    if (bgfx::isValid(uniform)) bgfx::destroy(uniform);
    uniform = BGFX_INVALID_HANDLE;
}

} // namespace

bool ToonProgram::create_ramp() {
    ramp_ = bgfx::createTexture2D(sizeof(kToonRamp), 1, false, 1, bgfx::TextureFormat::R8,
                                  kRampSampler, bgfx::copy(kToonRamp, sizeof(kToonRamp)));
    if (bgfx::isValid(ramp_)) return true;
    std::cerr << "[render] the toon ramp texture could not be created\n";
    return false;
}

bool ToonProgram::create() {
    surface_ = link("vs_toon", "fs_toon");
    outline_ = link("vs_outline", "fs_outline");
    skinned_surface_ = link("vs_skinned_toon", "fs_toon");
    skinned_outline_ = link("vs_skinned_outline", "fs_outline");
    ramp_sampler_ = bgfx::createUniform("s_toonRamp", bgfx::UniformType::Sampler);
    toon_light_ = bgfx::createUniform("u_toonLight", bgfx::UniformType::Vec4);
    line_params_ = bgfx::createUniform("u_lineParams", bgfx::UniformType::Vec4);
    line_colour_ = bgfx::createUniform("u_lineColour", bgfx::UniformType::Vec4);
    if (!create_ramp()) return false;
    if (ready()) return true;
    std::cerr << "[render] the cartoon shaders failed to link for "
              << bgfx::getRendererName(bgfx::getRendererType()) << "\n";
    return false;
}

bool ToonProgram::ready() const {
    return bgfx::isValid(surface_) && bgfx::isValid(outline_) &&
           bgfx::isValid(skinned_surface_) && bgfx::isValid(skinned_outline_) &&
           bgfx::isValid(ramp_) &&
           bgfx::isValid(ramp_sampler_) && bgfx::isValid(toon_light_) &&
           bgfx::isValid(line_params_) && bgfx::isValid(line_colour_);
}

void ToonProgram::bind_surface() const {
    bgfx::setTexture(1, ramp_sampler_, ramp_, kRampSampler);
    bgfx::setUniform(toon_light_, kToonLightDirection);
}

void ToonProgram::bind_outline() const {
    bgfx::setUniform(line_params_, kLineThickness);
    bgfx::setUniform(line_colour_, kLineColour);
}

void ToonProgram::bind_skinned_outline() const {
    bgfx::setUniform(line_params_, kSkinnedLineThickness);
    bgfx::setUniform(line_colour_, kLineColour);
}

void ToonProgram::destroy() {
    if (bgfx::isValid(ramp_)) bgfx::destroy(ramp_);
    ramp_ = BGFX_INVALID_HANDLE;
    destroy_uniform(ramp_sampler_);
    destroy_uniform(toon_light_);
    destroy_uniform(line_params_);
    destroy_uniform(line_colour_);
    if (bgfx::isValid(surface_)) bgfx::destroy(surface_);
    surface_ = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(outline_)) bgfx::destroy(outline_);
    outline_ = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(skinned_surface_)) bgfx::destroy(skinned_surface_);
    skinned_surface_ = BGFX_INVALID_HANDLE;
    if (bgfx::isValid(skinned_outline_)) bgfx::destroy(skinned_outline_);
    skinned_outline_ = BGFX_INVALID_HANDLE;
}

}
