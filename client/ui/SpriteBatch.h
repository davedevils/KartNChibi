// textured quads on bgfx with colour and clip one submit per texture run
#pragma once

#include <bgfx/bgfx.h>

#include <cstdint>
#include <vector>

namespace KnC::Client {

// packs a colour the way the vertex layout stores it r g b a bytes in memory
constexpr uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) |
           (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(a) << 24);
}

constexpr uint32_t kWhite = 0xFFFFFFFFu;

class SpriteBatch {
public:
    bool init();
    void shutdown();

    // the canvas maps into a centred letterbox of the framebuffer with one uniform scale
    void begin(bgfx::ViewId view, uint16_t fbWidth, uint16_t fbHeight, float canvasWidth, float canvasHeight);
    void setClip(float x, float y, float w, float h);
    void clearClip();
    // a canvas rect the next quads leave open so a scene drawn before the sprites shows through it
    void setHole(float x, float y, float w, float h);
    void clearHole();
    void draw(bgfx::TextureHandle texture, float x, float y, float w, float h, uint32_t colour = kWhite,
              float u0 = 0.f, float v0 = 0.f, float u1 = 1.f, float v1 = 1.f);
    // a w by h sprite whose pivot point lands on x y turned clockwise by the angle in radians
    void drawRotated(bgfx::TextureHandle texture, float x, float y, float pivotX, float pivotY, float w, float h,
                     float angle, uint32_t colour = kWhite);
    // four free corners in order top left top right bottom right bottom left with one uv each
    void drawQuad(bgfx::TextureHandle texture, const float* xy, const float* uv, uint32_t colour = kWhite);
    // a plain rectangle drawn with the one pixel white texture
    void fill(float x, float y, float w, float h, uint32_t colour);
    void setWhite(bgfx::TextureHandle white) { m_white = white; }
    void end();

    float scale() const { return m_scale; }
    float scaleX() const { return m_scaleX; }
    float scaleY() const { return m_scaleY; }
    float offsetX() const { return m_offsetX; }
    float offsetY() const { return m_offsetY; }
    // the stock scales the canvas by w over 1024 and h over 768 on its own sizes
    void setStretch(bool stretch) { m_stretch = stretch; }
    bool stretch() const { return m_stretch; }
    // window pixels to canvas units
    void toCanvas(double px, double py, float& cx, float& cy) const;
    size_t lastSubmits() const { return m_lastSubmits; }

private:
    struct Vertex {
        float x, y, u, v;
        uint32_t colour;
    };
    struct Clip {
        bool on = false;
        float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
        bool operator==(const Clip& o) const { return on == o.on && x == o.x && y == o.y && w == o.w && h == o.h; }
    };
    struct Run {
        bgfx::TextureHandle texture;
        Clip clip;
        Clip hole;
        uint32_t firstVertex;
        uint32_t vertexCount;
    };
    // opens a run for the texture when the last one cannot take the quad
    bool beginQuad(bgfx::TextureHandle texture);

    bgfx::VertexLayout m_layout;
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_sampler = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_white = BGFX_INVALID_HANDLE;
    bgfx::ViewId m_view = 0;
    std::vector<Vertex> m_vertices;
    std::vector<Run> m_runs;
    Clip m_clip;
    Clip m_hole;
    float m_scale = 1.f;
    float m_scaleX = 1.f;
    float m_scaleY = 1.f;
    bool m_stretch = false;
    float m_offsetX = 0.f;
    float m_offsetY = 0.f;
    uint16_t m_fbWidth = 0;
    uint16_t m_fbHeight = 0;
    size_t m_lastSubmits = 0;
};

}
