// Drives the client physics port with the inputs of a real ghost and prints the gap to the recorded samples
#include "ghost_replay.h"

#include "body.h"
#include "boost.h"
#include "constants.h"
#include "gimmicks.h"
#include "input.h"
#include "stats.h"
#include "tick.h"
#include "world_collision.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace KnC::Kart::Client;
using KnC::Tools::GhostRecording;
using KnC::Tools::load_ghost_file;

namespace {

constexpr float kSampleSeconds = 0.2f;  // ten ticks of 20 ms between two ghost samples
constexpr int kTicksPerSample = 10;
constexpr float kTickSeconds = kSampleSeconds / static_cast<float>(kTicksPerSample);
constexpr float kYawByteToDeg = 360.0f / 255.0f;

struct Options {
    std::string track_dir;
    std::string ghost_path;
    // car path is Data Car name car beside the track folder stats are the login burst floats for basic 1
    std::string car_path;
    std::array<float, KART_STAT_COUNT> stats = {0.52f, 0.52f, 0.52f, 0.52f, 0.30f, 0.52f,
                                                0.30f, 0.30f, 0.52f, 0.52f, 0.52f, 0.70f,
                                                0.52f, 0.0f,  9.0f,  37.0f, 3.5f};
    // tuning is track record 0x30 0x34 0x38 of Race 01 engine force is car 0x2610 until the stat formula lands
    float tuning[3] = {0.4f, 0.6f, 90.0f};
    float engine_force = 1400.0f;
    int seconds = 10;
    int vehicle_kind = 0;
    // resync snaps to the sample after compare only every N samples to keep a held turn free
    bool resync = false;
    int resync_every = 1;
    // resync stops at this time for a free slice mask bit 7 is slot 0 accelerate lsb for old files
    float resync_until = 1e9f;
    bool mask_msb = true;
    // green skips the start light keys 1 2 3 start a boost trace prints steer rate spin and yaw
    bool green = false;
    bool trace = false;
    // trace start time in seconds centre covers ticks 10i minus 5 to 10i plus 4 a snapshot not a hold
    float trace_from = 0.0f;
    bool centre = false;
    // flags arm drift and boost and set steer keys of the tick before edges follow the next two sample yaws
    bool flags = false;
    bool edges = false;
    // script drives keys not a ghost time accel brake left right drift item start row is the ini spawn row
    std::string script;
    int start_row = 0;
};

// one line of a key script the state holds from its time until the next line
struct ScriptLine {
    float time = 0.0f;
    int accel = 0;
    int brake = 0;
    int left = 0;
    int right = 0;
    int drift = 0;
    int item = 0;
};

// lines are time accel brake left right drift item a hash starts a comment
bool load_script(const std::string& path, std::vector<ScriptLine>& out, std::string& error) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) {
        error = "cannot open " + path;
        return false;
    }
    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        char* hash = std::strchr(line, '#');
        if (hash) *hash = '\0';
        ScriptLine s;
        int n = std::sscanf(line, "%f %d %d %d %d %d %d", &s.time, &s.accel, &s.brake, &s.left, &s.right, &s.drift,
                            &s.item);
        if (n < 1) continue;
        if (n < 7) {
            error = "a script line needs time and six keys, got " + std::string(line);
            std::fclose(f);
            return false;
        }
        if (!out.empty() && s.time < out.back().time) {
            error = "script times must not go backwards";
            std::fclose(f);
            return false;
        }
        out.push_back(s);
    }
    std::fclose(f);
    if (out.empty()) error = "the script holds no line";
    return !out.empty();
}

bool parse(int argc, char** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--seconds" && has_value) out.seconds = std::atoi(argv[++i]);
        else if (arg == "--engine-force" && has_value) out.engine_force = static_cast<float>(std::atof(argv[++i]));
        else if (arg == "--kind" && has_value) out.vehicle_kind = std::atoi(argv[++i]);
        else if (arg == "--resync") out.resync = true;
        else if (arg == "--resync-every" && has_value) { out.resync = true; out.resync_every = std::atoi(argv[++i]); }
        else if (arg == "--resync-until" && has_value) { out.resync = true; out.resync_until = static_cast<float>(std::atof(argv[++i])); }
        else if (arg == "--mask-lsb") out.mask_msb = false;
        else if (arg == "--green") out.green = true;
        else if (arg == "--trace") out.trace = true;
        else if (arg == "--trace-from" && has_value) { out.trace = true; out.trace_from = static_cast<float>(std::atof(argv[++i])); }
        else if (arg == "--centre" || arg == "--center") out.centre = true;
        else if (arg == "--flags") out.flags = true;
        else if (arg == "--edges") { out.edges = true; out.flags = true; out.centre = true; }
        else if (arg == "--car" && has_value) out.car_path = argv[++i];
        else if (arg == "--script" && has_value) out.script = argv[++i];
        else if (arg == "--start-row" && has_value) out.start_row = std::atoi(argv[++i]);
        else if (arg == "--stats" && i + static_cast<int>(KART_STAT_COUNT) < argc) {
            for (size_t k = 0; k < KART_STAT_COUNT; ++k) out.stats[k] = static_cast<float>(std::atof(argv[++i]));
        }
        else if (arg == "--tuning" && i + 3 < argc) {
            for (float& t : out.tuning) t = static_cast<float>(std::atof(argv[++i]));
        }
        else if (out.track_dir.empty() && arg[0] != '-') out.track_dir = arg;
        else if (out.ghost_path.empty() && arg[0] != '-') out.ghost_path = arg;
        else return false;
    }
    if (out.track_dir.empty()) return false;
    return !out.ghost_path.empty() || !out.script.empty();
}

// The track folder is Data Public World Race Race 01 so the car folder is four levels up
std::string default_car_path(const std::string& trackDir) {
    std::string dir = trackDir;
    for (int up = 0; up < 4; ++up) {
        while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) dir.pop_back();
        const size_t cut = dir.find_last_of("/\\");
        if (cut == std::string::npos) return std::string();
        dir.resize(cut);
    }
    return dir + "/Car/basic_1.car";
}

// car apply kart loadout 0x490A70 reads Data Car name car then writes the four setup overrides
SpawnCatalogue load_catalogue(const Options& options) {
    SpawnCatalogue cat;
    std::string path = options.car_path.empty() ? default_car_path(options.track_dir) : options.car_path;
    std::string error;
    if (!path.empty() && catalogue_load_car_file(cat, path, error)) {
        catalogue_apply_setup_overrides(cat);
        std::printf("catalogue %s\n", path.c_str());
    } else {
        KartStats stats;
        catalogue_from_stats(stats, 0, cat);
        std::printf("catalogue default.car values, %s\n", error.empty() ? "no car path" : error.c_str());
    }
    return cat;
}

// Ring entry 0x18 decoded by the port slot 0 accelerate on bit 7 slot 2 left slot 3 right
void apply_mask(uint8_t mask, bool msb, InputFlags& input) {
    if (msb) {
        input_flags_from_ghost_mask(mask, input);
        return;
    }
    input.accel = mask & 1u;
    input.brake = (mask >> 1) & 1u;
    input.steerLeft = (mask >> 2) & 1u;
    input.steerRight = (mask >> 3) & 1u;
}

// The boost bit 0x80 is car 0x3304 kind 0 and 0x40 any other kind only a missed boost starts here
void arm_boost_from_flags(GameState& game, CarState& car, uint16_t flags, int64_t nowMs) {
    const uint8_t low = static_cast<uint8_t>(flags & 0xFF);
    bool boosting = (low & 0x80) != 0 || (low & 0x40) != 0;
    int kind = (low & 0x80) ? 0 : 1;
    if (boosting && car.boostState == 0) car_boost_start(game, 0, kind, nowMs);
    if (!boosting && car.boostState != 0) car_boost_clear(game, 0);
}

// The status flags drive the drift state the mask never carries slot 5
void arm_from_flags(GameState& game, CarState& car, const KnC::Kart::Client::GhostSample& s, int64_t nowMs) {
    const uint8_t low = static_cast<uint8_t>(s.flags & 0xFF);
    int driftState = car.driftState;
    if (s.flags & 0x0010) driftState = 0;
    if (s.flags & 0x0100) driftState = 1;
    if (s.flags & 0x0200) driftState = 2;
    game.driftKeyHeld = driftState != 0 ? 1 : 0;
    if (driftState != car.driftState) {
        car.driftState = driftState;
        car.miniTurboTimestampMs = nowMs;
        if (driftState != 0) {
            // the recorded nibble is the gauge plus 45 over 6 the unit of the wire decode
            car.driftGauge = static_cast<float>(s.nibbles & 0xF) * 6.0f - 45.0f;
            car.driftGaugeSmoothed = car.driftGauge;
            game.driftStartBoost = 1.0f;
        }
    }
    // stage 1 only inside a drift the client zeroes the stage with the state a forced one never outlives it
    if ((low & 0x08) && car.driftState != 0 && car.miniTurboStage == 0) car.miniTurboStage = 1;
}

// car 0xa78e4 is 1 for slot 2 and 2 for slot 3 polled one tick before the record
void apply_turn_side(uint16_t flags, InputFlags& input) {
    input.steerLeft = (flags & 0x0004) ? 1 : 0;
    input.steerRight = (flags & 0x0002) ? 1 : 0;
}

// resync moves T turns R about z by yaw delta lean survives a bank pose unloads wheels for two ticks
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

// the script mode drives the port with the key lines and prints the drift state every 0 2 s
int run_script(const Options& options, const ColTrack& track, const std::vector<ScriptLine>& script, float x0,
               float y0, float z0, float yaw0) {
    auto gamePtr = std::make_unique<GameState>();
    GameState& game = *gamePtr;
    std::string error;
    if (!gimmick_load_boost(options.track_dir, game.boostRows, error)) {
        std::printf("boost.ini not loaded, %s\n", error.c_str());
    }
    game.tuning = TrackTuning{options.tuning[0], options.tuning[1], options.tuning[2]};
    game.worldReady = 1;
    game.sessionRunning = 1;
    game.localCarIndex = 0;
    game.raceMode = 0;
    game.startLightState = options.green ? 5 : 0;

    CarState& car = game.cars[0];
    car.slotOccupied = 1;
    car.slotActive = 1;
    car.fixedStepSeconds = kFixedPhysicsStepSeconds;
    car.frameDt = kTickSeconds;
    car.vehicleKind = options.vehicle_kind;
    car.trackProgress = 0;
    for (size_t i = 0; i < KART_STAT_COUNT; ++i) {
        car.stats.base[i] = options.stats[i];
        car.stats.bonus[i] = 0.0f;
    }
    SpawnCatalogue catalogue = load_catalogue(options);
    const bool created = body_create(car.body, catalogue, catalogue.tireGripBase[0], catalogue.tireGripBase[1],
                                     -x0, -y0, z0, -yaw0, 1, track);
    std::printf("body_create %s at %.3f %.3f %.3f yaw %.1f, max steer %.1f deg ackermann %.3f\n",
                created ? "ok" : "failed", static_cast<double>(x0), static_cast<double>(y0),
                static_cast<double>(z0), static_cast<double>(yaw0), static_cast<double>(catalogue.maxSteerDeg),
                static_cast<double>(car.body.wheels.ackermannRatio));
    if (!created) return 1;
    body_set_mass_friction(car.body, kSetupChannelRateMass, kSetupChannelRateFriction, car.steeringScale);
    car_ground_flag_set_car(car, 1);
    car.body.wheels.currentGear = 1;
    car.posX = x0; car.posY = y0; car.posZ = z0;
    car.prevPosX = x0; car.prevPosY = y0; car.prevPosZ = z0;
    car.yawDeg = yaw0;
    car.prevSubstepYawDeg = yaw0;
    car.engineForceBase = options.engine_force;

    const float endSeconds = static_cast<float>(options.seconds);
    const int totalTicks = static_cast<int>(endSeconds / kTickSeconds + 0.5f);
    std::printf("   t  acc brk L R drf | x y z | yaw motion slip | gauge smooth st mt | boost kind kmh | spinz steer roll pitch lean\n");
    int64_t nowMs = 0;
    size_t lineIndex = 0;
    int prevAccel = 0;
    for (int tick = 0; tick < totalTicks; ++tick) {
        const float t = static_cast<float>(tick) * kTickSeconds;
        while (lineIndex + 1 < script.size() && script[lineIndex + 1].time <= t + 1e-4f) ++lineIndex;
        const ScriptLine& s = script[lineIndex];
        car.input.accel = static_cast<uint8_t>(s.accel);
        car.input.brake = static_cast<uint8_t>(s.brake);
        car.input.steerLeft = static_cast<uint8_t>(s.left);
        car.input.steerRight = static_cast<uint8_t>(s.right);
        car.input.stuck = 0;
        game.driftKeyHeld = static_cast<uint8_t>(s.drift);
        game.accelKeyRawHeld = static_cast<uint8_t>(s.accel);
        // input manager update keys 0x44B4C0 marks a key pressed on the frame it goes down only
        game.accelKeyPressed = (s.accel != 0 && prevAccel == 0) ? 1 : 0;
        prevAccel = s.accel;
        const int boostBefore = car.boostState;
        const int stageBefore = car.miniTurboStage;
        const int stateBefore = car.driftState;
        car_physics_tick_local(game, 0, track, nowMs);
        nowMs += 20;
        if (car.driftState != stateBefore) {
            std::printf("  drift state %d at %.2f s gauge %.1f\n", car.driftState, static_cast<double>(nowMs) / 1000.0,
                        static_cast<double>(car.driftGauge));
        }
        if (car.miniTurboStage != stageBefore) {
            std::printf("  mini turbo stage %d at %.2f s gauge %.1f\n", car.miniTurboStage,
                        static_cast<double>(nowMs) / 1000.0, static_cast<double>(car.driftGauge));
        }
        if (car.boostState != 0 && boostBefore == 0) {
            std::printf("  boost kind %d started at %.2f s target %.1f kmh\n", car.boostKind,
                        static_cast<double>(nowMs) / 1000.0, static_cast<double>(car.boostTargetKmh));
        }
        if (options.trace) {
            const WheelSet& w = car.body.wheels;
            std::printf("  tick %5.2f kmh %6.1f vel %7.2f %7.2f %6.2f spinz %6.3f steer %6.3f ch %.3f %.3f air %d col %d thr %.4f "
                        "lat %.0f %.0f %.0f %.0f long %.0f %.0f %.0f %.0f slip %.3f %.3f %.3f %.3f r22 %.4f gauge %.1f yaw %.1f\n",
                        static_cast<double>(nowMs) / 1000.0, static_cast<double>(car.speedKmh),
                        static_cast<double>(w.velocity.x), static_cast<double>(w.velocity.y),
                        static_cast<double>(w.velocity.z), static_cast<double>(w.angularVelocity.z),
                        static_cast<double>(w.steerAverage), static_cast<double>(w.springChannels[2]),
                        static_cast<double>(w.springChannels[3]), body_wheels_on_ground_count(w),
                        car.body.collisionHappened, static_cast<double>(car.throttleFactorLast),
                        static_cast<double>(w.tireForceResult[0][1]), static_cast<double>(w.tireForceResult[1][1]),
                        static_cast<double>(w.tireForceResult[2][1]), static_cast<double>(w.tireForceResult[3][1]),
                        static_cast<double>(w.tireForceResult[0][2]), static_cast<double>(w.tireForceResult[1][2]),
                        static_cast<double>(w.tireForceResult[2][2]), static_cast<double>(w.tireForceResult[3][2]),
                        static_cast<double>(w.rk4BankA.state[0]), static_cast<double>(w.rk4BankA.state[1]),
                        static_cast<double>(w.rk4BankA.state[2]), static_cast<double>(w.rk4BankA.state[3]),
                        static_cast<double>(w.orientationR.m[2][2]), static_cast<double>(car.driftGauge),
                        static_cast<double>(wrap_degrees(car.yawDeg)));
        }
        if ((tick + 1) % kTicksPerSample == 0) {
            const float motionDeg = wrap_degrees(std::atan2(car.velY, -car.velX) * 57.29578f);
            std::printf("%5.1f  %d   %d   %d %d %d | %8.2f %8.2f %6.2f | %6.1f %6.1f %6.1f | %6.1f %6.1f %d %d | %d %d %6.1f | %6.3f %6.3f %5.1f %5.1f %5.1f\n",
                        static_cast<double>(nowMs) / 1000.0, car.input.accel, car.input.brake, car.input.steerLeft,
                        car.input.steerRight, game.driftKeyHeld, static_cast<double>(car.posX),
                        static_cast<double>(car.posY), static_cast<double>(car.posZ),
                        static_cast<double>(wrap_degrees(car.yawDeg)), static_cast<double>(motionDeg),
                        static_cast<double>(car.slipAngleDeg), static_cast<double>(car.driftGauge),
                        static_cast<double>(car.driftGaugeSmoothed), car.driftState, car.miniTurboStage,
                        car.boostState, car.boostKind, static_cast<double>(car.speedKmh),
                        static_cast<double>(car.body.wheels.angularVelocity.z),
                        static_cast<double>(car.body.wheels.steerAverage), static_cast<double>(car.rollDeg),
                        static_cast<double>(car.pitchDeg), static_cast<double>(car.driftSlipDeg));
        }
        if (std::isnan(car.posX) || std::isnan(car.posY) || std::isnan(car.posZ)) {
            std::printf("the port produced NaN, stopping\n");
            return 1;
        }
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse(argc, argv, options)) {
        std::fprintf(stderr,
                     "usage: ghost_compare <track folder> <ghost file> [--seconds N] [--resync] "
                     "[--green] [--trace] [--centre] [--flags] [--edges] [--resync-every N] [--car path] [--mask-lsb] [--kind K] [--engine-force F] "
                     "[--stats 17 floats] [--tuning a b c]\n"
                     "       ghost_compare <track folder> --script <file> [--start-row N] [--seconds N] [ghost file for the spawn]\n"
                     "       script lines are time accel brake left right drift item, the keys hold until the next line\n");
        return 2;
    }

    ColTrack track;
    std::string error;
    if (!world_load_track_pieces(track, options.track_dir, error)) {
        std::fprintf(stderr, "world_load_track_pieces failed, %s\n", error.c_str());
        return 1;
    }
    std::vector<GimmickStartRow> startRows;
    if (!gimmick_load_start(options.track_dir, startRows, error)) {
        std::fprintf(stderr, "gimmick_load_start failed, %s\n", error.c_str());
        return 1;
    }
    if (!options.script.empty()) {
        std::vector<ScriptLine> script;
        if (!load_script(options.script, script, error)) {
            std::fprintf(stderr, "load_script failed, %s\n", error.c_str());
            return 1;
        }
        // the spawn is the first sample of the ghost when one is given else the start ini row
        float sx = 0.0f, sy = 0.0f, sz = 0.0f, syaw = 0.0f;
        if (!options.ghost_path.empty()) {
            GhostRecording spawnRecording;
            if (!load_ghost_file(options.ghost_path, spawnRecording, error) || spawnRecording.samples.empty()) {
                std::fprintf(stderr, "load_ghost_file failed, %s\n", error.c_str());
                return 1;
            }
            const auto& first = spawnRecording.samples.front();
            sx = first.pos[0]; sy = first.pos[1]; sz = first.pos[2];
            syaw = static_cast<float>(first.yawByte) * kYawByteToDeg;
        } else {
            if (options.start_row < 0 || static_cast<size_t>(options.start_row) >= startRows.size()) {
                std::fprintf(stderr, "start row %d is outside the %zu rows of start.ini\n", options.start_row,
                             startRows.size());
                return 1;
            }
            const GimmickStartRow& row = startRows[static_cast<size_t>(options.start_row)];
            sx = row.x; sy = row.y; sz = row.z; syaw = row.heading;
            // the ground under the row gives the spawn height as the recorded spawn sits on it
            BspQuery probe;
            float ground = 0.0f;
            int surface = 0;
            if (world_locate_piece_by_height(probe, track, sx, sy, sz + 5.0f, &ground, &surface)) sz = ground + kOne;
        }
        std::printf("script %s, %zu lines, spawn %.3f %.3f %.3f yaw %.1f\n", options.script.c_str(), script.size(),
                    static_cast<double>(sx), static_cast<double>(sy), static_cast<double>(sz),
                    static_cast<double>(syaw));
        return run_script(options, track, script, sx, sy, sz, syaw);
    }
    GhostRecording recording;
    if (!load_ghost_file(options.ghost_path, recording, error)) {
        std::fprintf(stderr, "load_ghost_file failed, %s\n", error.c_str());
        return 1;
    }
    const auto& samples = recording.samples;
    if (samples.size() < 2) {
        std::fprintf(stderr, "the ghost holds %zu samples, nothing to compare\n", samples.size());
        return 1;
    }
    std::printf("ghost %s, %zu samples, car kind %u, time %d ms\n", recording.name.c_str(),
                samples.size(), recording.carKind, recording.timeMs);

    // The first sample is the spawn in wire space x y ground and z up like start ini
    const auto& first = samples.front();
    const float x0 = first.pos[0], y0 = first.pos[1], z0 = first.pos[2];
    const float yaw0 = static_cast<float>(first.yawByte) * kYawByteToDeg;
    int nearestRow = -1;
    float nearestDist = 1e9f;
    for (size_t i = 0; i < startRows.size(); ++i) {
        const float dx = startRows[i].x - x0, dy = startRows[i].y - y0;
        const float d = std::sqrt(dx * dx + dy * dy);
        if (d < nearestDist) { nearestDist = d; nearestRow = static_cast<int>(i); }
    }
    std::printf("first sample %.3f %.3f %.3f yaw %.1f, nearest start row %d at %.3f units\n",
                static_cast<double>(x0), static_cast<double>(y0), static_cast<double>(z0),
                static_cast<double>(yaw0), nearestRow, static_cast<double>(nearestDist));

    // Which argument order finds the ground under the spawn tells the col axis naming
    BspQuery probe;
    float groundA = 0.f, groundB = 0.f;
    int surfaceA = 0, surfaceB = 0;
    const bool hitA = world_locate_piece_by_height(probe, track, x0, y0, z0, &groundA, &surfaceA);
    const bool hitB = world_locate_piece_by_height(probe, track, x0, z0, y0, &groundB, &surfaceB);
    std::printf("ground under spawn with x y z order %s %.3f, with x z y order %s %.3f\n",
                hitA ? "hit" : "miss", static_cast<double>(groundA), hitB ? "hit" : "miss",
                static_cast<double>(groundB));

    // GameState embeds the 30 car array by value far too large for the default thread stack
    auto gamePtr = std::make_unique<GameState>();
    GameState& game = *gamePtr;
    // boost ini rows so the pad detector of car respawn state machine fires on the BOOST NNN cells
    if (!gimmick_load_boost(options.track_dir, game.boostRows, error)) {
        std::printf("boost.ini not loaded, %s\n", error.c_str());
    } else {
        std::printf("boost.ini %zu rows\n", game.boostRows.size());
    }
    game.tuning = TrackTuning{options.tuning[0], options.tuning[1], options.tuning[2]}; // the 0xC3 record of the track
    game.worldReady = 1;
    game.sessionRunning = 1; // game 0x1397384 the client polls keys and applies the engine force only with it set
    game.localCarIndex = 0;
    game.raceMode = 0;
    game.startLightState = options.green ? 5 : 0;

    CarState& car = game.cars[0];
    car.slotOccupied = 1;
    car.slotActive = 1;
    car.fixedStepSeconds = kFixedPhysicsStepSeconds; // game 0x30 the frame is cut into 12 substeps as the client does
    car.frameDt = kTickSeconds;
    car.vehicleKind = options.vehicle_kind;
    car.trackProgress = 0; // car 0x6BC 0 drives free 100 to 299 is the respawn teleport sequence with the brake forced
    for (size_t i = 0; i < KART_STAT_COUNT; ++i) {
        car.stats.base[i] = options.stats[i];
        car.stats.bonus[i] = 0.0f;
    }

    SpawnCatalogue catalogue = load_catalogue(options);
    // body create takes minus x minus y plus z minus yaw and the catalogue grips 0x130 0x134
    const bool created = body_create(car.body, catalogue, catalogue.tireGripBase[0], catalogue.tireGripBase[1],
                                     -x0, -y0, z0, -yaw0, 1, track);
    std::printf("body_create %s, mass %.0f principal inertia %.0f %.0f %.0f\n", created ? "ok" : "failed",
                static_cast<double>(car.body.wheels.massProps.mass), static_cast<double>(car.body.wheels.inertiaX),
                static_cast<double>(car.body.wheels.inertiaY), static_cast<double>(car.body.wheels.inertiaZ));
    // car physics setup 0x49510C body set mass friction 5 1 and car 0x32E8 the six channel rates
    body_set_mass_friction(car.body, kSetupChannelRateMass, kSetupChannelRateFriction, car.steeringScale);
    car_ground_flag_set_car(car, 1);
    car.body.wheels.currentGear = 1;
    // the ground bytes stay 0 loaded as the client record starts a byte 1 is a wheel in the air
    car.posX = x0; car.posY = y0; car.posZ = z0;
    car.prevPosX = x0; car.prevPosY = y0; car.prevPosZ = z0;
    car.yawDeg = yaw0;
    car.prevSubstepYawDeg = yaw0;
    car.engineForceBase = options.engine_force;

    const size_t lastSample = std::min(samples.size() - 1,
                                       static_cast<size_t>(options.seconds / kSampleSeconds));
    int64_t nowMs = 0;
    double sumErr = 0.0, maxErr = 0.0;
    size_t compared = 0;
    // the per sample error split by the key held drift release brake and gas the drift recording read
    struct ClassSum {
        const char* name;
        size_t n = 0;
        double gap = 0.0, gapMax = 0.0, yaw = 0.0, yawMax = 0.0, speed = 0.0;
        void add(double g, double y, double v) {
            ++n; gap += g; yaw += y; speed += v;
            if (g > gapMax) gapMax = g;
            if (y > yawMax) yawMax = y;
        }
    };
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
    std::printf("   t  mask  acc brk R L | rec x y z yaw | port x y z yaw | gap horiz vert yaw byte | rec speed port speed\n");
    // steer mask reads at tick 10i side bit at 10i plus 9 edge sits at tick e centre is 5
    auto steer_bit = [&](size_t sampleIndex, bool left, bool fromFlags) -> int {
        if (sampleIndex > lastSample) sampleIndex = lastSample;
        const auto& sm = samples[sampleIndex];
        if (fromFlags) return left ? ((sm.flags & 0x0004) ? 1 : 0) : ((sm.flags & 0x0002) ? 1 : 0);
        InputFlags f;
        apply_mask(sm.inputMask, options.mask_msb, f);
        return left ? f.steerLeft : f.steerRight;
    };
    // an edge is open when the mask at i and the side bit at i plus 1 disagree
    auto edge_open = [&](size_t i, bool left) -> bool {
        if (!options.flags || !options.centre) return false;
        return steer_bit(i, left, false) != steer_bit(i + 1, left, true);
    };
    const int kEdgeCentre = kTicksPerSample / 2;
    // a mask bit of sample i read as the key it is
    auto mask_bit = [&](size_t sampleIndex, uint8_t bit) -> int {
        if (sampleIndex > lastSample) sampleIndex = lastSample;
        return (options.mask_msb && (samples[sampleIndex].inputMask & bit)) ? 1 : 0;
    };
    // the drift key the gas and the brake have no second reading their edge is open when the masks differ
    auto key_edge_open = [&](size_t i, uint8_t bit) -> bool {
        if (!options.edges) return false;
        return mask_bit(i, bit) != mask_bit(i + 1, bit);
    };
    // the tick inside an interval where each key of the next mask takes over the centre is 5
    struct Edges {
        int left = kTicksPerSample / 2;
        int right = kTicksPerSample / 2;
        int drift = kTicksPerSample / 2;
        int accel = kTicksPerSample / 2;
        int brake = kTicksPerSample / 2;
    };

    // runs interval i ten ticks with key edges prints trace mask bit 3 slot 5 drift edge is mini turbo
    int prevAccel = 0;
    auto apply_raw_keys = [&](uint8_t mask) {
        game.driftKeyHeld = (options.mask_msb && (mask & GHOST_MASK_SLOT5_DRIFT)) ? 1 : 0;
        game.accelKeyRawHeld = car.input.accel;
        game.accelKeyPressed = (car.input.accel != 0 && prevAccel == 0) ? 1 : 0;
        prevAccel = car.input.accel;
    };
    auto run_interval = [&](size_t i, const Edges& edges, bool verbose) {
        const auto& s = samples[i];
        apply_mask(s.inputMask, options.mask_msb, car.input);
        car.input.stuck = 0;
        apply_raw_keys(s.inputMask);
        if (options.flags && !options.centre) {
            arm_from_flags(game, car, s, nowMs);
            arm_boost_from_flags(game, car, s.flags, nowMs);
        }
        for (int tick = 0; tick < kTicksPerSample; ++tick) {
            if (options.centre) {
                // the recorded mask is the state at the sample tick so it covers 5 ticks either side
                size_t maskIndex = (tick < kEdgeCentre) ? i : i + 1;
                if (maskIndex > lastSample) maskIndex = lastSample;
                apply_mask(samples[maskIndex].inputMask, options.mask_msb, car.input);
                car.input.stuck = 0;
                // the drift the gas and the brake keys switch at their own placed edge
                if (options.mask_msb) {
                    car.input.accel = static_cast<uint8_t>(mask_bit(tick < edges.accel ? i : i + 1, GHOST_MASK_SLOT0_ACCELERATE));
                    car.input.brake = static_cast<uint8_t>(mask_bit(tick < edges.brake ? i : i + 1, GHOST_MASK_SLOT1_BRAKE));
                }
                apply_raw_keys(samples[maskIndex].inputMask);
                if (options.mask_msb) {
                    game.driftKeyHeld = static_cast<uint8_t>(mask_bit(tick < edges.drift ? i : i + 1, GHOST_MASK_SLOT5_DRIFT));
                }
                if (options.flags && tick == kEdgeCentre) arm_from_flags(game, car, samples[maskIndex], nowMs);
                // side bits are steer keys polled the tick before record 0x497DA9 ticks past the edge take the next sample bits
                if (options.flags) {
                    car.input.steerLeft = static_cast<uint8_t>(tick < edges.left ? steer_bit(i, true, false) : steer_bit(i + 1, true, true));
                    car.input.steerRight = static_cast<uint8_t>(tick < edges.right ? steer_bit(i, false, false) : steer_bit(i + 1, false, true));
                }
                // the boost bit is read on the last tick so a pad the port crossed itself is not started twice
                if (options.flags && tick == kTicksPerSample - 1) {
                    arm_boost_from_flags(game, car, samples[maskIndex].flags, nowMs);
                }
            }
            const int boostBefore = car.boostState;
            const int progressBefore = car.trackProgress;
            const int stageBefore = car.miniTurboStage;
            const int stateBefore = car.driftState;
            car_physics_tick_local(game, 0, track, nowMs);
            if (verbose && car.boostState != 0 && boostBefore == 0) {
                std::printf("  boost kind %d started by the port at %.2f s\n", car.boostKind,
                            static_cast<double>(nowMs) / 1000.0);
            }
            const bool traceNow = options.trace && static_cast<float>(nowMs) * 0.001f >= options.trace_from;
            if (verbose && traceNow && (car.miniTurboStage != stageBefore || car.driftState != stateBefore)) {
                std::printf("  drift state %d stage %d gauge %.1f at %.2f s\n", car.driftState, car.miniTurboStage,
                            static_cast<double>(car.driftGauge), static_cast<double>(nowMs) / 1000.0);
            }
            if (verbose && car.trackProgress == -1 && progressBefore != -1) {
                std::printf("  pad cell %s under a wheel at %.2f s\n", car.boostPadCellName,
                            static_cast<double>(nowMs) / 1000.0);
            }
            nowMs += 20;
            if (verbose && traceNow) {
                const WheelSet& w = car.body.wheels;
                // the motion direction in the wire frame forward is minus cos A sin A
                const float motionDeg = wrap_degrees(std::atan2(car.velY, -car.velX) * 57.29578f);
                std::printf("  tick %6.2f in %g %g ch3 %.3f steer %.4f spinz %.3f yaw %.2f vdir %.2f "
                            "lat %.0f %.0f %.0f %.0f peak %.0f %.0f slip %.3f %.3f %.3f %.3f comp %.0f %.0f %.0f %.0f "
                            "r22 %.4f wx %.3f wy %.3f z %.3f air %d\n",
                            static_cast<double>(nowMs) / 1000.0, static_cast<double>(w.latestSubstepInput[2]),
                            static_cast<double>(w.latestSubstepInput[3]), static_cast<double>(w.springChannels[3]),
                            static_cast<double>(w.steerAverage), static_cast<double>(w.angularVelocity.z),
                            static_cast<double>(wrap_degrees(car.yawDeg)), static_cast<double>(motionDeg),
                            static_cast<double>(w.tireForceResult[0][1]), static_cast<double>(w.tireForceResult[1][1]),
                            static_cast<double>(w.tireForceResult[2][1]), static_cast<double>(w.tireForceResult[3][1]),
                            static_cast<double>(w.tireForceResult[0][0]), static_cast<double>(w.tireForceResult[2][0]),
                            static_cast<double>(w.rk4BankA.state[0]), static_cast<double>(w.rk4BankA.state[1]),
                            static_cast<double>(w.rk4BankA.state[2]), static_cast<double>(w.rk4BankA.state[3]),
                            static_cast<double>(w.tireScratch[0].compression), static_cast<double>(w.tireScratch[1].compression),
                            static_cast<double>(w.tireScratch[2].compression), static_cast<double>(w.tireScratch[3].compression),
                            static_cast<double>(w.orientationR.m[2][2]), static_cast<double>(w.angularVelocity.x),
                            static_cast<double>(w.angularVelocity.y),
                            static_cast<double>(car.posZ), body_wheels_on_ground_count(w));
                // the drive side speed the step 11 factor the gear the engine speed the clutch load the long forces
                std::printf("  drive %6.2f boost %d kind %d target %.1f str %.2f prog %d speed %.2f kmh %.1f thr %.5f gear %d omega %.1f load %.0f "
                            "long %.0f %.0f %.0f %.0f peak %.0f %.0f %.0f %.0f slipl %.3f %.3f %.3f %.3f spin %.1f %.1f %.1f %.1f\n",
                            static_cast<double>(nowMs) / 1000.0, car.boostState, car.boostKind,
                            static_cast<double>(car.boostTargetKmh), static_cast<double>(car.boostDecayStrength),
                            car.trackProgress, static_cast<double>(car.speed),
                            static_cast<double>(car.speedKmh), static_cast<double>(car.throttleFactorLast),
                            w.currentGear, static_cast<double>(w.gearRatio), static_cast<double>(w.engineLoad),
                            static_cast<double>(w.tireForceResult[0][2]), static_cast<double>(w.tireForceResult[1][2]),
                            static_cast<double>(w.tireForceResult[2][2]), static_cast<double>(w.tireForceResult[3][2]),
                            static_cast<double>(w.tireForceResult[0][0]), static_cast<double>(w.tireForceResult[1][0]),
                            static_cast<double>(w.tireForceResult[2][0]), static_cast<double>(w.tireForceResult[3][0]),
                            static_cast<double>(w.rk4BankB.state[0]), static_cast<double>(w.rk4BankB.state[1]),
                            static_cast<double>(w.rk4BankB.state[2]), static_cast<double>(w.rk4BankB.state[3]),
                            static_cast<double>(w.wheel[0].spinRate), static_cast<double>(w.wheel[1].spinRate),
                            static_cast<double>(w.wheel[2].spinRate), static_cast<double>(w.wheel[3].spinRate));
                // the tick side terms of the factor the roll the surface under each wheel and its friction
                int surf[4];
                for (int k = 0; k < 4; ++k) {
                    surf[k] = world_wheel_surface_index(car.body.groundQuery, car.wheelProbePoint[k].x,
                                                        car.wheelProbePoint[k].y, true);
                }
                // the plane under each hub its normal length and the hub height over it
                for (int k = 0; k < 4; ++k) {
                    const ColCell* c = w.contactPlane[k];
                    if (!c) { std::printf("  plane %6.2f wheel %d none\n", static_cast<double>(nowMs) / 1000.0, k); continue; }
                    const Vec3& hub = car.body.hubWorld[k];
                    const float nlen = std::sqrt(c->heightA * c->heightA + c->heightB * c->heightB + c->heightC * c->heightC);
                    const float h = c->heightA * hub.x + c->heightB * hub.y + c->heightC * hub.z + c->heightD;
                    std::printf("  plane %6.2f wheel %d abcd %.4f %.4f %.4f %.3f nlen %.4f hub h %.3f travel %.3f %s\n",
                                static_cast<double>(nowMs) / 1000.0, k, static_cast<double>(c->heightA),
                                static_cast<double>(c->heightB), static_cast<double>(c->heightC), static_cast<double>(c->heightD),
                                static_cast<double>(nlen), static_cast<double>(h), static_cast<double>(w.wheel[k].travel), c->surfaceName);
                }
                {
                    // the world tyre force sum and the velocity in the physics frame
                    Vec3 fsum{};
                    for (int k = 0; k < 4; ++k) body_vec3_add(fsum, w.tireForceWorld[k]);
                    std::printf("  force %6.2f sum %.0f %.0f %.0f vel %.2f %.2f %.2f\n", static_cast<double>(nowMs) / 1000.0,
                                static_cast<double>(fsum.x), static_cast<double>(fsum.y), static_cast<double>(fsum.z),
                                static_cast<double>(w.velocity.x), static_cast<double>(w.velocity.y), static_cast<double>(w.velocity.z));
                }
                // the heading of R itself before the quarter smoothing of step 18 and the spin the damper left
                const float headR = wrap_degrees(math_atan2_deg(-2.0f * w.orientationR.m[0][0], -2.0f * w.orientationR.m[1][0]) + 90.0f);
                std::printf("  head %6.2f headR %.2f yaw %.2f spinz %.4f\n", static_cast<double>(nowMs) / 1000.0,
                            static_cast<double>(headR), static_cast<double>(wrap_degrees(car.yawDeg)),
                            static_cast<double>(w.angularVelocity.z));
                std::printf("  rmat %6.2f row2 %.4f %.4f %.4f col2 %.4f %.4f %.4f\n", static_cast<double>(nowMs) / 1000.0,
                            static_cast<double>(w.orientationR.m[2][0]), static_cast<double>(w.orientationR.m[2][1]),
                            static_cast<double>(w.orientationR.m[2][2]), static_cast<double>(w.orientationR.m[0][2]),
                            static_cast<double>(w.orientationR.m[1][2]), static_cast<double>(w.orientationR.m[2][2]));
                std::printf("  terms %6.2f roll %.2f pitch %.2f surf %d %d %d %d fric %.2f %.2f drag %.4f %.4f jitter %.4f coast %d\n",
                            static_cast<double>(nowMs) / 1000.0, static_cast<double>(car.rollDeg),
                            static_cast<double>(car.pitchDeg), surf[0], surf[1], surf[2], surf[3],
                            static_cast<double>(world_surface_friction(surf[0])),
                            static_cast<double>(world_surface_friction(surf[2])),
                            static_cast<double>(world_surface_contact_drag(surf[0])),
                            static_cast<double>(world_surface_contact_drag(surf[2])),
                            static_cast<double>(car.throttleJitter), game.stoppedFlag);
            }
        }
    };

    // the yaw error of the port against the recorded sample once the ticks of an interval ran
    auto yaw_error_deg = [&](size_t sampleIndex) -> float {
        if (sampleIndex > lastSample) sampleIndex = lastSample;
        const float recYawDeg = static_cast<float>(samples[sampleIndex].yawByte) * kYawByteToDeg;
        return std::fabs(yaw_gap(car.yawDeg, recYawDeg));
    };
    // the mean speed error over the interval that ends on the sample the port mean against the recorded one
    auto speed_error = [&](size_t sampleIndex, float startX, float startY) -> float {
        if (sampleIndex > lastSample || sampleIndex == 0) return 0.0f;
        const auto& a = samples[sampleIndex - 1];
        const auto& b = samples[sampleIndex];
        const float rdx = b.pos[0] - a.pos[0], rdy = b.pos[1] - a.pos[1];
        const float pdx = car.posX - startX, pdy = car.posY - startY;
        return std::fabs(std::sqrt(pdx * pdx + pdy * pdy) - std::sqrt(rdx * rdx + rdy * rdy)) / kSampleSeconds;
    };
    // the gauge error at a sample the nibble is the gauge plus 45 over 6 truncated one step 6 units
    auto gauge_error = [&](size_t sampleIndex) -> float {
        if (sampleIndex > lastSample) return 0.0f;
        const auto& sm = samples[sampleIndex];
        if ((sm.flags & 0x0300) == 0) return 0.0f;
        const float recGauge = static_cast<float>(sm.nibbles & 0xF) * 6.0f - 45.0f + 3.0f;
        return std::fabs(car.driftGauge - recGauge);
    };

    // the snapshot of the whole state for the edge search the idle cars drop their ghost rings first
    for (size_t k = 1; k < game.cars.size(); ++k) game.cars[k].remote.ghost.samples.clear();
    auto snapshot = std::make_unique<GameState>();
    int edgesPlaced = 0;
    int edgesOffCentre = 0;

    for (size_t i = 0; i < lastSample; ++i) {
        const auto& s = samples[i];
        const auto& next = samples[i + 1];
        Edges edges;
        const Edges centreEdges;
        if (options.edges && i + 2 <= lastSample) {
            // open edges follow the next two samples steer and drift use yaw and gauge gas and brake use speed
            static const char* const kEdgeNames[5] = {"left", "right", "drift", "accel", "brake"};
            for (int pass = 0; pass < 5; ++pass) {
                bool open = false;
                if (pass == 0) open = edge_open(i, true);
                else if (pass == 1) open = edge_open(i, false);
                else if (pass == 2) open = key_edge_open(i, GHOST_MASK_SLOT5_DRIFT);
                else if (pass == 3) open = key_edge_open(i, GHOST_MASK_SLOT0_ACCELERATE);
                else open = key_edge_open(i, GHOST_MASK_SLOT1_BRAKE);
                if (!open) continue;
                int bestEdge = kEdgeCentre;
                float bestScore = 1e9f;
                for (int e = 1; e < kTicksPerSample; ++e) {
                    *snapshot = game;
                    const int64_t savedMs = nowMs;
                    const int savedAccel = prevAccel;
                    Edges trial = edges;
                    if (pass == 0) trial.left = e;
                    else if (pass == 1) trial.right = e;
                    else if (pass == 2) trial.drift = e;
                    else if (pass == 3) trial.accel = e;
                    else trial.brake = e;
                    const float x0 = car.posX, y0 = car.posY;
                    run_interval(i, trial, false);
                    float score = 0.0f;
                    if (pass < 3) score += yaw_error_deg(i + 1);
                    if (pass == 2) score += gauge_error(i + 1) * kHalf;
                    if (pass >= 3) score += speed_error(i + 1, x0, y0);
                    const float x1 = car.posX, y1 = car.posY;
                    run_interval(i + 1, centreEdges, false);
                    if (pass < 3) score += yaw_error_deg(i + 2);
                    if (pass >= 3) score += speed_error(i + 2, x1, y1);
                    game = *snapshot;
                    nowMs = savedMs;
                    prevAccel = savedAccel;
                    if (score < bestScore - 1e-4f) {
                        bestScore = score;
                        bestEdge = e;
                    }
                }
                if (pass == 0) edges.left = bestEdge;
                else if (pass == 1) edges.right = bestEdge;
                else if (pass == 2) edges.drift = bestEdge;
                else if (pass == 3) edges.accel = bestEdge;
                else edges.brake = bestEdge;
                ++edgesPlaced;
                if (bestEdge != kEdgeCentre) ++edgesOffCentre;
                if (options.trace && static_cast<float>(i) * kSampleSeconds >= options.trace_from) {
                    std::printf("  edge %s of interval %.1f placed at tick %d score %.2f\n", kEdgeNames[pass],
                                static_cast<double>(i) * kSampleSeconds, bestEdge, static_cast<double>(bestScore));
                }
            }
        }
        const float startX = car.posX, startY = car.posY;
        run_interval(i, edges, true);
        const float dx = car.posX - next.pos[0], dy = car.posY - next.pos[1], dz = car.posZ - next.pos[2];
        const float horiz = std::sqrt(dx * dx + dy * dy);
        const float recYaw = static_cast<float>(next.yawByte) * kYawByteToDeg;
        const float rdx = next.pos[0] - s.pos[0], rdy = next.pos[1] - s.pos[1];
        const float recSpeed = std::sqrt(rdx * rdx + rdy * rdy) / kSampleSeconds;
        // the recorded speed is a sample mean so the port speed is the same mean over its ten ticks
        const float pdx = car.posX - startX, pdy = car.posY - startY;
        const float portSpeed = std::sqrt(pdx * pdx + pdy * pdy) / kSampleSeconds;
        // the recorder truncates the yaw times 255 over 360 to a byte 0x49FB69 the byte gap is that value
        const float portYawByte =
            static_cast<float>(static_cast<int>(wrap_degrees(car.yawDeg) / kYawByteToDeg)) * kYawByteToDeg;
        std::printf("%5.1f  0x%02x  %d   %d   %d %d | %8.2f %8.2f %6.2f %6.1f | %8.2f %8.2f %6.2f %6.1f | %7.2f %6.2f %6.1f %5.1f | %7.2f %7.2f\n",
                    static_cast<double>((i + 1) * kSampleSeconds), s.inputMask, car.input.accel,
                    car.input.brake, car.input.steerRight, car.input.steerLeft,
                    static_cast<double>(next.pos[0]), static_cast<double>(next.pos[1]),
                    static_cast<double>(next.pos[2]), static_cast<double>(recYaw),
                    static_cast<double>(car.posX), static_cast<double>(car.posY),
                    static_cast<double>(car.posZ), static_cast<double>(wrap_degrees(car.yawDeg)),
                    static_cast<double>(horiz), static_cast<double>(dz),
                    static_cast<double>(yaw_gap(car.yawDeg, recYaw)), static_cast<double>(yaw_gap(portYawByte, recYaw)),
                    static_cast<double>(recSpeed), static_cast<double>(portSpeed));
        if (std::isnan(car.posX) || std::isnan(car.posY) || std::isnan(car.posZ)) {
            std::printf("the port produced NaN, stopping\n");
            return 1;
        }
        sumErr += horiz;
        if (horiz > maxErr) maxErr = horiz;
        ++compared;
        for (int c = 0; c < 4; ++c) {
            if (sample_class(i, c)) {
                classes[c].add(horiz, std::fabs(yaw_gap(portYawByte, recYaw)), portSpeed - recSpeed);
            }
        }
        const bool resyncNow = options.resync && static_cast<float>(i + 1) * kSampleSeconds <= options.resync_until;
        if (resyncNow && ((i + 1) % static_cast<size_t>(options.resync_every < 1 ? 1 : options.resync_every)) == 0) {
            // pose resets to the recorded sample wheel engine and steer reset slip state stays next sample is port's own prediction
            Vec3 keepVelocity = car.body.wheels.velocity;
            if (i + 2 < samples.size()) {
                // central difference of the recorded positions the chord of one pair lags a turn
                const auto& after = samples[i + 2];
                keepVelocity.x = -(after.pos[0] - s.pos[0]) / (2.0f * kSampleSeconds);
                keepVelocity.y = -(after.pos[1] - s.pos[1]) / (2.0f * kSampleSeconds);
                keepVelocity.z = (after.pos[2] - s.pos[2]) / (2.0f * kSampleSeconds);
            }
            // recorded yaw is car 0x3220 following R by a quarter per tick 0x49DFC7 byte truncates so centre offset is unbiased
            const float recYawCentre = recYaw + kYawByteToDeg * kHalf;
            const float yawDelta = yaw_gap(recYawCentre, car.yawDeg);
            car.posX = next.pos[0]; car.posY = next.pos[1]; car.posZ = next.pos[2];
            car.prevPosX = car.posX; car.prevPosY = car.posY; car.prevPosZ = car.posZ;
            car.yawDeg = recYawCentre;
            car.prevSubstepYawDeg = recYawCentre;
            resync_pose(car.body, -car.posX, -car.posY, car.posZ, -yawDelta, keepVelocity, track);  // R turns by this delta keeping its lead across the teleport
        }
    }
    std::printf("compared %zu samples, mean horizontal gap %.3f, max %.3f%s\n", compared,
                compared ? sumErr / static_cast<double>(compared) : 0.0, maxErr,
                options.resync ? " (resync after every sample)" : "");
    if (options.edges) {
        std::printf("edges placed %d, off the centre tick %d\n", edgesPlaced, edgesOffCentre);
    }
    for (const ClassSum& c : classes) {
        if (c.n == 0) continue;
        const double n = static_cast<double>(c.n);
        std::printf("class %-20s n %4zu gap %.3f max %.3f yaw byte %.2f max %.1f speed %+.2f\n", c.name, c.n,
                    c.gap / n, c.gapMax, c.yaw / n, c.yawMax, c.speed / n);
    }
    return 0;
}
