// Test the tick module Cookie 01 blind drive then the Race 01 launch and steer press of the recorded ghost
#include "../tick.h"
#include "../constants.h"

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

// The recorded Race 01 ghost of GHOST REFERENCE md the launch and the first steer press are the targets
int race01_ghost_checks(const std::string& dataRoot) {
    std::string trackDir = dataRoot + "/Public/World/Race/Race_01";
    if (!std::filesystem::exists(trackDir)) {
        std::printf("race 01 track folder missing at %s, skipping the ghost checks\n", trackDir.c_str());
        return 0;
    }
    ColTrack track;
    std::string error;
    if (!world_load_track_pieces(track, trackDir, error)) {
        std::printf("race 01 world_load_track_pieces failed, %s\n", error.c_str());
        return 1;
    }
    auto gamePtr = std::make_unique<GameState>();
    GameState& game = *gamePtr;
    game.worldReady = 1;
    game.localCarIndex = 0;
    CarState& car = game.cars[0];
    car.slotOccupied = 1;
    car.slotActive = 1;
    car.fixedStepSeconds = kFixedPhysicsStepSeconds;
    car.frameDt = 0.02f; // the ghost samples every ten ticks of 20 ms
    car.trackProgress = 0;
    for (size_t i = 0; i < KART_STAT_COUNT; ++i) {
        car.stats.base[i] = kGhostStats[i];
        car.stats.bonus[i] = 0.0f;
    }
    // the first ghost sample wire x y z and yaw byte 0
    const float x0 = -321.307f, y0 = 228.570f, z0 = 0.983f;
    SpawnCatalogue catalogue = load_catalogue(dataRoot, car.stats);
    bool created = body_create(car.body, catalogue, catalogue.tireGripBase[0], catalogue.tireGripBase[1],
                                -x0, -y0, z0, 0.0f, 1, track);
    if (!created) {
        std::printf("race 01 body_create failed on the recorded spawn\n");
        return 1;
    }
    // car physics setup 0x49510C body set mass friction 5 1 and car 0x32E8 the channel rates
    body_set_mass_friction(car.body, kSetupChannelRateMass, kSetupChannelRateFriction, car.steeringScale);
    car_ground_flag_set_car(car, 1);
    car.body.wheels.currentGear = 1;
    car.posX = x0; car.posY = y0; car.posZ = z0;
    car.prevPosX = x0; car.prevPosY = y0; car.prevPosZ = z0;
    car.yawDeg = 0.0f;
    car.prevSubstepYawDeg = 0.0f;
    game.accelKeyRawHeld = 1;

    int failures = 0;
    int64_t nowMs = 0;
    auto run = [&](int ticks, int steerRight) {
        for (int t = 0; t < ticks; ++t) {
            car.input.accel = 1;
            car.input.brake = 0;
            car.input.stuck = 0;
            car.input.steerRight = steerRight; // game 0x20 slot 3 the recorded 0x90 press turns the yaw up
            car.input.steerLeft = 0;
            car_physics_tick_local(game, 0, track, nowMs);
            nowMs += 20;
        }
    };
    run(150, 0);
    std::printf("race 01 launch 3 s speed %.1f units per second yaw %.2f z %.3f\n",
                static_cast<double>(car.speed), static_cast<double>(car.yawDeg), static_cast<double>(car.posZ));
    // the recording has 93 7 at 3 s the band stays wide on the low side
    if (has_nan(car.speed) || car.speed < 75.0f || car.speed > 110.0f) {
        std::printf("race 01 launch speed out of the 75 to 110 band\n");
        ++failures;
    }
    if (std::fabs(car.yawDeg) > 1.0f) {
        std::printf("race 01 the straight launch must keep yaw 0\n");
        ++failures;
    }
    float yawBefore = car.yawDeg;
    run(10, 1);
    run(20, 0);
    float yawTurn = car.yawDeg - yawBefore;
    while (yawTurn > 180.0f) yawTurn -= 360.0f;
    while (yawTurn < -180.0f) yawTurn += 360.0f;
    std::printf("race 01 one 0 2 s press on game 0x20 then 0 4 s straight turned the yaw by %.2f\n",
                static_cast<double>(yawTurn));
    // the recording turns 0 to 8 5 on the 0x90 sample the port must turn the same way
    if (has_nan(yawTurn) || yawTurn < 4.0f || yawTurn > 14.0f) {
        std::printf("race 01 the press must turn the yaw up by 4 to 14 degrees\n");
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
    std::string trackDir = dataRoot + "/Public/World/Cookie/Cookie_01";
    if (!std::filesystem::exists(trackDir)) {
        std::printf("cookie 01 track folder missing at %s, skipping\n", trackDir.c_str());
        return 0;
    }

    ColTrack track;
    std::string error;
    if (!world_load_track_pieces(track, trackDir, error)) {
        std::printf("world_load_track_pieces failed, %s\n", error.c_str());
        return 1;
    }

    std::vector<GimmickStartRow> startRows;
    if (!gimmick_load_start(trackDir, startRows, error) || startRows.empty()) {
        std::printf("gimmick_load_start failed or empty, %s, skipping\n", error.c_str());
        return 0;
    }
    const GimmickStartRow& row = startRows.front();
    std::printf("start row 0, x %f y %f z %f heading %f\n", static_cast<double>(row.x),
                static_cast<double>(row.y), static_cast<double>(row.z),
                static_cast<double>(row.heading));
    // Csv columns x y z the world query takes csv y as world z and csv z as height
    float startWorldX = row.x;
    float startWorldY = row.y;
    float startWorldZ = row.z;

    // GameState embeds the 30 car array by value far too large for the default thread stack
    auto gamePtr = std::make_unique<GameState>();
    GameState& game = *gamePtr;
    game.worldReady = 1;
    game.localCarIndex = 0;
    game.raceMode = 0; // not 0xd the item counter gate stays open
    game.startLightState = 0;

    CarState& car = game.cars[0];
    car.slotOccupied = 1;
    car.slotActive = 1;
    car.fixedStepSeconds = kFixedPhysicsStepSeconds; // game 0x30 the tyre slip relaxation needs the client step
    car.frameDt = 1.0f / 60.0f;
    car.vehicleKind = 0;
    car.trackProgress = 0; // car 0x6BC 0 drives free 100 to 299 is the respawn teleport with the brake forced

    // the basic 1 wire stats of the recorded ghost wire 1 max speed 2 steer 12 grip and so on
    for (size_t i = 0; i < KART_STAT_COUNT; ++i) {
        car.stats.base[i] = kGhostStats[i];
        car.stats.bonus[i] = 0.0f;
    }

    SpawnCatalogue catalogue = load_catalogue(dataRoot, car.stats);
    // body place probe minus x minus y z minus yaw 0x4A343D to 0x4A346C grips catalogue 0x130 0x134 car 0x32DC 0x32E0
    bool created = body_create(car.body, catalogue, catalogue.tireGripBase[0], catalogue.tireGripBase[1],
                                -startWorldX, -startWorldY, startWorldZ, -row.heading, 1, track);
    std::printf("body_create returned %s\n", created ? "true" : "false");
    body_set_mass_friction(car.body, kSetupChannelRateMass, kSetupChannelRateFriction, car.steeringScale);
    car_ground_flag_set_car(car, 1);
    car.body.wheels.currentGear = 1;
    // the ground bytes stay 0 loaded as the client record starts a byte 1 is a wheel in the air

    car.posX = startWorldX;
    car.posY = startWorldY;
    car.posZ = startWorldZ;
    car.prevPosX = startWorldX;
    car.prevPosY = startWorldY;
    car.prevPosZ = startWorldZ;
    car.yawDeg = row.heading;
    car.prevSubstepYawDeg = row.heading;
    car.engineForceBase = 1400.0f; // kept for the harness the tick reads the body R instead
    game.accelKeyRawHeld = 1;

    float startX = car.posX, startY = car.posY;
    int64_t nowMs = 0;
    int failures = 0;
    bool anyNaN = false;
    int heightChecks = 0;
    int bandLeaves = 0;

    BspQuery verifyQuery;

    auto printState = [&](int second) {
        std::printf("t %ds pos %.3f %.3f %.3f speed_kmh %.3f gear %d surface %d\n", second,
                    static_cast<double>(car.posX), static_cast<double>(car.posY),
                    static_cast<double>(car.posZ), static_cast<double>(car.speedKmh),
                    car.body.wheels.currentGear, static_cast<int>(car.wheelsTick[0].steerAngle));

        float groundZ = 0.0f;
        int groundSurface = 0;
        bool onGround = world_locate_piece_by_height(verifyQuery, track, car.posX, car.posY, car.posZ,
                                                      &groundZ, &groundSurface);
        if (onGround) {
            ++heightChecks;
            float heightDiff = std::fabs(car.posZ - groundZ);
            std::printf("  ground plane %.3f, height diff %.3f\n", static_cast<double>(groundZ),
                        static_cast<double>(heightDiff));
            if (heightDiff > 5.0f) {
                std::printf("  car height left the 5 unit band around the ground plane\n");
                ++bandLeaves; // Cookie 01 has crests the blind drive jumps off this counts not fails
            }
        }
    };

    float maxSpeedStatKmh =
        (stat_total(car.stats, KartStatIndex::MaxSpeed) + 1.0f) * kMaxSpeedToKmh + 1.0f; // small slack

    for (int second = 1; second <= 10; ++second) {
        for (int f = 0; f < 60; ++f) {
            car.input.accel = 1;
            car.input.brake = 0;
            car.input.steerRight = 0;
            car.input.steerLeft = 0;
            car_physics_tick_local(game, 0, track, nowMs);
            nowMs += 16;
            if (has_nan(car.posX) || has_nan(car.posY) || has_nan(car.posZ) || has_nan(car.speedKmh)) {
                anyNaN = true;
            }
        }
        printState(second);
    }

    for (int second = 11; second <= 15; ++second) {
        for (int f = 0; f < 60; ++f) {
            car.input.accel = 1;
            car.input.brake = 0;
            car.input.steerRight = 1;
            car.input.steerLeft = 0;
            car_physics_tick_local(game, 0, track, nowMs);
            nowMs += 16;
            if (has_nan(car.posX) || has_nan(car.posY) || has_nan(car.posZ) || has_nan(car.speedKmh)) {
                anyNaN = true;
            }
        }
        printState(second);
    }

    if (anyNaN) {
        std::printf("a printed value was NaN or infinite\n");
        ++failures;
    }

    float dx = car.posX - startX;
    float dy = car.posY - startY;
    float traveled = std::sqrt(dx * dx + dy * dy);
    std::printf("distance traveled from start, %f\n", static_cast<double>(traveled));
    if (traveled < 1.0f) {
        std::printf("the car did not move forward\n");
        ++failures;
    }

    std::printf("height checks that located on the track, %d of 15, %d left the 5 unit band\n", heightChecks,
                bandLeaves);

    std::printf("max speed stat bound %f kmh, final speed %f kmh\n", static_cast<double>(maxSpeedStatKmh),
                static_cast<double>(car.speedKmh));
    if (car.speedKmh > maxSpeedStatKmh) {
        std::printf("speed exceeded the max speed stat bound\n");
        ++failures;
    }

    failures += race01_ghost_checks(dataRoot);

    if (failures == 0) {
        std::printf("tick_test PASS\n");
        return 0;
    }
    std::printf("tick_test FAIL %d\n", failures);
    return 1;
}
