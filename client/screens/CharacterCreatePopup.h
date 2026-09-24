// RegistDriver popup of 0x0003 pick rows from 0x00BF nickname and 0x0004 answer
#pragma once

#include "ui/Screen.h"
#include "ui/Widgets.h"

#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Client {

class App;

class CharacterCreatePopup : public Screen {
public:
    explicit CharacterCreatePopup(App& app);
    const char* name() const override { return m_waiting ? "wait" : "createdriver"; }
    bool opaque() const override { return false; }
    void enter() override;
    void update(float dt) override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onChar(unsigned codepoint) override;
    void onMouseMove(float x, float y) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;

    // sub 4E15E0 taboo check of def taboo 32 marks dropped A to Z lowered then substring search
    static bool tabooHit(App& app, const std::u16string& nickname);

private:
    // 0x00BF rows with both flags set at most four as sub 473A20 walks them
    std::vector<uint32_t> pickKeys() const;
    // sub 473950 OK closes popup then checks key length and taboo list
    void confirm();
    // refusal box OK opens a fresh popup Cancel quits as stage window code does
    void refuse(const char* key);
    void takeHand();
    float backX() const;
    float backY() const;

    App& m_app;
    uint32_t m_driverKey = 0;
    std::string m_nick;
    float m_time = 0.f;
    // MSG WAIT stands in place of popup until 0x0004 answer lands
    bool m_waiting = false;
    bool m_closed = false;
    bool m_captured = false;
    bool m_okHover = false;
    bool m_okPressed = false;
};

}
