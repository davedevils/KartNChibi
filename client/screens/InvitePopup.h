// invite arrives as 0x006C accept sends join 0x002F decline sends answer 0x006D
#pragma once

#include "net/Session.h"
#include "ui/Screen.h"

namespace KnC::Client {

class App;

class InvitePopup : public Screen {
public:
    InvitePopup(App& app, RoomInvite invite) : m_app(app), m_invite(std::move(invite)) {}
    const char* name() const override { return "invite"; }
    bool opaque() const override { return false; }
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseMove(float x, float y) override;
    void onMouseButton(int button, int action, float x, float y) override;

private:
    void accept();
    void decline();
    void close();
    App& m_app;
    RoomInvite m_invite;
    float m_mouseX = 0.f;
    float m_mouseY = 0.f;
    bool m_closed = false;
};

}
