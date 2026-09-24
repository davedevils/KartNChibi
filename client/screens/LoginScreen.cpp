#include "LoginScreen.h"

#include "app/App.h"

#include <GLFW/glfw3.h>

#include <cstdio>

namespace KnC::Client {

void LoginScreen::enter() {
    loadLayout("ui_state_02_login.json");
    m_time = 0.f;
    m_sent = false;
    m_status.clear();
    // sub 427190 loads both plates sub 427040 draws login02 with the box and login01 under a popup
    m_plain = m_app.assets().texture("Login/Login01.png");
    m_boxed = m_app.assets().texture("Login/Login02.png");
    if (ImageWidget* back = findAs<ImageWidget>("bg_layer1")) {
        if (m_boxed) back->texture = m_boxed;
        back->rect = {0.f, 0.f, m_app.canvasWidth(), m_app.canvasHeight()};
    }
    InputWidget* user = findAs<InputWidget>("input_username");
    InputWidget* pass = findAs<InputWidget>("input_password");
    // the stock inputs are font 9 Arial 19 in the blue 0E82E7 on the boxes the login art draws
    for (InputWidget* in : {user, pass}) {
        if (!in) continue;
        in->bare = true;
        in->px = 19.f;
        in->ink = rgba(14, 130, 231, 255);
    }
    if (user) {
        user->text = m_app.options().user;
        user->onSubmit = [this]() { doLogin(); };
    }
    if (pass) {
        pass->text = m_app.options().pass;
        pass->onSubmit = [this]() { doLogin(); };
    }
    if (user && user->text.empty()) focus(user);
    else if (pass) focus(pass);
}

void LoginScreen::update(float dt) {
    m_time += dt;
    // sub 427040 the box plate goes away while a message box is up and the inputs go with it
    const bool covered = m_app.topScreen() != nullptr && m_app.topScreen() != this;
    if (ImageWidget* back = findAs<ImageWidget>("bg_layer1")) {
        const Texture* want = covered ? m_plain : m_boxed;
        if (want) back->texture = want;
    }
    for (const char* id : {"input_username", "input_password", "btn_confirm", "btn_cancel", "btn_find", "btn_register",
                           "chk_remember"})
        if (Widget* w = find(id)) w->visible = !covered;
    if (m_app.autoLeaves("login") && !m_sent && m_time > 0.3f && !m_app.options().user.empty()) doLogin();
}

void LoginScreen::onKey(int key, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) { m_app.quit(); return; }
    WidgetScreen::onKey(key, action, mods);
}

void LoginScreen::onAction(const std::string& action, Widget&) {
    if (action == "login") doLogin();
    else if (action == "cancel") m_app.quit();
    else m_status = "Button " + action + " is not part of this phase";
}

void LoginScreen::doLogin() {
    if (m_sent) return;
    InputWidget* user = findAs<InputWidget>("input_username");
    InputWidget* pass = findAs<InputWidget>("input_password");
    const std::string u = user ? user->text : m_app.options().user;
    const std::string p = pass ? pass->text : m_app.options().pass;
    if (u.empty()) { m_status = "Type an account name"; return; }
    Session& session = m_app.session();
    if (!session.connected()) {
        if (!session.connect(m_app.options().host, m_app.options().port)) {
            m_status = session.lastError();
            m_app.showMessage(m_status);
            return;
        }
    }
    session.login(u, p);
    m_sent = true;
    m_status = "Connecting as " + u;
    std::printf("[time] login sent at %.0f ms\n", App::uptimeMs());
}

void LoginScreen::onSession(SessionEvent event) {
    if (event == SessionEvent::ChannelList) m_app.go("channel");
    if (event == SessionEvent::Disconnected) {
        m_sent = false;
        m_status = m_app.session().lastError();
    }
}

// the stock title capture prints its version line at 6 12 in a small bold white font
void LoginScreen::drawOverlay(DrawContext& ctx) {
    ctx.bold.draw(ctx.batch, "2026.09.15 knc client", 6.f, 12.f, 11.f, rgba(255, 255, 255, 200));
    if (!m_status.empty()) ctx.font.drawCentered(ctx.batch, m_status, m_app.canvasWidth() * 0.5f, 484.f, 15.f, rgba(255, 240, 160, 255));
}

}
