// the licence stage 14 of sub 437190 the medals the four test tiles and the test run of stage 13
#pragma once

#include "race/AutoDriver.h"
#include "race/RaceHud.h"
#include "race/RaceSim.h"
#include "race/RaceView.h"
#include "race/TrackData.h"
#include "ui/Screen.h"
#include "ui/WidgetScreen.h"

#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Client {

class LicenceScreen : public WidgetScreen {
public:
    explicit LicenceScreen(App& app) : WidgetScreen(app) {}
    const char* name() const override { return "licence"; }
    void enter() override;
    void leave() override;
    void update(float dt) override;
    bool drawScene() override;
    void sceneLost() override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onSession(SessionEvent event) override;

    // the stock top bar gate of sub 42BCE0 level ten or a licence grade opens ghost and mission
    static bool stageGateOpen(const App& app);

protected:
    void onAction(const std::string& action, Widget& source) override;
    void drawOverlay(DrawContext& ctx) override;

private:
    void exitToLobby();
    // the init rule of sub 437190 on the licence grade and the level of the profile
    int startTier() const;
    // a tier opens once the grade and the level of the profile reach it
    bool tierOpen(int tier) const;
    // sub 437770 the first test always picks a later one wants a row on the test before it
    bool tilePickable(int test) const;
    // the four tile buttons take the art of the tier
    void applyTierArt();
    // the tiles and the medals of the menu state or the start and back pair of the picked state
    void showPickedWidgets(bool picked);
    // sub 436FC0 keeps the key and sends the empty 0x0062 the server ack opens stage 13
    void sendStart();
    // the own kart and driver in the blue box of license back 003 the stock preview of sub 4A5E00
    void loadPreview();
    // 0 rookie 1 advanced 2 master the medal picked on the left column
    int m_tier = 0;
    // the picked test tile below zero when none
    int m_pick = -1;
    bool m_startSent = false;
    float m_time = 0.f;
    bool m_captured = false;
    bool m_capturedPick = false;
    bool m_autoStarted = false;
    bool m_rowsAsked = false;
    RaceView m_view;
    RaceWorld m_previewWorld;
    bool m_sceneReady = false;
    int m_previewCar = -1;
    std::string m_status;
};

// the licence test of stage 13 FUN 0041B860 one car on World License the phases of FUN 0041F870
class LicenceRunScreen : public Screen {
public:
    LicenceRunScreen(App& app, uint32_t licenceKey);
    ~LicenceRunScreen() override;
    const char* name() const override { return "licencerun"; }
    void enter() override;
    void leave() override;
    void update(float dt) override;
    bool drawScene() override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;

private:
    // FUN 0041F870 phase 1 board 2 and 3 countdown then run 4 pass 5 fail 7 submit
    enum class Phase { Loading, Intro, Countdown, Running, Passed, Failed, Submit, Result, FailBox, ExitBox };

    bool loadWorld();
    // the minimap nif lists its four corners in strip order the quad wants them around the ring
    void orderMinimapQuad();
    void spawnLocal();
    void restart();
    // gauge update 0x4ADC20 the drift gauge of the tests 2 and 13 grows and hands the booster
    void updateDriftGauge(float dt, bool drifting);
    // the held booster fired with the item key car boost start kind 1 of FUN 004AEFA0 case 0
    void useBooster();
    // the arc of the gauge board filled from the drift gauge the stock draws its red gauge nif here
    void drawGaugeFill(DrawContext& ctx, float canvasW, float canvasH) const;
    void beginCountdown();
    void release();
    void pass();
    void fail();
    void watchFaces();
    void submit();
    void leaveToMenu();
    CarPose localPose() const;
    // the world folder of license track init 0x487A90 by the test key
    std::string worldFolder() const;
    // the pass rule of the test the rookie tests read the faces the items and the boosts
    bool goalReached();

    App& m_app;
    uint32_t m_key = 0;
    RaceWorld m_world;
    RaceSim m_sim;
    RaceView m_view;
    RaceHud m_hud;
    AutoDriver m_auto;
    Phase m_phase = Phase::Loading;
    int m_viewHandle = -1;
    bool m_worldLoaded = false;
    bool m_autoDrive = false;
    bool m_accelWas = false;
    bool m_itemWas = false;
    float m_time = 0.f;
    float m_phaseTime = 0.f;
    float m_runClock = 0.f;
    float m_frameDt = 0.f;
    // the time limit of FUN 0041B860 in ms the rookie driving test never counts it down
    double m_limitMs = 130000.0;
    bool m_limitCounts = false;
    CarPose m_chase;
    // the faces the test saw the START pass then the middle check then START again
    bool m_startSeen = false;
    bool m_midSeen = false;
    int m_lastFace = -1;
    int m_itemUses = 0;
    int m_boosts = 0;
    // the accumulated drift gauge of 0x4ADC20 the booster lands in the slot at 127
    float m_gauge = 0.f;
    bool m_gaugeTest = false;
    // the item slot holds the booster kind 0 minus one when empty
    int m_slotItem = -1;
    float m_slotAge = -1.f;
    // the test 2 pass flag of FUN 004AEFA0 the booster was fired
    bool m_boosterUsed = false;
    int m_cueStage = -1;
    // the 0x00A3 answer landed and whether the row was new
    bool m_resultLanded = false;
    bool m_resultFresh = false;
    uint32_t m_resultPassed = 0;
    bool m_capturedIntro = false;
    bool m_capturedRun = false;
    bool m_capturedBoard = false;
    std::string m_status;
};

}
