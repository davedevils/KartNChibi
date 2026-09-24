// the pendant box of the char panel sub 46F0F0 the grid of 0x0119 and the detail with Install and Remove
#pragma once

#include "ui/Screen.h"

#include <cstdint>

namespace KnC::Client {

class App;

class PendantPopup : public Screen {
public:
    explicit PendantPopup(App& app);
    const char* name() const override { return m_detail >= 0 ? "pendantinfo" : "pendant"; }
    bool opaque() const override { return false; }
    void enter() override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseMove(float x, float y) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;

private:
    // sub 46EDB0 the cell under the point counting hidden rows as the stock does minus one when none
    int cellAt(float x, float y) const;
    void close();
    App& m_app;
    // the definition index the detail shows minus one while the grid is up
    int m_detail = -1;
    float m_mouseX = 0.f;
    float m_mouseY = 0.f;
    bool m_closed = false;
};

}
