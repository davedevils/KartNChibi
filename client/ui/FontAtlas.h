// stb truetype atlas baked from a ttf file the Roboto header of the bgfx tree is the fallback
#pragma once

#include "SpriteBatch.h"

#include <bgfx/bgfx.h>

#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Client {

class FontAtlas {
public:
    // bakes ascii and latin one at the given pixel height into one texture from the ttf when it reads
    bool init(float pixelHeight, const std::string& ttfPath = std::string());
    // the face that was baked Arial when the windows font read else Roboto
    const std::string& faceName() const { return m_face; }
    void shutdown();

    float bakedHeight() const { return m_pixelHeight; }
    // full line advance at the drawn size
    float lineHeight(float px) const { return m_lineAdvance * px / m_pixelHeight; }
    float measure(const std::string& utf8, float px) const;
    // x y is the top left of the line box
    void draw(SpriteBatch& batch, const std::string& utf8, float x, float y, float px, uint32_t colour) const;
    void drawCentered(SpriteBatch& batch, const std::string& utf8, float cx, float y, float px, uint32_t colour) const;
    // draws inside a width cutting the text with three dots when it overflows
    void drawClipped(SpriteBatch& batch, const std::string& utf8, float x, float y, float maxWidth, float px, uint32_t colour) const;

private:
    struct Glyph {
        float x0, y0, x1, y1;
        float u0, v0, u1, v1;
        float advance;
    };
    const Glyph* glyph(uint32_t codepoint) const;

    bgfx::TextureHandle m_texture = BGFX_INVALID_HANDLE;
    std::vector<Glyph> m_glyphs;
    std::vector<uint32_t> m_codepoints;
    float m_pixelHeight = 0.f;
    float m_ascent = 0.f;
    float m_lineAdvance = 0.f;
    std::string m_face;
};

}
