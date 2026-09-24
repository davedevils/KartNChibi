// the two option panels of the Menu box sub 46D480 game setting and sub 45B8D0 key setting
#pragma once

#include "app/Settings.h"
#include "ui/Screen.h"

namespace KnC::Client {

class App;

// Popup Option art at 356 161 the Graphic and Sound tabs OK writes Option2 ini Cancel reads it back
class GameOptionPopup : public Screen {
public:
    explicit GameOptionPopup(App& app);
    const char* name() const override { return "gameoption"; }
    bool opaque() const override { return false; }
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseMove(float x, float y) override;
    void onMouseButton(int button, int action, float x, float y) override;

private:
    void confirm();
    void cancel();
    void close();
    // the bar counts from the three volumes as sub 46C840 rounds them
    void barsFromVolumes();
    void applyBgm();
    App& m_app;
    GameOptions m_edit;
    int m_tab = 0;
    int m_bars[3] = {8, 12, 12};
    float m_mouseX = 0.f;
    float m_mouseY = 0.f;
    bool m_closed = false;
};

// Popup Input art at 339 131 Key and Pad tabs a row click waits for a key OK writes
class ControlOptionPopup : public Screen {
public:
    explicit ControlOptionPopup(App& app);
    const char* name() const override { return "controloption"; }
    bool opaque() const override { return false; }
    void update(float dt) override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseMove(float x, float y) override;
    void onMouseButton(int button, int action, float x, float y) override;

private:
    void confirm();
    void cancel();
    void close();
    App& m_app;
    InputBindings m_edit;
    int m_tab = 0;
    // the row waiting for a key minus one when none
    int m_editRow = -1;
    float m_blink = 0.f;
    bool m_blinkOn = false;
    float m_mouseX = 0.f;
    float m_mouseY = 0.f;
    bool m_closed = false;
};

}
