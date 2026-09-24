// the stage 24 mission menu on the 0x0088 rows and the solo run of stage 25
#pragma once

#include "race/AutoDriver.h"
#include "race/RaceHud.h"
#include "race/RaceSim.h"
#include "race/RaceView.h"
#include "race/TrackData.h"
#include "ui/WidgetScreen.h"

#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Client {

struct MissionProgress;

// sub 43B9A0 a row plays when cleared or the first or right after a cleared one
bool missionRowPlayable(const std::vector<MissionProgress>& rows, int index);

class Session;
// the reward icon of a type and key the drawers of 0x443220 to 0x4438D0 seven the pendant
std::string missionRewardIcon(const Session& session, uint32_t type, uint32_t key);

class MissionMenuScreen : public WidgetScreen {
public:
    explicit MissionMenuScreen(App& app) : WidgetScreen(app) {}
    const char* name() const override { return "missions"; }
    void enter() override;
    void update(float dt) override;
    void onKey(int key, int action, int mods) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;

protected:
    void onAction(const std::string& action, Widget& source) override;
    void drawOverlay(DrawContext& ctx) override;

private:
    // sub 43B940 and sub 43B970 the arrows page five rows the pick stays
    void page(int delta);
    // the Start button opens the MISSION ENTER confirm its OK sends 0x0090
    void askStart();
    void startSelected();
    // the row picked on entry the first not cleared else the last one
    void pickEntryRow();
    // the picked row index in the 0x0088 list minus one when the list is empty
    int m_selected = -1;
    // the first row of the five slots a multiple of five
    int m_top = 0;
    float m_time = 0.f;
    bool m_captured = false;
    bool m_autoSent = false;
    bool m_picked = false;
    std::string m_status;
};

// the solo run of stage 25 FUN 0043A320 on World Mission world name with the gimmicks of its kind
class MissionRunScreen : public Screen {
public:
    explicit MissionRunScreen(App& app);
    ~MissionRunScreen() override;
    const char* name() const override { return m_phase == Phase::ExitBox ? "missionexit" : "mission"; }
    void enter() override;
    void update(float dt) override;
    bool drawScene() override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;

private:
    // FUN 0043AB00 1000 board 1010 pause 1100 run 3000 win clip 4000 lose clip then popup
    enum class Phase { Loading, Board, Pre, Countdown, Running, Success, Fail, Popup, ExitBox };

    // one gimmick of the kind 0 time items or of the kind 1 boxes
    struct Gimmick {
        int type = 0;
        float x = 0.f, y = 0.f, z = 0.f;
        int prop = -1;
        bool taken = false;
        float respawnAt = 0.f;
    };

    bool loadWorld(const std::string& worldName);
    // world gimmick load by track 0x4D4180 kind 0 sub 4D8590 kind 1 sub 4D9ED0 kind 2 nothing
    void loadGimmicks(uint32_t kind);
    void placeGimmicks();
    void spawnLocal();
    void watchGimmicks();
    void watchEnd();
    void autoSteerToBox(KnC::Kart::Client::InputFlags& flags);
    void goalReached();
    void failed();
    // the exit box OK sends 0x008F as stage 25 answers 0x7E8 the menu ack ends the run
    void leaveRun();
    void backToMenu();
    CarPose localPose() const;
    // the time left of the def plus the time items of kind 0 in milliseconds
    double timeLeftMs() const;
    // the box counter of FUN 00439DB0 on the Mission Num glyphs at y 300
    void drawCounter(DrawContext& ctx, int count, int goal);

    App& m_app;
    RaceWorld m_world;
    RaceSim m_sim;
    RaceView m_view;
    RaceHud m_hud;
    AutoDriver m_auto;
    Phase m_phase = Phase::Loading;
    int m_viewHandle = -1;
    bool m_worldLoaded = false;
    bool m_goalSent = false;
    bool m_autoDrive = false;
    bool m_captured = false;
    bool m_capturedBoard = false;
    bool m_capturedEnd = false;
    bool m_passed = false;
    // 0xD0956C the cleared flag of the row when the run started a replay pays and shows nothing
    bool m_firstClear = true;
    float m_time = 0.f;
    float m_phaseTime = 0.f;
    float m_runClock = 0.f;
    float m_frameDt = 0.f;
    int m_cueStage = -1;
    CarPose m_chase;
    bool m_accelWas = false;
    // the lap flag 0x928 raised once the checkpoint cursor went round the START face
    int m_checkpoint = 0;
    int m_lastFace = -1;
    bool m_lapFlag = false;
    int m_boxes = 0;
    // distance driven since the release a lap needs some of it first
    float m_travelled = 0.f;
    float m_lastX = 0.f;
    float m_lastY = 0.f;
    std::vector<Gimmick> m_gimmicks;
    std::string m_gimmickFile;
    // qword D09570 the milliseconds the time items added or took
    double m_bonusMs = 0.0;
    // the flash of a time item 0 blue 1 red its alpha and the zooming number at 482 300
    int m_flashColour = -1;
    float m_flashAlpha = 0.f;
    int m_flashType = -1;
    float m_flashAge = 0.f;
    std::string m_status;
};

}
