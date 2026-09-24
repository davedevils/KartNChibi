// the race the track scene under the hud the local car on the port the others on 0x0040
#pragma once

#include "race/AutoDriver.h"
#include "race/GaugeScene.h"
#include "race/PodiumScene.h"
#include "race/RaceHud.h"
#include "race/RaceSession.h"
#include "race/RaceSim.h"
#include "race/RaceView.h"
#include "race/TrackData.h"
#include "ui/Screen.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace KnC::Client {

class App;

class RaceScreen : public Screen {
public:
    explicit RaceScreen(App& app);
    ~RaceScreen() override;
    const char* name() const override { return "race"; }
    void enter() override;
    void leave() override;
    void update(float dt) override;
    bool drawScene() override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;
    // false while the track loads and till the GO lands the script waitfor holds till then
    bool ready() const override;
    // the stage the race replaced keeps drawing while the track loads the stock holds its last frame
    bool holdsPrevious() const override;

private:
    // Finished is the mode 9 camera on the own kart Podium the winer scene after the fade
    enum class Stage { Loading, Grid, Countdown, Racing, Finished, Podium };

    // one racer as the sim and the view know it
    struct Slot {
        uint32_t playerId = 0;
        int carIndex = -1;
        int viewHandle = -1;
        // the pose the view was given this frame the podium seat is not the sim pose
        CarPose shown;
    };

    // the load thread of the track and what the main thread does once it is done
    struct LoadJob;
    void startLoad(int trackId);
    void finishLoad();
    bool finishWorld(const TrackFiles& files);
    void joinLoad();
    void adoptEffects();
    // sub 4B2220 the Effect startcount nif at scale 0 05 one prop beside the cars
    void loadCountdownProp();
    // sub 4B23C0 mode 2 places it 5 ahead of the car 4 5 up resets its clip on every beat
    void updateCountdownProp(int stage);
    void spawnRacer(const Racer& racer);
    void readKeys(KnC::Kart::Client::InputFlags& flags, bool& drift, bool& item);
    void tickWire(int ticks);
    void watchCheckpoints();
    void watchItems(bool useKey);
    // KNC REMOTE LEAN prints the ground under a bot kart and the roll it implies a probe aid
    void logRemoteLean();
    // itemdrum hit test 0x4bed40 the barrels of the track stop or bounce the local car
    void watchDrums();
    // world gimmick hit dispatch 0x4D3950 the themed gimmicks of the track hit the local car
    void watchGimmicks();
    // the themed gimmick rows of the track loader one per placed gimmick nif
    KnC::Kart::Client::GimmickWorld m_gimmicks;
    // one broken flag per itemdrum row the barrel stays down for the rest of the race
    std::vector<uint8_t> m_drumBroken;
    uint32_t progressScore() const;
    CarPose localPose() const;
    void fillHud(HudState& s) const;
    // FUN 0043ed70 case 0xf the grid line the fly over walks from the first row to the last
    void armIntroPan();
    // FUN 004024f0 the intro the mode 15 fly over the rows then mode 16 in front then the chase
    void updateIntro(float dt);
    // true while the fly over holds the camera the race view then draws a fixed eye
    bool introCamera(float eye[3], float look[3]) const;
    // the race end of FUN 00401D90 six seconds of finish camera a fade then the podium
    void updateFinish(float dt);
    void beginPodium();
    void updatePodium(float dt);
    // the name plates over the other karts the system line and the black fade over everything
    void drawFinishOverlay(DrawContext& ctx, const HudState& state, float w, float h);
    // the 0x0046 rows the result board lists
    std::vector<HudResultRow> boardRows() const;
    void leaveRace();
    Slot* slot(uint32_t playerId);
    // the game mode of the launch KNC RACE MODE forces it for a capture
    uint32_t raceGameMode() const;
    std::string kartModel(uint32_t kartKey) const;
    std::string driverAsset(uint32_t driverKey) const;

    App& m_app;
    std::unique_ptr<LoadJob> m_load;
    bool m_loading = false;
    int m_loadTrackId = 0;
    // the frames of the wire that came while the track loaded in their order
    std::vector<std::pair<uint16_t, Packet>> m_heldFrames;
    double m_enterMs = 0.0;
    RaceSession m_wire;
    RaceWorld m_world;
    RaceSim m_sim;
    RaceView m_view;
    RaceHud m_hud;
    AutoDriver m_auto;
    std::vector<Slot> m_slots;
    Stage m_stage = Stage::Loading;
    bool m_worldLoaded = false;
    bool m_rankBoardSeen = false;
    bool m_paused = false;
    bool m_autoDrive = false;
    float m_stageTime = 0.f;
    double m_raceClock = 0.0;
    double m_motionAt = 0.0;
    double m_time = 0.0;
    float m_frameDt = 0.f;
    CarPose m_chase;
    bool m_accelWas = false;
    bool m_itemWas = false;
    // checkpoint bookkeeping the current face and the laps closed on the START face
    int m_checkpoint = 0;
    int m_laps = 0;
    int m_lastFace = -1;
    // the item boxes of itembox ini one prop each hidden while its cooldown runs
    void loadItemBoxes();
    void updateItemBoxes();
    // item boxes a cooldown per box and the hold timer of the auto driver
    std::vector<float> m_boxCooldown;
    float m_useItemIn = -1.f;
    uint32_t m_itemRng = 12345;
    // the staged captures of the auto race
    bool m_capturedLoading = false;
    bool m_capturedCountdown = false;
    bool m_capturedRace = false;
    bool m_capturedResult = false;
    bool m_capturedPodium = false;
    float m_resultTime = 0.f;
    // the race end the board time the fade level 0 clear 1 black and its direction
    PodiumScene m_podium;
    // sub 4AE230 the gauge nifs drawn through the gauge camera after the world frame
    GaugeScene m_gauge;
    // the weather veil went in the world frame this time so the sprite pass leaves it out
    bool m_veilInScene = false;
    double m_boardAt = -1.0;
    float m_fade = 0.f;
    int m_fadeDir = 0;
    bool m_podiumWanted = false;
    // the system line of a 0x00CE game message and when it came
    std::string m_systemLine;
    double m_systemLineAt = -1.0;
    // the last countdown stage that played its cue
    int m_cueStage = -1;
    // the Effect startcount prop of the countdown and the beat its clip last started on
    int m_countdownProp = -1;
    int m_countdownBeat = -1;
    // the result board of the 0x0046 rows shown by sub 4B67E0 and cleared by state 2020
    bool m_boardOpen = false;
    // the seconds left of the rain loop clip the bank has no third loop channel
    float m_rainLoopIn = 0.f;
    // the intro pan of FUN 004024f0 the two ends of the grid line and where the eye stands on it
    float m_panFrom[3] = {0.f, 0.f, 0.f};
    float m_panTo[3] = {0.f, 0.f, 0.f};
    float m_panHeadingDeg = 0.f;
    float m_panSpan = 0.f;
    float m_panPos = 0.f;
    int m_panDir = 1;
    bool m_panReady = false;
    // the seconds of the fly over and of the whole intro before the chase camera takes over
    float m_panSeconds = 0.f;
    float m_panRate = 0.f;
    // the lap words 0 none 1 lap two 2 final lap and the moment they started
    int m_lapFlash = 0;
    double m_lapFlashAt = 0.0;
    // the clock at the last lap close and the best lap so far
    double m_lapStartClock = 0.0;
    double m_bestLap = -1.0;
    // the last box taken and the last hit the hud shows them briefly
    double m_pickupAt = -1.0;
    int m_pickupItem = -1;
    double m_hitAt = -1.0;
    std::string m_status;
};

}
