// Test the side walls of Race 01 a car driven across the road at full gas stays on the col
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

// the 17 stat floats of basic 1 the same as the tick test
const float kStats[KART_STAT_COUNT] = {0.52f, 0.52f, 0.52f, 0.52f, 0.30f, 0.52f, 0.30f, 0.30f, 0.52f,
                                       0.52f, 0.52f, 0.70f, 0.52f, 0.0f,  9.0f,  37.0f, 3.5f};

// true when the wire point stands inside a cell of any piece the col has no cell past a wall
bool on_col(const ColTrack& track, float x, float y) {
    BspQuery probe;
    for (const ColPiece& piece : track.pieces)
        if (world_bsp_set_piece(probe, piece, -x, -y)) return true;
    return false;
}

// a head on hit at full speed may sink the body a unit or two past the edge
constexpr int kMaxOffTicks = 50;

// one run from the Race 01 grid nose toward a side wall returns the ticks spent off the col
int drive_into_wall(const std::string& dataRoot, const ColTrack& track, float yawDeg, float& outReach,
                    bool& outEndOnCol) {
    auto gamePtr = std::make_unique<GameState>();
    GameState& game = *gamePtr;
    game.worldReady = 1;
    game.localCarIndex = 0;
    CarState& car = game.cars[0];
    car.slotOccupied = 1;
    car.slotActive = 1;
    car.fixedStepSeconds = kFixedPhysicsStepSeconds;
    car.frameDt = 0.02f;
    for (size_t i = 0; i < KART_STAT_COUNT; ++i) car.stats.base[i] = kStats[i];
    SpawnCatalogue catalogue;
    std::string error;
    if (catalogue_load_car_file(catalogue, dataRoot + "/Car/basic_1.car", error))
        catalogue_apply_setup_overrides(catalogue);
    else
        catalogue_from_stats(car.stats, 0, catalogue);
    const float x0 = -321.307f, y0 = 228.570f, z0 = 0.983f;
    if (!body_create(car.body, catalogue, catalogue.tireGripBase[0], catalogue.tireGripBase[1], -x0, -y0, z0,
                     -yawDeg, 1, track)) {
        std::printf("body_create failed on the grid\n");
        return -1;
    }
    body_set_mass_friction(car.body, kSetupChannelRateMass, kSetupChannelRateFriction, car.steeringScale);
    car_ground_flag_set_car(car, 1);
    car.body.wheels.currentGear = 1;
    car.posX = x0; car.posY = y0; car.posZ = z0;
    car.prevPosX = x0; car.prevPosY = y0; car.prevPosZ = z0;
    car.yawDeg = yawDeg;
    car.prevSubstepYawDeg = yawDeg;
    game.accelKeyRawHeld = 1;
    int offTicks = 0;
    outReach = 0.f;
    int64_t nowMs = 0;
    for (int t = 0; t < 400; ++t) {
        car.input.accel = 1;
        car.input.brake = 0;
        car.input.steerLeft = 0;
        car.input.steerRight = 0;
        car_physics_tick_local(game, 0, track, nowMs);
        nowMs += 20;
        const float reach = std::fabs(car.posY - y0);
        if (reach > outReach) outReach = reach;
        if (!on_col(track, car.posX, car.posY)) ++offTicks;
        if (std::getenv("KNC_WALL_TRACE") && t % 5 == 0)
            std::printf("  t %d y %.2f kmh %.1f col %d hit %d\n", t, static_cast<double>(car.posY),
                        static_cast<double>(car.speedKmh), on_col(track, car.posX, car.posY) ? 1 : 0,
                        car.body.collisionHappened);
    }
    outEndOnCol = on_col(track, car.posX, car.posY);
    std::printf("yaw %.0f end %.2f %.2f speed %.1f widest %.2f off the col %d of 400 ticks\n",
                static_cast<double>(yawDeg), static_cast<double>(car.posX), static_cast<double>(car.posY),
                static_cast<double>(car.speedKmh), static_cast<double>(outReach), offTicks);
    return offTicks;
}

} // namespace

int main() {
    const std::string dataRoot = client_data_dir();
    const std::string trackDir = dataRoot + "/Public/World/Race/Race_01";
    if (!std::filesystem::exists(trackDir)) {
        std::printf("race 01 track folder missing at %s, skipping\n", trackDir.c_str());
        return 0;
    }
    ColTrack track;
    std::string error;
    if (!world_load_track_pieces(track, trackDir, error)) {
        std::printf("world_load_track_pieces failed, %s\n", error.c_str());
        return 1;
    }
    int failures = 0;
    // yaw 90 drives toward plus y yaw 270 toward minus y both cross the road into a side wall
    for (float yaw : {90.f, 270.f}) {
        float reach = 0.f;
        bool endOnCol = false;
        const int off = drive_into_wall(dataRoot, track, yaw, reach, endOnCol);
        if (off < 0 || off > kMaxOffTicks || !endOnCol) {
            std::printf("the car left the col through the wall at yaw %.0f\n", static_cast<double>(yaw));
            ++failures;
        }
    }
    if (failures == 0) {
        std::printf("wall_test PASS\n");
        return 0;
    }
    std::printf("wall_test FAIL %d\n", failures);
    return 1;
}
