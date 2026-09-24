#include "RaceEffects.h"

#include "engine/render/nif_prop_model.h"
#include "engine/render/scene_renderer.h"
#include "engine/render/texture_cache.h"

#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace KnC::Client {

using namespace KnC::Render;
using namespace KnC::Tools;

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kNoModel = static_cast<std::size_t>(-1);

// drift sparks show past 10 of gauge either way 0x59F404 and 0x5A324C
constexpr float kSparkGaugeGate = 10.f;
// rear wheel spark sits 0x5A32B4 0 7 above wheel dummy
constexpr float kSparkLift = 0.7f;
// landing puff plays 800 ms car 0x373C crash sprite plays 1500 ms
constexpr float kLandSeconds = 0.8f;
constexpr float kHitSeconds = 1.5f;
// wheel dust needs 10 km per hour 0x59F404 ground shake uses same gate
constexpr float kDustSpeedGate = 10.f;
// landing puff rides car scaled 0 9 moved 0 4 toward nose 0x48F14D
constexpr float kLandScale = 0.9f;
constexpr float kLandForward = -0.4f;
// exhaust smoke scaled 1 by 2 5 by 2 5 on its dummy 0x4918 7A
constexpr float kSmogScaleY = 2.5f;
// crash sprites of effect pool scaled 0 6 0x4D2120
constexpr float kHitScale = 0.6f;
// car impact effect play 0x4981B0 sheet sits quarter way from eye to car
constexpr float kImpactToward = 0.25f;
// repeat of same tier within this counts as one bump hit call and port hook both land here
constexpr float kImpactRepeatGuard = 0.1f;
// car dust 0x4A44A0 emitter frequency is km per hour times 0 005 clamped 0 1 to 8 then doubled
constexpr float kDustRatePerKmh = 0.005f;
constexpr float kDustRateFloor = 0.1f;
constexpr float kDustRateCeil = 8.f;
constexpr float kDustRateDouble = 2.f;
// water kind runs rate four times 0x5A0054 and gravity a fifth 0x5A15EC
constexpr float kDustWaterRate = 4.f;
constexpr float kDustWaterGravity = 0.2f;
// gravity strength is 250 minus km per hour clamped 50 to 250 0x5A6A34 0x59F450
constexpr float kDustGravityBase = 250.f;
constexpr float kDustGravityFloor = 50.f;
// landing guess needs drop over one unit like exe rule on car 0x371C
constexpr float kLandDrop = 1.f;
// shadow fan darkens 55 percent at centre fades to rim
constexpr float kShadowAlpha = 0.55f;
constexpr int kShadowSegments = 20;
constexpr float kShadowInnerReach = 0.7f;
// exe speed 0x32F4 is unit speed times 1 728 remote pose has no km per hour
constexpr float kUnitToKmh = 1.728f;

std::string lower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// finds file in folder any case empty when folder lacks it
std::string findFileCi(const std::string& dir, const std::string& name) {
    std::error_code ignored;
    const fs::path direct = fs::path(dir) / name;
    if (fs::exists(direct, ignored)) return direct.string();
    const std::string wanted = lower(name);
    for (const auto& entry : fs::directory_iterator(dir, ignored))
        if (entry.is_regular_file(ignored) && lower(entry.path().filename().string()) == wanted)
            return entry.path().string();
    return std::string();
}

// finds folder in folder any case given spelling when missing
std::string findDirCi(const std::string& dir, const std::string& name) {
    std::error_code ignored;
    const fs::path direct = fs::path(dir) / name;
    if (fs::is_directory(direct, ignored)) return direct.string();
    const std::string wanted = lower(name);
    for (const auto& entry : fs::directory_iterator(dir, ignored))
        if (entry.is_directory(ignored) && lower(entry.path().filename().string()) == wanted)
            return entry.path().string();
    return direct.string();
}

// moves dummy matrix by a lift then multiplies by car matrix
void placeAt(const float local[16], float lift, const float car[16], float out[16]) {
    float translate[16];
    bx::mtxTranslate(translate, local[12], local[13], local[14] + lift);
    bx::mtxMul(out, translate, car);
}

const char* dustNif(int kind, bool local) {
    switch (kind) {
    case 1: return local ? "particle_grass" : "particle_grass1";
    case 2: return local ? "particle_water" : "particle_water1";
    case 3: return local ? "particle_snow" : "particle_snow1";
    default: return local ? "particle_dust" : "particle_dust1";
    }
}

}

std::string raceGameDirOf(const std::string& trackDir) {
    if (trackDir.empty()) return std::string();
    fs::path dir(trackDir);
    for (int up = 0; up < 5; ++up) dir = dir.parent_path();
    return dir.string();
}

void RaceEffects::reset(const std::string& gameDir) {
    m_cars.clear();
    m_models.clear();
    m_paths.clear();
    m_gameDir = gameDir;
    m_awaitingParse = false;
    // KNC FX OFF disables every effect a bisect aid when race crashes
    if (std::getenv("KNC_FX_OFF") != nullptr) m_gameDir.clear();
    const char* test = std::getenv("KNC_FX_TEST");
    m_test = test != nullptr;
    m_testPhase = m_test ? std::atoi(test) : 0;
    if (m_test) std::printf("[fx] KNC FX TEST %d on the local car\n", m_testPhase);
    // KNC FX SHOT names png written once sparks and flame show together on local car
    const char* shot = std::getenv("KNC_FX_SHOT");
    m_shotPath = shot != nullptr ? shot : "";
    m_shotFrames = 0;
    // KNC FX PROBE names nif prints its particle systems parts and pose at four times
    if (const char* probe = std::getenv("KNC_FX_PROBE")) {
        if (const PropModel* model = loadNif(probe)) {
            ModelPose posed;
            evaluate_model_animation(model->animation, 0.5f, posed);
            for (const ParticleSystemDefinition& system : model->particle_systems) {
                const float* node = system.node >= 0 && static_cast<size_t>(system.node) * 16 < posed.node_world.size()
                                        ? &posed.node_world[static_cast<size_t>(system.node) * 16] : nullptr;
                std::printf("[fx] probe %s node %d node scale %.3f birth scale %.3f rest scale %.3f flip %zu mesh %d half %.3f uv %.2f %.2f to %.2f %.2f patches %zu\n",
                            system.name.c_str(), system.node,
                            node ? std::sqrt(node[0] * node[0] + node[1] * node[1] + node[2] * node[2]) : 1.f,
                            std::sqrt(system.birth[0] * system.birth[0] + system.birth[1] * system.birth[1] +
                                      system.birth[2] * system.birth[2]),
                            std::sqrt(system.rest[0] * system.rest[0] + system.rest[1] * system.rest[1] +
                                      system.rest[2] * system.rest[2]),
                            system.flip_textures.size(), system.mesh_particles ? 1 : 0, system.mesh_half_extent,
                            system.mesh_uv_min[0], system.mesh_uv_min[1], system.mesh_uv_max[0], system.mesh_uv_max[1],
                            system.mesh_uv_patches.size());
            }
            for (std::size_t i = 0; i < model->parts.size(); ++i) {
                const PropPart& part = model->parts[i];
                std::printf("[fx] probe part %zu %s node %d uv %d alpha %d\n", i,
                            fs::path(part.texture_path).filename().string().c_str(), part.animation.node,
                            part.animation.uv, part.animation.alpha);
            }
            for (float t : {0.f, 0.1f, 0.3f, 0.6f}) {
                evaluate_model_animation(model->animation, t, posed);
                std::printf("[fx] probe t %.1f nodes", t);
                for (std::size_t n = 0; n * 16 < posed.node_world.size(); ++n) {
                    const float* m = &posed.node_world[n * 16];
                    std::printf(" %.3f", std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]));
                }
                std::printf(" alpha");
                for (float a : posed.alpha) std::printf(" %.2f", a);
                std::printf("\n");
            }
        }
    }
}

std::string RaceEffects::cachedPath(const std::string& key, const std::string& dir, const std::string& file) const {
    const auto found = m_paths.find(key);
    if (found != m_paths.end()) return found->second;
    const std::string path = findFileCi(findDirCi(findDirCi(findDirCi(m_gameDir + "/Data", "Public"), "Car"), dir), file);
    m_paths.emplace(key, path);
    return path;
}

std::string RaceEffects::effectNif(const char* name) const {
    if (m_gameDir.empty()) return std::string();
    return cachedPath(std::string("Effect|") + name, "Effect", std::string(name) + ".nif");
}

std::string RaceEffects::partNif(const char* folder, const std::string& model) const {
    if (m_gameDir.empty() || model.empty()) return std::string();
    return cachedPath(std::string(folder) + "|" + model, folder, model + ".nif");
}

namespace {

// car effect names decide asks for local and remote spelling of each
const char* const kEffectNames[] = {"land", "slips", "turbo", "turbo_wind", "smog", "drift1", "drift2",
                                    "drift1_p", "drift2_p", "particle_dust", "particle_dust1",
                                    "particle_grass", "particle_grass1", "particle_water", "particle_water1",
                                    "particle_snow", "particle_snow1"};

// parses nif like loadNif then resolves textures beside it
bool parseEffectNif(const std::string& path, PropModel& model) {
    NifModelRequest request;
    request.nif_path = path;
    request.texture_dir = fs::path(path).parent_path().string();
    std::string error;
    if (!load_prop_model(request, model, error)) {
        std::printf("[fx] %s failed %s\n", path.c_str(), error.c_str());
        return false;
    }
    resolve_textures(request.texture_dir, model);
    return true;
}

}

std::map<std::string, PropModel> RaceEffects::parseAll(const std::string& gameDir, const std::atomic<bool>* stop) {
    std::map<std::string, PropModel> out;
    if (gameDir.empty() || std::getenv("KNC_FX_OFF") != nullptr) return out;
    const std::string carEffect = findDirCi(findDirCi(findDirCi(gameDir + "/Data", "Public"), "Car"), "Effect");
    const std::string hitDir = findDirCi(findDirCi(gameDir + "/Data", "Public"), "Effect");
    std::vector<std::string> paths;
    for (const char* name : kEffectNames) paths.push_back(findFileCi(carEffect, std::string(name) + ".nif"));
    paths.push_back(findFileCi(hitDir, "crush_B.nif"));
    paths.push_back(findFileCi(hitDir, "crush_S.nif"));
    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    paths.erase(std::remove(paths.begin(), paths.end(), std::string()), paths.end());
    // nif reader keeps no shared state so files parse side by side
    std::vector<PropModel> models(paths.size());
    std::vector<char> parsed(paths.size(), 0);
    std::atomic<size_t> next{0};
    auto work = [&]() {
        for (size_t at = next++; at < paths.size(); at = next++) {
            if (stop != nullptr && stop->load()) return;
            parsed[at] = parseEffectNif(paths[at], models[at]) ? 1 : 0;
        }
    };
    std::vector<std::thread> helpers;
    for (size_t i = 1; i < std::min<size_t>(2, paths.size()); ++i)
        helpers.emplace_back([&work]() { KnC::Render::lower_current_thread_priority(); work(); });
    work();
    for (std::thread& helper : helpers) helper.join();
    for (size_t i = 0; i < paths.size(); ++i)
        if (parsed[i]) out.emplace(paths[i], std::move(models[i]));
    return out;
}

void RaceEffects::adopt(std::map<std::string, PropModel>&& models) {
    for (auto& entry : models) m_models.emplace(entry.first, std::move(entry.second));
    m_awaitingParse = false;
    std::printf("[fx] %zu effect nifs parsed before the race\n", m_models.size());
}

void RaceEffects::setSignals(int handle, const CarEffectSignals& signals) {
    Car& c = m_cars[handle];
    c.signals = signals;
    c.signalsGiven = true;
}

void RaceEffects::hit(int handle, int code, float clock) {
    Car& c = m_cars[handle];
    // car effect apply 0x496066 only ice code 1000 plays impact sprite tier 1 rest is physics
    if (code == 1000) impact(c, 1, clock);
}

void RaceEffects::impact(Car& c, int tier, float clock) {
    if (tier < 0 || tier > 1) return;
    if (c.hitTier == tier && clock - c.hitAt < kImpactRepeatGuard) return;
    c.hitAt = clock;
    c.hitTier = tier;
    c.pools[kHit].restart = true;
}

void RaceEffects::setLook(int handle, const std::string& plateModel, const std::string& antModel) {
    Car& c = m_cars[handle];
    c.plateModel = plateModel;
    c.antModel = antModel;
}

void RaceEffects::followDummies(int handle, const float name[16], const float ant[16]) {
    auto it = m_cars.find(handle);
    if (it == m_cars.end() || !it->second.alive) return;
    Car& c = it->second;
    bx::mtxMul(c.pools[kPlate].world, name, c.world);
    bx::mtxMul(c.pools[kAnt].world, ant, c.world);
}

void RaceEffects::remove(int handle) {
    auto it = m_cars.find(handle);
    if (it != m_cars.end()) it->second.alive = false;
}

void RaceEffects::shakeWheels(int handle, GhostWheelState& wheels) const {
    auto it = m_cars.find(handle);
    if (it == m_cars.end() || !it->second.alive) return;
    const Car& c = it->second;
    const float kmh = c.pose.speedKmh > 0.f ? c.pose.speedKmh : c.pose.speed * kUnitToKmh;
    ghost_wheels_shake(kmh, c.signals.grip, !c.signals.airborne, wheels);
}

void RaceEffects::update(int handle, const CarPose& pose, const GhostCar* car, const float world[16],
                         float dt, float clock) {
    if (m_gameDir.empty()) return;
    Car& c = m_cars[handle];
    c.alive = true;
    c.local = pose.hasBody;
    c.pose = pose;
    c.car = car;
    c.clock = clock;
    for (int i = 0; i < 16; ++i) c.world[i] = world[i];
    if (c.pose.speedKmh <= 0.f) c.pose.speedKmh = c.pose.speed * kUnitToKmh;
    // sim fills signals for a race pose preview pose has none
    if (pose.hasSignals) {
        c.signals = pose.signals;
        c.signalsGiven = true;
    }

    if (!c.signalsGiven) {
        // landing guess fast rise starts flight stop of fall ends it
        const float vz = c.prevValid && dt > 0.f ? (pose.z - c.prevZ) / dt : 0.f;
        if (c.prevValid) {
            if (!c.flying && vz > 1.5f) { c.flying = true; c.peakZ = pose.z; }
            if (c.flying) {
                c.peakZ = std::max(c.peakZ, pose.z);
                if (c.prevVz < -2.f && vz > -0.2f) {
                    c.flying = false;
                    c.signals.landed = c.peakZ - pose.z > kLandDrop;
                }
            }
        }
        c.signals.airborne = c.flying;
        c.prevZ = pose.z;
        c.prevVz = vz;
        c.prevValid = true;
    }

    if (m_test && c.local) {
        // 16 second loop stage one sparks stage two sparks boost landing slip stream hit
        c.testClock += dt;
        const float t = std::fmod(c.testClock, 16.f);
        c.pose.speedKmh = 90.f;
        if (m_testPhase == 0) {
            c.pose.driftState = t < 8.f ? 2 : 0;
            c.pose.driftGauge = t < 8.f ? -40.f : 0.f;
            c.pose.miniTurboStage = t >= 4.f && t < 8.f ? 1 : 0;
            c.pose.boosting = t >= 8.f && t < 12.f;
            c.signals.draft = t >= 12.f;
            if (t >= 8.4f && t < 8.5f && clock - c.landAt > 2.f) c.signals.landed = true;
            if (t >= 12.5f && t < 12.6f && clock - c.hitAt > 2.f) hit(handle, 1000, clock);
        } else {
            // one held phase 1 sparks 2 charged 3 boost 4 landings 5 slip stream 6 hits 7 dust 8 parked
            c.pose.driftState = m_testPhase <= 2 ? 2 : 0;
            c.pose.driftGauge = m_testPhase <= 2 ? -40.f : 0.f;
            c.pose.miniTurboStage = m_testPhase == 2 ? 1 : 0;
            c.pose.boosting = m_testPhase == 3;
            c.signals.draft = m_testPhase == 5;
            c.signals.landed = m_testPhase == 4 && std::fmod(c.testClock, 2.f) < dt;
            if (m_testPhase == 6 && std::fmod(c.testClock, 3.f) < dt) hit(handle, 1000, clock);
            if (m_testPhase == 8) c.pose.speedKmh = 0.f;
        }
    }

    decide(c, dt);
    place(c);
    if (c.local && !m_shotPath.empty()) {
        const bool sparks = c.pools[kDrift1L].on || c.pools[kDrift2L].on;
        const bool flame = c.pools[kTurboL].on;
        m_shotFrames = sparks && flame ? m_shotFrames + 1 : 0;
        // fourth frame lets flame grow before shot
        if (m_shotFrames == 4) {
            std::printf("[fx] shot %s at clock %.2f speed %.0f\n", m_shotPath.c_str(), clock, c.pose.speedKmh);
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, m_shotPath.c_str());
            m_shotPath.clear();
        }
    }
}

void RaceEffects::decide(Car& c, float dt) {
    (void)dt;
    const CarPose& p = c.pose;
    CarEffectSignals& s = c.signals;
    for (Pool& pool : c.pools) pool.on = false;
    // airborne gate only from port flag z guess feeds landing puff alone
    const bool onGround = !c.signalsGiven || !s.airborne;

    // car 0x373C puff restarts when flag rises and stays 800 ms
    if (s.landed && !c.wasLanded) {
        c.landAt = c.clock;
        c.pools[kLand].restart = true;
    }
    c.wasLanded = s.landed;
    // guess and caller without port flag hand one frame of landed
    if (!c.signalsGiven) s.landed = false;
    c.pools[kLand].on = c.clock - c.landAt < kLandSeconds;
    c.pools[kLand].nif = effectNif("land");

    // car 0x3715 slip stream sprite while draft runs
    c.pools[kSlips].on = s.draft;
    c.pools[kSlips].nif = effectNif("slips");

    // car 0x3300 boost shows two flames and wind reset on start else local exhaust smoke
    if (p.boosting) {
        for (Slot slot : {kTurboL, kTurboR, kTurboWind}) {
            c.pools[slot].on = true;
            if (!c.wasBoosting) c.pools[slot].restart = true;
        }
    } else if (c.local) {
        c.pools[kSmogL].on = true;
        c.pools[kSmogR].on = true;
    }
    c.wasBoosting = p.boosting;
    c.pools[kTurboL].nif = c.pools[kTurboR].nif = effectNif("turbo");
    c.pools[kTurboWind].nif = effectNif("turbo_wind");
    c.pools[kSmogL].nif = c.pools[kSmogR].nif = effectNif("smog");

    // car 0x35A4 drift past 10 of gauge stage one 0x35F0 shows second pair else first
    if (p.driftState != 0 && std::fabs(p.driftGauge) > kSparkGaugeGate && onGround) {
        const bool charged = p.miniTurboStage == 1;
        c.pools[charged ? kDrift2L : kDrift1L].on = true;
        c.pools[charged ? kDrift2R : kDrift1R].on = true;
    }
    c.pools[kDrift1L].nif = c.pools[kDrift1R].nif = effectNif(c.local ? "drift1" : "drift1_p");
    c.pools[kDrift2L].nif = c.pools[kDrift2R].nif = effectNif(c.local ? "drift2" : "drift2_p");

    // car dust 0x49A390 over 10 km per hour on ground by kind under each rear wheel
    for (int side = 0; side < 2; ++side) {
        const int kind = s.groundKind[side];
        const bool dusting = p.speedKmh > kDustSpeedGate && onGround && kind >= 0;
        const Slot slot = side == 0 ? kDustL : kDustR;
        c.pools[slot].on = dusting;
        if (dusting && !c.wasDusting[side]) c.pools[slot].restart = true;
        c.wasDusting[side] = dusting;
        c.pools[slot].nif = effectNif(dustNif(kind, c.local));
        // car dust 0x4A44A0 writes emitter frequency and gravity strength from speed every frame
        if (!c.dustDrive[side]) c.dustDrive[side] = std::make_shared<ParticleDrive>();
        ParticleDrive& drive = *c.dustDrive[side];
        float rate = std::clamp(p.speedKmh * kDustRatePerKmh, kDustRateFloor, kDustRateCeil);
        if (kind == 2) rate *= kDustWaterRate;
        drive.has_frequency = true;
        drive.frequency = rate * kDustRateDouble;
        float gravity = std::clamp(kDustGravityBase - p.speedKmh, kDustGravityFloor, kDustGravityBase);
        if (kind == 2) gravity *= kDustWaterGravity;
        drive.has_gravity_strength = true;
        drive.gravity_strength = gravity;
    }

    // car impact effect play 0x4981B0 smaller age than last seen means new bump
    if (s.impactTier >= 0 && (c.lastImpactAge < 0 || s.impactAgeMs < c.lastImpactAge))
        impact(c, s.impactTier, c.clock);
    c.lastImpactAge = s.impactTier >= 0 ? s.impactAgeMs : -1;
    // tier 0 big sheet for fast bump tier 1 small one for slow bump or ice
    c.pools[kHit].on = c.clock - c.hitAt < kHitSeconds;
    if (c.pools[kHit].on && !m_gameDir.empty()) {
        // a folder walk on every frame of the 1 5 s sheet is too slow the path stays per race
        const char* sheet = c.hitTier == 0 ? "crush_B.nif" : "crush_S.nif";
        const std::string key = std::string("Hit|") + sheet;
        auto found = m_paths.find(key);
        if (found == m_paths.end())
            found = m_paths.emplace(key, findFileCi(findDirCi(findDirCi(m_gameDir + "/Data", "Public"), "Effect"), sheet)).first;
        c.pools[kHit].nif = found->second;
    }

    // blob shadow every frame plate and antenna show when look names them
    c.pools[kShadow].on = c.car != nullptr && !c.car->wheel_local.empty();
    c.pools[kShadow].nif = "shadow";
    c.pools[kPlate].nif = partNif("Parts", c.plateModel);
    c.pools[kPlate].on = c.car != nullptr && c.car->has_name && !c.pools[kPlate].nif.empty();
    c.pools[kAnt].nif = partNif("Item", c.antModel);
    c.pools[kAnt].on = c.car != nullptr && c.car->has_ant && !c.pools[kAnt].nif.empty();
}

void RaceEffects::place(Car& c) {
    const GhostCar* car = c.car;
    const float* world = c.world;
    // rear pair of four wheels sparks and dust sit on them
    const std::array<float, 16>* rear[2] = {nullptr, nullptr};
    float rearRadius[2] = {0.5f, 0.5f};
    if (car != nullptr && car->wheel_local.size() >= 4) {
        rear[0] = &car->wheel_local[2];
        rear[1] = &car->wheel_local[3];
        rearRadius[0] = car->wheel_radius[2];
        rearRadius[1] = car->wheel_radius[3];
    } else if (car != nullptr && !car->wheel_local.empty()) {
        rear[0] = rear[1] = &car->wheel_local.back();
        rearRadius[0] = rearRadius[1] = car->wheel_radius.back();
    }
    for (int side = 0; side < 2; ++side) {
        if (rear[side] == nullptr) {
            c.pools[side == 0 ? kDrift1L : kDrift1R].on = false;
            c.pools[side == 0 ? kDrift2L : kDrift2R].on = false;
            c.pools[side == 0 ? kDustL : kDustR].on = false;
            continue;
        }
        placeAt(rear[side]->data(), kSparkLift, world, c.pools[side == 0 ? kDrift1L : kDrift1R].world);
        placeAt(rear[side]->data(), kSparkLift, world, c.pools[side == 0 ? kDrift2L : kDrift2R].world);
        placeAt(rear[side]->data(), -rearRadius[side], world, c.pools[side == 0 ? kDustL : kDustR].world);
    }

    // two exhaust dummies rear wheel middle stands in for body without them
    for (int side = 0; side < 2; ++side) {
        float smoke[16];
        if (car != nullptr && car->smoke_local.size() >= 2) {
            bx::mtxTranslate(smoke, car->smoke_local[side][12], car->smoke_local[side][13], car->smoke_local[side][14]);
        } else if (rear[side] != nullptr) {
            const float* w = rear[side]->data();
            bx::mtxTranslate(smoke, w[12] + rearRadius[side], w[13] * 0.5f, w[14]);
        } else {
            bx::mtxTranslate(smoke, 0.8f, side == 0 ? -0.3f : 0.3f, 0.3f);
        }
        bx::mtxMul(c.pools[side == 0 ? kTurboL : kTurboR].world, smoke, world);
        float scale[16];
        bx::mtxScale(scale, 1.f, kSmogScaleY, kSmogScaleY);
        float scaled[16];
        bx::mtxMul(scaled, scale, smoke);
        bx::mtxMul(c.pools[side == 0 ? kSmogL : kSmogR].world, scaled, world);
    }

    for (int i = 0; i < 16; ++i) {
        c.pools[kTurboWind].world[i] = world[i];
        c.pools[kSlips].world[i] = world[i];
        c.pools[kShadow].world[i] = world[i];
    }
    {
        float scale[16], translate[16], local[16];
        bx::mtxScale(scale, kLandScale, kLandScale, kLandScale);
        bx::mtxTranslate(translate, kLandForward, 0.f, 0.f);
        bx::mtxMul(local, scale, translate);
        bx::mtxMul(c.pools[kLand].world, local, world);
    }
    {
        // sheet keeps authored facing scaled 0 6 quarter way from eye to car
        float at[3] = {world[12], world[13], world[14]};
        if (m_eyeValid)
            for (int axis = 0; axis < 3; ++axis)
                at[axis] = m_eye[axis] + (world[12 + axis] - m_eye[axis]) * kImpactToward;
        float scale[16], translate[16];
        bx::mtxScale(scale, kHitScale, kHitScale, kHitScale);
        bx::mtxTranslate(translate, at[0], at[1], at[2]);
        bx::mtxMul(c.pools[kHit].world, scale, translate);
    }
    if (car != nullptr) {
        bx::mtxMul(c.pools[kPlate].world, car->name_local.data(), world);
        bx::mtxMul(c.pools[kAnt].world, car->ant_local.data(), world);
    }
}

const PropModel* RaceEffects::loadNif(const std::string& path) {
    auto it = m_models.find(path);
    if (it != m_models.end()) return it->second.parts.empty() && it->second.particle_systems.empty() ? nullptr : &it->second;
    PropModel& model = m_models[path];
    const auto parseFrom = std::chrono::steady_clock::now();
    if (!parseEffectNif(path, model)) return nullptr;
    std::printf("[fx] %s parsed on the frame thread in %.1f ms\n", fs::path(path).filename().string().c_str(),
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - parseFrom).count());
    // vertex extent tells sprite authored around camera from one on car
    float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
    for (const PropPart& part : model.parts)
        for (const SceneVertex& v : part.vertices) {
            lo[0] = std::min(lo[0], v.x); hi[0] = std::max(hi[0], v.x);
            lo[1] = std::min(lo[1], v.y); hi[1] = std::max(hi[1], v.y);
            lo[2] = std::min(lo[2], v.z); hi[2] = std::max(hi[2], v.z);
        }
    std::printf("[fx] %s %zu parts %zu particle systems", fs::path(path).filename().string().c_str(),
                model.parts.size(), model.particle_systems.size());
    if (!model.parts.empty())
        std::printf(" extent %.1f %.1f %.1f to %.1f %.1f %.1f", lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
    for (const ParticleSystemDefinition& system : model.particle_systems)
        std::printf(" system %s pool %u %s rate %s node %d rest %.2f %.2f %.2f scale %.3f", system.name.c_str(),
                    system.pool_size, system.world_space ? "world" : "local",
                    system.has_birth_rate ? "driven" : "none", system.node, system.rest[12], system.rest[13],
                    system.rest[14],
                    std::sqrt(system.rest[0] * system.rest[0] + system.rest[1] * system.rest[1] +
                              system.rest[2] * system.rest[2]));
    std::printf("\n");
    return &model;
}

// soft dark fan under wheels black at centre clear at rim on ground
static void buildShadow(const GhostCar& car, PropModel& out) {
    float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
    float radius = 0.5f;
    for (std::size_t i = 0; i < car.wheel_local.size(); ++i) {
        const float* w = car.wheel_local[i].data();
        for (int axis = 0; axis < 3; ++axis) {
            lo[axis] = std::min(lo[axis], w[12 + axis]);
            hi[axis] = std::max(hi[axis], w[12 + axis]);
        }
        if (i < car.wheel_radius.size()) radius = car.wheel_radius[i];
    }
    const float cx = 0.5f * (lo[0] + hi[0]);
    const float cy = 0.5f * (lo[1] + hi[1]);
    // wheels sink a little into road fan sits a tenth over their bottom
    const float cz = lo[2] - radius + 0.1f;
    const float rx = 0.5f * (hi[0] - lo[0]) + radius * 1.6f;
    const float ry = 0.5f * (hi[1] - lo[1]) + radius * 1.4f;
    out = PropModel{};
    out.name = "shadow";
    out.lit_by_map_ambient = false;
    PropPart part;
    // KNC FX SHADOW DEBUG paints fan red and solid to see where it lands
    const bool debug = std::getenv("KNC_FX_SHADOW_DEBUG") != nullptr;
    const float centre[4] = {debug ? 1.f : 0.f, 0.f, 0.f, debug ? 1.f : kShadowAlpha};
    const float rim[4] = {debug ? 1.f : 0.f, 0.f, 0.f, debug ? 1.f : 0.f};
    // centre then dark inner ring at seven tenths then clear rim
    SceneVertex middle;
    middle.x = cx; middle.y = cy; middle.z = cz;
    middle.abgr = nif_colour_abgr(centre);
    middle.u = 0.5f; middle.v = 0.5f;
    part.vertices.push_back(middle);
    for (int ring = 0; ring < 2; ++ring) {
        const float reach = ring == 0 ? kShadowInnerReach : 1.f;
        for (int i = 0; i < kShadowSegments; ++i) {
            const float a = static_cast<float>(i) / kShadowSegments * 6.2831853f;
            SceneVertex v;
            v.x = cx + std::cos(a) * rx * reach;
            v.y = cy + std::sin(a) * ry * reach;
            v.z = cz;
            v.abgr = nif_colour_abgr(ring == 0 ? centre : rim);
            v.u = 0.5f + 0.5f * std::cos(a) * reach; v.v = 0.5f + 0.5f * std::sin(a) * reach;
            part.vertices.push_back(v);
        }
    }
    // both windings so surface cull keeps the one facing camera
    const uint32_t segments = static_cast<uint32_t>(kShadowSegments);
    for (uint32_t i = 0; i < segments; ++i) {
        const uint32_t next = 1 + (i + 1) % segments;
        const uint32_t tri[3] = {0, 1 + i, next};
        const uint32_t quad[6] = {1 + i, 1 + segments + i, 1 + segments + (i + 1) % segments,
                                  1 + i, 1 + segments + (i + 1) % segments, next};
        for (int k = 0; k < 3; ++k) part.indices.push_back(tri[k]);
        for (int k = 2; k >= 0; --k) part.indices.push_back(tri[k]);
        for (int k = 0; k < 6; ++k) part.indices.push_back(quad[k]);
        for (int k = 5; k >= 0; --k) part.indices.push_back(quad[k]);
    }
    if (debug) std::printf("[fx] shadow fan at z %.2f radii %.2f %.2f centre %.2f %.2f\n", cz, rx, ry, cx, cy);
    // blend source alpha over ground depth test no write vertex colour unlit
    part.surface.alpha.flags = 0x0001u | (6u << 1) | (7u << 5);
    part.surface.has_alpha = true;
    part.surface.depth.flags = 0x0001u | (3u << 2);
    part.surface.has_depth = true;
    part.surface.vertex_colour.lighting = KnC::NifLightingMode::EmissiveOnly;
    part.surface.vertex_colour.vertex = KnC::NifVertexMode::Emissive;
    part.surface.has_vertex_colour = true;
    part.has_vertex_colours = true;
    part.bound.center[0] = cx; part.bound.center[1] = cy; part.bound.center[2] = cz;
    part.bound.radius = std::max(rx, ry);
    out.bound = part.bound;
    out.parts.push_back(std::move(part));
}

bool RaceEffects::ensureModel(SceneRenderer& renderer, Car& c, Slot slot) {
    Pool& pool = c.pools[slot];
    if (pool.nif.empty()) return false;
    const bool dust = slot == kDustL || slot == kDustR;
    std::string key = slot == kShadow && c.car != nullptr ? "shadow@" + c.car->body.name + std::to_string(c.car->wheel_local.size()) : pool.nif;
    // each rear wheel owns dust copy of world space pool rides one placement with own drive
    if (dust) key += slot == kDustL ? "#L" : "#R";
    auto& appended = c.appended;
    auto found = appended.find(key);
    if (found != appended.end()) {
        pool.model = found->second;
        return pool.model != kNoModel;
    }
    const PropModel* model = nullptr;
    PropModel driven;
    if (slot == kShadow) {
        auto it = m_models.find(key);
        if (it == m_models.end()) {
            PropModel& built = m_models[key];
            buildShadow(*c.car, built);
            it = m_models.find(key);
        }
        model = &it->second;
    } else {
        // a parse here on the frame thread held the grid up to 2 s the load thread hands it soon
        if (m_awaitingParse && slot != kPlate && slot != kAnt && m_models.find(pool.nif) == m_models.end())
            return false;
        model = loadNif(pool.nif);
        if (model != nullptr && dust) {
            driven = *model;
            for (ParticleSystemDefinition& system : driven.particle_systems)
                system.drive = c.dustDrive[slot == kDustL ? 0 : 1];
            model = &driven;
        }
    }
    if (model == nullptr) {
        appended[key] = kNoModel;
        pool.model = kNoModel;
        return false;
    }
    pool.model = renderer.append_prop_model(*model);
    pool.restart = true;
    appended[key] = pool.model;
    return true;
}

void RaceEffects::submit(SceneRenderer& renderer, std::vector<PropInstance>& out, const float eye[3]) {
    if (eye != nullptr) {
        for (int axis = 0; axis < 3; ++axis) m_eye[axis] = eye[axis];
        m_eyeValid = true;
    }
    for (auto& entry : m_cars) {
        Car& c = entry.second;
        if (!c.alive) continue;
        for (int slot = 0; slot < kSlotCount; ++slot) {
            Pool& pool = c.pools[slot];
            if (!pool.on) continue;
            if (!ensureModel(renderer, c, static_cast<Slot>(slot))) continue;
            if (pool.restart) {
                renderer.restart_prop_particles(pool.model, c.clock);
                pool.restart = false;
            }
            PropInstance instance;
            instance.model_index = pool.model;
            instance.layer = SceneLayer::Props;
            for (int i = 0; i < 16; ++i) instance.world[i] = pool.world[i];
            out.push_back(instance);
        }
    }
}

}
