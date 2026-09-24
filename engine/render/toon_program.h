#pragma once
#include <bgfx/bgfx.h>

namespace KnC::Render {

// toon surface with outline two passes ramp light line constants
class ToonProgram {
public:
    bool create();
    void destroy();
    bool ready() const;

    bgfx::ProgramHandle surface() const { return surface_; }
    bgfx::ProgramHandle outline() const { return outline_; }
    // two passes for skinned mesh with bone palette
    bgfx::ProgramHandle skinned_surface() const { return skinned_surface_; }
    bgfx::ProgramHandle skinned_outline() const { return skinned_outline_; }

    // bgfx binds uniform to submit each pass sets constants
    void bind_surface() const;
    void bind_outline() const;
    // constants at character line thickness override
    void bind_skinned_outline() const;

private:
    bool create_ramp();

    bgfx::ProgramHandle surface_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle outline_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle skinned_surface_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle skinned_outline_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle ramp_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle ramp_sampler_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle toon_light_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle line_params_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle line_colour_ = BGFX_INVALID_HANDLE;
};

}
