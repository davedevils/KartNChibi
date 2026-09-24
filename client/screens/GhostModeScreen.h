// ghost mode menu is stage 23 FUN 00439350 reads 0x00AB board
#pragma once

#include "race/AutoDriver.h"
#include "race/RaceHud.h"
#include "race/RaceSim.h"
#include "race/RaceView.h"
#include "net/Session.h"
#include "race/TrackData.h"
#include "ui/Screen.h"
#include "ui/WidgetScreen.h"

#include "games/kart/physics/client/remote_car.h"

#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Client {

// info carried from 0x00AA ack into ghost race screen samples already in port format
struct GhostSessionInfo {
    uint32_t trackId = 0;
    std::string name;
    uint32_t carKind = 3;
    int32_t recordTimeMs = 0;
    uint32_t driverKey = 0;
    uint32_t kartKey = 0;
    std::vector<KnC::Kart::Client::GhostSample> samples;
};

class GhostModeScreen : public WidgetScreen {
public:
    explicit GhostModeScreen(App& app) : WidgetScreen(app) {}
    const char* name() const override { return "ghostmode"; }
    void enter() override;
    void leave() override;
    void update(float dt) override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;

    // top bar Ghost button sub 42BCE0 case 1 pushes screen past level and grade gate
    static bool open(App& app);

protected:
    void onAction(const std::string& action, Widget& source) override;
    void drawOverlay(DrawContext& ctx) override;

private:
    void rebuildThemes();
    // top arrows scroll thumb window bottom arrows step track of theme
    void stepTheme(int delta);
    void stepTrack(int delta);
    void start();
    void exitToLobby();
    // picked theme index track index within theme first visible thumb
    int m_theme = 0;
    int m_track = 0;
    int m_first = 0;
    // theme ids with thumb and their visible track ids
    std::vector<uint32_t> m_themes;
    std::vector<std::vector<uint32_t>> m_tracks;
    // 0x011D ack landed board is complete
    bool m_boardReady = false;
    // second 0x011D ack came back from race above
    bool m_raceDone = false;
    bool m_startSent = false;
    bool m_captured = false;
    bool m_autoStarted = false;
    float m_time = 0.f;
    std::string m_status;
};

// ghost race stage 15 local car runs on port ghost car replays downloaded ring
class GhostRaceScreen : public Screen {
public:
    GhostRaceScreen(App& app, GhostSessionInfo info);
    ~GhostRaceScreen() override;
    const char* name() const override { return "ghostrace"; }
    void enter() override;
    void leave() override;
    void update(float dt) override;
    bool drawScene() override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;

private:
    // FUN 00425350 1000 wait 1010 countdown 2000 finish 2010 to 2013 upload 2015 to 2040 result
    enum class Stage { Loading, Wait, Countdown, Racing, Finished, Upload, Waiting, Result };

    bool loadWorld();
    void spawnCars();
    void release();
    void finish();
    void upload();
    void watchCheckpoints();
    void fillHud(HudState& s) const;
    void leaveToMenu();
    CarPose localPose() const;
    // message key from FUN 00458E20 compares run time to three rows and own record
    std::string resultKey() const;

    App& m_app;
    GhostSessionInfo m_info;
    RaceWorld m_world;
    RaceSim m_sim;
    RaceView m_view;
    RaceHud m_hud;
    AutoDriver m_auto;
    Stage m_stage = Stage::Loading;
    int m_viewHandle = -1;
    int m_ghostCar = -1;
    int m_ghostView = -1;
    bool m_worldLoaded = false;
    bool m_autoDrive = false;
    bool m_accelWas = false;
    float m_time = 0.f;
    float m_stageTime = 0.f;
    double m_runClock = 0.0;
    float m_frameDt = 0.f;
    CarPose m_chase;
    int m_checkpoint = 0;
    int m_laps = 0;
    int m_lastFace = -1;
    int m_cueStage = -1;
    int m_lapFlash = 0;
    double m_lapFlashAt = 0.0;
    double m_lapStartClock = 0.0;
    double m_bestLap = -1.0;
    int32_t m_finishMs = 0;
    GhostSubmitResult m_result;
    bool m_capturedRace = false;
    bool m_capturedResult = false;
    bool m_leaveSent = false;
    bool m_exitBox = false;
    std::string m_status;
};

}
