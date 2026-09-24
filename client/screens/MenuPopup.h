// the escape menu of the lobby stages Menu Back with help settings end and close
#pragma once

#include "ui/Screen.h"

namespace KnC::Client {

class App;

class MenuPopup : public Screen {
public:
    explicit MenuPopup(App& app) : m_app(app) {}
    const char* name() const override { return "escmenu"; }
    bool opaque() const override { return false; }
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseMove(float x, float y) override;
    void onMouseButton(int button, int action, float x, float y) override;

private:
    void close();
    App& m_app;
    float m_mouseX = 0.f;
    float m_mouseY = 0.f;
    bool m_closed = false;
};

}
