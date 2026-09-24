// Test the drift path against the stock client the first 30 s of the drift recording of GHOST REFERENCE md
#include "../tick.h"
#include "../boost.h"
#include "../constants.h"
#include "../gimmicks.h"
#include "../input.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace KnC::Kart::Client;

namespace {

std::string client_data_dir() {
    const char* env = std::getenv("KNC_CLIENT_DATA");
    if (env && env[0] != '\0') return env;
    return "Data";
}

// the recordings live in tools replay recordings the build passes the folder an env var overrides it
std::string recordings_dir() {
    const char* env = std::getenv("KNC_RECORDINGS_DIR");
    if (env && env[0] != '\0') return env;
#ifdef KNC_RECORDINGS_DIR
    return KNC_RECORDINGS_DIR;
#else
    return "tools/replay/recordings";
#endif
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

// the 17 stat floats the login burst sent for template 10010 on the recording day see GHOST REFERENCE md
const float kGhostStats[KART_STAT_COUNT] = {0.52f, 0.52f, 0.52f, 0.52f, 0.30f, 0.52f, 0.30f, 0.30f, 0.52f,
                                            0.52f, 0.52f, 0.70f, 0.52f, 0.0f,  9.0f,  37.0f, 3.5f};

constexpr float kSampleSeconds = 0.2f;
constexpr int kTicksPerSample = 10;
constexpr float kTickSeconds = 0.02f;
constexpr float kYawByteToDeg = 360.0f / 255.0f;
constexpr size_t kSliceSamples = 150; // 30 s two drift runs two releases two mini turbos and the brakes

uint32_t read_u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

float read_f32(const uint8_t* p) {
    uint32_t u = read_u32(p);
    float f = 0.0f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

// the KCGR file of tools replay a 28 byte frame per sample after the header see docs tools README md
bool load_kcgr(const std::string& path, std::vector<GhostSample>& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.size() < 4 || std::memcmp(data.data(), "KCGR", 4) != 0) return false;
    size_t off = 4 + 4 + 4 + 4 + 4 + 4;
    if (data.size() < off + 4) return false;
    const uint32_t frames = read_u32(&data[off]);
    off += 4 + 0x2C + 0x38;
    if (data.size() < off + 2) return false;
    const size_t nameLen = static_cast<size_t>(data[off]) | (static_cast<size_t>(data[off + 1]) << 8);
    off += 2 + nameLen;
    if (data.size() < off + static_cast<size_t>(frames) * 28) return false;
    out.clear();
    for (uint32_t i = 0; i < frames; ++i) {
        const uint8_t* p = &data[off + static_cast<size_t>(i) * 28];
        GhostSample s;
        s.pos[0] = read_f32(p);
        s.pos[1] = read_f32(p + 4);
        s.pos[2] = read_f32(p + 8);
        s.yawByte = p[0x0C];
        s.flags = static_cast<uint16_t>(read_u32(p + 0x10) & 0xFFFF);
        s.nibbles = static_cast<uint8_t>(read_u32(p + 0x14) & 0xFF);
        s.inputMask = p[0x18];
        out.push_back(s);
    }
    return true;
}

float wrap_degrees(float deg) {
    deg = std::fmod(deg, 360.0f);
    if (deg < 0.0f) deg += 360.0f;
    return deg;
}

float yaw_gap(float a, float b) {
    float d = wrap_degrees(a) - wrap_degrees(b);
    if (d > 180.0f) d -= 360.0f;
    if (d < -180.0f) d += 360.0f;
    return d;
}

// the status flags arm the drift state the gauge from the nibble and stage 1 as the harness does
void arm_from_flags(GameState& game, CarState& car, const GhostSample& s, int64_t nowMs) {
    int driftState = car.driftState;
    if (s.flags & 0x0010) driftState = 0;
    if (s.flags & 0x0100) driftState = 1;
    if (s.flags & 0x0200) driftState = 2;
    game.driftKeyHeld = driftState != 0 ? 1 : 0;
    if (driftState != car.driftState) {
        car.driftState = driftState;
        car.miniTurboTimestampMs = nowMs;
        if (driftState != 0) {
            car.driftGauge = static_cast<float>(s.nibbles & 0xF) * 6.0f - 45.0f;
            car.driftGaugeSmoothed = car.driftGauge;
            game.driftStartBoost = 1.0f;
        }
    }
    if ((s.flags & 0x08) && car.driftState != 0 && car.miniTurboStage == 0) car.miniTurboStage = 1;
}

// the boost bit of the flags starts a boost the port missed and clears one the client no longer has
void arm_boost_from_flags(GameState& game, CarState& car, uint16_t flags, int64_t nowMs) {
    const uint8_t low = static_cast<uint8_t>(flags & 0xFF);
    const bool boosting = (low & 0x80) != 0 || (low & 0x40) != 0;
    const int kind = (low & 0x80) ? 0 : 1;
    if (boosting && car.boostState == 0) car_boost_start(game, 0, kind, nowMs);
    if (!boosting && car.boostState != 0) car_boost_clear(game, 0);
}

// the harness resync moves T turns R about world z by the yaw delta and puts the velocity back
void resync_pose(CarBody& body, float negPosX, float negPosY, float posZ, float negYawDeltaDeg, const Vec3& velocity,
                 const ColTrack& track) {
    WheelSet& w = body.wheels;
    Vec3 pos{negPosX, negPosY, posZ};
    Vec3 up{0.0f, 0.0f, 1.0f};
    Quat dq = body_quat_from_axis_angle(up, negYawDeltaDeg * kDegToRadPlace);
    Mat3 rd;
    body_quat_to_matrix(rd, dq);
    Mat3 rNew;
    body_mat3_multiply(rNew, rd, w.orientationR);
    w.orientationR = rNew;
    body_mat3_to_quat(w.orientationQ, rNew);
    Vec3 originWorld = body_mat3_transform_vec(rNew, w.originOffset);
    body_vec3_sub(w.translationT, pos, originWorld);
    w.velocity = velocity;
    body.referenceOrientation = w.orientationQ;
    body_update_wheel_transforms(body);
    body_update_transform(body);
    for (int i = 0; i < 4; ++i) {
        const Vec3& hub = body.hubWorld[i];
        float y = 0.0f;
        int surface = 0;
        if (world_locate_piece_by_height(body.wheelQuery[i], track, -hub.x, -hub.y, hub.z, &y, &surface)) {
            w.contactPlane[i] = body.wheelQuery[i].cell;
        }
    }
}

struct ClassSum {
    const char* name;
    int n = 0;
    double gap = 0.0;
    double yaw = 0.0;
    double gapMax = 0.0;
};

int drift_replay_checks(const std::string& dataRoot, const std::string& ghostPath) {
    std::string trackDir = dataRoot + "/Public/World/Race/Race_01";
    if (!std::filesystem::exists(trackDir)) {
        std::printf("race 01 track folder missing at %s, skipping the drift replay checks\n", trackDir.c_str());
        return 0;
    }
    std::vector<GhostSample> samples;
    if (!load_kcgr(ghostPath, samples) || samples.size() < kSliceSamples + 2) {
        std::printf("drift recording missing or short at %s\n", ghostPath.c_str());
        return 1;
    }
    ColTrack track;
    std::string error;
    if (!world_load_track_pieces(track, trackDir, error)) {
        std::printf("race 01 world_load_track_pieces failed, %s\n", error.c_str());
        return 1;
    }
    auto gamePtr = std::make_unique<GameState>();
    GameState& game = *gamePtr;
    if (!gimmick_load_boost(trackDir, game.boostRows, error)) {
        std::printf("race 01 boost ini failed, %s\n", error.c_str());
        return 1;
    }
    game.tuning = TrackTuning{0.4f, 0.6f, 90.0f}; // the 0xC3 record of Race 01
    game.worldReady = 1;
    game.sessionRunning = 1;
    game.localCarIndex = 0;
    game.raceMode = 0;
    CarState& car = game.cars[0];
    car.slotOccupied = 1;
    car.slotActive = 1;
    car.fixedStepSeconds = kFixedPhysicsStepSeconds;
    car.frameDt = kTickSeconds;
    car.trackProgress = 0;
    for (size_t i = 0; i < KART_STAT_COUNT; ++i) {
        car.stats.base[i] = kGhostStats[i];
        car.stats.bonus[i] = 0.0f;
    }
    const GhostSample& first = samples.front();
    const float x0 = first.pos[0], y0 = first.pos[1], z0 = first.pos[2];
    const float yaw0 = static_cast<float>(first.yawByte) * kYawByteToDeg;
    SpawnCatalogue catalogue = load_catalogue(dataRoot, car.stats);
    if (!body_create(car.body, catalogue, catalogue.tireGripBase[0], catalogue.tireGripBase[1], -x0, -y0, z0, -yaw0,
                     1, track)) {
        std::printf("body_create failed on the first sample of the drift recording\n");
        return 1;
    }
    body_set_mass_friction(car.body, kSetupChannelRateMass, kSetupChannelRateFriction, car.steeringScale);
    car_ground_flag_set_car(car, 1);
    car.body.wheels.currentGear = 1;
    car.posX = x0; car.posY = y0; car.posZ = z0;
    car.prevPosX = x0; car.prevPosY = y0; car.prevPosZ = z0;
    car.yawDeg = yaw0;
    car.prevSubstepYawDeg = yaw0;

    ClassSum classes[4] = {{"drift key down"}, {"two after a release"}, {"brake no drift"}, {"gas no brake"}};
    auto sample_class = [&](size_t i, int which) -> bool {
        const uint8_t mk = samples[i].inputMask;
        const bool drift = (mk & GHOST_MASK_SLOT5_DRIFT) != 0;
        if (which == 0) return drift;
        if (which == 1) {
            if (drift) return false;
            const bool one = i >= 1 && (samples[i - 1].inputMask & GHOST_MASK_SLOT5_DRIFT) != 0;
            const bool two = i >= 2 && (samples[i - 2].inputMask & GHOST_MASK_SLOT5_DRIFT) != 0;
            return one || two;
        }
        if (which == 2) return (mk & GHOST_MASK_SLOT1_BRAKE) != 0 && !drift;
        return (mk & GHOST_MASK_SLOT0_ACCELERATE) != 0 && (mk & GHOST_MASK_SLOT1_BRAKE) == 0 && !drift;
    };

    int64_t nowMs = 0;
    int prevAccel = 0;
    bool prevKey = false;
    int portMiniTurbos = 0;   // kind 0 boosts port started on gas edge after release lateClears drift states still set 4s after key up
    int lateClears = 0;
    int64_t releaseMs = -1;
    int failures = 0;
    for (size_t i = 0; i < kSliceSamples; ++i) {
        const GhostSample& next = samples[i + 1];
        for (int tick = 0; tick < kTicksPerSample; ++tick) {
            // the mask is a snapshot on its sample tick so the next mask takes over at the centre tick
            const size_t maskIndex = (tick < kTicksPerSample / 2) ? i : i + 1;
            const GhostSample& sm = samples[maskIndex];
            input_flags_from_ghost_mask(sm.inputMask, car.input);
            car.input.stuck = 0;
            game.driftKeyHeld = (sm.inputMask & GHOST_MASK_SLOT5_DRIFT) ? 1 : 0;
            game.accelKeyRawHeld = car.input.accel;
            game.accelKeyPressed = (car.input.accel != 0 && prevAccel == 0) ? 1 : 0;
            prevAccel = car.input.accel;
            // the release is the mask bit going down while the port still holds a drift state
            const bool keyNow = game.driftKeyHeld != 0;
            if (prevKey && !keyNow && car.driftState != 0) releaseMs = nowMs;
            prevKey = keyNow;
            const int stateBefore = car.driftState;
            if (tick == kTicksPerSample / 2) arm_from_flags(game, car, sm, nowMs);
            // the side bits are the steer keys at the poll one tick before the record
            car.input.steerLeft = static_cast<uint8_t>((sm.flags & 0x0004) ? 1 : 0);
            car.input.steerRight = static_cast<uint8_t>((sm.flags & 0x0002) ? 1 : 0);
            if (tick == kTicksPerSample - 1) arm_boost_from_flags(game, car, sm.flags, nowMs);
            const int boostBefore = car.boostState;
            car_physics_tick_local(game, 0, track, nowMs);
            if (car.boostState != 0 && boostBefore == 0 && car.boostKind == 0 && car.trackProgress != -1 &&
                releaseMs >= 0 && nowMs - releaseMs < 400) {
                ++portMiniTurbos;
            }
            if (car.driftState == 0 && stateBefore != 0 && releaseMs >= 0) {
                if (nowMs - releaseMs > 400) ++lateClears;
                releaseMs = -1;
            }
            nowMs += 20;
        }
        if (std::isnan(car.posX) || std::isnan(car.posY) || std::isnan(car.posZ)) {
            std::printf("the port produced NaN at sample %zu\n", i);
            return failures + 1;
        }
        const float dx = car.posX - next.pos[0], dy = car.posY - next.pos[1];
        const float horiz = std::sqrt(dx * dx + dy * dy);
        const float recYaw = static_cast<float>(next.yawByte) * kYawByteToDeg;
        const float portYawByte = static_cast<float>(static_cast<int>(wrap_degrees(car.yawDeg) / kYawByteToDeg)) * kYawByteToDeg;
        const float yawByteGap = std::fabs(yaw_gap(portYawByte, recYaw));
        for (int c = 0; c < 4; ++c) {
            if (!sample_class(i, c)) continue;
            ClassSum& cs = classes[c];
            ++cs.n;
            cs.gap += horiz;
            cs.yaw += yawByteGap;
            if (horiz > cs.gapMax) cs.gapMax = horiz;
        }
        // the resync of the harness the pose goes back on the sample the velocity is the central difference
        Vec3 keepVelocity = car.body.wheels.velocity;
        if (i + 2 < samples.size()) {
            const GhostSample& after = samples[i + 2];
            const GhostSample& s = samples[i];
            keepVelocity.x = -(after.pos[0] - s.pos[0]) / (2.0f * kSampleSeconds);
            keepVelocity.y = -(after.pos[1] - s.pos[1]) / (2.0f * kSampleSeconds);
            keepVelocity.z = (after.pos[2] - s.pos[2]) / (2.0f * kSampleSeconds);
        }
        const float recYawCentre = recYaw + kYawByteToDeg * kHalf;
        const float yawDelta = yaw_gap(recYawCentre, car.yawDeg);
        car.posX = next.pos[0]; car.posY = next.pos[1]; car.posZ = next.pos[2];
        car.prevPosX = car.posX; car.prevPosY = car.posY; car.prevPosZ = car.posZ;
        car.yawDeg = recYawCentre;
        car.prevSubstepYawDeg = recYawCentre;
        resync_pose(car.body, -car.posX, -car.posY, car.posZ, -yawDelta, keepVelocity, track);
    }

    for (const ClassSum& cs : classes) {
        if (cs.n == 0) continue;
        std::printf("class %-20s n %3d gap %.3f max %.3f yaw byte %.2f\n", cs.name, cs.n, cs.gap / cs.n, cs.gapMax,
                    cs.yaw / cs.n);
    }
    std::printf("port mini turbos on the gas edge after a release %d, drift states cleared late %d\n", portMiniTurbos,
                lateClears);

    // the drift samples read 0 32 units and 0 96 degrees on the closed run the bounds leave room
    if (classes[0].n < 20 || classes[0].gap / classes[0].n > 0.6 || classes[0].yaw / classes[0].n > 2.0) {
        std::printf("the drift samples must stay under 0 6 units and 2 degrees a sample\n");
        ++failures;
    }
    // the two samples after a release read 0 80 units and 3 2 degrees the unwinding and the counter steer
    if (classes[1].n < 8 || classes[1].gap / classes[1].n > 1.5 || classes[1].yaw / classes[1].n > 6.0) {
        std::printf("the two samples after a release must stay under 1 5 units and 6 degrees a sample\n");
        ++failures;
    }
    // the brake samples read 0 53 units and 1 9 degrees with the brake channel rate of 1 per second
    if (classes[2].n < 20 || classes[2].gap / classes[2].n > 1.0 || classes[2].yaw / classes[2].n > 4.0) {
        std::printf("the brake samples must stay under 1 0 units and 4 degrees a sample\n");
        ++failures;
    }
    // the slice holds three releases followed by a gas edge inside the unwinding the port fires all three
    if (portMiniTurbos < 3) {
        std::printf("the port must fire the kind 0 boost on the gas edge after a release three times not %d\n",
                    portMiniTurbos);
        ++failures;
    }
    // the gauge decays 120 a second so the state clears inside 0 4 s of the key going up
    if (lateClears != 0) {
        std::printf("the drift state must clear inside 0 4 s of the release %d cleared later\n", lateClears);
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
    std::string ghostPath = recordings_dir() + "/drift_t90_c7_20260915.ghost";
    if (!std::filesystem::exists(ghostPath)) {
        std::printf("drift recording missing at %s, skipping\n", ghostPath.c_str());
        return 0;
    }
    int failures = drift_replay_checks(dataRoot, ghostPath);
    if (failures != 0) {
        std::printf("drift replay test failed with %d failures\n", failures);
        return 1;
    }
    std::printf("drift replay test ok\n");
    return 0;
}
