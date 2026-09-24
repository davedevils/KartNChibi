// the login screen two inputs the OK sends 0x00FA then 0x0007
#pragma once

#include "ui/WidgetScreen.h"

#include <string>

namespace KnC::Client {

class LoginScreen : public WidgetScreen {
public:
    explicit LoginScreen(App& app) : WidgetScreen(app) {}
    const char* name() const override { return "login"; }
    void enter() override;
    void update(float dt) override;
    void onKey(int key, int action, int mods) override;
    void onSession(SessionEvent event) override;

protected:
    void onAction(const std::string& action, Widget& source) override;
    void drawOverlay(DrawContext& ctx) override;

private:
    void doLogin();
    float m_time = 0.f;
    bool m_sent = false;
    std::string m_status;
    // login01 the plain wallpaper login02 the same wallpaper with the login box of sub 427190
    const Texture* m_plain = nullptr;
    const Texture* m_boxed = nullptr;
};

}
