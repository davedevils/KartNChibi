// a screen built from one ui state JSON file with hit testing and focus
#pragma once

#include "Screen.h"
#include "UiJson.h"
#include "Widgets.h"

#include <memory>
#include <string>
#include <vector>

namespace KnC::Client {

class App;

class WidgetScreen : public Screen {
public:
    explicit WidgetScreen(App& app) : m_app(app) {}

    // loads a ui state file through the app search path and builds the widgets
    bool loadLayout(const std::string& fileName);
    Widget* find(const std::string& id) const;
    template <class T> T* findAs(const std::string& id) const { return dynamic_cast<T*>(find(id)); }
    // adds a widget the JSON does not name the screen owns it too
    Widget* add(std::unique_ptr<Widget> widget);

    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onChar(unsigned codepoint) override;
    void onMouseMove(float x, float y) override;
    void onMouseButton(int button, int action, float x, float y) override;

    void focus(Widget* widget);
    void focusNext();

protected:
    // a button with an action was clicked
    virtual void onAction(const std::string& action, Widget& source) { (void)action; (void)source; }
    // drawn after the widgets for the dynamic parts of a screen
    virtual void drawOverlay(DrawContext& ctx) { (void)ctx; }
    Widget* hit(float x, float y) const;
    void sortWidgets();

    App& m_app;
    ScreenLayout m_layout;
    std::vector<std::unique_ptr<Widget>> m_widgets;
    std::vector<Widget*> m_order;
    Widget* m_focus = nullptr;
    Widget* m_pressed = nullptr;
    float m_mouseX = 0.f;
    float m_mouseY = 0.f;
};

}
