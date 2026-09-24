// the menu screen the multi button sends 0x0018 the lobby ack moves on
#pragma once

#include "ui/WidgetScreen.h"

#include <string>

namespace KnC::Client {

class MenuScreen : public WidgetScreen {
public:
    explicit MenuScreen(App& app) : WidgetScreen(app) {}
    const char* name() const override { return "menu"; }
    void enter() override;
    void update(float dt) override;
    void onKey(int key, int action, int mods) override;
    void onSession(SessionEvent event) override;

protected:
    void onAction(const std::string& action, Widget& source) override;
    void drawOverlay(DrawContext& ctx) override;

private:
    void enterLobby();
    float m_time = 0.f;
    bool m_sent = false;
    std::string m_status;
};

}
