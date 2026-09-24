#include "Widgets.h"

#include "assets/AssetStore.h"
#include "net/Utf.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>

namespace KnC::Client {

namespace {

constexpr uint32_t kFrame = rgba(255, 255, 255, 220);
constexpr uint32_t kInputBack = rgba(20, 24, 40, 190);
constexpr uint32_t kInputFocus = rgba(255, 220, 90, 255);
constexpr uint32_t kPlaceholder = rgba(200, 200, 200, 160);
constexpr uint32_t kListBack = rgba(40, 40, 50, 200);
constexpr uint32_t kListSelect = rgba(80, 130, 220, 200);

void frame(SpriteBatch& batch, const Rect& r, uint32_t colour, float t = 1.f) {
    batch.fill(r.x, r.y, r.w, t, colour);
    batch.fill(r.x, r.y + r.h - t, r.w, t, colour);
    batch.fill(r.x, r.y, t, r.h, colour);
    batch.fill(r.x + r.w - t, r.y, t, r.h, colour);
}

void drawTexture(SpriteBatch& batch, const Texture* tex, const Rect& r, uint32_t tint) {
    if (tex && tex->valid()) batch.draw(tex->handle, r.x, r.y, r.w, r.h, tint);
}

}

// a missing image draws nothing the box it would fill is what the stock art keeps transparent
void ImageWidget::draw(DrawContext& ctx) {
    if (texture && texture->valid()) drawTexture(ctx.batch, texture, rect, tint);
}

void ButtonWidget::draw(DrawContext& ctx) {
    const Texture* tex = normal;
    if (!enabled && disabled && disabled->valid()) tex = disabled;
    else if (state == State::Pressed && pressed && pressed->valid()) tex = pressed;
    else if ((state == State::Hover || active) && hover && hover->valid()) tex = hover;
    if (tex && tex->valid()) {
        drawTexture(ctx.batch, tex, rect, enabled ? kWhite : rgba(160, 160, 160, 255));
    } else if (plain) {
        const uint32_t fill = !enabled ? rgba(120, 120, 130, 255) : state == State::Pressed ? rgba(40, 90, 170, 255)
                            : state == State::Hover ? rgba(90, 150, 230, 255) : rgba(60, 120, 200, 255);
        ctx.batch.fill(rect.x, rect.y + 2.f, rect.w, rect.h - 2.f, rgba(20, 40, 80, 255));
        ctx.batch.fill(rect.x + 1.f, rect.y, rect.w - 2.f, rect.h - 3.f, fill);
    } else {
        const uint32_t fill = state == State::Pressed ? rgba(70, 70, 120, 255)
                            : state == State::Hover ? rgba(120, 120, 180, 255) : rgba(100, 100, 150, 255);
        ctx.batch.fill(rect.x, rect.y, rect.w, rect.h, fill);
        frame(ctx.batch, rect, rgba(253, 249, 0, 255), 2.f);
    }
    if (!label.empty())
        ctx.font.drawCentered(ctx.batch, label, rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f - 8.f, 15.f, kWhite);
}

void ButtonWidget::onHover(bool inside) {
    if (state == State::Pressed) return;
    state = inside ? State::Hover : State::Normal;
}

void ButtonWidget::onPress(float, float) {
    if (enabled) state = State::Pressed;
}

void ButtonWidget::onRelease(float, float, bool inside) {
    const bool clicked = state == State::Pressed && inside && enabled;
    state = inside ? State::Hover : State::Normal;
    if (clicked && onClick) onClick();
}

void TextWidget::draw(DrawContext& ctx) {
    if (centered) ctx.font.drawCentered(ctx.batch, text, rect.x + rect.w * 0.5f, rect.y, px, colour);
    else ctx.font.draw(ctx.batch, text, rect.x, rect.y, px, colour);
}

void InputWidget::draw(DrawContext& ctx) {
    if (!bare) {
        ctx.batch.fill(rect.x, rect.y, rect.w, rect.h, kInputBack);
        frame(ctx.batch, rect, focused ? kInputFocus : kFrame, 1.f);
    }
    const float ty = rect.y + (rect.h - ctx.font.lineHeight(px)) * 0.5f;
    if (text.empty() && !focused) {
        if (!bare) ctx.font.drawClipped(ctx.batch, placeholder, rect.x + 4.f, ty, rect.w - 8.f, px, kPlaceholder);
        return;
    }
    std::string shown = text;
    if (password) {
        shown.clear();
        size_t i = 0;
        while (i < text.size()) { nextUtf8(text, i); shown += '*'; }
    }
    ctx.batch.setClip(rect.x + 2.f, rect.y, rect.w - 4.f, rect.h);
    float width = ctx.font.measure(shown, px);
    float tx = rect.x + 4.f;
    if (width > rect.w - 8.f) tx = rect.x + rect.w - 4.f - width;
    ctx.font.draw(ctx.batch, shown, tx, ty, px, ink);
    if (focused) ctx.batch.fill(tx + width + 1.f, ty + 1.f, 1.f, ctx.font.lineHeight(px) - 2.f, ink);
    ctx.batch.clearClip();
}

void InputWidget::onKey(int key, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (key == GLFW_KEY_BACKSPACE && !text.empty()) {
        // step back over one utf8 code point
        size_t n = text.size();
        do { --n; } while (n > 0 && (static_cast<unsigned char>(text[n]) & 0xC0) == 0x80);
        text.resize(n);
    } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
        if (onSubmit) onSubmit();
    }
}

void InputWidget::onChar(unsigned codepoint) {
    if (codepoint < 32) return;
    size_t count = 0;
    size_t i = 0;
    while (i < text.size()) { nextUtf8(text, i); ++count; }
    if (static_cast<int>(count) >= maxLength) return;
    appendUtf8(text, codepoint);
}

void ListWidget::draw(DrawContext& ctx) {
    if (drawFrame) {
        ctx.batch.fill(rect.x, rect.y, rect.w, rect.h, kListBack);
        frame(ctx.batch, rect, rgba(100, 100, 150, 255), 1.f);
    }
    ctx.batch.setClip(rect.x, rect.y, rect.w, rect.h);
    const int shown = visibleRows();
    for (int i = 0; i < shown; ++i) {
        const int row = scroll + i;
        if (row >= static_cast<int>(rows.size())) break;
        const float y = rect.y + i * rowHeight;
        if (row == selected) ctx.batch.fill(rect.x + 1.f, y, rect.w - 2.f, rowHeight, kListSelect);
        ctx.font.drawClipped(ctx.batch, rows[row], rect.x + 6.f, y + (rowHeight - ctx.font.lineHeight(px)) * 0.5f,
                             rect.w - 12.f, px, kWhite);
    }
    ctx.batch.clearClip();
}

void ListWidget::onPress(float, float y) {
    if (rowHeight <= 0.f) return;
    const int row = scroll + static_cast<int>((y - rect.y) / rowHeight);
    if (row < 0 || row >= static_cast<int>(rows.size())) return;
    selected = row;
    if (onSelect) onSelect(row);
}

void PanelWidget::draw(DrawContext& ctx) {
    ctx.batch.fill(rect.x, rect.y, rect.w, rect.h, colour);
}

void CheckboxWidget::draw(DrawContext& ctx) {
    if (texture && texture->valid()) {
        drawTexture(ctx.batch, texture, rect, kWhite);
    } else {
        ctx.batch.fill(rect.x, rect.y, rect.w, rect.h, rgba(80, 80, 100, 255));
        frame(ctx.batch, rect, kWhite, 1.f);
    }
    if (checked) {
        ctx.batch.fill(rect.x + 4.f, rect.y + rect.h * 0.5f - 2.f, rect.w - 8.f, 4.f, rgba(0, 228, 48, 255));
    }
}

void CheckboxWidget::onRelease(float, float, bool inside) {
    if (inside && enabled) checked = !checked;
}

std::unique_ptr<Widget> makeWidget(const LayoutElement& e, AssetStore& assets) {
    std::unique_ptr<Widget> w;
    const Texture* main = e.asset.empty() ? nullptr : assets.texture(e.asset);
    switch (e.type) {
    case ElementType::Image: {
        auto img = std::make_unique<ImageWidget>();
        img->texture = main;
        w = std::move(img);
        break;
    }
    case ElementType::Button: {
        auto btn = std::make_unique<ButtonWidget>();
        btn->normal = main;
        btn->hover = e.hoverAsset.empty() ? nullptr : assets.texture(e.hoverAsset);
        btn->pressed = e.pressedAsset.empty() ? nullptr : assets.texture(e.pressedAsset);
        btn->disabled = e.disabledAsset.empty() ? nullptr : assets.texture(e.disabledAsset);
        w = std::move(btn);
        break;
    }
    case ElementType::Text: {
        auto txt = std::make_unique<TextWidget>();
        txt->text = e.text;
        txt->px = static_cast<float>(e.fontSize);
        txt->colour = rgba(e.colour[0], e.colour[1], e.colour[2], e.colour[3]);
        w = std::move(txt);
        break;
    }
    case ElementType::Input: {
        auto in = std::make_unique<InputWidget>();
        in->placeholder = e.placeholder;
        in->maxLength = e.maxLength;
        in->password = e.password;
        w = std::move(in);
        break;
    }
    case ElementType::List: {
        w = std::make_unique<ListWidget>();
        break;
    }
    case ElementType::Panel: {
        w = std::make_unique<PanelWidget>();
        break;
    }
    case ElementType::Checkbox: {
        auto cb = std::make_unique<CheckboxWidget>();
        cb->texture = main;
        cb->checked = e.checked;
        w = std::move(cb);
        break;
    }
    }
    w->type = e.type;
    w->id = e.id;
    w->action = e.action;
    w->visible = e.visible;
    w->enabled = e.enabled;
    w->zIndex = e.zIndex;
    w->rect.x = e.x;
    w->rect.y = e.y;
    if (e.hasSize) {
        w->rect.w = e.width;
        w->rect.h = e.height;
    } else if (main && main->valid()) {
        w->rect.w = static_cast<float>(main->width);
        w->rect.h = static_cast<float>(main->height);
    } else {
        w->rect.w = e.type == ElementType::Input ? 184.f : 100.f;
        w->rect.h = e.type == ElementType::Input ? 20.f : 30.f;
    }
    return w;
}

}
