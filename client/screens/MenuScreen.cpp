#include "MenuScreen.h"

#include "app/App.h"
#include "net/Utf.h"

#include <GLFW/glfw3.h>

namespace KnC::Client {

void MenuScreen::enter() {
    loadLayout("ui_state_04_menu.json");
    m_time = 0.f;
    m_sent = false;
    m_status.clear();
}

void MenuScreen::update(float dt) {
    m_time += dt;
    if (m_app.autoLeaves("menu") && !m_sent && m_time > 0.3f) enterLobby();
}

void MenuScreen::onKey(int key, int action, int mods) {
    if (action == GLFW_PRESS && (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)) { enterLobby(); return; }
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) { m_app.quit(); return; }
    WidgetScreen::onKey(key, action, mods);
}

void MenuScreen::onAction(const std::string& action, Widget&) {
    if (action == "on_multiplayer") enterLobby();
    else if (action == "ui_close") m_app.quit();
    else m_status = "Button " + action + " is not part of this phase";
}

void MenuScreen::enterLobby() {
    if (m_sent) return;
    Session& session = m_app.session();
    if (!session.connected()) { m_status = "Not connected"; return; }
    m_sent = true;
    // on the game server after a 0x0011 the lobby opens with 0x0012 on the login server it is 0x0018
    if (session.stage() == Stage::GameServer || session.stage() == Stage::Lobby) {
        session.openLobby();
        m_status = "Sent 0x0012, waiting for the lobby";
        return;
    }
    const uint32_t stage = session.stageForProfile();
    session.requestStage(stage, session.selectedChannel());
    m_status = "Sent 0x0018 stage " + std::to_string(stage) + " channel " + std::to_string(session.selectedChannel()) + ", waiting for the game server";
}

void MenuScreen::onSession(SessionEvent event) {
    if (event == SessionEvent::LobbyAck) m_app.go("lobby");
    if (event == SessionEvent::Disconnected) m_sent = false;
}

// the stock menu stage draws the wallpaper the base plate and its two buttons and no text
void MenuScreen::drawOverlay(DrawContext& ctx) {
    if (!m_status.empty()) ctx.font.draw(ctx.batch, m_status, 12.f, m_app.canvasHeight() - 24.f, 12.f, rgba(255, 255, 255, 180));
}

}
