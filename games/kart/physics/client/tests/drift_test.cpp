// Test the drift machine a scripted right drift on the Race 01 launch straight gauge slip mini turbo unwinding
#include "../tick.h"
#include "../constants.h"
#include "../drift.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

using namespace KnC::Kart::Client;

namespace {

std::string client_data_dir() {
    const char* env = std::getenv("KNC_CLIENT_DATA");
    if (env && env[0] != '\0') return env;
    return "Data";
}

// car apply kart loadout 0x490A70 reads Data Car basic 1 car then the four setup overrides
SpawnCatalogue load_catalogue(const std::string& dataRoot, const KartStats& stats) {
    SpawnCatalogue cat;
    std::string error;
    if (catalogue_load_car_file(cat, dataRoot + "/Car/basic_1.car", error)) {
        catalogue_apply_setup_overrides(cat);
    } else {
        catalogue_from_stats(stats, 0, cat);
    }
    return cat;
}

// the 17 stat floats the login burst sends for basic 1 see GHOST REFERENCE md
const float kGhostStats[KART_STAT_COUNT] = {0.52f, 0.52f, 0.52f, 0.52f, 0.30f, 0.52f, 0.30f, 0.30f, 0.52f,
                                            0.52f, 0.52f, 0.70f, 0.52f, 0.0f,  9.0f,  37.0f, 3.5f};

bool has_nan(float v) { return std::isnan(v) || std::isinf(v); }

float wrap180(float d) {
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

// one key line of the script the state holds from its time to the next line
struct KeyLine {
    float time;
    int accel;
    int left;
    int right;
    int drift;
};

// start row 7 is the left edge of the grid the right drift has the whole road to cross
constexpr int kStartRow = 7;
// accelerate 0 9 s hold right plus drift 1 s release with a left steer then tap the gas
const KeyLine kScript[] = {
    {0.0f, 1, 0, 0, 0}, {0.9f, 1, 0, 1, 1}, {1.9f, 1, 1, 0, 0}, {2.0f, 0, 1, 0, 0}, {2.1f, 1, 1, 0, 0},
};
constexpr float kEndSeconds = 2.3f; // the straight ends on the right wall soon after the boost

int drift_checks(const std::string& dataRoot) {
    std::string trackDir = dataRoot + "/Public/World/Race/Race_01";
    if (!std::filesystem::exists(trackDir)) {
        std::printf("race 01 track folder missing at %s, skipping the drift checks\n", trackDir.c_str());
        return 0;
    }
    ColTrack track;
    std::string error;
    if (!world_load_track_pieces(track, trackDir, error)) {
        std::printf("race 01 world_load_track_pieces failed, %s\n", error.c_str());
        return 1;
    }
    std::vector<GimmickStartRow> startRows;
    if (!gimmick_load_start(trackDir, startRows, error) || startRows.size() <= kStartRow) {
        std::printf("race 01 start.ini failed or short, %s\n", error.c_str());
        return 1;
    }
    const GimmickStartRow& row = startRows[kStartRow];

    auto gamePtr = std::make_unique<GameState>();
    GameState& game = *gamePtr;
    game.worldReady = 1;
    game.sessionRunning = 1;
    game.localCarIndex = 0;
    CarState& car = game.cars[0];
    car.slotOccupied = 1;
    car.slotActive = 1;
    car.fixedStepSeconds = kFixedPhysicsStepSeconds;
    car.frameDt = 0.02f;
    car.trackProgress = 0;
    for (size_t i = 0; i < KART_STAT_COUNT; ++i) {
        car.stats.base[i] = kGhostStats[i];
        car.stats.bonus[i] = 0.0f;
    }
    // the recorded spawn sits one unit over the ground under the row
    float x0 = row.x, y0 = row.y, z0 = row.z;
    {
        BspQuery probe;
        float ground = 0.0f;
        int surface = 0;
        if (world_locate_piece_by_height(probe, track, x0, y0, z0 + 5.0f, &ground, &surface)) z0 = ground + kOne;
    }
    SpawnCatalogue catalogue = load_catalogue(dataRoot, car.stats);
    bool created = body_create(car.body, catalogue, catalogue.tireGripBase[0], catalogue.tireGripBase[1], -x0, -y0,
                                z0, -row.heading, 1, track);
    if (!created) {
        std::printf("race 01 body_create failed on start row %d\n", kStartRow);
        return 1;
    }
    body_set_mass_friction(car.body, kSetupChannelRateMass, kSetupChannelRateFriction, car.steeringScale);
    car_ground_flag_set_car(car, 1);
    car.body.wheels.currentGear = 1;
    car.posX = x0; car.posY = y0; car.posZ = z0;
    car.prevPosX = x0; car.prevPosY = y0; car.prevPosZ = z0;
    car.yawDeg = row.heading;
    car.prevSubstepYawDeg = row.heading;

    int failures = 0;
    int64_t nowMs = 0;
    const int totalTicks = static_cast<int>(kEndSeconds / 0.02f + 0.5f);
    const size_t lineCount = sizeof(kScript) / sizeof(kScript[0]);
    size_t line = 0;
    int prevAccel = 0;
    float maxSlipDrifting = 0.0f;
    float maxGaugeSize = 0.0f;
    float yawAtRelease = 0.0f;
    float yawAtStateClear = 0.0f;
    int stageOneTick = -1;
    int stageTwoTick = -1;
    int stateClearTick = -1;
    int boostTick = -1;
    int boostKind = -1;
    int firstCollisionTick = -1;
    float slipAtEnd = 0.0f;
    std::printf("   t  acc L R drf | yaw slip | gauge st mt | boost kind kmh\n");
    for (int tick = 0; tick < totalTicks; ++tick) {
        const float t = static_cast<float>(tick) * 0.02f;
        while (line + 1 < lineCount && kScript[line + 1].time <= t + 1e-4f) ++line;
        const KeyLine& k = kScript[line];
        car.input.accel = static_cast<uint8_t>(k.accel);
        car.input.brake = 0;
        car.input.stuck = 0;
        car.input.steerLeft = static_cast<uint8_t>(k.left);
        car.input.steerRight = static_cast<uint8_t>(k.right);
        game.driftKeyHeld = static_cast<uint8_t>(k.drift);
        game.accelKeyRawHeld = static_cast<uint8_t>(k.accel);
        // input manager update keys 0x44B4C0 the pressed edge is the frame the key goes down
        game.accelKeyPressed = (k.accel != 0 && prevAccel == 0) ? 1 : 0;
        prevAccel = k.accel;
        const int stageBefore = car.miniTurboStage;
        const int stateBefore = car.driftState;
        const int boostBefore = car.boostState;
        if (k.drift == 0 && stateBefore != 0 && yawAtRelease == 0.0f) yawAtRelease = car.yawDeg;
        car_physics_tick_local(game, 0, track, nowMs);
        nowMs += 20;
        if (has_nan(car.posX) || has_nan(car.yawDeg) || has_nan(car.driftGauge)) {
            std::printf("nan at %.2f s\n", static_cast<double>(nowMs) / 1000.0);
            return failures + 1;
        }
        if (car.body.collisionHappened == 1 && firstCollisionTick < 0) firstCollisionTick = tick;
        if (car.driftState != 0) {
            float slip = std::fabs(car.slipAngleDeg);
            if (slip > maxSlipDrifting) maxSlipDrifting = slip;
            float g = std::fabs(car.driftGauge);
            if (g > maxGaugeSize) maxGaugeSize = g;
        }
        if (car.miniTurboStage == 1 && stageBefore != 1 && stageOneTick < 0) stageOneTick = tick;
        if (car.miniTurboStage == 2 && stageBefore != 2 && stageTwoTick < 0) stageTwoTick = tick;
        if (car.driftState == 0 && stateBefore != 0 && stateClearTick < 0) {
            stateClearTick = tick;
            yawAtStateClear = car.yawDeg;
        }
        if (car.boostState != 0 && boostBefore == 0 && boostTick < 0) {
            boostTick = tick;
            boostKind = car.boostKind;
        }
        if ((tick + 1) % 10 == 0) {
            std::printf("%5.2f  %d   %d %d %d | %6.1f %6.1f | %6.1f %d %d | %d %d %6.1f\n",
                        static_cast<double>(nowMs) / 1000.0, car.input.accel, car.input.steerLeft,
                        car.input.steerRight, game.driftKeyHeld, static_cast<double>(car.yawDeg),
                        static_cast<double>(car.slipAngleDeg), static_cast<double>(car.driftGauge), car.driftState,
                        car.miniTurboStage, car.boostState, car.boostKind, static_cast<double>(car.speedKmh));
        }
        slipAtEnd = car.slipAngleDeg;
    }
    std::printf("drift max slip %.1f deg max gauge %.1f stage 1 tick %d stage 2 tick %d clear tick %d boost tick %d kind %d "
                "yaw at release %.1f at clear %.1f slip at end %.1f first collision tick %d\n",
                static_cast<double>(maxSlipDrifting), static_cast<double>(maxGaugeSize), stageOneTick, stageTwoTick,
                stateClearTick, boostTick, boostKind, static_cast<double>(yawAtRelease),
                static_cast<double>(yawAtStateClear), static_cast<double>(slipAtEnd), firstCollisionTick);

    // the wall of the straight must stay out of the window else the numbers are the wall not the drift
    if (firstCollisionTick >= 0) {
        std::printf("the car touched the world at tick %d inside the window\n", firstCollisionTick);
        ++failures;
    }
    // 0x49ACD2 the gauge fills at wire stat 8 times half plus 0 3 times 80 a second cap 45
    if (maxGaugeSize < 40.0f) {
        std::printf("the gauge must fill past 40 in a one second drift\n");
        ++failures;
    }
    // the body slides the motion angle lags the yaw by more than 10 degrees while drifting
    if (maxSlipDrifting < 10.0f) {
        std::printf("the slip angle must pass 10 degrees while drifting\n");
        ++failures;
    }
    // 0x49AEE1 stage 1 needs the gauge past 10 times frac12 and 800 times frac13 ms held
    if (stageOneTick < 0 || stageOneTick < 45 + 16 || stageOneTick > 45 + 22) {
        std::printf("stage 1 must arm about 0 35 s after the drift start not at tick %d\n", stageOneTick);
        ++failures;
    }
    // 0x49B001 stage 2 is the drift key release with stage 1 armed
    if (stageTwoTick < 0 || stageTwoTick < 95 || stageTwoTick > 97) {
        std::printf("stage 2 must follow the release at tick 95 not tick %d\n", stageTwoTick);
        ++failures;
    }
    // 0x49B051 accel edge fires stage 3 kind 0 boost clears drift edge must land inside unwinding 18 ticks tick 105
    if (boostTick < 0 || boostKind != BOOST_KIND_MINI_TURBO || boostTick < 105 || boostTick > 106) {
        std::printf("the kind 0 boost must start on the accel edge at tick 105 not tick %d kind %d\n", boostTick,
                    boostKind);
        ++failures;
    }
    // 0x49ADD3 the unwinding turns the yaw on in the drift direction 1 34 degrees a tick
    float unwindTurn = wrap180(yawAtStateClear - yawAtRelease);
    if (stateClearTick < 0 || unwindTurn < 6.0f) {
        std::printf("the unwinding must turn the yaw further right by 6 degrees or more not %.1f\n",
                    static_cast<double>(unwindTurn));
        ++failures;
    }
    if (car.driftState != 0 || car.driftGauge != 0.0f || car.miniTurboStage != 0) {
        std::printf("the drift must be clear at the end state %d gauge %.1f stage %d\n", car.driftState,
                    static_cast<double>(car.driftGauge), car.miniTurboStage);
        ++failures;
    }
    // the body aligns with its motion again once the drift is spent
    if (std::fabs(slipAtEnd) > 6.0f) {
        std::printf("the slip angle must be back under 6 degrees at the end not %.1f\n", static_cast<double>(slipAtEnd));
        ++failures;
    }
    // the visuals read the slip turn the lean and the matrix every tick
    if (car.driftSlipDeg == 0.0f && car.driftGaugeSmoothed != 0.0f) {
        std::printf("the drift slip turn must follow the smoothed gauge\n");
        ++failures;
    }
    return failures;
}

} // namespace

int main() {
    std::string dataRoot = client_data_dir();
    if (!std::filesystem::exists(dataRoot)) {
        std::printf("client data folder missing at %s, skipping\n", dataRoot.c_str());
        return 0;
    }
    int failures = drift_checks(dataRoot);
    if (failures != 0) {
        std::printf("drift test failed with %d failures\n", failures);
        return 1;
    }
    std::printf("drift test ok\n");
    return 0;
}
