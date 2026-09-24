#include "engine/render/glow_pass.h"

#include "engine/render/shaders/fs_blur.bin.h"
#include "engine/render/shaders/fs_brightpass.bin.h"
#include "engine/render/shaders/fs_copy.bin.h"
#include "engine/render/shaders/vs_fullscreen.bin.h"
#include "engine/render/view_order.h"

#include <bgfx/embedded_shader.h>

#include <iostream>

namespace KnC::Render {

namespace {

// CxPostProcess Create 0x80 working targets 128x128 frame squeezed
constexpr uint16_t kGlowTargetSize = 128;
constexpr bgfx::TextureFormat::Enum kGlowTargetFormat = bgfx::TextureFormat::BGRA8;
// PostProcess fx clamps filters every sampler in chain
constexpr uint64_t kGlowSampler = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
// SrcBlend One DestBlend Zero technique first pass writes
constexpr uint64_t kReplaceState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
// SrcBlend One DestBlend One second blur pass CombineBuffer
constexpr uint64_t kAddState = BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ADD;

// PostProcess fx offsets multiplied by blur scale 1 5
constexpr float kFirstBlurTaps[8] = {-0.0075f, 0.f, 0.0075f, 0.f, 0.f, -0.0075f, 0.f, 0.0075f};
// Offsets diagonal taps second pass adds by scale
constexpr float kSecondBlurTaps[8] = {-0.0045f, -0.0045f, 0.0045f, -0.0045f,
                                      -0.0045f, 0.0045f,  0.0045f, 0.0045f};

// One triangle larger than screen no seam middle
struct FullscreenVertex {
    float x;
    float y;
    float z;
    float u;
    float v;
};

const bgfx::EmbeddedShader kGlowShaders[] = {
    BGFX_EMBEDDED_SHADER(vs_fullscreen), BGFX_EMBEDDED_SHADER(fs_copy),
    BGFX_EMBEDDED_SHADER(fs_brightpass), BGFX_EMBEDDED_SHADER(fs_blur),
    BGFX_EMBEDDED_SHADER_END()};

bgfx::ProgramHandle link(const char* fragment) {
    const bgfx::RendererType::Enum backend = bgfx::getRendererType();
    return bgfx::createProgram(
        bgfx::createEmbeddedShader(kGlowShaders, backend, "vs_fullscreen"),
        bgfx::createEmbeddedShader(kGlowShaders, backend, fragment), true);
}

void destroy_frame_buffer(bgfx::FrameBufferHandle& target) {
    if (bgfx::isValid(target)) bgfx::destroy(target);
    target = BGFX_INVALID_HANDLE;
}

void destroy_program(bgfx::ProgramHandle& program) {
    if (bgfx::isValid(program)) bgfx::destroy(program);
    program = BGFX_INVALID_HANDLE;
}

void destroy_uniform(bgfx::UniformHandle& uniform) {
    if (bgfx::isValid(uniform)) bgfx::destroy(uniform);
    uniform = BGFX_INVALID_HANDLE;
}

} // namespace

bool GlowPass::create_programs() {
    copy_ = link("fs_copy");
    bright_pass_ = link("fs_brightpass");
    blur_ = link("fs_blur");
    if (bgfx::isValid(copy_) && bgfx::isValid(bright_pass_) && bgfx::isValid(blur_)) return true;
    std::cerr << "[render] the post-process shaders failed to link for "
              << bgfx::getRendererName(bgfx::getRendererType()) << "\n";
    return false;
}

bool GlowPass::create_quad() {
    // Backend textures start bottom left reads off screen upside
    const bool bottom_left = bgfx::getCaps()->originBottomLeft;
    const float base = bottom_left ? 0.f : 1.f;
    const float apex = bottom_left ? 2.f : -1.f;
    const FullscreenVertex corners[3] = {{-1.f, -1.f, 0.f, 0.f, base},
                                         {3.f, -1.f, 0.f, 2.f, base},
                                         {-1.f, 3.f, 0.f, 0.f, apex}};
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    triangle_ = bgfx::createVertexBuffer(bgfx::copy(corners, sizeof(corners)), layout);
    if (bgfx::isValid(triangle_)) return true;
    std::cerr << "[render] the post-process triangle could not be created\n";
    return false;
}

bool GlowPass::create_scene_target(uint16_t width, uint16_t height) {
    bgfx::TextureHandle attachments[2];
    attachments[0] = bgfx::createTexture2D(width, height, false, 1, kGlowTargetFormat,
                                           BGFX_TEXTURE_RT | kGlowSampler);
    attachments[1] = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::D24S8,
                                           BGFX_TEXTURE_RT_WRITE_ONLY);
    if (bgfx::isValid(attachments[0]) && bgfx::isValid(attachments[1])) {
        scene_ = bgfx::createFrameBuffer(2, attachments, true);
        width_ = width;
        height_ = height;
        return bgfx::isValid(scene_);
    }
    std::cerr << "[render] the off-screen frame the glow reads back could not be created\n";
    return false;
}

bool GlowPass::create(uint16_t width, uint16_t height) {
    if (!create_programs() || !create_quad()) return false;
    source_sampler_ = bgfx::createUniform("s_source", bgfx::UniformType::Sampler);
    blur_taps_ = bgfx::createUniform("u_blurTaps", bgfx::UniformType::Vec4, 2);
    ping_ =
        bgfx::createFrameBuffer(kGlowTargetSize, kGlowTargetSize, kGlowTargetFormat, kGlowSampler);
    pong_ =
        bgfx::createFrameBuffer(kGlowTargetSize, kGlowTargetSize, kGlowTargetFormat, kGlowSampler);
    if (!create_scene_target(width, height)) return false;
    if (ready()) return true;
    std::cerr << "[render] the glow chain could not be set up\n";
    return false;
}

bool GlowPass::resize(uint16_t width, uint16_t height) {
    if (!bgfx::isValid(scene_) || (width == width_ && height == height_)) return true;
    destroy_frame_buffer(scene_);
    return create_scene_target(width, height);
}

void GlowPass::set_output_origin(uint16_t x, uint16_t y) {
    output_x_ = x;
    output_y_ = y;
}

bool GlowPass::ready() const {
    return bgfx::isValid(scene_) && bgfx::isValid(ping_) && bgfx::isValid(pong_) &&
           bgfx::isValid(copy_) && bgfx::isValid(bright_pass_) && bgfx::isValid(blur_) &&
           bgfx::isValid(triangle_) && bgfx::isValid(source_sampler_) && bgfx::isValid(blur_taps_);
}

void GlowPass::configure_view(bgfx::ViewId view, bgfx::FrameBufferHandle target,
                              bool full_size) const {
    bgfx::setViewFrameBuffer(view, target);
    if (full_size) bgfx::setViewRect(view, output_x_, output_y_, width_, height_);
    else bgfx::setViewRect(view, 0, 0, kGlowTargetSize, kGlowTargetSize);
    bgfx::setViewTransform(view, nullptr, nullptr);
    bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
}

void GlowPass::submit_quad(const QuadDraw& draw) const {
    bgfx::setTexture(0, source_sampler_, draw.source, kGlowSampler);
    bgfx::setState(draw.state);
    bgfx::setVertexBuffer(0, triangle_);
    bgfx::submit(draw.view, draw.program);
}

// FullScreenBlur one technique two passes same target first four
void GlowPass::blur(bgfx::ViewId view, bgfx::FrameBufferHandle source,
                    bgfx::FrameBufferHandle target) const {
    configure_view(view, target, false);
    const bgfx::TextureHandle texture = bgfx::getTexture(source);
    bgfx::setUniform(blur_taps_, kFirstBlurTaps, 2);
    submit_quad({view, texture, blur_, kReplaceState});
    bgfx::setUniform(blur_taps_, kSecondBlurTaps, 2);
    submit_quad({view, texture, blur_, kAddState});
}

void GlowPass::submit() const {
    const bgfx::TextureHandle world = bgfx::getTexture(scene_);
    // StretchRect back buffer 128x128 D3DTEXF LINEAR frame squeezed
    configure_view(kGlowCaptureView, ping_, false);
    submit_quad({kGlowCaptureView, world, copy_, kReplaceState});
    configure_view(kGlowBrightPassView, pong_, false);
    submit_quad({kGlowBrightPassView, bgfx::getTexture(ping_), bright_pass_, kReplaceState});
    blur(kGlowFirstBlurView, pong_, ping_);
    blur(kGlowSecondBlurView, ping_, pong_);
    // World already back buffer client combines put world first
    configure_view(kGlowCombineView, BGFX_INVALID_HANDLE, true);
    submit_quad({kGlowCombineView, world, copy_, BGFX_STATE_WRITE_RGB});
    submit_quad({kGlowCombineView, bgfx::getTexture(pong_), copy_, kAddState});
}

void GlowPass::destroy() {
    destroy_frame_buffer(scene_);
    destroy_frame_buffer(ping_);
    destroy_frame_buffer(pong_);
    destroy_program(copy_);
    destroy_program(bright_pass_);
    destroy_program(blur_);
    if (bgfx::isValid(triangle_)) bgfx::destroy(triangle_);
    triangle_ = BGFX_INVALID_HANDLE;
    destroy_uniform(source_sampler_);
    destroy_uniform(blur_taps_);
}

}
