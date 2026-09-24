// the lobby on the stock frame the room cards from 0x002D the char info the chat a card click joins
#pragma once

#include "race/RaceView.h"
#include "ui/CharPanel.h"
#include "ui/WidgetScreen.h"

#include <string>
#include <vector>

namespace KnC::Client {

class LobbyScreen : public WidgetScreen {
public:
    explicit LobbyScreen(App& app) : WidgetScreen(app) {}
    const char* name() const override { return "lobby"; }
    void enter() override;
    void leave() override;
    void update(float dt) override;
    bool drawScene() override;
    void sceneLost() override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;

protected:
    void onAction(const std::string& action, Widget& source) override;
    void drawOverlay(DrawContext& ctx) override;

private:
    void sendChat();
    void joinRow(size_t shownIndex);
    void openMessenger();
    // the own kart and driver on the empty scene of the User Info tab
    void refreshPreview();
    // the six card rects of the page as the stock lays them two columns of three
    Rect cardRect(int index) const;
    RaceView m_view;
    // the preview world the view keeps a pointer into it so it lives with the stage
    RaceWorld m_previewWorld;
    UserListState m_users;
    int m_previewCar = -1;
    std::string m_previewKart;
    std::string m_previewDriver;
    // the kart yaw of the preview the arrows turn it the front ball resets it
    float m_orbit = 0.f;
    bool m_sceneReady = false;
    InputWidget* m_chatInput = nullptr;
    // the whisper target box left of the chat a name here sends the line as a whisper
    InputWidget* m_whisperInput = nullptr;
    std::string m_status;
    float m_time = 0.f;
    bool m_autoSent = false;
    bool m_saySent = false;
    bool m_socialOpened = false;
    int m_page = 0;
    int m_selected = -1;
    // the room ids of the cards as drawn on the page
    std::vector<uint32_t> m_shownRooms;
};

// the create room box on the stock MakeRoom art a name a password the mode picker OK sends 0x002D
class CreateRoomPopup : public Screen {
public:
    explicit CreateRoomPopup(App& app);
    const char* name() const override { return "makeroom"; }
    bool opaque() const override { return false; }
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onChar(unsigned codepoint) override;
    void onMouseMove(float x, float y) override;
    void onMouseButton(int button, int action, float x, float y) override;

private:
    void confirm();
    void close();
    // a random line of def title txt as sub 4634C0 fills the name
    std::string randomTitle() const;
    App& m_app;
    std::string m_name;
    std::string m_password;
    // the field the typing goes to 0 the name 1 the password
    int m_field = 0;
    int m_mode = 0;
    int m_players = 8;
    bool m_private = false;
    bool m_closed = false;
    float m_mouseX = 0.f;
    float m_mouseY = 0.f;
};

// the password box of a locked row OK sends 0x002F with the text
class PasswordPopup : public Screen {
public:
    PasswordPopup(App& app, uint32_t roomId);
    const char* name() const override { return "password"; }
    bool opaque() const override { return false; }
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onChar(unsigned codepoint) override;
    void onMouseButton(int button, int action, float x, float y) override;

private:
    void confirm();
    void close();
    App& m_app;
    uint32_t m_roomId = 0;
    std::string m_password;
    bool m_closed = false;
};

}
