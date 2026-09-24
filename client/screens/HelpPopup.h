// sub 45A220 draws help png at 153 232 closes on Escape or F1
#pragma once

#include "ui/Screen.h"

namespace KnC::Client {

class App;

class HelpPopup : public Screen {
public:
    explicit HelpPopup(App& app) : m_app(app) {}
    const char* name() const override { return "help"; }
    bool opaque() const override { return false; }
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseButton(int button, int action, float x, float y) override;

private:
    void close();
    App& m_app;
    bool m_closed = false;
};

}
