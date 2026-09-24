// Crazy Lotto box from gacha button machine scene roll 0x00ED and prize
#pragma once

#include "net/Catalog.h"
#include "race/RaceView.h"
#include "ui/Screen.h"

#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Client {

class App;

class GachaPopup : public Screen {
public:
    explicit GachaPopup(App& app) : m_app(app) {}
    const char* name() const override { return "gacha"; }
    bool opaque() const override { return false; }
    void leave() override;
    void update(float dt) override;
    bool drawScene() override;
    void sceneLost() override;
    bool sceneHole(float& x, float& y, float& w, float& h) const override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseMove(float x, float y) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;

private:
    // Idle waits for START Spinning plays machine Prize shows win over machine
    enum class State { Idle, Spinning, Prize };

    // one camera track from gacha camera 01 eye path or target path sampled by time
    struct CameraTrack {
        std::vector<float> times;
        std::vector<float> values;
        void sample(float t, float out[3]) const;
    };

    void close();
    void play();
    void tickCues();
    int ticketsFor(int tab) const;
    const OwnedItem* ticketRow(int tab) const;
    // sub 4576B0 four machine nifs on an empty scene and two camera paths from camera nif
    bool loadMachine();
    // machine from sub 4571D0 tab picks pair rare flag of answer picks second of it
    int machineIndex() const;

    App& m_app;
    // 0 is astro lotto 1 is gold lotto stock opens on astro
    int m_tab = 0;
    float m_mouseX = 0.f;
    float m_mouseY = 0.f;
    bool m_closed = false;
    State m_state = State::Idle;
    // machine clock from sub 4571D0 runs from play press and stops at twelve seconds
    float m_time = 0.f;
    // next cue in sound table not yet played
    int m_cue = 0;
    bool m_haveResult = false;
    // prize from 0x00ED answer category drives icon and name
    uint32_t m_prizeCategory = 6;
    uint32_t m_prizeBaseKey = 0;
    uint32_t m_rareFlag = 0;
    // machine scene and its four props one per tab and rarity
    RaceView m_view;
    int m_machine[4] = {-1, -1, -1, -1};
    CameraTrack m_eyePath;
    CameraTrack m_lookPath;
    bool m_sceneReady = false;
    bool m_sceneTried = false;
    float m_frameDt = 0.f;
    std::string m_status;
};

}
