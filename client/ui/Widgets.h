// the widgets the JSON names image button text input list panel checkbox
#pragma once

#include "FontAtlas.h"
#include "SpriteBatch.h"
#include "UiJson.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace KnC::Client {

class AssetStore;
struct Texture;

struct Rect {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

// what every widget draws with the bold face is the stock weight 600 and 700 text
struct DrawContext {
    SpriteBatch& batch;
    const FontAtlas& font;
    const FontAtlas& bold = font;
};

class Widget {
public:
    virtual ~Widget() = default;
    virtual void draw(DrawContext& ctx) = 0;
    virtual bool wantsFocus() const { return false; }
    virtual void onHover(bool inside) { (void)inside; }
    virtual void onPress(float x, float y) { (void)x; (void)y; }
    // release inside the rect counts as a click
    virtual void onRelease(float x, float y, bool inside) { (void)x; (void)y; (void)inside; }
    virtual void onFocus(bool focused) { (void)focused; }
    virtual void onKey(int key, int action, int mods) { (void)key; (void)action; (void)mods; }
    virtual void onChar(unsigned codepoint) { (void)codepoint; }

    ElementType type = ElementType::Image;
    std::string id;
    std::string action;
    Rect rect;
    bool visible = true;
    bool enabled = true;
    int zIndex = 0;
};

class ImageWidget : public Widget {
public:
    const Texture* texture = nullptr;
    uint32_t tint = kWhite;
    void draw(DrawContext& ctx) override;
};

class ButtonWidget : public Widget {
public:
    enum class State { Normal, Hover, Pressed };
    const Texture* normal = nullptr;
    const Texture* hover = nullptr;
    const Texture* pressed = nullptr;
    const Texture* disabled = nullptr;
    std::string label;
    // a labelled bar button with no art of its own drawn in the bottom bar style
    bool plain = false;
    // an open tab rests on its 01 art as the stock category tabs do
    bool active = false;
    std::function<void()> onClick;
    State state = State::Normal;
    void draw(DrawContext& ctx) override;
    void onHover(bool inside) override;
    void onPress(float x, float y) override;
    void onRelease(float x, float y, bool inside) override;
};

class TextWidget : public Widget {
public:
    std::string text;
    float px = 16.f;
    uint32_t colour = kWhite;
    bool centered = false;
    void draw(DrawContext& ctx) override;
};

class InputWidget : public Widget {
public:
    std::string text;
    std::string placeholder;
    int maxLength = 64;
    bool password = false;
    bool focused = false;
    float px = 14.f;
    // the stock inputs sit on a box the art already draws bare skips our fill and frame
    bool bare = false;
    uint32_t ink = kWhite;
    std::function<void()> onSubmit;
    void draw(DrawContext& ctx) override;
    bool wantsFocus() const override { return true; }
    void onFocus(bool f) override { focused = f; }
    void onKey(int key, int action, int mods) override;
    void onChar(unsigned codepoint) override;
};

class ListWidget : public Widget {
public:
    std::vector<std::string> rows;
    float rowHeight = 22.f;
    float px = 14.f;
    int selected = -1;
    int scroll = 0;
    bool drawFrame = true;
    std::function<void(int)> onSelect;
    void draw(DrawContext& ctx) override;
    void onPress(float x, float y) override;
    int visibleRows() const { return rowHeight > 0.f ? static_cast<int>(rect.h / rowHeight) : 0; }
};

class PanelWidget : public Widget {
public:
    uint32_t colour = rgba(50, 50, 60, 200);
    void draw(DrawContext& ctx) override;
};

class CheckboxWidget : public Widget {
public:
    const Texture* texture = nullptr;
    bool checked = false;
    void draw(DrawContext& ctx) override;
    void onRelease(float x, float y, bool inside) override;
};

// builds one widget from a layout element the textures come from the store
std::unique_ptr<Widget> makeWidget(const LayoutElement& element, AssetStore& assets);

}
