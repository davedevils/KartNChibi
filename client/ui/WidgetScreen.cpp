#include "WidgetScreen.h"

#include "app/App.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>

namespace KnC::Client {

bool WidgetScreen::loadLayout(const std::string& fileName) {
    std::string text;
    if (!m_app.loadUiFile(fileName, text)) {
        std::printf("[ui] %s not found the screen draws without a layout\n", fileName.c_str());
        return false;
    }
    if (!parseScreenLayout(text, m_layout)) {
        std::printf("[ui] %s does not parse\n", fileName.c_str());
        return false;
    }
    for (const LayoutElement& e : m_layout.elements) {
        std::unique_ptr<Widget> w = makeWidget(e, m_app.assets());
        if (w->type == ElementType::Button) {
            ButtonWidget* btn = static_cast<ButtonWidget*>(w.get());
            Widget* raw = w.get();
            // the action is read at click time a screen may rename it after the load
            btn->onClick = [this, raw]() { onAction(raw->action, *raw); };
        }
        std::printf("[ui]   %s %s at %.0f %.0f size %.0f %.0f z %d%s\n", w->id.c_str(), elementTypeName(w->type),
                    w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->zIndex, w->visible ? "" : " hidden");
        m_widgets.push_back(std::move(w));
    }
    sortWidgets();
    std::printf("[ui] %s loaded %zu elements\n", fileName.c_str(), m_widgets.size());
    return true;
}

Widget* WidgetScreen::find(const std::string& id) const {
    for (const auto& w : m_widgets) if (w->id == id) return w.get();
    return nullptr;
}

Widget* WidgetScreen::add(std::unique_ptr<Widget> widget) {
    Widget* raw = widget.get();
    if (raw->type == ElementType::Button) {
        ButtonWidget* btn = static_cast<ButtonWidget*>(raw);
        if (!btn->onClick) btn->onClick = [this, raw]() { onAction(raw->action, *raw); };
    }
    m_widgets.push_back(std::move(widget));
    sortWidgets();
    return raw;
}

void WidgetScreen::sortWidgets() {
    m_order.clear();
    for (const auto& w : m_widgets) m_order.push_back(w.get());
    std::stable_sort(m_order.begin(), m_order.end(), [](const Widget* a, const Widget* b) { return a->zIndex < b->zIndex; });
}

Widget* WidgetScreen::hit(float x, float y) const {
    for (auto it = m_order.rbegin(); it != m_order.rend(); ++it) {
        Widget* w = *it;
        if (!w->visible || !w->enabled) continue;
        if (w->type == ElementType::Image || w->type == ElementType::Panel || w->type == ElementType::Text) continue;
        if (w->rect.contains(x, y)) return w;
    }
    return nullptr;
}

void WidgetScreen::draw(SpriteBatch& batch) {
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    for (Widget* w : m_order) if (w->visible) w->draw(ctx);
    drawOverlay(ctx);
}

void WidgetScreen::focus(Widget* widget) {
    if (m_focus == widget) return;
    if (m_focus) m_focus->onFocus(false);
    m_focus = widget && widget->wantsFocus() ? widget : nullptr;
    if (m_focus) m_focus->onFocus(true);
}

void WidgetScreen::focusNext() {
    std::vector<Widget*> inputs;
    for (Widget* w : m_order) if (w->visible && w->enabled && w->wantsFocus()) inputs.push_back(w);
    if (inputs.empty()) return;
    size_t index = 0;
    for (size_t i = 0; i < inputs.size(); ++i) if (inputs[i] == m_focus) { index = i + 1; break; }
    focus(inputs[index % inputs.size()]);
}

void WidgetScreen::onKey(int key, int action, int mods) {
    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) { focusNext(); return; }
    if (m_focus) m_focus->onKey(key, action, mods);
}

void WidgetScreen::onChar(unsigned codepoint) {
    if (m_focus) m_focus->onChar(codepoint);
}

void WidgetScreen::onMouseMove(float x, float y) {
    m_mouseX = x;
    m_mouseY = y;
    for (Widget* w : m_order) {
        if (!w->visible) continue;
        w->onHover(w->enabled && w->rect.contains(x, y));
    }
}

void WidgetScreen::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    if (action == GLFW_PRESS) {
        Widget* w = hit(x, y);
        m_pressed = w;
        focus(w);
        if (w) w->onPress(x, y);
    } else if (action == GLFW_RELEASE) {
        if (m_pressed) {
            Widget* w = m_pressed;
            m_pressed = nullptr;
            const bool inside = w->rect.contains(x, y);
            if (inside && w->enabled && w->type == ElementType::Button) m_app.click();
            w->onRelease(x, y, inside);
        }
    }
}

}
