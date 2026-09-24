// the messenger over the lobby the friend list the pending requests and the notes with a compose box
#pragma once

#include "ui/Screen.h"
#include "ui/Widgets.h"

#include <cstdint>
#include <string>

namespace KnC::Client {

class App;

class MessengerPopup : public Screen {
public:
    explicit MessengerPopup(App& app);
    const char* name() const override { return "messenger"; }
    bool opaque() const override { return false; }
    void enter() override;
    void update(float dt) override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onChar(unsigned codepoint) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;

private:
    enum class Tab { Friends, Requests, Notes, Blocks };
    enum class Input { None, AddFriend, NoteTo, NoteBody };

    int rowCount() const;
    void blockAction();
    void userInfoAction();
    void inviteAction();
    void openRowMenu(float x, float y, int row);
    bool menuClick(float x, float y);
    bool isBlocked(uint32_t playerId) const;
    void selectRow(int row);
    void primaryAction();
    void deleteAction();
    void submitInput();
    void close();
    float panelX() const;
    float panelY() const;

    App& m_app;
    Tab m_tab = Tab::Friends;
    Input m_input = Input::None;
    std::string m_text;
    std::string m_noteTo;
    int m_selected = 0;
    float m_time = 0.f;
    float m_pollAt = 0.f;
    float m_nextVerbAt = 0.f;
    bool m_closed = false;
    bool m_autoDone = false;
    bool m_noteSent = false;
    bool m_captured = false;
    std::string m_status;
    // the stock row menu of FUN 00464A50 opens on a friend row the six items sit under the click
    bool m_menuOpen = false;
    float m_menuX = 0.f;
    float m_menuY = 0.f;
    int m_menuRow = -1;
    // the user info item asked for a card the answer draws under the list until the popup closes
    bool m_cardOpen = false;
};

}
