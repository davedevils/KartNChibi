#include "FontAtlas.h"

#include "net/Utf.h"

#include <cstdio>
#include <cstring>
#include <fstream>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4244 4505)
#endif
#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "roboto_regular.ttf.h"

namespace KnC::Client {

namespace {

constexpr int kAtlasSize = 1024;
constexpr int kOversample = 2;
// two ranges ascii and latin one the wire names outside them draw a question mark
constexpr int kRangeA0 = 32, kRangeA1 = 126;
constexpr int kRangeB0 = 160, kRangeB1 = 255;

}

// the whole file or empty when it cannot be read
static std::vector<unsigned char> readFile(const std::string& path) {
    std::vector<unsigned char> bytes;
    if (path.empty()) return bytes;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return bytes;
    const std::streamoff size = file.tellg();
    if (size <= 0) return bytes;
    bytes.resize(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    return bytes;
}

bool FontAtlas::init(float pixelHeight, const std::string& ttfPath) {
    m_pixelHeight = pixelHeight;
    // the stock client draws its text through GDI Arial the windows file gives the same face
    std::vector<unsigned char> file = readFile(ttfPath);
    const unsigned char* ttf = file.empty() ? s_robotoRegularTtf : file.data();
    m_face = file.empty() ? "Roboto" : ttfPath;
    std::vector<uint8_t> bitmap(static_cast<size_t>(kAtlasSize) * kAtlasSize, 0);
    stbtt_pack_context pack;
    if (!stbtt_PackBegin(&pack, bitmap.data(), kAtlasSize, kAtlasSize, 0, 1, nullptr)) return false;
    stbtt_PackSetOversampling(&pack, kOversample, kOversample);
    std::vector<stbtt_packedchar> charsA(kRangeA1 - kRangeA0 + 1);
    std::vector<stbtt_packedchar> charsB(kRangeB1 - kRangeB0 + 1);
    stbtt_pack_range ranges[2];
    ranges[0].font_size = pixelHeight;
    ranges[0].first_unicode_codepoint_in_range = kRangeA0;
    ranges[0].array_of_unicode_codepoints = nullptr;
    ranges[0].num_chars = kRangeA1 - kRangeA0 + 1;
    ranges[0].chardata_for_range = charsA.data();
    ranges[1].font_size = pixelHeight;
    ranges[1].first_unicode_codepoint_in_range = kRangeB0;
    ranges[1].array_of_unicode_codepoints = nullptr;
    ranges[1].num_chars = kRangeB1 - kRangeB0 + 1;
    ranges[1].chardata_for_range = charsB.data();
    const int packed = stbtt_PackFontRanges(&pack, ttf, 0, ranges, 2);
    stbtt_PackEnd(&pack);
    if (!packed) {
        std::fprintf(stderr, "[font] pack failed\n");
        return false;
    }

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) return false;
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    const float scale = stbtt_ScaleForPixelHeight(&info, pixelHeight);
    m_ascent = ascent * scale;
    m_lineAdvance = (ascent - descent + lineGap) * scale;

    auto take = [&](int first, const std::vector<stbtt_packedchar>& chars) {
        for (size_t i = 0; i < chars.size(); ++i) {
            const stbtt_packedchar& c = chars[i];
            Glyph g;
            g.x0 = c.xoff; g.y0 = c.yoff; g.x1 = c.xoff2; g.y1 = c.yoff2;
            g.u0 = static_cast<float>(c.x0) / kAtlasSize;
            g.v0 = static_cast<float>(c.y0) / kAtlasSize;
            g.u1 = static_cast<float>(c.x1) / kAtlasSize;
            g.v1 = static_cast<float>(c.y1) / kAtlasSize;
            g.advance = c.xadvance;
            m_glyphs.push_back(g);
            m_codepoints.push_back(static_cast<uint32_t>(first + static_cast<int>(i)));
        }
    };
    take(kRangeA0, charsA);
    take(kRangeB0, charsB);

    std::vector<uint8_t> rgba(bitmap.size() * 4);
    for (size_t i = 0; i < bitmap.size(); ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = bitmap[i];
    }
    m_texture = bgfx::createTexture2D(kAtlasSize, kAtlasSize, false, 1, bgfx::TextureFormat::RGBA8,
                                      BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
                                      bgfx::copy(rgba.data(), static_cast<uint32_t>(rgba.size())));
    if (bgfx::isValid(m_texture)) bgfx::setName(m_texture, "font atlas");
    return bgfx::isValid(m_texture);
}

void FontAtlas::shutdown() {
    if (bgfx::isValid(m_texture)) bgfx::destroy(m_texture);
    m_texture = BGFX_INVALID_HANDLE;
    m_glyphs.clear();
    m_codepoints.clear();
}

const FontAtlas::Glyph* FontAtlas::glyph(uint32_t codepoint) const {
    if (codepoint >= static_cast<uint32_t>(kRangeA0) && codepoint <= static_cast<uint32_t>(kRangeA1))
        return &m_glyphs[codepoint - kRangeA0];
    if (codepoint >= static_cast<uint32_t>(kRangeB0) && codepoint <= static_cast<uint32_t>(kRangeB1))
        return &m_glyphs[(kRangeA1 - kRangeA0 + 1) + (codepoint - kRangeB0)];
    return &m_glyphs['?' - kRangeA0];
}

float FontAtlas::measure(const std::string& utf8, float px) const {
    if (m_glyphs.empty()) return 0.f;
    const float k = px / m_pixelHeight;
    float width = 0.f;
    size_t i = 0;
    while (i < utf8.size()) width += glyph(nextUtf8(utf8, i))->advance * k;
    return width;
}

void FontAtlas::draw(SpriteBatch& batch, const std::string& utf8, float x, float y, float px, uint32_t colour) const {
    if (m_glyphs.empty() || !bgfx::isValid(m_texture)) return;
    const float k = px / m_pixelHeight;
    float pen = x;
    const float baseline = y + m_ascent * k;
    size_t i = 0;
    while (i < utf8.size()) {
        const Glyph* g = glyph(nextUtf8(utf8, i));
        const float gx = pen + g->x0 * k;
        const float gy = baseline + g->y0 * k;
        batch.draw(m_texture, gx, gy, (g->x1 - g->x0) * k, (g->y1 - g->y0) * k, colour, g->u0, g->v0, g->u1, g->v1);
        pen += g->advance * k;
    }
}

void FontAtlas::drawCentered(SpriteBatch& batch, const std::string& utf8, float cx, float y, float px, uint32_t colour) const {
    draw(batch, utf8, cx - measure(utf8, px) * 0.5f, y, px, colour);
}

void FontAtlas::drawClipped(SpriteBatch& batch, const std::string& utf8, float x, float y, float maxWidth, float px,
                            uint32_t colour) const {
    if (measure(utf8, px) <= maxWidth) {
        draw(batch, utf8, x, y, px, colour);
        return;
    }
    const float dots = measure("...", px);
    std::string cut;
    size_t i = 0;
    float width = 0.f;
    const float k = px / m_pixelHeight;
    while (i < utf8.size()) {
        const size_t start = i;
        const float advance = glyph(nextUtf8(utf8, i))->advance * k;
        if (width + advance + dots > maxWidth) break;
        width += advance;
        cut.append(utf8, start, i - start);
    }
    draw(batch, cut + "...", x, y, px, colour);
}

}
