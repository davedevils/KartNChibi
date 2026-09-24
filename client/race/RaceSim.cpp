#include "RaceSim.h"

#include "games/kart/physics/client/body.h"
#include "games/kart/physics/client/boost.h"
#include "games/kart/physics/client/constants.h"
#include "games/kart/physics/client/effects.h"
#include "games/kart/physics/client/respawn.h"
#include "games/kart/physics/client/stats.h"
#include "games/kart/physics/client/tick.h"
#include "games/kart/physics/client/world_collision.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>

namespace KnC::Client {

using namespace KnC::Kart::Client;

namespace {

// client tick is 20 ms frame cut into substeps of fixed step inside tick
constexpr float kTickSeconds = 0.02f;
constexpr int kTickMs = 20;
// stalled frame runs at most this many ticks rest is dropped
constexpr int kMaxTicksPerAdvance = 8;
// car 0x373C landing flag shows puff this long
constexpr int64_t kLandedShowMs = 800;
// surface table 0x5EA88C dust kind for nine col surfaces 0x486DD0 reads it
constexpr int kSurfaceDustKind[9] = {0, 0, 2, 1, 3, 3, 0, 2, 1};
// gauge update 0x4ADC20 band climbs 3 2 a second 0x5A6E6C hands boost at 127 0x5A6E60
constexpr float kBandRate = 3.2f;
constexpr float kBandFull = 127.f;
// drift gauge car 0x35A8 must stand past 10 either way 0x59F404 0x5A324C for band to climb
constexpr float kBandDriftGate = 10.f;
// stage car 0x35F0 triples rate boost car 0x3300 doubles it
constexpr float kBandStageScale = 3.f;
constexpr float kBandBoostScale = 2.f;
// shown fill eases toward target by an eighth a 60 Hz frame
constexpr float kBandEase = 0.125f;
constexpr float kStockFrameSeconds = 1.f / 60.f;
// 0x5A6E84 boost get clip plays 1 7 s
constexpr float kBandFlashSeconds = 1.7f;
// band stops when two wheels or more leave ground 0x4EFA50 counts car 0x2DF0
constexpr int kBandAirborneLimit = 1;
// released drift stocks half of its gain full band a third of it times 0 37037 at eight racers
constexpr float kBandReleaseShare = 0.5f;
constexpr float kBandFullShare = 0.33333334f * 0.37037036f;
// sub 4AE590 pet kind 0x13 of key 10 sets the gauge factor 0x3F866666 that 0x4ADC20 multiplies in
constexpr int32_t kPetKindGauge = 0x13;
constexpr float kBandPetScale = 1.05f;

int dustKindOf(int surfaceIndex) {
    if (surfaceIndex < 0 || surfaceIndex >= 9) return -1;
    return kSurfaceDustKind[surfaceIndex];
}

}

RaceSim::RaceSim() : m_game(std::make_unique<GameState>()) {
    m_ids.fill(0);
    for (auto& sides : m_rearSurface) sides = {-1, -1};
}

RaceSim::~RaceSim() = default;

void RaceSim::setEquippedPet(uint32_t petKey) {
    GameState& game = *m_game;
    game.pets.records.clear();
    if (petKey != 0) {
        KnC::Kart::Client::OwnedPet worn;
        worn.base_key = petKey;
        worn.equipped = 1;
        game.pets.records.push_back(worn);
    }
    m_bandPetScale = pet_equipped_kind(game.pets) == kPetKindGauge ? kBandPetScale : 1.f;
    std::printf("[sim] worn pet %u kind %d\n", petKey, pet_equipped_kind(game.pets));
}

bool RaceSim::init(RaceWorld& world, uint32_t localPlayerId, std::string& error) {
    m_world = &world;
    m_game = std::make_unique<GameState>();
    m_bandPetScale = 1.f;
    m_ids.fill(0);
    for (auto& sides : m_rearSurface) sides = {-1, -1};
    m_impact.fill(Impact{});
    m_ghostReplay.fill(false);
    m_nowMs = 0;
    m_accum = 0.f;
    m_localReady = false;
    GameState& game = *m_game;
    game.hooks.user = this;
    game.hooks.impactEffect = &RaceSim::onImpact;
    if (!respawn_follow_lists_load(game, world.files.trackDir, error)) {
        std::printf("[sim] follow lists %s\n", error.c_str());
        error.clear();
    }
    game.boostRows = world.boostRows;
    game.tuning = TrackTuning{world.files.tuning[0], world.files.tuning[1], world.files.tuning[2]};
    game.worldReady = 1;
    game.worldTrackId = world.files.trackId;
    // key poll and engine force wait for green light countdown turns it on
    game.sessionRunning = 0;
    game.localCarIndex = 0;
    // 0xB is online race state 0x0040 sender and rally skip key on it
    game.raceMode = 0xB;
    game.startLightState = 0;
    game.localPlayerId = static_cast<int32_t>(localPlayerId);
    game.standingsPlayerIds.fill(-1);
    return true;
}

bool RaceSim::spawnLocal(const CarSetup& setup, std::string& error) {
    if (!m_world) { error = "no world"; return false; }
    GameState& game = *m_game;
    CarState& car = game.cars[0];
    car = CarState();
    car.slotOccupied = 1;
    car.slotActive = 1;
    car.playerOrGhostId = static_cast<int32_t>(setup.playerId);
    car.fixedStepSeconds = kFixedPhysicsStepSeconds;
    car.frameDt = kTickSeconds;
    car.vehicleKind = setup.vehicleKind;
    car.trackProgress = 0;
    for (size_t i = 0; i < KART_STAT_COUNT; ++i) {
        car.stats.base[i] = setup.stats[i];
        car.stats.bonus[i] = 0.f;
    }
    SpawnCatalogue catalogue;
    std::string carError;
    if (!setup.carFile.empty() && catalogue_load_car_file(catalogue, setup.carFile, carError)) {
        catalogue_apply_setup_overrides(catalogue);
        std::printf("[sim] catalogue %s\n", setup.carFile.c_str());
    } else {
        catalogue_from_stats(car.stats, setup.vehicleKind, catalogue);
        std::printf("[sim] catalogue default car values %s\n", carError.empty() ? "no car file" : carError.c_str());
    }
    const ColTrack& track = m_world->scene.collision;
    const bool created = body_create(car.body, catalogue, catalogue.tireGripBase[0], catalogue.tireGripBase[1],
                                     -setup.x, -setup.y, setup.z, -setup.yawDeg, 1, track);
    if (!created) {
        error = "body create failed on the spawn row";
        return false;
    }
    body_set_mass_friction(car.body, kSetupChannelRateMass, kSetupChannelRateFriction, car.steeringScale);
    car_ground_flag_set_car(car, 1);
    car.body.wheels.currentGear = 1;
    car.posX = setup.x; car.posY = setup.y; car.posZ = setup.z;
    car.prevPosX = setup.x; car.prevPosY = setup.y; car.prevPosZ = setup.z;
    car.yawDeg = setup.yawDeg;
    car.prevSubstepYawDeg = setup.yawDeg;
    world_place_probe_local(car.body.groundQuery, track, car.posX, car.posY, car.posZ, nullptr, true);
    m_ids[0] = setup.playerId;
    m_rearSurface[0] = {-1, -1};
    m_impact[0] = Impact{};
    m_localReady = true;
    std::printf("[sim] local car %u at %.2f %.2f %.2f yaw %.1f mass %.0f\n", setup.playerId, setup.x, setup.y,
                setup.z, setup.yawDeg, car.body.wheels.massProps.mass);
    return true;
}

int RaceSim::spawnRemote(uint32_t playerId, float x, float y, float z, float yawDeg) {
    GameState& game = *m_game;
    for (int i = 1; i < kCarSlotCount; ++i) {
        if (m_ids[static_cast<size_t>(i)] != 0) continue;
        CarState& car = game.cars[static_cast<size_t>(i)];
        car = CarState();
        car.slotOccupied = 1;
        car.slotActive = 1;
        car.playerOrGhostId = static_cast<int32_t>(playerId);
        car.fixedStepSeconds = kFixedPhysicsStepSeconds;
        car.frameDt = kTickSeconds;
        car.posX = x; car.posY = y; car.posZ = z;
        car.yawDeg = yawDeg;
        car.remote.posX = x; car.remote.posY = y; car.remote.posZ = z;
        car.remote.yawDeg = yawDeg;
        car.remote.frameDt = kTickSeconds;
        if (m_world) {
            world_place_probe_local(car.body.groundQuery, m_world->scene.collision, x, y, z, nullptr, true);
        }
        m_ids[static_cast<size_t>(i)] = playerId;
        m_rearSurface[static_cast<size_t>(i)] = {-1, -1};
        m_impact[static_cast<size_t>(i)] = Impact{};
        return i;
    }
    return -1;
}

void RaceSim::onImpact(void* user, int carIndex, int tier) {
    RaceSim* self = static_cast<RaceSim*>(user);
    if (self == nullptr || carIndex < 0 || carIndex >= kCarSlotCount) return;
    self->m_impact[static_cast<size_t>(carIndex)] = Impact{tier, self->m_nowMs};
}

void RaceSim::updateRearSurfaces() {
    if (!m_world) return;
    GameState& game = *m_game;
    for (int i = 0; i < kCarSlotCount; ++i) {
        CarState& car = game.cars[static_cast<size_t>(i)];
        if (car.slotOccupied == 0) continue;
        for (int side = 0; side < 2; ++side) {
            // local car has its probe points remote car asks under its position
            const float x = i == 0 ? car.wheelProbePoint[2 + side].x : car.posX;
            const float y = i == 0 ? car.wheelProbePoint[2 + side].y : car.posY;
            const int index = world_wheel_surface_index(car.body.groundQuery, x, y, game.worldReady != 0);
            // car 0x3748 keeps last index while query misses
            if (index != -1) m_rearSurface[static_cast<size_t>(i)][static_cast<size_t>(side)] = index;
        }
    }
}

void RaceSim::removeCar(uint32_t playerId) {
    const int i = carIndex(playerId);
    if (i <= 0) return;
    m_game->cars[static_cast<size_t>(i)].slotOccupied = 0;
    m_game->cars[static_cast<size_t>(i)].slotActive = 0;
    m_ids[static_cast<size_t>(i)] = 0;
    m_ghostReplay[static_cast<size_t>(i)] = false;
}

int RaceSim::carIndex(uint32_t playerId) const {
    if (playerId == 0) return -1;
    for (int i = 0; i < kCarSlotCount; ++i) {
        if (m_ids[static_cast<size_t>(i)] == playerId) return i;
    }
    return -1;
}

void RaceSim::setLocalInput(const InputFlags& flags, bool driftHeld, bool accelPressed) {
    GameState& game = *m_game;
    CarState& car = game.cars[0];
    car.input = flags;
    // KNC SIM DRIFT holds drift key and steer in a pattern sparks come off port
    static const bool driftTest = std::getenv("KNC_SIM_DRIFT") != nullptr;
    // cycle starts over 80 km per hour and runs six seconds whatever speed does
    if (driftTest && m_driftTestStartMs >= 0 && m_nowMs - m_driftTestStartMs >= 6000) m_driftTestStartMs = -1;
    if (driftTest && m_driftTestStartMs < 0 && car.speedKmh > 80.f) m_driftTestStartMs = m_nowMs;
    if (driftTest && m_driftTestStartMs >= 0) {
        const int phase = static_cast<int>(m_nowMs - m_driftTestStartMs);
        // 700 ms of drift left forced release with accel off then key back with accel press
        if (phase < 700) {
            driftHeld = true;
            car.input.steerLeft = 1;
            car.input.steerRight = 0;
        } else if (phase < 800) {
            driftHeld = false;
            car.input.accel = 0;
        } else if (phase < 1600) {
            // second drift turns right so car comes back toward its line under mini turbo
            driftHeld = true;
            car.input.steerLeft = 0;
            car.input.steerRight = 1;
            if (phase < 1000) {
                car.input.accel = 1;
                accelPressed = true;
            }
        }
        // state of drift chain once a tenth so missing boost shows where it stops
        static int64_t lastPrintMs = -1000;
        if (m_nowMs - lastPrintMs >= 100) {
            lastPrintMs = m_nowMs;
            std::printf("[sim] drift test phase %d state %d gauge %.1f stage %d boost %d kmh %.0f ground %d\n", phase,
                        car.driftState, car.driftGauge, car.miniTurboStage, car.boostState, car.speedKmh,
                        car.body.overValidGround);
        }
    }
    game.driftKeyHeld = driftHeld ? 1 : 0;
    game.accelKeyRawHeld = flags.accel != 0 ? 1 : 0;
    game.accelKeyPressed = accelPressed ? 1 : 0;
}

void RaceSim::pressKey(int keyCode) {
    if (keyCode < 0 || keyCode >= static_cast<int>(INPUT_PRESSED_STATE_SIZE)) return;
    m_game->keyPressed[static_cast<size_t>(keyCode)] = 1;
}

void RaceSim::setGreenLight(bool on) {
    m_game->startLightState = on ? 5 : 0;
}

void RaceSim::setSessionRunning(bool on) {
    m_game->sessionRunning = on ? 1 : 0;
}

void RaceSim::applyMotion(const MotionRecvEntry& entry) {
    const int i = carIndex(static_cast<uint32_t>(entry.id));
    if (i <= 0) return;
    MotionSample s;
    for (int a = 0; a < 3; ++a) { s.pos[a] = entry.pos[a]; s.predPos[a] = entry.predPos[a]; }
    s.yawByte = entry.yawByte;
    s.statusLo = static_cast<uint8_t>(entry.status & 0xFF);
    s.statusHi = static_cast<uint8_t>((entry.status >> 8) & 0xFF);
    s.hint = entry.hint == 0 ? static_cast<int16_t>(1000) : entry.hint;
    net_motion_sample_push(m_game->cars[static_cast<size_t>(i)].remote.mailbox, s);
}

void RaceSim::teleport(uint32_t playerId, float x, float y, float z, float yawDeg) {
    const int i = carIndex(playerId);
    if (i <= 0) return;
    CarState& car = m_game->cars[static_cast<size_t>(i)];
    car.remote.posX = x; car.remote.posY = y; car.remote.posZ = z;
    car.remote.yawDeg = yawDeg;
    car.remote.everReceivedSample = false;
    net_motion_queue_clear(car.remote.mailbox);
    car.posX = x; car.posY = y; car.posZ = z;
    car.yawDeg = yawDeg;
}

void RaceSim::applyEffect(uint32_t playerId, int code) {
    const int i = carIndex(playerId);
    if (i < 0) return;
    car_effect_apply(*m_game, i, code, m_nowMs);
}

void RaceSim::applyEffectOnCar(int carIndex, int code) {
    if (carIndex < 0 || carIndex >= kCarSlotCount) return;
    if (m_game->cars[static_cast<size_t>(carIndex)].slotOccupied == 0 && carIndex != 0) return;
    car_effect_apply(*m_game, carIndex, code, m_nowMs);
}

void RaceSim::pushCar(int carIndex, float headingDeg, float strength) {
    if (carIndex != 0 || !m_localReady) return;
    car_boost_push(*m_game, 0, headingDeg, strength);
}

void RaceSim::kickCar(int carIndex, float up) {
    if (carIndex != 0 || !m_localReady || m_game->sessionRunning == 0) return;
    car_apply_engine_force(m_game->cars[0], 0.f, 0.f, up);
}

void RaceSim::startBoost(int kind) {
    if (!m_localReady) return;
    car_boost_start(*m_game, 0, kind, m_nowMs);
}

void RaceSim::startCarry(int carIndex, bool blue, float x, float y, float z, float yawDeg) {
    if (carIndex < 0 || carIndex >= kCarSlotCount) return;
    auto& pool = blue ? m_game->effect900Pool : m_game->carryPool;
    for (const GimmickPoolSlot& s : pool)
        if (s.active && s.car_index == carIndex) return;
    for (GimmickPoolSlot& s : pool) {
        if (s.active) continue;
        s = GimmickPoolSlot{};
        s.active = true;
        s.car_index = carIndex;
        s.x = x; s.y = y; s.z = z;
        s.yaw_deg = yawDeg;
        s.last_dist = 1e9f;
        // sub 4C7B80 the slot starts on the nearest point of the follow lists
        int list = 0, point = 0;
        float dist = 0.f;
        if (respawn_checkpoint_nearest_all(m_game->checkpoints, x, y, z, &list, &point, &dist)) {
            s.follow_list = list;
            s.follow_point = point;
        }
        return;
    }
}

const GimmickPoolSlot* RaceSim::carrySlot(int carIndex, bool blue) const {
    const auto& pool = blue ? m_game->effect900Pool : m_game->carryPool;
    for (const GimmickPoolSlot& s : pool)
        if (s.active && s.car_index == carIndex) return &s;
    return nullptr;
}

// itemdrum hit test 0x4bed40 three responses bounce pushes car away from barrel
void RaceSim::applyDrumHit(const GimmickDrumResult& hit) {
    if (!m_localReady || hit.kind == GimmickDrumHit::None) return;
    GameState& game = *m_game;
    CarState& car = game.cars[0];
    switch (hit.kind) {
    case GimmickDrumHit::Bounce:
        // 0x4bef58 impact sprite then push at 270 degrees off bearing of barrel
        m_impact[0] = Impact{1, m_nowMs};
        car_boost_push(game, 0, hit.pushHeadingDeg, hit.pushStrength);
        break;
    case GimmickDrumHit::Slow:
        // 0x4befb5 x and y velocity halve z stays
        car.body.wheels.velocity.x *= GIMMICK_DRUM_SLOW_SCALE;
        car.body.wheels.velocity.y *= GIMMICK_DRUM_SLOW_SCALE;
        break;
    case GimmickDrumHit::BoostCancel:
        // 0x4befd1 boost goes and car keeps 0 7 of its velocity
        car_boost_clear(game, 0);
        car.body.wheels.velocity.x *= GIMMICK_DRUM_BOOST_SCALE;
        car.body.wheels.velocity.y *= GIMMICK_DRUM_BOOST_SCALE;
        break;
    default:
        break;
    }
}

float RaceSim::localDriftGaugeSmoothed() const {
    return m_localReady ? m_game->cars[0].driftGaugeSmoothed : 0.f;
}

float RaceSim::driftGaugeSmoothed(int carIndex) const {
    if (carIndex == 0) return localDriftGaugeSmoothed();
    if (carIndex < 0 || carIndex >= kCarSlotCount) return 0.f;
    return m_game->cars[static_cast<size_t>(carIndex)].remote.driftGaugeSmoothed;
}

void RaceSim::setFinished(uint32_t playerId) {
    const int i = carIndex(playerId);
    if (i < 0) return;
    m_game->cars[static_cast<size_t>(i)].finishedOrSpectating = 1;
}

void RaceSim::setFinishRank(uint32_t playerId, int rank) {
    const int i = carIndex(playerId);
    if (i < 0) return;
    m_game->cars[static_cast<size_t>(i)].finishRank = rank;
}

// FUN 0049fa90 car 1 recorder runs inside local tick step 4 while state is one
void RaceSim::setGhostRecording(bool on) {
    if (!m_localReady) return;
    CarState& car = m_game->cars[0];
    if (on) {
        car.remote.ghost.writeIndex = 0;
        car.remote.ghost.readIndex = 0;
        car.remote.ghost.tickCounter = 0;
        car.finishedOrSpectating = 1;
    } else if (car.finishedOrSpectating == 1) {
        car.finishedOrSpectating = 0;
    }
}

std::vector<GhostSample> RaceSim::localGhostSamples() const {
    std::vector<GhostSample> out;
    if (!m_localReady) return out;
    const GhostRing& ring = m_game->cars[0].remote.ghost;
    const int count = std::min(ring.writeIndex, static_cast<int>(ring.samples.size()));
    out.assign(ring.samples.begin(), ring.samples.begin() + count);
    return out;
}

int RaceSim::spawnGhost(uint32_t ghostId, const std::vector<GhostSample>& samples) {
    if (samples.empty()) return -1;
    const GhostSample& first = samples.front();
    const int i = spawnRemote(ghostId, first.pos[0], first.pos[1], first.pos[2], static_cast<float>(first.yawByte) * (360.f / 255.f));
    if (i < 0) return -1;
    GhostRing& ring = m_game->cars[static_cast<size_t>(i)].remote.ghost;
    const size_t count = std::min(samples.size(), ring.samples.size());
    for (size_t k = 0; k < count; ++k) ring.samples[k] = samples[k];
    ring.writeIndex = static_cast<int>(count);
    ring.readIndex = 0;
    ring.tickCounter = 0;
    m_ghostReplay[static_cast<size_t>(i)] = false;
    return i;
}

void RaceSim::setGhostReplay(int carIndex, bool on) {
    if (carIndex <= 0 || carIndex >= kCarSlotCount) return;
    m_ghostReplay[static_cast<size_t>(carIndex)] = on;
    m_game->cars[static_cast<size_t>(carIndex)].finishedOrSpectating = on ? 2 : 0;
}

int RaceSim::advance(float dt) {
    if (!m_world || !m_localReady) return 0;
    m_accum += dt;
    int ticks = 0;
    GameState& game = *m_game;
    const ColTrack& track = m_world->scene.collision;
    while (m_accum >= kTickSeconds && ticks < kMaxTicksPerAdvance) {
        m_accum -= kTickSeconds;
        cars_frame_update(game, track, m_nowMs);
        // sub 4C7ED0 and sub 4BA380 the two rabbit pools carry their car each frame
        gimmick_pool_update_pool(game, game.carryPool, kGimmickCarryGrabCode, track, m_nowMs);
        gimmick_pool_update_pool(game, game.effect900Pool, kGimmickBlueGrabCode, track, m_nowMs);
        // remote mover keeps its own pose car record copy feeds overlap and rival scans
        for (int i = 1; i < kCarSlotCount; ++i) {
            CarState& car = game.cars[static_cast<size_t>(i)];
            if (car.slotOccupied == 0) continue;
            // stock ghost car runs car tick in state 2 port routes it here instead
            if (m_ghostReplay[static_cast<size_t>(i)] && car.remote.ghost.writeIndex > 1) car_ghost_sample_apply(car.remote, false);
            car.posX = car.remote.posX; car.posY = car.remote.posY; car.posZ = car.remote.posZ;
            car.yawDeg = car.remote.yawDeg;
            car.velX = car.remote.velX; car.velY = car.remote.velY; car.velZ = car.remote.velZ;
            car.boostState = car.remote.boostState;
            car.boostKind = car.remote.boostKind;
            car.reverseFlag = car.remote.reverseFlag;
            car.driftState = car.remote.driftState;
            car.miniTurboStage = car.remote.miniTurboStage;
            car.turnState = car.remote.turnState;
            car.rpm = car.remote.rpm;
        }
        game.keyPressed.fill(0);
        game.accelKeyPressed = 0;
        m_nowMs += kTickMs;
        ++ticks;
    }
    if (m_accum > kTickSeconds * kMaxTicksPerAdvance) m_accum = 0.f;
    updateRearSurfaces();
    updateDriftBand(dt);
    return ticks;
}

// 0x4ADC20 race stage branch band climbs while drift holds resets at full mark
void RaceSim::updateDriftBand(float dt) {
    if (dt <= 0.f) return;
    const CarState& car = m_game->cars[0];
    if (m_gaugeFlash != 0) {
        m_gaugeFlashAge += dt;
        if (m_gaugeFlashAge > kBandFlashSeconds) m_gaugeFlash = 0;
    }
    int airborne = 0;
    for (int w = 0; w < 4; ++w) airborne += car.body.wheels.wheelOnGround[static_cast<size_t>(w)] == 1 ? 1 : 0;
    bool charging = car.driftState != 0 && std::fabs(car.driftGauge) > kBandDriftGate;
    if (airborne > kBandAirborneLimit) charging = false;
    m_gaugeSpark = charging;
    if (!m_gaugeCharging) {
        if (charging) { m_gaugeCharging = true; m_gaugeStart = m_gaugeValue; }
    } else if (charging) {
        float rate = kBandRate * m_bandPetScale;
        if (car.miniTurboStage != 0) rate *= kBandStageScale;
        if (car.boostState != 0) rate *= kBandBoostScale;
        m_gaugeValue += rate * dt;
        if (m_gaugeValue >= kBandFull) {
            // full band hands its third to blue stock then starts from nothing with red get
            if (m_gaugeTeam) m_gaugeValueBlue += (m_gaugeValue - m_gaugeStart) * kBandFullShare;
            m_gaugeValue = 0.f;
            m_gaugeStart = 0.f;
            m_gaugeCharging = false;
            m_gaugeFlash = 1;
            m_gaugeFlashAge = 0.f;
        }
    } else {
        // released drift hands half of what it gained band keeps its level
        if (m_gaugeTeam && m_gaugeValue > m_gaugeStart)
            m_gaugeValueBlue += (m_gaugeValue - m_gaugeStart) * kBandReleaseShare;
        m_gaugeCharging = false;
    }
    // sub 4ADB90 full blue stock fires blue boost get and starts again
    if (m_gaugeValueBlue >= kBandFull) { m_gaugeValueBlue = 0.f; m_gaugeFlash = 2; m_gaugeFlashAge = 0.f; }
    const float blend = 1.f - std::pow(1.f - kBandEase, dt / kStockFrameSeconds);
    m_gaugeShown += (m_gaugeValue - m_gaugeShown) * blend;
    m_gaugeShown = std::min(kBandFull, std::max(0.f, m_gaugeShown));
    m_gaugeShownBlue += (m_gaugeValueBlue - m_gaugeShownBlue) * blend;
    m_gaugeShownBlue = std::min(kBandFull, std::max(0.f, m_gaugeShownBlue));
}

CarPose RaceSim::pose(int carIndex) const {
    CarPose p;
    if (carIndex < 0 || carIndex >= kCarSlotCount) return p;
    const CarState& car = m_game->cars[static_cast<size_t>(carIndex)];
    p.x = car.posX; p.y = car.posY; p.z = car.posZ;
    p.yawDeg = car.yawDeg;
    if (carIndex == 0) {
        p.speed = car.speed;
        p.speedKmh = car.speedKmh;
    } else {
        p.speed = std::sqrt(car.velX * car.velX + car.velY * car.velY + car.velZ * car.velZ);
        p.speedKmh = p.speed * kSpeedToKmh;
    }
    p.boosting = car.boostState != 0;
    p.boostKind = car.boostKind;
    p.reversing = car.reverseFlag == 1;
    p.driftState = car.driftState;
    p.miniTurboStage = car.miniTurboStage;
    p.turnState = car.turnState;
    p.driftGauge = car.driftGauge;
    p.rpm = car.rpm;
    p.greenLight = m_game->startLightState >= 4;
    // effect signals for car flags tick keeps and surface under rear wheels
    CarEffectSignals& s = p.signals;
    p.hasSignals = true;
    s.airborne = car.airborneFlag != 0.f;
    s.landed = car.landedFlag == 1 && m_nowMs - car.landedAtMs < kLandedShowMs;
    s.draft = car.draftActiveFlag != 0;
    s.hitCode = car.effect.activeCode;
    for (int w = 0; w < 4; ++w) s.grip[w] = car.wheelsTick[static_cast<size_t>(w)].grip;
    for (int side = 0; side < 2; ++side)
        s.groundKind[side] = dustKindOf(m_rearSurface[static_cast<size_t>(carIndex)][static_cast<size_t>(side)]);
    const Impact& impact = m_impact[static_cast<size_t>(carIndex)];
    s.impactTier = impact.tier;
    s.impactAgeMs = impact.tier >= 0 ? static_cast<int>(m_nowMs - impact.atMs) : 0;
    // drift steer from row scaled and clamped like tick does at 0x49D3C9
    float driftSteer = stat_total(m_game->cars[0].stats, KartStatIndex::DriftSteer) * kDriftSteerScale +
                       kDriftSteerClampFloor;
    if (driftSteer < kDriftSteerClampFloor) driftSteer = kDriftSteerClampFloor;
    if (driftSteer > kDriftSteerClampCeil) driftSteer = kDriftSteerClampCeil;
    if (carIndex == 0) {
        if (!m_localReady) return p;
        // car effect lean update 0x49B591 lean times slip times body R side lean keeps its gauge part
        const Mat3& r = car.body.wheels.orientationR;
        Mat3 bodyWire;
        body_mat3_set(bodyWire, r.m[0][0], r.m[0][1], -r.m[0][2], r.m[1][0], r.m[1][1], -r.m[1][2], -r.m[2][0],
                      -r.m[2][1], r.m[2][2]);
        const float slipRad = car.driftSlipDeg * kDegToRad;
        Mat3 slipZ;
        body_mat3_set(slipZ, std::cos(slipRad), -std::sin(slipRad), 0.f, std::sin(slipRad), std::cos(slipRad), 0.f,
                      0.f, 0.f, 1.f);
        const float liftRad = car.leanPitchDeg * kDegToRad;
        Mat3 liftY;
        body_mat3_set(liftY, std::cos(liftRad), 0.f, std::sin(liftRad), 0.f, 1.f, 0.f, -std::sin(liftRad), 0.f,
                      std::cos(liftRad));
        const float shakePart = stat_total(car.stats, KartStatIndex::WheelSteerAngle) * car_suspension_shake(car);
        const float leanRad = (car.leanRollDeg + shakePart) * kDegToRad;
        Mat3 leanX;
        body_mat3_set(leanX, 1.f, 0.f, 0.f, 0.f, std::cos(leanRad), -std::sin(leanRad), 0.f, std::sin(leanRad),
                      std::cos(leanRad));
        Mat3 lean, slipLean, world;
        body_mat3_multiply(lean, liftY, leanX);
        body_mat3_multiply(slipLean, slipZ, lean);
        body_mat3_multiply(world, bodyWire, slipLean);
        // port keeps column order bx row order is its transpose
        for (int rr = 0; rr < 3; ++rr)
            for (int c = 0; c < 3; ++c) p.body[rr * 4 + c] = world.m[c][rr];
        p.body[12] = car.posX;
        p.body[13] = car.posY;
        p.body[14] = car.posZ;
        p.hasBody = true;
        for (int w = 0; w < 4; ++w) p.wheelSpin[w] = car.wheelsTick[static_cast<size_t>(w)].spinAngle;
        p.wheelSteer = car.wheelsTick[0].steerAngle;
        p.hasWheels = true;
        p.steerAverage = car.yawRateBody;
        p.driftSlipDeg = car.driftSlipDeg;
        p.pitchDeg = car.pitchDeg;
        // row tail lands at base 14 15 16 with wire order feed zero there means no row
        if (car.stats.base[14] > 1.f) p.camDistance = car.stats.base[14];
        if (car.stats.base[15] > 1.f) p.camPitchDeg = car.stats.base[15];
        if (car.stats.base[16] > 0.1f) p.camHeight = car.stats.base[16];
    } else {
        p.steerAverage = car.remote.yawRateLean;
        p.driftSlipDeg = driftSteer * car.remote.driftGaugeSmoothed;
    }
    return p;
}

MotionSend0x40 RaceSim::localMotion() const {
    const CarState& car = m_game->cars[0];
    MotionSendInputs in;
    in.pos[0] = car.posX; in.pos[1] = car.posY; in.pos[2] = car.posZ;
    in.vel[0] = car.velX; in.vel[1] = car.velY; in.vel[2] = car.velZ;
    in.yawDeg = car.yawDeg;
    in.driftGaugeSmoothed = car.driftGaugeSmoothed;
    in.driftGauge = car.driftGauge;
    in.rpm = car.rpm;
    in.frameDt = kTickSeconds;
    in.miniTurboBoost = car.boostState != 0 && car.boostKind == 0;
    in.itemBoost = car.boostState != 0 && car.boostKind != 0;
    in.reverse = car.reverseFlag == 1;
    in.drift = car.driftState != 0;
    in.miniTurboStage1 = car.miniTurboStage == 1;
    in.turnState = car.turnState;
    return motion_send_0x40(in);
}

int RaceSim::localCheckpointFace() {
    if (!m_localReady) return -1;
    CarState& car = m_game->cars[0];
    if (!car.body.groundQuery.active) return -1;
    int best = -1;
    for (int w = 0; w < 4; ++w) {
        const Vec3& p = car.wheelProbePoint[w];
        const char* name = world_wheel_query_surface(car.body.groundQuery, p.x, p.y);
        if (!name) continue;
        if (std::strncmp(name, "START", 5) == 0) { best = 0; break; }
        int index = 0;
        if (std::sscanf(name, "CHECK_%d", &index) == 1 && index > 0) { best = index; break; }
    }
    return best;
}

bool RaceSim::localOnFace(const char* upperName) {
    if (!m_localReady || !upperName) return false;
    CarState& car = m_game->cars[0];
    if (!car.body.groundQuery.active) return false;
    for (int w = 0; w < 4; ++w) {
        const Vec3& p = car.wheelProbePoint[w];
        const char* name = world_wheel_query_surface(car.body.groundQuery, p.x, p.y);
        if (!name) continue;
        size_t i = 0;
        for (; upperName[i] != 0 && name[i] != 0; ++i)
            if (std::toupper(static_cast<unsigned char>(name[i])) != upperName[i]) break;
        if (upperName[i] == 0 && name[i] == 0) return true;
    }
    return false;
}

const std::vector<CheckpointPoint>& RaceSim::line() const {
    return m_game->checkpoints.lists[0];
}


float RaceSim::draftFactor() const {
    return m_game ? m_game->draftFactor : 0.f;
}

bool RaceSim::draftActive() const {
    return m_game && m_localReady && m_game->cars[0].draftActiveFlag != 0;
}

// stock hud draws slip stream plus this once local car runs over 50 kmh
float RaceSim::slipStreamBonus() const {
    if (!m_game || !m_localReady) return -1.f;
    if (m_game->cars[0].speedKmh < 50.f) return -1.f;
    return draftFactor() * 100.f - 15.f;
}

}
