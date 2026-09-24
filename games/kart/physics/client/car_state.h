#pragma once
// The car record as one struct every field named by meaning with its car offset see CLIENT PHYSICS MAP

#include <array>
#include <cstdint>
#include <vector>

#include "body.h"
#include "constants.h"
#include "gimmicks.h"
#include "input.h"
#include "math_helpers.h"
#include "remote_car.h"
#include "stats.h"
#include "world_collision.h"

namespace KnC::Kart::Client {

constexpr int kCarSlotCount = 30; // game object 30 car array stride 0xA7260

// hooks the host fills scene node sound and particle calls become no ops here
struct CarHooks {
    void (*playSound)(void* user, int carIndex, int slot, int soundId) = nullptr;
    void (*setNodeTransform)(void* user, int carIndex, int nodeIndex, const Mat3& rot,
                              const Vec3& pos) = nullptr;
    void (*spawnParticle)(void* user, int carIndex, int kind, float x, float y, float z) = nullptr;
    // car impact effect play 0x4981B0 tier 0 or 1 a one shot scene effect near the car
    void (*impactEffect)(void* user, int carIndex, int tier) = nullptr;
    // FUN 0043D7E0 the camera pan of effect 0x44C and the carry drop
    void (*cameraPan)(void* user) = nullptr;
    // FUN 0043ED70 freezes the local input for the carry the pool update calls it
    void (*freezeInput)(void* user, int carIndex) = nullptr;
    // FUN 004815F0 a voice cue id the rival cue plays 3
    void (*voiceCue)(void* user, int carIndex, int cueId) = nullptr;
    // car has ability 0x4B8580 catalogue ability pairs of the driver kart and parts host answers
    bool (*carHasAbility)(void* user, int carIndex, int abilityId) = nullptr;
    // FUN 00481B60 the packet 105 notice car gimmick hit 0x4982D0 sends after an effect lands
    void (*effectNotify)(void* user, int carIndex, int effectCode) = nullptr;
    // FUN 0043EAD0 keeps the largest screen shake of the frame the mushman catch asks 20000
    void (*cameraShake)(void* user, float magnitude) = nullptr;
    void* user = nullptr;
};

// a plain 4x4 style transform rotation plus translation stands in for the D3DX matrix
struct Transform3 {
    Mat3 rotation;
    Vec3 translation;
};

// per wheel scratch the tick keeps outside the body module named by meaning
struct TickWheelState {
    float spinAngle = 0.0f;  // car 0x32A4 stride 4 per wheel visual spin minus body spin angle front steer wheel matrix radians clamped 30 degrees
    float steerAngle = 0.0f;
    float squash = 1.0f;  // bump scale of wheel matrix one plus a quarter of bump car 0x3728 stride 4 per wheel grip after table
    float grip = 0.0f;
    Transform3 matrix;            // car 0x3020 stride 0x40 per wheel world matrix render only
};

// simple id lookup table shape shared by car is slowed and car is camera reversed
struct StatusIdSlot {
    uint8_t active = 0;
    int32_t id = -1;
};
constexpr size_t STATUS_TABLE_SIZE = 16; // both tables are 16 entries TICK HELPERS md

// effect state car 0x36A8 family car effect apply and car effect update
struct EffectState {
    int32_t activeCode = 0;  // car 0x36A8 active effect code 0 is none car 0x36AC steering loss or wobble while spun
    float wobbleAmplitude = 0.0f;
    float snapshotYaw = 0.0f;  // car 0x36CC yaw snapshot on apply car 0x36D0 position snapshot on apply
    float snapshotX = 0.0f;
    float snapshotY = 0.0f;  // car 0x36D4 position snapshot on apply car 0x36D8 position snapshot on apply
    float snapshotZ = 0.0f;
    float quadraticInput = 34.0f;  // car 0x36BC code 300 quadratic coefficient input seeds 34 0 car 0x36C8 code 300 end condition value
    float endCondition = 0.0f;
    float secondaryAccum = 0.0f;  // car 0x2604 code 300 secondary accumulator car 0x36B0 family code 300 elapsed time accumulator
    float elapsedAccum = 0.0f;
    int32_t leanState = 0;        // car 0x36DC lean and wobble state 0 to 9
};

// crash recovery reward state machine game 0x13972F4 states 0 1 2 3
struct CrashRecoveryState {
    int32_t state = 0;  // game 0x13972F4 game 0x13972F8
    int64_t timestampMs = 0;
};

// progress watchdog game 0x1397368 family respawn crash recovery update
struct ProgressWatchdogState {
    int32_t state = 0;  // game 0x1397368 game 0x1397370
    int32_t progressIndex = 0;
    int32_t nearestListIndex = 0;  // which of the 4 checkpoint lists is armed not a doc offset game 0x1397374
    float distance = 0.0f;
    int64_t lastDirectionChangeMs = 0; // game 0x1397378
};

// stuck flag state game 0x1397174 to 0x1397184 drift update escalation
struct StuckWatchState {
    uint8_t stuckFlag = 0;  // game 0x1397174 game 0x1397178 0 idle 1 armed 2 stuck
    int32_t phase = 0;
    int64_t armedAtMs = 0; // game 0x1397180
};

// one follow ini row world object 0x1ADF810 plus 0x2A14 stride 0x10 gimmick load follow 0x489730
struct CheckpointPoint {
    float x = 0.0f, y = 0.0f, z = 0.0f;  // row columns 1 to 3 x y ground z height row column 4 the teleport yaw read at 0x4A342B
    float yawDeg = 0.0f;
};

// the 4 follow lists follow 01 ini to follow 04 ini counts at 0x1AE2214 points at 0x1AE2224
struct CheckpointList {
    std::vector<CheckpointPoint> lists[4];
};

// the same follow point seen through the grid index list times 400 plus point 0x4A33E2
struct RespawnGridEntry {
    float x = 0.0f, y = 0.0f, z = 0.0f, yawDeg = 0.0f;
};

// one item slot record 16 bytes car 0x3490 stride 0x10 item slot lookup 0x451D30
struct ItemSlot {
    int32_t field0 = 0;  // record 0x0 not read by the tick record 0x4 kind 3 held with no count costs throttle
    int32_t kind = 0;
    int32_t count = 0;  // record 0x8 count record 0xC not read by the tick
    int32_t field12 = 0;
};

constexpr int kItemSlotCount = 4; // car 0x3490 to 0x34D0 four records before the count

// the car item inventory car 0x348C count at car 0x34D0
struct ItemInventory {
    std::array<ItemSlot, kItemSlotCount> slots{};  // car 0x3490 car 0x34D0 item slot lookup bound
    int32_t count = 0;
};

// one car car 0x0 to car 0xA7260 every persistent field the tick touches
struct CarState {
    CarBody body;  // car 0x211C car 0x3440 and 0xA7940
    KartStats stats;
    InputFlags input;  // car 0x18 0x1C 0x20 0x24 0x2C car 0x3244 family used only while this car is a remote car
    RemoteCarState remote;

    // timing game 0x30 written once by cars manager init 0x495607
    float fixedStepSeconds = kFixedPhysicsStepSeconds;
    float frameDt = 0.0f;                  // car 0x34

    // engine and drive car 0x2610 is R element 0 2 of the body the harness still sets this unused here
    float engineForceBase = 0.0f;
    float throttleJitter = 1.0f;  // game 0x6F8 one per game tick reads at 0x49CD97 car 0x2F34 catalogue max steer degrees mirrored from body
    float steerAngleDeg = 0.0f;
    float frictionExtra = 0.0f;  // car 0xA7988 part record 0xCC into friction pair car 0xA798C part record 0xD0 added to contact drag per wheel
    float contactDragExtra = 0.0f;
    float gripExtra = 0.0f;  // car 0xA7990 part def record 0xD4 added to the surface grip zero with no part car 0x348C
    ItemInventory items;

    // track progress and slot car 0x6BC checkpoint or respawn phase
    int32_t trackProgress = 0;
    uint64_t checkpointTimestampMs = 0;  // car 0x6E8 car 0x740
    uint8_t slotOccupied = 0;
    uint8_t slotActive = 0;  // car 0x743 car 0x744
    int32_t playerOrGhostId = 0;
    uint8_t forceRespawnFlag = 0;  // car 0x9D9 car 0x9D8 is body overValidGround car 0x9D4 set to 1 by setup 0x494F4C flag body step world
    int32_t substepInputFlag = 1;
    int32_t attachedNodeCount = 0;      // car 0x9E0

    // orientation speed transform outputs car 0x3220
    float yawDeg = 0.0f;
    float pitchDeg = 0.0f;  // car 0x3224 car 0x3228
    float rollDeg = 0.0f;
    float yawRateOut = 0.0f;  // car 0x322C yaw velocity car 0x3230 restored on collision
    float prevSubstepYawDeg = 0.0f;
    float speed = 0.0f;  // car 0x3234 car 0x3238 0x323C 0x3240
    float velX = 0.0f, velY = 0.0f, velZ = 0.0f;
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;  // car 0x3244 0x3248 0x324C car 0x3250 0x3254 0x3258
    float accelX = 0.0f, accelY = 0.0f, accelZ = 0.0f;
    float prevPosX = 0.0f, prevPosY = 0.0f, prevPosZ = 0.0f;  // car 0x325C 0x3260 0x3264 car 0x3268 0x326C
    float prevVelX = 0.0f, prevVelY = 0.0f;
    float prevVelZSnapshot = 0.0f;                           // car 0x3270 restored on collision

    Vec3 wheelProbePoint[4]; // car 0x3274 stride 0xC body 0x2120 with x and y negated the wheel probe points

    std::array<TickWheelState, 4> wheelsTick;  // car 0x32A4 spin angle car 0x3728 grip car 0x3528 stride 0xC O WHEEL01 to 04 node positions host fills
    Vec3 wheelNodePos[4];
    float gripBaseFront = 0.0f;  // car 0x32DC and 0x32E0 catalogue 0x130 front and 0x134 rear tyre grip copied at setup
    float gripBaseRear = 0.0f;
    float steeringScale = 0.4f;               // car 0x32E8 also body material rate

    float speedKmh = 0.0f;  // car 0x32F4 car 0x32E4 copy of car 0x297C wheel set 0x520 the mean front steer tangent
    float yawRateBody = 0.0f;
    float rpm = 0.0f;  // car 0x32F8 car 0x32FC display copy of body wheels currentGear
    float gearMirror = 0.0f;
    float rpmMirrorA = 0.0f;  // car 0x32EC car 0x32F0
    float rpmMirrorB = 0.0f;

    // boost car 0x3300 0 idle 1 push 2 decay 3 end
    int32_t boostState = 0;
    int32_t boostKind = 0;  // car 0x3304 car 0x3308
    float boostDecayStrength = 0.0f;
    float boostTargetKmh = 0.0f;  // car 0x330C car 0x3310
    int64_t boostStartMs = 0;

    uint8_t reverseFlag = 0;  // car 0x332D car 0x332C cleared by car input poll dispatch
    uint8_t brakeLatch = 0;

    int32_t lapCheckpointCounter = 0;  // car 0x3334 car 0x3338
    float checkpointBearing = 0.0f;

    int32_t vehicleKind = 0; // car 0x33B4

    int32_t partSlotType = 0;  // car 0x3520 driver slot indexes sound bank and anim table 0x1AF2CAC plus car 0x3520 times 0x7C0 animation state host fills
    int32_t driverAnimState = 0;
    int32_t finishRank = -1;  // car 0x3724 S2C 0x3D rank minus one while racing car 0x6C0 cell name kept while car 0x6BC is minus one
    char boostPadCellName[35] = {};
    float shakeSumX = 0.0f, shakeSumY = 0.0f, shakeSumZ = 0.0f; // car 0x3320 0x3324 0x3328

    // drift car 0x35A4
    int32_t driftState = 0;
    float driftGauge = 0.0f;  // car 0x35A8 car 0x35AC
    float driftGaugeSmoothed = 0.0f;
    int32_t miniTurboStage = 0;  // car 0x35F0 car 0x35F8
    int64_t miniTurboTimestampMs = 0;
    int32_t gaugeThresholdCount = 0; // car 0x3600

    float steeringGainYaw = 0.0f;  // car 0x2974 steering gain written by the drift update car 0x2A78
    float steeringGainA = 0.0f;
    float steeringGainB = 0.0f;   // car 0x2A7C

    EffectState effect; // car 0x36A8 family

    float airborneFlag = 0.0f;  // car 0x3716 set with four wheels in the air cleared on landing stored 0 or 1 car 0x3718
    float airTimeScale = 0.0f;
    float airPeakHeight = 0.0f; // car 0x371C

    int32_t slowFlag = 0;  // car 0x3720 car 0x373C
    uint8_t landedFlag = 0;
    int64_t landedAtMs = 0;  // car 0x3740

    float bodyPitchOffset = 0.0f; // car 0x3738

    int32_t draftActiveFlag = 0; // car 0x3715

    Transform3 carMatrix;  // car 0x2FE0 the car world matrix wire space column order car 0x3018
    float accumulatedRoll = 0.0f;
    float driftSlipDeg = 0.0f;  // car 0x35B0 z turn model drift steer times smoothed gauge lean update nose lift model y axis wire stat 6
    float leanPitchDeg = 0.0f;
    float leanRollDeg = 0.0f;  // lean update lean model x axis minus wire stat 7 tick owned yaw minus motion angle 180 not client field
    float slipAngleDeg = 0.0f;

    int32_t finishedOrSpectating = 0;  // car 0xA7854 1 finished 2 spectating car 0xA7878 family
    float recoveryPosX = 0.0f, recoveryPosY = 0.0f, recoveryPosZ = 0.0f;

    int32_t turnState = 0; // car 0xA78E4 cosmetic lean mirrors remote turnState

    float spinPeriod = 0.0f;  // car 0xA7994 vehicle kind 5 extra part car 0xA7998
    float spinAccumulator = 0.0f;

    // distance traveled tick owned feeds the wheel spin angle not a documented offset
    float distanceTraveled = 0.0f;
    // the step 11 factor of the last tick the local 7a4 kept for the harness trace
    float throttleFactorLast = 1.0f;
};

// speed curve index 0x49CEFC capped kmh minus 30 times half truncated then clamped 0 to 99
inline int speed_curve_index(float speedCappedKmh) {
    int idx = static_cast<int>((speedCappedKmh - kSpeedCurveOffsetKmh) * kSpeedCurveScale);
    if (idx < 0) idx = 0;
    if (idx > 99) idx = 99;
    return idx;
}

// impact angle index 0x498A06 to 0x498B22 heading folded to 0 90 over 90 clamped 0 1 times 100 truncated
inline int impact_angle_index(float headingDeg) {
    float folded = 0.0f;
    if (headingDeg >= kZero && headingDeg < kAxisCorrection90Deg) folded = kAxisCorrection90Deg - headingDeg;
    else if (headingDeg >= kAxisCorrection90Deg && headingDeg < kCollisionAngleFold180) folded = headingDeg - kAxisCorrection90Deg;
    else if (headingDeg >= kCollisionAngleFold180 && headingDeg < kCollisionAngleFold270) folded = headingDeg - kCollisionAngleFold180;
    else if (headingDeg >= kCollisionAngleFold270 && headingDeg < kCollisionAngleWrap360) folded = headingDeg - kCollisionAngleFold270;
    else return 0; // out of 0 360 keeps the zero on the fpu stack
    float unit = folded * kCollisionAngleToUnit;
    if (unit < kZero) unit = kZero;
    else if (unit > kOne) unit = kOne;
    return static_cast<int>(unit * kCollisionAngleToIndex);
}

// the input manager 0x5DEAF0 the pad 0xE52048 and game 0x94 the tick polls them when armed
struct InputHostState {
    bool armed = false;  // false the host writes the six flags itself the harness does that game 0x94
    InputDeviceIndex device = InputDeviceIndex::Keyboard;
    uint8_t controlsEnabled = 1;  // DAT 00B23614 the second gate of the dispatch input manager 0x5DEAF0 plus 0x100A0
    InputBindingTable bindings;
    InputKeyState keyState{};  // input manager plus 4 by key code 0xE52048
    PadDeviceState pad;
    AnalogInputScratch scratch;  // game 0x98 to 0x2B0 and DAT 02EB0440 FUN 0044B830 the mouse x since the last poll
    int32_t mouseDeltaX = 0;
};

// per race globals the docs name at these game addresses plus the 30 car array itself
struct GameState {
    int32_t raceMode = 0;  // DAT 00b2360c race mode switch 0xb 0xd 0xf 0x11 0x19 DAT 01a20b21 4 or 5 is green gates start boost
    uint8_t startLightState = 0;
    uint8_t sessionRunning = 0;  // game 0x1397384 DAT 01af2b5c family world object loaded and armed
    uint8_t worldReady = 0;
    int32_t localCarIndex = 0;  // game 0x6B0 game 0x28 set while coasting airborne under speed 1 clears the accel flag
    int32_t stoppedFlag = 0;
    float deflectHeadingDeg = 0.0f;  // game 0x6FC collision heading the impact angle index reads global 0x2EB0688 ramp of the air kick push
    float airKickRamp = 0.0f;
    // DAT 01AF2B5C record plus 4 the 0xC3 track id of the world the watchdog reads it
    int32_t worldTrackId = 0;
    uint8_t themeSpecialRow = 0;  // world theme is special row 0x487230 theme record field 4 at 0x1312D00 DAT 02EB4828 nonzero blocks kind 5 boost
    uint8_t themeBoostBlocked = 0;
    uint8_t accelKeyRawHeld = 0;  // input key down and pressed on slot 0 binding read at 0x49C884 and by drift update host fills both
    uint8_t accelKeyPressed = 0;
    uint8_t driftKeyHeld = 0;  // input key down on slot 5 binding read by drift update host fills game 0xB0 arms drift slot 5 key
    int32_t driftArmLatch = 0;
    int32_t viewMode = 0;  // global 0x2F0DDB0 value 2 scales the friction pair DAT 01A20658 the local player id set on stage entry
    int32_t localPlayerId = -1;
    std::array<int32_t, 16> standingsPlayerIds{};  // standings 0x2EBD6A0 plus 0xDC0 stride 0xC player id per row DAT 00D6E1D0 cutscene busy flag effect 0x44C ends when clears
    uint8_t cutsceneBusy = 0;
    int32_t licenceTestId = 0;  // DAT 00BFDAC4 the current licence test id in race mode 0xD DAT 00BFDABC counts collisions in licence test 0x17
    int32_t licenceTestCollisionCount = 0;
    InputPressedState keyPressed{};  // input key pressed 0x44B590 by raw key code host fills game 0x5C 0x60 two boost scene floats decay no boost
    float boostNodeFade[2] = {0.0f, 0.0f};
    int64_t boostStartGlobalMs = 0;  // DAT 005C8508 time of last boost start any car 0x1A69708 owned pet list first equipped pet acts in race
    OwnedPetList pets;
    TrackTuning tuning{0.4f, 0.6f, 90.0f};  // 0x5EB6F0 0x5EB6F4 0x5EB6F8 from the 0xC3 record game 0x1397300
    int32_t boostHudFlag = 0;
    float boostHudStrength = 0.0f;  // game 0x1397304 game 0x1397308
    int64_t boostHudMs = 0;

    CrashRecoveryState crashRecovery;  // game 0x13972F4 family game 0x1397368 family
    ProgressWatchdogState progressWatchdog;
    StuckWatchState stuckWatch;  // game 0x1397174 family game 0x1397360
    float draftFactor = 0.0f;
    int64_t rivalCueMs = 0;                 // game 0x1397348 the last rival cue time

    // car launch pad kick 0x497190 arms it kind 3 boost ini row game 0x1397188 one while kick pushes up
    uint8_t engineKickActive = 0;
    float engineKickYawDeg = 0.0f;  // game 0x139718C yaw the launch pad set game 0x1397190 up push per tick row float 3 times 1 6
    float engineKickValue = 0.0f;
    float engineKickDecay = 0.0f;  // game 0x1397194 the per tick decay row float 4 game 0x1397198 decaying drift start torque boost
    float driftStartBoost = 0.0f;

    uint8_t driftChangedLatch = 0;  // game 0x13971a4 drift state changed this tick game 0x13971a8 decays to 0 or grows to 0 4 then clears latch
    float driftChangedValue = 0.0f;

    InputDispatchCounters itemCounters; // 0xBFC3B0 to 0xBFDA1C the twelve counters

    CheckpointList checkpoints;  // global 0x1ADF810 global 0x1AE2224 loader hook
    std::vector<RespawnGridEntry> respawnGrid;

    std::array<GimmickBumpSlot, GIMMICK_BUMP_TABLE_SIZE> itembiteTable{};  // 0x2EF3000 0x2EF48E8
    std::array<GimmickBumpSlot, GIMMICK_BUMP_TABLE_SIZE> itemdrumTable{};
    std::array<GimmickHazardSlot, GIMMICK_HAZARD_TABLE_SIZE> hazardTable{};  // 0x2F08E10 0x2F077F0 effect 400
    std::array<GimmickHiveSlot, GIMMICK_HIVE_TABLE_SIZE> hiveTable{};
    std::array<GimmickPoolSlot, GIMMICK_POOL_LIVE_SLOTS> carryPool{};  // 0x2EFC818 effect 600 0x2ECDA38 effect 900
    std::array<GimmickPoolSlot, GIMMICK_POOL_LIVE_SLOTS> effect900Pool{};
    std::vector<GimmickBoostRow> boostRows;                                 // world 0x1ADF810 plus 0x1168 boost ini
    std::array<StatusIdSlot, STATUS_TABLE_SIZE> slowedTable{};
    std::array<StatusIdSlot, STATUS_TABLE_SIZE> cameraReversedTable{};

    CarHooks hooks; // scene node sound and particle calls no op when null

    InputHostState inputHost; // the input manager 0x5DEAF0 and the pad 0xE52048 armed by the host

    std::array<CarState, kCarSlotCount> cars; // the 30 car array stride 0xA7260 in the client
};

} // namespace KnC Kart Client
