#include "SpriteBatch.h"

#include <bgfx/embedded_shader.h>
#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "vs_ocornut_imgui.bin.h"
#include "fs_ocornut_imgui.bin.h"

namespace KnC::Client {

namespace {

// the bgfx imgui shaders draw exactly this vertex position uv and colour times a texture
const bgfx::EmbeddedShader kShaders[] = {
    BGFX_EMBEDDED_SHADER(vs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER_END()
};

constexpr uint32_t kMaxVertices = 60000;

}

bool SpriteBatch::init() {
    const bgfx::RendererType::Enum type = bgfx::getRendererType();
    m_program = bgfx::createProgram(bgfx::createEmbeddedShader(kShaders, type, "vs_ocornut_imgui"),
                                    bgfx::createEmbeddedShader(kShaders, type, "fs_ocornut_imgui"), true);
    if (!bgfx::isValid(m_program)) {
        std::fprintf(stderr, "[sprite] embedded shader failed for %s\n", bgfx::getRendererName(type));
        return false;
    }
    m_sampler = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
    m_layout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();
    return true;
}

void SpriteBatch::shutdown() {
    if (bgfx::isValid(m_sampler)) bgfx::destroy(m_sampler);
    if (bgfx::isValid(m_program)) bgfx::destroy(m_program);
    m_sampler = BGFX_INVALID_HANDLE;
    m_program = BGFX_INVALID_HANDLE;
}

void SpriteBatch::begin(bgfx::ViewId view, uint16_t fbWidth, uint16_t fbHeight, float canvasWidth, float canvasHeight) {
    m_view = view;
    m_fbWidth = fbWidth;
    m_fbHeight = fbHeight;
    m_vertices.clear();
    m_runs.clear();
    m_clip = Clip();
    m_scale = std::min(static_cast<float>(fbWidth) / canvasWidth, static_cast<float>(fbHeight) / canvasHeight);
    m_scaleX = m_stretch ? static_cast<float>(fbWidth) / canvasWidth : m_scale;
    m_scaleY = m_stretch ? static_cast<float>(fbHeight) / canvasHeight : m_scale;
    const float viewW = canvasWidth * m_scaleX;
    const float viewH = canvasHeight * m_scaleY;
    m_offsetX = std::floor((static_cast<float>(fbWidth) - viewW) * 0.5f);
    m_offsetY = std::floor((static_cast<float>(fbHeight) - viewH) * 0.5f);

    const bgfx::Caps* caps = bgfx::getCaps();
    float ortho[16];
    bx::mtxOrtho(ortho, 0.f, canvasWidth, canvasHeight, 0.f, 0.f, 1000.f, 0.f, caps->homogeneousDepth);
    bgfx::setViewName(view, "sprites");
    bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
    bgfx::setViewTransform(view, nullptr, ortho);
    bgfx::setViewRect(view, static_cast<uint16_t>(m_offsetX), static_cast<uint16_t>(m_offsetY),
                      static_cast<uint16_t>(viewW), static_cast<uint16_t>(viewH));
}

void SpriteBatch::setClip(float x, float y, float w, float h) {
    m_clip.on = true;
    m_clip.x = x; m_clip.y = y; m_clip.w = w; m_clip.h = h;
}

void SpriteBatch::clearClip() { m_clip = Clip(); }

void SpriteBatch::setHole(float x, float y, float w, float h) {
    m_hole.on = true;
    m_hole.x = x; m_hole.y = y; m_hole.w = w; m_hole.h = h;
}

void SpriteBatch::clearHole() { m_hole = Clip(); }

bool SpriteBatch::beginQuad(bgfx::TextureHandle texture) {
    if (!bgfx::isValid(texture)) return false;
    if (m_vertices.size() + 4 > kMaxVertices) return false;
    if (m_runs.empty() || m_runs.back().texture.idx != texture.idx || !(m_runs.back().clip == m_clip) ||
        !(m_runs.back().hole == m_hole)) {
        Run run;
        run.texture = texture;
        run.clip = m_clip;
        run.hole = m_hole;
        run.firstVertex = static_cast<uint32_t>(m_vertices.size());
        run.vertexCount = 0;
        m_runs.push_back(run);
    }
    return true;
}

void SpriteBatch::draw(bgfx::TextureHandle texture, float x, float y, float w, float h, uint32_t colour,
                       float u0, float v0, float u1, float v1) {
    if (!beginQuad(texture)) return;
    m_vertices.push_back({x, y, u0, v0, colour});
    m_vertices.push_back({x + w, y, u1, v0, colour});
    m_vertices.push_back({x + w, y + h, u1, v1, colour});
    m_vertices.push_back({x, y + h, u0, v1, colour});
    m_runs.back().vertexCount += 4;
}

void SpriteBatch::drawRotated(bgfx::TextureHandle texture, float x, float y, float pivotX, float pivotY, float w,
                              float h, float angle, uint32_t colour) {
    if (!beginQuad(texture)) return;
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    // the corners relative to the pivot then turned y down means a positive angle turns clockwise
    const float cx[4] = {-pivotX, w - pivotX, w - pivotX, -pivotX};
    const float cy[4] = {-pivotY, -pivotY, h - pivotY, h - pivotY};
    const float uu[4] = {0.f, 1.f, 1.f, 0.f};
    const float vv[4] = {0.f, 0.f, 1.f, 1.f};
    for (int i = 0; i < 4; ++i)
        m_vertices.push_back({x + cx[i] * c - cy[i] * s, y + cx[i] * s + cy[i] * c, uu[i], vv[i], colour});
    m_runs.back().vertexCount += 4;
}

void SpriteBatch::drawQuad(bgfx::TextureHandle texture, const float* xy, const float* uv, uint32_t colour) {
    if (!beginQuad(texture)) return;
    for (int i = 0; i < 4; ++i) m_vertices.push_back({xy[i * 2], xy[i * 2 + 1], uv[i * 2], uv[i * 2 + 1], colour});
    m_runs.back().vertexCount += 4;
}

void SpriteBatch::fill(float x, float y, float w, float h, uint32_t colour) {
    draw(m_white, x, y, w, h, colour, 0.5f, 0.5f, 0.5f, 0.5f);
}

void SpriteBatch::end() {
    m_lastSubmits = 0;
    if (m_vertices.empty()) {
        bgfx::touch(m_view);
        return;
    }
    const uint32_t vertexCount = static_cast<uint32_t>(m_vertices.size());
    const uint32_t indexCount = vertexCount / 4 * 6;
    if (bgfx::getAvailTransientVertexBuffer(vertexCount, m_layout) < vertexCount ||
        bgfx::getAvailTransientIndexBuffer(indexCount) < indexCount) {
        std::fprintf(stderr, "[sprite] transient buffers full %u vertices dropped\n", vertexCount);
        bgfx::touch(m_view);
        return;
    }
    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer tib;
    bgfx::allocTransientVertexBuffer(&tvb, vertexCount, m_layout);
    bgfx::allocTransientIndexBuffer(&tib, indexCount, false);
    std::memcpy(tvb.data, m_vertices.data(), vertexCount * sizeof(Vertex));
    // each run submits with its first vertex as the base so its indices start at zero
    uint16_t* indices = reinterpret_cast<uint16_t*>(tib.data);
    for (const Run& run : m_runs) {
        for (uint32_t q = 0; q < run.vertexCount / 4; ++q) {
            const uint32_t at = (run.firstVertex / 4 + q) * 6;
            const uint16_t base = static_cast<uint16_t>(q * 4);
            indices[at + 0] = base;
            indices[at + 1] = static_cast<uint16_t>(base + 1);
            indices[at + 2] = static_cast<uint16_t>(base + 2);
            indices[at + 3] = base;
            indices[at + 4] = static_cast<uint16_t>(base + 2);
            indices[at + 5] = static_cast<uint16_t>(base + 3);
        }
    }

    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
    bgfx::Encoder* encoder = bgfx::begin();
    for (const Run& run : m_runs) {
        // the clip of the run in framebuffer pixels the whole target without one
        float x0 = 0.f, y0 = 0.f, x1 = static_cast<float>(m_fbWidth), y1 = static_cast<float>(m_fbHeight);
        if (run.clip.on) {
            x0 = std::max(m_offsetX + run.clip.x * m_scaleX, 0.f);
            y0 = std::max(m_offsetY + run.clip.y * m_scaleY, 0.f);
            x1 = std::min(m_offsetX + (run.clip.x + run.clip.w) * m_scaleX, x1);
            y1 = std::min(m_offsetY + (run.clip.y + run.clip.h) * m_scaleY, y1);
            if (x1 <= x0 || y1 <= y0) continue;
        }
        // a hole cuts the clip into the four strips around it each strip is one scissor and one submit
        float strips[4][4];
        int stripCount = 0;
        if (run.hole.on) {
            const float hx0 = m_offsetX + run.hole.x * m_scaleX;
            const float hy0 = m_offsetY + run.hole.y * m_scaleY;
            const float hx1 = hx0 + run.hole.w * m_scaleX;
            const float hy1 = hy0 + run.hole.h * m_scaleY;
            const float rows[2][2] = {{y0, std::min(hy0, y1)}, {std::max(hy1, y0), y1}};
            for (const float* row : rows)
                if (row[1] > row[0]) { strips[stripCount][0] = x0; strips[stripCount][1] = row[0]; strips[stripCount][2] = x1; strips[stripCount][3] = row[1]; ++stripCount; }
            const float midY0 = std::max(hy0, y0), midY1 = std::min(hy1, y1);
            if (midY1 > midY0) {
                const float cols[2][2] = {{x0, std::min(hx0, x1)}, {std::max(hx1, x0), x1}};
                for (const float* col : cols)
                    if (col[1] > col[0]) { strips[stripCount][0] = col[0]; strips[stripCount][1] = midY0; strips[stripCount][2] = col[1]; strips[stripCount][3] = midY1; ++stripCount; }
            }
            if (stripCount == 0) continue;
        } else {
            strips[0][0] = x0; strips[0][1] = y0; strips[0][2] = x1; strips[0][3] = y1;
            stripCount = 1;
        }
        for (int i = 0; i < stripCount; ++i) {
            const float* r = strips[i];
            if (run.clip.on || run.hole.on)
                encoder->setScissor(static_cast<uint16_t>(r[0]), static_cast<uint16_t>(r[1]),
                                    static_cast<uint16_t>(r[2] - r[0]), static_cast<uint16_t>(r[3] - r[1]));
            encoder->setState(state);
            encoder->setTexture(0, m_sampler, run.texture);
            encoder->setVertexBuffer(0, &tvb, run.firstVertex, run.vertexCount);
            encoder->setIndexBuffer(&tib, run.firstVertex / 4 * 6, run.vertexCount / 4 * 6);
            encoder->submit(m_view, m_program);
            ++m_lastSubmits;
        }
    }
    bgfx::end(encoder);
}

void SpriteBatch::toCanvas(double px, double py, float& cx, float& cy) const {
    cx = (static_cast<float>(px) - m_offsetX) / m_scaleX;
    cy = (static_cast<float>(py) - m_offsetY) / m_scaleY;
}

}
