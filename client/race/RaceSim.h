// 30 car array from port local car on tick others on remote mover
#pragma once

#include "TrackData.h"

#include "games/kart/physics/client/car_state.h"
#include "games/kart/physics/client/gimmicks.h"
#include "games/kart/physics/client/motion_packet.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace KnC::Client {

// local body built from harness setup for ghost compare
struct CarSetup {
    uint32_t playerId = 0;
    std::array<float, 17> stats{};
    std::string carFile;
    int vehicleKind = 0;
    float x = 0.f, y = 0.f, z = 0.f, yawDeg = 0.f;
};

// port state beyond pose effects read it every frame
struct CarEffectSignals {
    // car 0x3716 four wheels in air
    bool airborne = false;
    // car 0x373C set on landing after drop over one unit puff shows 800 ms
    bool landed = false;
    // car 0x3715 slip stream behind car ahead over 80 km per hour
    bool draft = false;
    // dust kind under each rear wheel 0 dust 1 grass 2 water 3 snow minus one none
    int groundKind[2] = {0, 0};
    // car 0x36A8 effect code of a hit zero none
    int hitCode = 0;
    // car 0x3728 grip per wheel after surface table
    float grip[4] = {1.f, 1.f, 1.f, 1.f};
    // car impact effect play 0x4981B0 tier of last bump minus one none plus its age
    int impactTier = -1;
    int impactAgeMs = 0;
};

// one car as render and hud read it
struct CarPose {
    float x = 0.f, y = 0.f, z = 0.f;
    float yawDeg = 0.f;
    float speed = 0.f;
    float speedKmh = 0.f;
    bool boosting = false;
    int boostKind = 0;
    bool reversing = false;
    int driftState = 0;
    int miniTurboStage = 0;
    int turnState = 0;
    float driftGauge = 0.f;
    float rpm = 0.f;

    // visual feed of port car 0x2FE0 world matrix lean slip body R false means yaw only
    bool hasBody = false;
    float body[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    // car 0x32A4 D3DX RotationY spin of each wheel and tick front steer radians
    float wheelSpin[4] = {0.f, 0.f, 0.f, 0.f};
    float wheelSteer = 0.f;
    bool hasWheels = false;
    // car 0x32E4 of remote car eased lean side radians front wheels turn by it
    float steerAverage = 0.f;
    // car 0x35B0 model slip turn remote car gets drift steer times its smoothed gauge
    float driftSlipDeg = 0.f;
    // car 0x3224 chase camera lowers by 2 5 times this
    float pitchDeg = 0.f;
    // three tail floats of 0xC0 row chase camera distance pitch and look height
    float camDistance = 9.f;
    float camPitchDeg = 37.f;
    float camHeight = 3.5f;
    // start light camera eases slower before green
    bool greenLight = false;
    // port signals for car true once sim filled them
    CarEffectSignals signals;
    bool hasSignals = false;
};

class RaceSim {
public:
    RaceSim();
    ~RaceSim();
    RaceSim(const RaceSim&) = delete;
    RaceSim& operator=(const RaceSim&) = delete;

    // follow lists boost rows and globals of a race on this world
    bool init(RaceWorld& world, uint32_t localPlayerId, std::string& error);
    // body create from car file 17 stats and setup overrides from harness
    bool spawnLocal(const CarSetup& setup, std::string& error);
    // remote car parked on its grid row fed by 0x0040 later returns car index
    int spawnRemote(uint32_t playerId, float x, float y, float z, float yawDeg);
    void removeCar(uint32_t playerId);
    int carIndex(uint32_t playerId) const;
    int localIndex() const { return 0; }
    bool hasLocal() const { return m_localReady; }

    // six flags and two raw keys tick and drift update read
    void setLocalInput(const KnC::Kart::Client::InputFlags& flags, bool driftHeld, bool accelPressed);
    // raw key code pressed this tick debug boost keys at green light
    void pressKey(int keyCode);
    // green light on lets start boost keys work and engine push car
    void setGreenLight(bool on);
    void setSessionRunning(bool on);
    // one 0x0040 entry into mailbox of that car
    void applyMotion(const KnC::Kart::Client::MotionRecvEntry& entry);
    // 0x0068 hard teleport of a remote car
    void teleport(uint32_t playerId, float x, float y, float z, float yawDeg);
    // 0x0069 effect code on a car
    void applyEffect(uint32_t playerId, int code);
    // itemdrum hit test 0x4bed40 barrel response on local car caller keeps broken rows
    void applyDrumHit(const KnC::Kart::Client::GimmickDrumResult& hit);
    // drift gauge drum sweep reads car 0x35ac of local car
    float localDriftGaugeSmoothed() const;
    // gauge update 0x4ADC20 shown fill of red band 0 to 127 hud draws it
    float gaugeFill() const { return m_gaugeShown; }
    // 0x85C spark at O POS node of band while charge climbs
    bool gaugeSpark() const { return m_gaugeSpark; }
    // 0 none 1 red boost at full band 2 blue and seconds since fired
    int gaugeFlash() const { return m_gaugeFlash; }
    float gaugeFlashAge() const { return m_gaugeFlashAge; }
    // second band for team mode below zero when mode lacks it
    float gaugeFillBlue() const { return m_gaugeTeam ? m_gaugeShownBlue : -1.f; }
    // dword 0xB23178 mode 3 is only one that stocks blue band
    void setGaugeTeamMode(bool on) { m_gaugeTeam = on; }
    void setFinished(uint32_t playerId);
    void setFinishRank(uint32_t playerId, int rank);
    // car 0xA7854 mode 1 local car writes ghost ring every ten ticks like stock ghost stage
    void setGhostRecording(bool on);
    // ring local car wrote so far 28 byte samples upload of 0x00AE and 0x00AF carries
    std::vector<KnC::Kart::Client::GhostSample> localGhostSamples() const;
    // car on downloaded samples parked on their first one returns car index
    int spawnGhost(uint32_t ghostId, const std::vector<KnC::Kart::Client::GhostSample>& samples);
    // car 0xA7854 mode 2 ghost car replays ring one sample per ten ticks with stock blend
    void setGhostReplay(int carIndex, bool on);

    // advances fixed 20 ms ticks frame time earned returns how many ran
    int advance(float dt);
    int64_t nowMs() const { return m_nowMs; }
    CarPose pose(int carIndex) const;
    const KnC::Kart::Client::CarState& car(int carIndex) const { return m_game->cars[static_cast<size_t>(carIndex)]; }
    KnC::Kart::Client::GameState& game() { return *m_game; }
    // 0x0040 body for local car
    KnC::Kart::Client::MotionSend0x40 localMotion() const;
    // checkpoint face under local wheel START is 0 CHECK NNN is NNN none is minus one
    int localCheckpointFace();
    // true when local wheel stands on face of that name stock upper cases names with strupr
    bool localOnFace(const char* upperName);
    // racing line from follow 01 ini in same frame as car
    const std::vector<KnC::Kart::Client::CheckpointPoint>& line() const;
    // draft factor game 0x1397360 zero to one behind car ahead
    float draftFactor() const;
    // car 0x3715 draft push is on this tick
    bool draftActive() const;
    // Panel Stream number factor times 100 minus 15 below zero when nothing shows
    float slipStreamBonus() const;

private:
    // car boost speed gate 0x49A390 surface index under each rear wheel kept while query misses
    void updateRearSurfaces();
    // gauge update 0x4ADC20 drift band for race stage once a rendered frame
    void updateDriftBand(float dt);
    // car impact effect play 0x4981B0 port hook lands here
    static void onImpact(void* user, int carIndex, int tier);

    struct Impact {
        int tier = -1;
        int64_t atMs = 0;
    };
    std::unique_ptr<KnC::Kart::Client::GameState> m_game;
    RaceWorld* m_world = nullptr;
    // cars that replay ghost ring tick applies their samples like stock finished state 2
    std::array<bool, KnC::Kart::Client::kCarSlotCount> m_ghostReplay{};
    std::array<uint32_t, KnC::Kart::Client::kCarSlotCount> m_ids{};
    std::array<std::array<int, 2>, KnC::Kart::Client::kCarSlotCount> m_rearSurface{};
    std::array<Impact, KnC::Kart::Client::kCarSlotCount> m_impact{};
    int64_t m_nowMs = 0;
    float m_accum = 0.f;
    bool m_localReady = false;
    // KNC SIM DRIFT start of running drift cycle minus one between cycles
    int64_t m_driftTestStartMs = -1;
    // 0x868 band target 0x870 shown fill 0x878 value charge started at
    float m_gaugeValue = 0.f;
    float m_gaugeShown = 0.f;
    float m_gaugeValueBlue = 0.f;
    float m_gaugeShownBlue = 0.f;
    float m_gaugeStart = 0.f;
    bool m_gaugeCharging = false;
    bool m_gaugeSpark = false;
    bool m_gaugeTeam = false;
    int m_gaugeFlash = 0;
    float m_gaugeFlashAge = 0.f;
};

}
