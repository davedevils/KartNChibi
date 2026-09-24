#pragma once
#include <bgfx/bgfx.h>

#include <cstdint>

namespace KnC::Render {

// PostProcess fx run way CxPostProcess Render stretched 128x128
class GlowPass {
public:
    bool create(uint16_t width, uint16_t height);
    bool resize(uint16_t width, uint16_t height);
    // Output frame landing back buffer editor viewport not origin
    void set_output_origin(uint16_t x, uint16_t y);
    void destroy();
    bool ready() const;

    // Off screen frame world drawn into glow reads back
    bgfx::FrameBufferHandle scene_target() const { return scene_; }
    // Puts frame on screen adds glow over
    void submit() const;

private:
    // One full screen draw bgfx clears draw state each submit
    struct QuadDraw {
        bgfx::ViewId        view;
        bgfx::TextureHandle source;
        bgfx::ProgramHandle program;
        uint64_t            state;
    };

    bool create_programs();
    bool create_quad();
    bool create_scene_target(uint16_t width, uint16_t height);
    void configure_view(bgfx::ViewId view, bgfx::FrameBufferHandle target, bool full_size) const;
    void submit_quad(const QuadDraw& draw) const;
    void blur(bgfx::ViewId view, bgfx::FrameBufferHandle source,
              bgfx::FrameBufferHandle target) const;

    bgfx::FrameBufferHandle  scene_ = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle  ping_  = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle  pong_  = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle      copy_        = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle      bright_pass_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle      blur_        = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle triangle_    = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle      source_sampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle      blur_taps_      = BGFX_INVALID_HANDLE;
    uint16_t                 width_  = 0;
    uint16_t                 height_ = 0;
    uint16_t                 output_x_ = 0;
    uint16_t                 output_y_ = 0;
};

}
