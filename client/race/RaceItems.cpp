#include "RaceItems.h"

#include "engine/render/nif_prop_model.h"
#include "engine/render/scene_renderer.h"
#include "games/kart/physics/client/boost.h"
#include "games/kart/physics/client/effects.h"
#include "games/kart/physics/client/math_helpers.h"
#include "tools/track_scene/ghost_car.h"

#include <bx/math.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace KnC::Client {

using namespace KnC::Kart::Client;
namespace fs = std::filesystem;

namespace {

constexpr float kDeg = 3.14159265f / 180.f;
constexpr std::size_t kNoModel = static_cast<std::size_t>(-1);
constexpr float kStockFrame = 1.f / 60.f;

// FUN 004B0570 table 0 at 0x5EBE50 weights per item by racer count and rank
constexpr int kRollWeights[kItemKindCount][64] = {
    {50, 0, 0, 0, 0, 0, 0, 0, 50, 120, 0, 0, 0, 0, 0, 0, 40, 100, 150, 0, 0, 0, 0, 0, 40, 100, 120, 250, 0, 0, 0, 0, 40, 100, 100, 150, 250, 0, 0, 0, 40, 100, 150, 150, 180, 250, 0, 0, 40, 100, 100, 150, 150, 200, 250, 0, 40, 100, 100, 100, 100, 200, 200, 240},
    {0},
    {20, 0, 0, 0, 0, 0, 0, 0, 250, 10, 0, 0, 0, 0, 0, 0, 250, 10, 10, 0, 0, 0, 0, 0, 240, 10, 10, 0, 0, 0, 0, 0, 240, 10, 10, 10, 0, 0, 0, 0, 250, 10, 10, 10, 10, 0, 0, 0, 250, 10, 10, 10, 10, 10, 0, 0, 260, 10, 10, 10, 10, 10, 10, 10},
    {0, 0, 0, 0, 0, 0, 0, 0, 200, 10, 0, 0, 0, 0, 0, 0, 150, 10, 10, 0, 0, 0, 0, 0, 140, 10, 10, 10, 0, 0, 0, 0, 140, 10, 10, 10, 10, 0, 0, 0, 160, 10, 10, 10, 10, 10, 0, 0, 160, 10, 10, 10, 10, 10, 10, 0, 180, 10, 10, 10, 10, 10, 10, 10},
    {10, 0, 0, 0, 0, 0, 0, 0, 10, 100, 0, 0, 0, 0, 0, 0, 10, 90, 100, 0, 0, 0, 0, 0, 20, 90, 60, 50, 0, 0, 0, 0, 10, 90, 60, 80, 50, 0, 0, 0, 10, 110, 60, 110, 50, 80, 0, 0, 10, 110, 60, 110, 50, 100, 60, 0, 10, 110, 60, 110, 50, 100, 60, 100},
    {6, 0, 0, 0, 0, 0, 0, 0, 70, 50, 0, 0, 0, 0, 0, 0, 110, 50, 50, 0, 0, 0, 0, 0, 110, 50, 40, 10, 0, 0, 0, 0, 110, 50, 50, 50, 10, 0, 0, 0, 110, 50, 50, 50, 30, 10, 0, 0, 100, 50, 50, 50, 30, 50, 10, 0, 100, 50, 50, 50, 30, 50, 10, 10},
    {6, 0, 0, 0, 0, 0, 0, 0, 10, 100, 0, 0, 0, 0, 0, 0, 10, 100, 120, 0, 0, 0, 0, 0, 10, 100, 100, 120, 0, 0, 0, 0, 10, 100, 100, 120, 150, 0, 0, 0, 10, 150, 100, 140, 150, 160, 0, 0, 10, 150, 100, 140, 150, 100, 160, 0, 10, 150, 100, 140, 150, 100, 150, 160},
    {6, 0, 0, 0, 0, 0, 0, 0, 10, 60, 0, 0, 0, 0, 0, 0, 10, 60, 100, 0, 0, 0, 0, 0, 10, 50, 80, 150, 0, 0, 0, 0, 10, 50, 60, 100, 150, 0, 0, 0, 10, 50, 60, 100, 100, 120, 0, 0, 10, 50, 60, 100, 100, 100, 120, 0, 10, 50, 60, 100, 100, 100, 100, 120},
    {4, 0, 0, 0, 0, 0, 0, 0, 150, 20, 0, 0, 0, 0, 0, 0, 150, 50, 20, 0, 0, 0, 0, 0, 120, 50, 50, 10, 0, 0, 0, 0, 120, 50, 80, 50, 10, 0, 0, 0, 150, 50, 80, 50, 60, 10, 0, 0, 150, 50, 80, 50, 60, 50, 10, 0, 150, 50, 80, 50, 60, 50, 50, 10},
    {30, 0, 0, 0, 0, 0, 0, 0, 150, 10, 0, 0, 0, 0, 0, 0, 150, 10, 0, 0, 0, 0, 0, 0, 150, 10, 0, 10, 0, 0, 0, 0, 150, 10, 10, 0, 10, 0, 0, 0, 120, 10, 10, 0, 10, 10, 0, 0, 120, 10, 10, 0, 10, 10, 0, 0, 120, 10, 10, 0, 10, 10, 0, 0},
    {6, 0, 0, 0, 0, 0, 0, 0, 30, 100, 0, 0, 0, 0, 0, 0, 30, 140, 150, 0, 0, 0, 0, 0, 30, 120, 160, 130, 0, 0, 0, 0, 30, 140, 150, 120, 130, 0, 0, 0, 30, 140, 150, 120, 100, 100, 0, 0, 30, 140, 150, 120, 100, 100, 140, 0, 30, 140, 150, 120, 100, 100, 140, 120},
    {6, 0, 0, 0, 0, 0, 0, 0, 10, 50, 0, 0, 0, 0, 0, 0, 10, 50, 50, 0, 0, 0, 0, 0, 10, 50, 20, 0, 0, 0, 0, 0, 10, 50, 50, 80, 0, 0, 0, 0, 10, 50, 50, 80, 10, 0, 0, 0, 10, 50, 50, 80, 10, 40, 0, 0, 10, 50, 50, 80, 10, 40, 50, 10},
    {0},
    {0},
    {30, 0, 0, 0, 0, 0, 0, 0, 10, 50, 0, 0, 0, 0, 0, 0, 30, 80, 100, 0, 0, 0, 0, 0, 50, 100, 80, 30, 0, 0, 0, 0, 30, 80, 100, 30, 30, 0, 0, 0, 30, 80, 100, 30, 100, 30, 0, 0, 30, 80, 100, 30, 100, 60, 30, 0, 30, 80, 100, 30, 100, 60, 100, 10},
    {0, 0, 0, 0, 0, 0, 0, 0, 20, 20, 0, 0, 0, 0, 0, 0, 10, 10, 10, 0, 0, 0, 0, 0, 20, 10, 20, 10, 0, 0, 0, 0, 10, 10, 10, 10, 20, 0, 0, 0, 10, 10, 10, 10, 20, 0, 0, 0, 10, 10, 10, 10, 20, 0, 0, 0, 10, 10, 10, 10, 20, 0, 0, 0},
    {6, 0, 0, 0, 0, 0, 0, 0, 10, 100, 0, 0, 0, 0, 0, 0, 10, 100, 100, 0, 0, 0, 0, 0, 10, 90, 100, 160, 0, 0, 0, 0, 10, 100, 80, 100, 160, 0, 0, 0, 10, 100, 80, 100, 100, 180, 0, 0, 10, 100, 80, 100, 100, 100, 180, 0, 10, 100, 80, 100, 100, 100, 100, 180},
    {20, 0, 0, 0, 0, 0, 0, 0, 20, 150, 0, 0, 0, 0, 0, 0, 20, 120, 50, 0, 0, 0, 0, 0, 20, 120, 100, 30, 0, 0, 0, 0, 10, 50, 100, 50, 30, 0, 0, 0, 10, 50, 100, 50, 100, 30, 0, 0, 10, 50, 100, 50, 100, 50, 30, 0, 10, 50, 100, 50, 100, 50, 10, 10},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 50, 0, 0, 0, 0, 0, 0, 20, 40, 60, 0, 0, 0, 0, 0, 20, 40, 50, 30, 0, 0, 0, 0, 20, 30, 30, 40, 30, 0, 0, 0, 20, 30, 30, 40, 50, 30, 0, 0, 20, 30, 30, 40, 50, 20, 30, 0, 20, 30, 30, 40, 50, 20, 10, 10},
    {0},
    {0},
    {0},
};

float wrap180(float a) {
    while (a > 180.f) a -= 360.f;
    while (a < -180.f) a += 360.f;
    return a;
}

// sub 44DE00 moves a point along a heading the stock turns the heading less 90 into the step
void along(float& x, float& y, float dist, float headingDeg) {
    Vec3 dir;
    math_dir_from_heading_pitch(headingDeg, 0.f, dir);
    x += dir.x * dist;
    y += dir.y * dist;
}

// sub 44E240 plus 90 the yaw that faces from one point to another
float yawToward(float fromX, float fromY, float toX, float toY) {
    return math_atan2_deg(toX - fromX, toY - fromY) + 90.f;
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string findCi(const std::string& dir, const std::string& name) {
    std::error_code ignored;
    const fs::path direct = fs::path(dir) / name;
    if (fs::exists(direct, ignored)) return direct.string();
    const std::string wanted = lower(name);
    for (const auto& entry : fs::directory_iterator(dir, ignored))
        if (lower(entry.path().filename().string()) == wanted) return entry.path().string();
    return std::string();
}

// a stock factor applied once per 60 Hz frame over a frame of dt
float perFrame(float factor, float dt) { return std::pow(factor, dt / kStockFrame); }

// the nifs each manager loads in slot order sub 4CD1B0 sub 4D11D0 and the other item inits
const std::vector<std::string>& nifsOf(int kind) {
    static const std::vector<std::string> none;
    static const std::array<std::vector<std::string>, kItemKindCount> table = {{
        {},
        {},
        {"Spike/spike_throw", "Spike/spike_action", "Spike/spike_boom"},
        {"Storm/ministom_action"},
        {"Thunder/lighting_action", "Thunder/lighting_actiondamge"},
        {"Handle/handel_action"},
        {"Turtle/turtleg_throw", "Turtle/turtleg_pung", "Turtle/turtleg_pung2", "Turtle/turtleg_run"},
        {"Rabbit/ribbit_pung", "Rabbit/ribbit_run"},
        {"Shield/defense_action", "Shield/defense_degi"},
        {"Smoke/smbomb", "Smoke/smbomb_G", "Smoke/smbomb_throw"},
        {"Rocket/rocket", "Rocket/rocket_boom"},
        {"Hive/bee_throw", "Hive/bee_degi"},
        {"Angel/angel_action", "Angel/angel_degi"},
        {"BlueRabbit/ribbit_pung", "BlueRabbit/ribbit2_run"},
        {"Ice/ice_throw", "Ice/ice_degi"},
        {"Flash/flash_throw", "Flash/flash_degi"},
        {"Magnet/magnet_01", "Magnet/magnet_02"},
        {"Hammer/hammer_att", "Hammer/hammer_dmg"},
        {"Bomb/bomb", "Bomb/boomef"},
        {"Dung/ddong_throw", "Dung/ddong_action"},
        {"Devil/devil_att", "Devil/devil_dmg"},
        {"DevilRed/devil_def", "DevilRed/devil_def_throw"},
    }};
    return kind >= 0 && kind < kItemKindCount ? table[static_cast<size_t>(kind)] : none;
}

// the pool sizes of the managers 8 for the storm the hive the ice the smoke and the rabbits
int poolSize(int kind) {
    switch (kind) {
    case kItemStorm: case kItemHive: case kItemIce: case kItemSmoke: case kItemRabbit: case kItemBlueRabbit:
        return 8;
    case kItemDung: return 48;
    default: return 16;
    }
}

// the victim visual of the storm the hive and the ice the attack nif on the car
const char* victimNif(int kind) {
    switch (kind) {
    case kItemStorm: return "Storm/ministom_attack";
    case kItemHive: return "Hive/bee_attack";
    case kItemIce: return "Ice/ice_attack";
    default: return nullptr;
    }
}

// sub 4CC460 sub 4BB900 the thrown hazards settle live sink and grow a frame to 10
constexpr double kThrownSettleMs = 2000.0;
constexpr double kThrownLiveMs = 10000.0;
constexpr double kThrownSinkMs = 2600.0;
constexpr double kBoomHideMs = 1667.0;
constexpr float kThrownGravity = 29.4f;
constexpr float kThrownPitch = 60.f;
constexpr float kThrownStartScale = 3.f;
constexpr float kThrownGrow = 1.06f;
constexpr float kThrownScaleCap = 10.f;
constexpr float kSinkGrow = 1.02f;
constexpr float kGroundLift = 0.2f;
constexpr float kThrownCoarse = 10.f;
constexpr float kSpikeReach = 4.f;
constexpr float kBombReach = 5.f;
constexpr float kBombSplash = 15.f;
constexpr float kGaugeTurn = 1.2f;
constexpr float kFlightLimit = 4.f;
// sub 4CD800 the storm scale 0 5 spray 30 pitch 50 speed 30 gravity 39 2
constexpr float kStormGravity = 39.2f;
constexpr float kStormPitch = 50.f;
constexpr float kStormSpeed = 30.f;
constexpr float kStormReach = 6.f;
constexpr float kStormWind = 36.f;
constexpr double kStormLiveMs = 6000.0;
constexpr double kStormDieMs = 5000.0;
constexpr double kStormVictimMs = 2000.0;
// sub 4C3810 the hive 550 on the line thrown 1800 ms live 5000 then 6600 hit under 10 sting 5000
constexpr float kHiveReach = 550.f;
constexpr double kHiveThrowMs = 1800.0;
constexpr double kHiveLiveMs = 5000.0;
constexpr double kHiveFadeMs = 6600.0;
constexpr float kHiveHit = 10.f;
constexpr double kHiveStingMs = 5000.0;
// sub 4C4790 the ice 380 on the line thrown 1500 live 7200 then 7200 hit under 11 freeze 6500
constexpr float kIceReach = 380.f;
constexpr double kIceThrowMs = 1500.0;
constexpr double kIceLiveMs = 7200.0;
constexpr float kIceHit = 11.f;
constexpr double kIceFreezeMs = 6500.0;
constexpr float kIcePush = 80.f;
// sub 4C1970 the flash 340 on the line 800 then 500 hidden then 500 live under 120 then 500
constexpr float kFlashReach = 340.f;
constexpr double kFlashThrowMs = 800.0;
constexpr double kFlashWaitMs = 500.0;
constexpr double kFlashLiveMs = 500.0;
constexpr float kFlashHit = 120.f;
// sub 489E60 the line walk stops on a point with no racer inside 90
constexpr float kLineClear = 90.f;
// sub 4CB160 the shield holds 4000 ms sub 4B8D20 the angel 5000 ms a pop shows 1000 ms
constexpr double kShieldMs = 4000.0;
constexpr double kAngelMs = 5000.0;
constexpr double kGuardPopMs = 1000.0;
constexpr float kShieldLift = 0.3f;
// sub 4C2FA0 the handle and sub 4BD5F0 the devil ride every other car 5000 ms
constexpr double kCurseMs = 5000.0;
constexpr float kHandleDrop = -0.25f;
// sub 4CFCF0 the turtle toss pitch 50 speed 24 gravity 19 6 scale 7 chase 12000 ms under 10 grabs
constexpr float kTurtlePitch = 50.f;
constexpr float kTurtleSpeed = 24.f;
constexpr float kTurtleGravity = 19.6f;
constexpr float kTurtleStartScale = 7.f;
constexpr float kTurtleHop = 1.3f;
constexpr float kTurtleHit = 10.f;
constexpr float kTurtleDirect = 150.f;
constexpr float kTurtleFarJump = 300.f;
constexpr float kTurtleFlyScale = 0.28f;
constexpr float kTurtleRideScale = 0.112f;
constexpr float kTurtleFlyLift = 1.5f;
constexpr float kTurtleBudget = 50.f;
constexpr double kTurtleChaseMs = 12000.0;
constexpr double kTurtleArmMs = 1350.0;
constexpr double kTurtleBiteMs = 1300.0;
constexpr double kTurtleHoldMs = 3000.0;
constexpr double kTurtleHoldOwnMs = 4000.0;
constexpr double kTurtleEndMs = 1300.0;
constexpr float kTurtleKick = 20.f;
// sub 4C9140 the rocket starts 3 ahead budget 50 frames hits under 6 after 500 ms scale 2 one up
constexpr float kRocketAhead = 3.f;
constexpr float kRocketBudget = 50.f;
constexpr float kRocketHit = 6.f;
constexpr float kRocketRescan = 150.f;
constexpr float kRocketTurnScale = 0.0066666668f * 16.f;
constexpr float kRocketLiftDecay = 0.92f;
constexpr float kRocketSpeedCap = 5.2f;
constexpr double kRocketArmMs = 500.0;
constexpr double kRocketLifeMs = 8000.0;
constexpr double kRocketBoomMs = 9000.0;
constexpr float kRocketScale = 2.f;
constexpr float kRocketKick = 50.f;
// sub 4C8FC0 the search runs 300 ahead and pings the target once a second then the launch holds 1000 ms
constexpr float kLockReach = 300.f;
constexpr double kLockPingMs = 1000.0;
constexpr double kLockResetMs = 1000.0;
constexpr float kLockCone = 30.f;
constexpr float kLockAim = 60.f;
// sub 4C6950 the magnet pulls 4000 ms the shooter by 1 or 3 the target by 0 5 or 1
constexpr double kMagnetMs = 4000.0;
constexpr double kMagnetFreeMs = 9000.0;
constexpr float kMagnetCone = 70.f;
// sub 4C7ED0 the rabbit nifs at scale 1 3 one over the car
constexpr float kRabbitScale = 1.3f;
constexpr double kRabbitRemoteMs = 4000.0;
// sub 4C25E0 the hammer squash settles over 1300 ms
constexpr double kSquashMs = 1300.0;

}

void RaceItems::begin(const std::string& gameDir, RaceWorld& world, RaceSim& sim, const ItemHooks& hooks,
                      bool teamMode, bool offline) {
    end();
    m_world = &world;
    m_sim = &sim;
    m_hooks = hooks;
    m_team = teamMode;
    m_offline = offline;
    const std::string data = findCi(gameDir, "Data");
    const std::string pub = data.empty() ? std::string() : findCi(data, "Public");
    m_itemDir = pub.empty() ? std::string() : findCi(pub, "Item");
    for (int kind = 0; kind < kItemKindCount; ++kind) {
        Pool& p = m_pools[static_cast<size_t>(kind)];
        p.nifs = nifsOf(kind);
        p.objs.assign(static_cast<size_t>(poolSize(kind)), Obj{});
        p.appended.assign(p.objs.size(), {kNoModel, kNoModel, kNoModel, kNoModel, kNoModel});
        Pool& v = m_victims[static_cast<size_t>(kind)];
        v.nifs.clear();
        if (const char* nif = victimNif(kind)) v.nifs.push_back(nif);
        v.objs.assign(v.nifs.empty() ? 0u : 16u, Obj{});
        v.appended.assign(v.objs.size(), {kNoModel, kNoModel, kNoModel, kNoModel, kNoModel});
    }
    m_locks = {};
    m_squash.clear();
    m_now = 0.0;
    m_active = !m_itemDir.empty();
    std::printf("[items] item folder %s %s\n", m_itemDir.empty() ? "missing" : m_itemDir.c_str(),
                offline ? "offline" : "online");
}

void RaceItems::end() {
    m_active = false;
    for (Pool& p : m_pools) { p.objs.clear(); p.appended.clear(); }
    for (Pool& p : m_victims) { p.objs.clear(); p.appended.clear(); }
    m_models.clear();
    m_cars.clear();
}

// FUN 004B0570 the racer count and the rank pick the column a rabbit in hand rolls again without the rabbits
int RaceItems::roll(int racers, int rank, bool heldRabbit, bool teamMode, uint32_t& seed) {
    auto rnd = [&seed]() {
        seed = seed * 1103515245u + 12345u;
        return static_cast<int>((seed >> 16) & 0x7FFF);
    };
    if (teamMode) {
        if (racers % 2 > 0) ++racers;
        racers = std::min(racers, 8);
    }
    racers = std::clamp(racers, 1, 8);
    if (rank < 0) rank = 0;
    if (rank > 29) return -1;
    rank = std::min(rank, racers - 1);
    auto weight = [&](int item) { return kRollWeights[item][(racers - 1) * 8 + rank]; };
    int pick = kItemHammer;
    int sum = 0;
    const int first = rnd() % 1000;
    for (int item = 0; item < kItemKindCount; ++item) {
        sum += weight(item);
        if (sum > first) { pick = item; break; }
    }
    if ((pick == kItemRabbit || pick == kItemBlueRabbit) && heldRabbit) {
        const int again = rnd() % std::max(1, 1000 - weight(kItemRabbit) - weight(kItemBlueRabbit));
        sum = 0;
        for (int item = 0; item < kItemKindCount; ++item) {
            if (item == kItemRabbit || item == kItemBlueRabbit) continue;
            sum += weight(item);
            if (sum > again) return item;
        }
        return kItemHammer;
    }
    return pick;
}

RaceItems::Obj* RaceItems::alloc(int kind) {
    Pool& p = m_pools[static_cast<size_t>(kind)];
    for (size_t i = 0; i < p.objs.size(); ++i) {
        if (p.objs[i].live) continue;
        Obj& o = p.objs[i];
        o = Obj{};
        o.live = true;
        o.kind = kind;
        o.slot = static_cast<int>(i);
        o.stateAt = m_now;
        o.bornAt = m_now;
        return &o;
    }
    return nullptr;
}

void RaceItems::release(Obj& o) {
    o.live = false;
    o.shown = false;
    o.model2 = -1;
}

void RaceItems::setModel(Obj& o, int model) {
    o.model = model;
    o.restart = true;
    o.shown = true;
}

void RaceItems::setModel2(Obj& o, int model) {
    o.model2 = model;
    o.restart2 = true;
}

void RaceItems::enter(Obj& o, int state) {
    o.state = state;
    o.stateAt = m_now;
}

CarPose RaceItems::carPose(int carIndex) const {
    return m_sim && carIndex >= 0 ? m_sim->pose(carIndex) : CarPose{};
}

float RaceItems::carGauge(int carIndex) const {
    return m_sim ? m_sim->driftGaugeSmoothed(carIndex) : 0.f;
}

const ItemCar* RaceItems::car(int carIndex) const {
    for (const ItemCar& c : m_cars)
        if (c.carIndex == carIndex) return &c;
    return nullptr;
}

bool RaceItems::isLocal(int carIndex) const {
    const ItemCar* c = car(carIndex);
    return c != nullptr && c->local;
}

int RaceItems::teamOf(int carIndex) const {
    const ItemCar* c = car(carIndex);
    return c ? c->team : -1;
}

// sub 4B4B80 never reads its car it gives the car of rank 0 the thunder and the hammer strike it
int RaceItems::leader() const {
    for (const ItemCar& c : m_cars)
        if (c.rank == 0) return c.carIndex;
    return m_cars.empty() ? -1 : m_cars.front().carIndex;
}

bool RaceItems::ground(Obj& o, float x, float y, float z, float& out) {
    if (!m_world) return false;
    return world_locate_piece_by_height(o.probe, m_world->scene.collision, x, y, z + 2.f, &out, nullptr);
}

bool RaceItems::shielded(int carIndex) const {
    for (int kind : {kItemShield, kItemAngel}) {
        for (const Obj& o : m_pools[static_cast<size_t>(kind)].objs)
            if (o.live && o.owner == carIndex && o.state == 0) return true;
    }
    return false;
}

bool RaceItems::casting(int kind, int carIndex) const {
    if (kind < 0 || kind >= kItemKindCount) return false;
    for (const Obj& o : m_pools[static_cast<size_t>(kind)].objs)
        if (o.live && o.owner == carIndex) return true;
    return false;
}

bool RaceItems::cursed(int kind, int carIndex) const {
    if (kind < 0 || kind >= kItemKindCount) return false;
    for (const Obj& o : m_pools[static_cast<size_t>(kind)].objs)
        if (o.live && o.victim == carIndex) return true;
    return false;
}

bool RaceItems::absorb(int carIndex) {
    for (int kind : {kItemShield, kItemAngel}) {
        for (Obj& o : m_pools[static_cast<size_t>(kind)].objs) {
            if (!o.live || o.owner != carIndex || o.state != 0) continue;
            // sub 4CACE0 and sub 4B8A30 the bubble goes the pop nif plays 1000 ms then the slot frees
            enter(o, 2);
            setModel(o, 1);
            sound(kind == kItemShield ? "item_shield_damage_snd" : "item_angel_damage_snd");
            return true;
        }
    }
    return false;
}

void RaceItems::effect(int carIndex, int code) {
    if (m_sim && carIndex >= 0) m_sim->applyEffectOnCar(carIndex, code);
}

bool RaceItems::effectBusy(int carIndex) const {
    return m_sim && carIndex >= 0 && m_sim->car(carIndex).effect.activeCode != 0;
}

void RaceItems::sound(const char* name, float volume) {
    if (m_hooks.sound) m_hooks.sound(name, volume);
}

void RaceItems::carMatrix(int carIndex, float out[16]) const {
    const CarPose p = carPose(carIndex);
    if (p.hasBody) {
        for (int i = 0; i < 16; ++i) out[i] = p.body[i];
        return;
    }
    float rotate[16], translate[16];
    bx::mtxRotateZ(rotate, (p.yawDeg - p.driftSlipDeg) * kDeg);
    bx::mtxTranslate(translate, p.x, p.y, p.z);
    bx::mtxMul(out, rotate, translate);
}

// sub 4CC970 Scale then RotZ of minus yaw plus 180 then Translate
void RaceItems::placeThrown(Obj& o) {
    placeAt(o, o.pos, o.yaw + 180.f, o.scale);
}

// D3DX RotationZ of minus a is the bx turn of plus a
void RaceItems::placeAt(Obj& o, const float at[3], float yawDeg, float scale) {
    float s[16], r[16], t[16], sr[16];
    bx::mtxScale(s, scale);
    bx::mtxRotateZ(r, yawDeg * kDeg);
    bx::mtxTranslate(t, at[0], at[1], at[2]);
    bx::mtxMul(sr, s, r);
    bx::mtxMul(o.world, sr, t);
}

// a nif that rides a car its offsets in the car frame then the car world matrix
void RaceItems::placeOnCar(Obj& o, int carIndex, float lift, float scale, float turnDeg) {
    float carWorld[16], s[16], r[16], t[16], sr[16], local[16];
    carMatrix(carIndex, carWorld);
    bx::mtxScale(s, scale);
    bx::mtxRotateZ(r, turnDeg * kDeg);
    bx::mtxTranslate(t, 0.f, 0.f, lift);
    bx::mtxMul(sr, s, r);
    bx::mtxMul(local, sr, t);
    bx::mtxMul(o.world, local, carWorld);
}

void RaceItems::placeOnCar2(Obj& o, int carIndex, float lift, float scale) {
    float carWorld[16], s[16], t[16], local[16];
    carMatrix(carIndex, carWorld);
    bx::mtxScale(s, scale);
    bx::mtxTranslate(t, 0.f, 0.f, lift);
    bx::mtxMul(local, s, t);
    bx::mtxMul(o.world2, local, carWorld);
}

// sub 489E60 from the nearest line point walk on until the reach is spent and no racer stands inside 90
bool RaceItems::lineAhead(int ownerCar, float reach, float out[3]) const {
    if (!m_sim) return false;
    const auto& pts = m_sim->line();
    if (pts.empty()) return false;
    const CarPose p = carPose(ownerCar);
    size_t best = 0;
    float bestD = 1e30f;
    for (size_t i = 0; i < pts.size(); ++i) {
        const float dx = pts[i].x - p.x, dy = pts[i].y - p.y, dz = pts[i].z - p.z;
        const float d = dx * dx + dy * dy + dz * dz;
        if (d < bestD) { bestD = d; best = i; }
    }
    float walked = 0.f;
    float px = p.x, py = p.y, pz = p.z;
    size_t i = best;
    for (size_t step = 0; step < pts.size(); ++step) {
        const auto& q = pts[i];
        walked += std::sqrt((q.x - px) * (q.x - px) + (q.y - py) * (q.y - py) + (q.z - pz) * (q.z - pz));
        out[0] = q.x; out[1] = q.y; out[2] = q.z;
        if (walked > reach) {
            bool taken = false;
            for (const ItemCar& c : m_cars) {
                if (c.carIndex == ownerCar) continue;
                const CarPose cp = carPose(c.carIndex);
                if (math_hypot2d(cp.x - q.x, cp.y - q.y) < kLineClear) taken = true;
            }
            if (!taken) return true;
        }
        px = q.x; py = q.y; pz = q.z;
        i = (i + 1) % pts.size();
    }
    return true;
}

bool RaceItems::lineNext(const float at[3], float out[3]) const {
    if (!m_sim) return false;
    const auto& pts = m_sim->line();
    if (pts.size() < 2) return false;
    size_t best = 0;
    float bestD = 1e30f;
    for (size_t i = 0; i < pts.size(); ++i) {
        const float d = math_hypot2d(pts[i].x - at[0], pts[i].y - at[1]);
        if (d < bestD) { bestD = d; best = i; }
    }
    const auto& next = pts[(best + 1) % pts.size()];
    out[0] = next.x; out[1] = next.y; out[2] = next.z;
    return true;
}

// sub 499CB0 the bearing less the heading and 90 must sit inside 30 either side
int RaceItems::scanCone(int shooter, const float from[3], float headingDeg, float reach) const {
    int best = -1;
    float bestD = reach;
    for (const ItemCar& c : m_cars) {
        if (c.carIndex == shooter || c.finished) continue;
        const CarPose p = carPose(c.carIndex);
        const float dx = from[0] - p.x, dy = from[1] - p.y;
        const float off = wrap180(math_atan2_deg(dx, dy) - headingDeg - 90.f);
        if (std::fabs(off) >= kLockCone) continue;
        const float d = math_hypot2d(dx, dy);
        if (d < bestD) { bestD = d; best = c.carIndex; }
    }
    return best;
}

int RaceItems::liveCount(int kind) const {
    if (kind < 0 || kind >= kItemKindCount) return 0;
    int n = 0;
    for (const Obj& o : m_pools[static_cast<size_t>(kind)].objs) n += o.live ? 1 : 0;
    return n;
}

bool RaceItems::slotPosition(int kind, int slot, float out[3]) const {
    if (kind < 0 || kind >= kItemKindCount) return false;
    const Pool& p = m_pools[static_cast<size_t>(kind)];
    if (slot < 0 || slot >= static_cast<int>(p.objs.size()) || !p.objs[static_cast<size_t>(slot)].live) return false;
    for (int i = 0; i < 3; ++i) out[i] = p.objs[static_cast<size_t>(slot)].pos[i];
    return true;
}

bool RaceItems::slotCars(int kind, int slot, int& owner, int& target) const {
    if (kind < 0 || kind >= kItemKindCount) return false;
    const Pool& p = m_pools[static_cast<size_t>(kind)];
    if (slot < 0 || slot >= static_cast<int>(p.objs.size()) || !p.objs[static_cast<size_t>(slot)].live) return false;
    owner = p.objs[static_cast<size_t>(slot)].owner;
    target = p.objs[static_cast<size_t>(slot)].victim;
    return true;
}

float RaceItems::squash(int carIndex) const {
    auto it = m_squash.find(carIndex);
    if (it == m_squash.end()) return 1.f;
    const double ms = (m_now - it->second) * 1000.0;
    if (ms >= kSquashMs) return 1.f;
    // the car flattens at once then bounces back to its height
    const float t = static_cast<float>(ms / kSquashMs);
    return 1.f - 0.7f * (1.f - t) * std::cos(t * 3.14159265f * 2.5f);
}

// sub 47A110 one case per kind the four floats are the owner position and its yaw
void RaceItems::spawn(int kind, int ownerCar, float x, float y, float z, float yawDeg) {
    if (!m_active || ownerCar < 0) return;
    switch (kind) {
    case kItemSpike: case kItemBomb: case kItemDung: spawnThrown(kind, ownerCar, x, y, z, yawDeg); break;
    case kItemStorm: spawnStorm(ownerCar, x, y, z, yawDeg); break;
    case kItemHive: case kItemIce: case kItemFlash: spawnPlaced(kind, ownerCar, yawDeg); break;
    case kItemThunder: case kItemHammer: spawnStrike(kind, ownerCar); break;
    case kItemShield: case kItemAngel: case kItemDevilRed: spawnGuard(kind, ownerCar); break;
    case kItemHandle: case kItemDevil: spawnCurse(kind, ownerCar); break;
    case kItemSmoke: spawnSmoke(ownerCar, x, y, z, yawDeg); break;
    case kItemRabbit: case kItemBlueRabbit: spawnRabbit(kind, ownerCar, x, y, z, yawDeg); break;
    default: break;
    }
}

// sub 4CC460 sub 4BB3F0 sub 4BF960 the spike the bomb and the three dung pieces fly from the owner
void RaceItems::spawnThrown(int kind, int owner, float x, float y, float z, float yawDeg) {
    struct Throw { float turn; float speed; };
    std::vector<Throw> throws;
    if (kind == kItemSpike) throws = {{90.f, 20.f}};
    else if (kind == kItemBomb) throws = {{-90.f, 20.f}};
    else throws = {{90.f, 20.f}, {35.f, 30.f}, {145.f, 30.f}};
    const float yaw = yawDeg - carGauge(owner) * kGaugeTurn;
    for (const Throw& t : throws) {
        Obj* o = alloc(kind);
        if (!o) return;
        o->owner = owner;
        o->yaw = yaw;
        o->scale = kThrownStartScale;
        o->target[0] = x; o->target[1] = y; o->target[2] = z;
        Vec3 dir;
        math_dir_from_heading_pitch(yaw + t.turn, kThrownPitch, dir);
        o->vel[0] = dir.x * t.speed;
        o->vel[1] = dir.y * t.speed;
        o->vel[2] = dir.z * t.speed;
        o->pos[0] = x; o->pos[1] = y; o->pos[2] = z + (kind == kItemSpike ? 1.6f : 0.f);
        setModel(*o, 0);
        enter(*o, 0);
    }
    sound(kind == kItemSpike ? "item_spike_on_snd" : kind == kItemBomb ? "item_bomb_throw" : "item_dung_throw");
    if (isLocal(owner) && m_hooks.eventView) m_hooks.eventView(3, owner, 0);
}

void RaceItems::updateThrown(Obj& o, float dt) {
    switch (o.state) {
    case 0: {
        // the flight z is z0 plus vz t less 29 4 t squared from the throw point
        const float tx = o.target[0] + o.vel[0] * o.t;
        const float ty = o.target[1] + o.vel[1] * o.t;
        const float tz = o.target[2] + (o.kind == kItemSpike ? 1.6f : 0.f) + o.vel[2] * o.t -
                         o.t * o.t * kThrownGravity;
        o.pos[0] = tx; o.pos[1] = ty; o.pos[2] = tz;
        float g = 0.f;
        if (o.t > 0.05f && ground(o, tx, ty, tz, g) && tz < g) {
            o.pos[2] = g + kGroundLift;
            enter(o, 2);
            if (o.kind != kItemBomb) setModel(o, 1);
            break;
        }
        o.t += dt;
        placeThrown(o);
        if (o.t > kFlightLimit) release(o);
        return;
    }
    case 2:
        if (age(o) > kThrownSettleMs) {
            enter(o, 3);
            if (o.kind == kItemSpike && isLocal(o.owner) && m_hooks.eventView) m_hooks.eventView(4, kItemSpike, o.slot);
        }
        break;
    case 3:
        if (age(o) > kThrownLiveMs) { enter(o, 4); o.sink = 1.f; }
        break;
    case 4:
        o.pos[2] -= o.sink - 1.f;
        o.sink *= perFrame(kSinkGrow, dt);
        if (age(o) > kThrownSinkMs) { release(o); return; }
        break;
    case 5: case 6:
        if (o.kind == kItemBomb && age(o) > kBoomHideMs) o.shown = false;
        if (age(o) > kThrownSinkMs) { release(o); return; }
        if (o.kind == kItemSpike && o.victim >= 0) {
            // sub 4CC970 the boom rides the victim car turned half round
            placeOnCar(o, o.victim, 0.f, 1.f, 180.f);
            return;
        }
        if (o.kind == kItemBomb) {
            placeAt(o, o.pos, o.yaw + 180.f, kThrownStartScale);
            return;
        }
        break;
    default:
        break;
    }
    if (o.state > 1 && o.state < 5) o.scale = std::min(kThrownScaleCap, o.scale * perFrame(kThrownGrow, dt));
    placeThrown(o);
}

// sub 4CC750 sub 4BB6E0 sub 4BFD00 a car within 10 then a sweep of three points along its heading
void RaceItems::hitThrown(Obj& o) {
    if (o.state != 2 && o.state != 3) return;
    const float reach = o.kind == kItemSpike ? kSpikeReach : kBombReach;
    for (const ItemCar& c : m_cars) {
        if (c.finished) continue;
        const CarPose p = carPose(c.carIndex);
        if (math_hypot2d(p.x - o.pos[0], p.y - o.pos[1]) > kThrownCoarse) continue;
        const float heading = p.yawDeg - carGauge(c.carIndex) * kGaugeTurn - 90.f;
        float sx = p.x, sy = p.y;
        bool hit = false;
        for (int step = 0; step < 3 && !hit; ++step) {
            if (step > 0) along(sx, sy, 0.5f, heading);
            const float dx = sx - o.pos[0], dy = sy - o.pos[1], dz = p.z - o.pos[2];
            hit = std::sqrt(dx * dx + dy * dy + dz * dz) < reach;
        }
        if (!hit) continue;
        o.victim = c.carIndex;
        if (absorb(c.carIndex)) {
            enter(o, 6);
            o.shown = false;
            return;
        }
        if (o.kind == kItemSpike) {
            effect(c.carIndex, 100);
            sound("item_spike_damage_snd");
            enter(o, 5);
            setModel(o, 2);
        } else if (o.kind == kItemBomb) {
            effect(c.carIndex, 300);
            sound("item_bomb_attack");
            enter(o, 5);
            setModel(o, 1);
            // the boom frame crashes every car inside 15 with no shield test
            for (const ItemCar& other : m_cars) {
                const CarPose q = carPose(other.carIndex);
                if (other.carIndex != c.carIndex && math_hypot2d(q.x - o.pos[0], q.y - o.pos[1]) < kBombSplash)
                    effect(other.carIndex, 300);
            }
        } else {
            sound("item_dung_attack");
            enter(o, 5);
            o.shown = false;
            if (c.local && m_hooks.dungSplat) m_hooks.dungSplat();
        }
        if (c.local && m_hooks.attackView) m_hooks.attackView(c.carIndex);
        return;
    }
}

// sub 4CD800 the storm sprays up to 30 off the throw lands then drifts and pushes the cars
void RaceItems::spawnStorm(int owner, float x, float y, float z, float yawDeg) {
    Obj* o = alloc(kItemStorm);
    if (!o) return;
    const float yaw = yawDeg - carGauge(owner) * kGaugeTurn;
    o->owner = owner;
    o->pos[0] = x; o->pos[1] = y; o->pos[2] = z + 1.f;
    o->target[0] = x; o->target[1] = y; o->target[2] = z;
    o->yaw = yaw + 180.f;
    o->heading = o->yaw;
    o->scale = 0.5f;
    m_rng = m_rng * 1103515245u + 12345u;
    const float spray = static_cast<float>(static_cast<int>((m_rng >> 16) % 61u) - 30);
    Vec3 dir;
    math_dir_from_heading_pitch(yaw - 90.f + spray, kStormPitch, dir);
    o->vel[0] = dir.x * kStormSpeed;
    o->vel[1] = dir.y * kStormSpeed;
    o->vel[2] = dir.z * kStormSpeed;
    setModel(*o, 0);
    enter(*o, 0);
    sound("item_ministorm_move_snd");
    if (isLocal(owner) && m_hooks.eventView) m_hooks.eventView(3, owner, 0);
}

void RaceItems::updateStorm(Obj& o, float dt) {
    if (o.state == 0) {
        o.t += dt;
        const float tx = o.target[0] + o.vel[0] * o.t;
        const float ty = o.target[1] + o.vel[1] * o.t;
        const float tz = o.target[2] + 1.f + o.vel[2] * o.t - o.t * o.t * kStormGravity;
        float g = 0.f;
        o.pos[0] = tx; o.pos[1] = ty; o.pos[2] = tz;
        if (o.t > 0.05f && ground(o, tx, ty, tz, g) && tz < g) {
            o.pos[2] = g + kGroundLift;
            enter(o, 2);
            if (isLocal(o.owner) && m_hooks.eventView) m_hooks.eventView(4, kItemStorm, o.slot);
        } else if (o.t > kFlightLimit) {
            release(o);
            return;
        }
    } else {
        // sub 4CE280 the storm eases a 512th toward a point 1 2 ahead on the racing line
        float next[3];
        if (lineNext(o.pos, next)) {
            const float heading = yawToward(o.pos[0], o.pos[1], next[0], next[1]) - 90.f;
            float ax = o.pos[0], ay = o.pos[1];
            along(ax, ay, 1.2f, heading);
            const float k = 1.f - std::pow(1.f - 1.f / 512.f, dt / kStockFrame);
            o.pos[0] += (ax - o.pos[0]) * k;
            o.pos[1] += (ay - o.pos[1]) * k;
            o.heading += wrap180(heading + 90.f - o.heading) * (1.f - std::pow(0.875f, dt / kStockFrame));
        }
        float g = 0.f;
        if (ground(o, o.pos[0], o.pos[1], o.pos[2], g)) o.pos[2] += (g + kGroundLift - o.pos[2]) * (1.f / 16.f);
        o.scale = std::min(kThrownScaleCap, o.scale * perFrame(kThrownGrow, dt));
        if (o.state == 2 && age(o) > kStormLiveMs) enter(o, 3);
        if (o.state == 3) {
            o.scale *= perFrame(1.032f, dt);
            if (age(o) > kStormDieMs) { release(o); return; }
        }
    }
    placeAt(o, o.pos, o.heading, o.scale * 0.11f);
}

// sub 4C3810 sub 4C4790 sub 4C1970 the throw plays on the owner then the item stands ahead
void RaceItems::spawnPlaced(int kind, int owner, float yawDeg) {
    Obj* o = alloc(kind);
    if (!o) return;
    o->owner = owner;
    o->yaw = yawDeg;
    const float reach = kind == kItemHive ? kHiveReach : kind == kItemIce ? kIceReach : kFlashReach;
    if (!lineAhead(owner, reach, o->target)) {
        const CarPose p = carPose(owner);
        o->target[0] = p.x; o->target[1] = p.y; o->target[2] = p.z;
        along(o->target[0], o->target[1], reach * 0.25f, p.yawDeg - 90.f);
    }
    if (kind == kItemIce) {
        m_rng = m_rng * 1103515245u + 12345u;
        o->target[0] += static_cast<float>(static_cast<int>((m_rng >> 16) % 21u) - 10);
        m_rng = m_rng * 1103515245u + 12345u;
        o->target[1] += static_cast<float>(static_cast<int>((m_rng >> 16) % 21u) - 10);
    }
    float g = 0.f;
    if (ground(*o, o->target[0], o->target[1], o->target[2], g)) o->target[2] = g;
    const CarPose p = carPose(owner);
    o->pos[0] = p.x; o->pos[1] = p.y; o->pos[2] = p.z;
    setModel(*o, 0);
    enter(*o, 0);
    sound(kind == kItemHive ? "item_hive_use_snd" : kind == kItemIce ? "item_ice_throw_snd" : "item_flash_throw_snd");
}

void RaceItems::updatePlaced(Obj& o) {
    const double throwMs = o.kind == kItemHive ? kHiveThrowMs : o.kind == kItemIce ? kIceThrowMs : kFlashThrowMs;
    switch (o.state) {
    case 0: {
        const CarPose p = carPose(o.owner);
        o.pos[0] = p.x; o.pos[1] = p.y; o.pos[2] = p.z;
        if (age(o) > throwMs) {
            if (o.kind == kItemFlash) {
                enter(o, 1);
                o.shown = false;
            } else {
                for (int i = 0; i < 3; ++i) o.pos[i] = o.target[i];
                enter(o, 100);
                if (isLocal(o.owner) && m_hooks.eventView) m_hooks.eventView(4, o.kind, o.slot);
            }
        }
        break;
    }
    case 1:
        if (age(o) > kFlashWaitMs) {
            for (int i = 0; i < 3; ++i) o.pos[i] = o.target[i];
            enter(o, 100);
            if (isLocal(o.owner) && m_hooks.eventView) m_hooks.eventView(4, kItemFlash, o.slot);
        }
        break;
    case 100:
        enter(o, o.kind == kItemFlash ? 200 : 101);
        setModel(o, 1);
        if (o.kind == kItemHive) sound("item_hive_on_snd");
        if (o.kind == kItemIce) sound("item_ice_on_snd");
        break;
    case 101:
        if (age(o) > (o.kind == kItemHive ? kHiveLiveMs : kIceLiveMs)) enter(o, 300);
        break;
    case 200:
        if (age(o) > kFlashLiveMs) enter(o, 300);
        break;
    case 300:
        if (age(o) > (o.kind == kItemHive ? kHiveFadeMs : o.kind == kItemIce ? kIceLiveMs : kFlashLiveMs)) {
            release(o);
            return;
        }
        break;
    default:
        break;
    }
    const float scale = o.kind == kItemFlash ? 1.8f : o.kind == kItemIce && o.state >= 300 ? 1.6f : 1.f;
    const float at[3] = {o.pos[0], o.pos[1], o.pos[2] + (o.kind == kItemFlash ? 1.f : 0.f)};
    if (o.state == 0) {
        placeOnCar(o, o.owner, 0.f, 1.f, 0.f);
    } else {
        placeAt(o, at, o.yaw, scale);
    }
    // sub 4B8020 hands the storm the hive the ice and the flash to the own car alone online
    const bool live = (o.kind == kItemFlash && o.state == 200) || (o.kind != kItemFlash && o.state == 101);
    if (!live) return;
    for (const ItemCar& c : m_cars) {
        if (!tested(c) || c.carIndex == o.owner || c.finished) continue;
        const CarPose p = carPose(c.carIndex);
        const float dx = p.x - o.pos[0], dy = p.y - o.pos[1], dz = p.z - o.pos[2];
        const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float reach = o.kind == kItemHive ? kHiveHit : o.kind == kItemIce ? kIceHit : kFlashHit;
        if (d >= reach || effectBusy(c.carIndex)) continue;
        if (absorb(c.carIndex)) continue;
        if (o.kind == kItemFlash) {
            sound("item_flash_damage_snd");
            if (c.local) {
                effect(c.carIndex, 1100);
                if (m_hooks.flashBlind) m_hooks.flashBlind();
                if (m_hooks.reportHit) m_hooks.reportHit(1100);
            }
        } else {
            victimSlot(o.kind, c.carIndex);
            if (o.kind == kItemIce && m_sim) {
                // sub 4C4B70 the bearing from the car to the block plus 270 pushes the car 80 away from it
                m_sim->pushCar(c.carIndex, math_atan2_deg(o.pos[0] - p.x, o.pos[1] - p.y) + 270.f, kIcePush);
            }
            if (c.local && m_hooks.reportHit) m_hooks.reportHit(o.kind == kItemHive ? 700 : 1000);
        }
        if (isLocal(o.owner) && m_hooks.attackView) m_hooks.attackView(c.carIndex);
    }
}

// sub 4C3A60 sub 4C4A10 sub 4CDA80 the sting freeze or whirl on the car its slot holds the effect
void RaceItems::victimSlot(int kind, int carIndex) {
    Pool& v = m_victims[static_cast<size_t>(kind)];
    Obj* slot = nullptr;
    for (Obj& o : v.objs) if (o.live && o.victim == carIndex) slot = &o;
    for (size_t i = 0; i < v.objs.size() && !slot; ++i) {
        if (v.objs[i].live) continue;
        v.objs[i] = Obj{};
        v.objs[i].live = true;
        v.objs[i].slot = static_cast<int>(i);
        slot = &v.objs[i];
    }
    if (!slot) return;
    slot->kind = kind;
    slot->victim = carIndex;
    slot->stateAt = m_now;
    slot->bornAt = m_now;
    slot->state = 0;
    setModel(*slot, 0);
    const int code = kind == kItemHive ? 700 : kind == kItemIce ? 1000 : 100;
    if (m_sim && kind != kItemStorm) {
        auto& table = kind == kItemHive ? m_sim->game().itembiteTable : m_sim->game().itemdrumTable;
        GimmickBumpSlot& b = table[static_cast<size_t>(slot->slot) % GIMMICK_BUMP_TABLE_SIZE];
        b.active = 1;
        b.car_index = carIndex;
        b.payload = 0;
    }
    effect(carIndex, code);
    sound(kind == kItemHive ? "item_hive_damage_snd" : kind == kItemIce ? "item_ice_damage_snd"
                                                                        : "item_ministorm_damage_snd");
}

void RaceItems::updateVictim(Obj& o) {
    const double lifeMs = o.kind == kItemHive ? kHiveStingMs : o.kind == kItemIce ? kIceFreezeMs : kStormVictimMs;
    if (age(o) > lifeMs) {
        if (m_sim && o.kind != kItemStorm) {
            auto& table = o.kind == kItemHive ? m_sim->game().itembiteTable : m_sim->game().itemdrumTable;
            GimmickBumpSlot& b = table[static_cast<size_t>(o.slot) % GIMMICK_BUMP_TABLE_SIZE];
            b.active = 0;
            b.car_index = -1;
        }
        release(o);
        return;
    }
    placeOnCar(o, o.victim, 0.f, o.kind == kItemHive ? 1.3f : 1.f, 0.f);
}

void RaceItems::remoteHit(int carIndex, int code) {
    if (!m_active || carIndex < 0) return;
    if (code == 700) victimSlot(kItemHive, carIndex);
    else if (code == 1000) victimSlot(kItemIce, carIndex);
}

// sub 4C2550 the thunder and the hammer rise on the caster and strike the leader of the standings
void RaceItems::spawnStrike(int kind, int owner) {
    Obj* o = alloc(kind);
    if (!o) return;
    o->owner = owner;
    o->victim = leader();
    setModel(*o, 0);
    enter(*o, 1);
    sound(kind == kItemThunder ? "item_thunder_use_snd" : "item_hammer_throw");
    if (isLocal(owner) && m_hooks.eventView) m_hooks.eventView(4, kind, o->slot);
}

void RaceItems::updateStrike(Obj& o) {
    const bool hammer = o.kind == kItemHammer;
    // sub 4CF0D0 and sub 4C25E0 the state timers of the thunder and of the hammer
    const double t1 = 500.0, t2 = hammer ? 2666.0 : 2668.0, t101 = hammer ? 100.0 : 500.0,
                 t102 = hammer ? 250.0 : 1000.0, t103 = hammer ? 500.0 : 2000.0, t104 = hammer ? 800.0 : 2200.0;
    switch (o.state) {
    case 1: if (age(o) > t1) enter(o, 2); break;
    case 2:
        if (age(o) > t2) {
            if (o.victim < 0) { release(o); return; }
            enter(o, 101);
            setModel(o, 1);
        }
        break;
    case 101: if (age(o) > t101) enter(o, 102); break;
    case 102:
        if (age(o) > t102) {
            if (o.victim >= 0 && !isLocal(o.victim) && m_hooks.attackView) m_hooks.attackView(o.victim);
            enter(o, 103);
        }
        break;
    case 103:
        if (age(o) > t103) {
            sound(hammer ? "item_hammer_attack" : "item_thunder_damage_snd");
            enter(o, 104);
        }
        break;
    case 104:
        if (age(o) > t104) {
            if (absorb(o.victim)) { release(o); return; }
            if (hammer) {
                // sub 4C25E0 writes car 0x36DC 1 car 0x36E0 0 car 0x36E4 1 the squash of the car
                m_squash[o.victim] = m_now;
            } else if (m_sim) {
                // the thunder slot holds the effect 400 while its state reads 0x69 sub 4CF020
                GimmickHiveSlot& h = m_sim->game().hiveTable[static_cast<size_t>(o.slot) % GIMMICK_HIVE_TABLE_SIZE];
                h.active = 1;
                h.car_index = o.victim;
                h.state = GIMMICK_HIVE_STATE_HELD;
                effect(o.victim, 400);
            }
            enter(o, 105);
        }
        break;
    case 105:
        if (age(o) > 3000.0) {
            if (m_sim && !hammer) m_sim->game().hiveTable[static_cast<size_t>(o.slot) % GIMMICK_HIVE_TABLE_SIZE].active = 0;
            enter(o, 200);
        }
        break;
    case 200:
        if (age(o) > 3000.0) { release(o); return; }
        break;
    default: break;
    }
    const int at = o.state < 100 ? o.owner : o.victim;
    if (at < 0) { o.shown = false; return; }
    placeOnCar(o, at, 0.f, 1.f, 0.f);
}

// sub 4CB160 the shield sub 4B8D20 the angel sub 4BE470 the devil red ride their car
void RaceItems::spawnGuard(int kind, int carIndex) {
    std::vector<int> cars{carIndex};
    // sub 4B91C0 a team race gives the angel and the devil red to every car of the team
    if (m_team && kind != kItemShield) {
        cars.clear();
        for (const ItemCar& c : m_cars) if (c.team == teamOf(carIndex)) cars.push_back(c.carIndex);
    }
    for (int target : cars) {
        bool taken = false;
        for (Obj& o : m_pools[static_cast<size_t>(kind)].objs) if (o.live && o.owner == target && o.state == 0) taken = true;
        if (taken) continue;
        Obj* o = alloc(kind);
        if (!o) return;
        o->owner = target;
        setModel(*o, 0);
        enter(*o, 0);
    }
    sound(kind == kItemShield ? "item_shield_use_snd" : "item_angel_on_snd");
    if (isLocal(carIndex) && kind != kItemShield && m_hooks.eventView) m_hooks.eventView(3, carIndex, 0);
}

void RaceItems::updateGuard(Obj& o) {
    if (o.kind == kItemDevilRed) {
        // sub 4BE110 the devil red waits 250 ms shows its throw on a devil then clears it at 683 ms
        int devil = -1;
        for (Obj& d : m_pools[kItemDevil].objs) if (d.live && d.victim == o.owner) devil = d.slot;
        if (o.state == 0 && age(o) > 250.0 && devil >= 0) { enter(o, 1); setModel(o, 1); }
        else if (o.state == 1 && age(o) > 500.0) enter(o, 2);
        else if (o.state == 2 && age(o) > 683.0) {
            if (devil >= 0) release(m_pools[kItemDevil].objs[static_cast<size_t>(devil)]);
            enter(o, 3);
        } else if (o.state == 3 && age(o) > 1083.0) { release(o); return; }
        if (o.state == 0 && age(o) > kCurseMs) { release(o); return; }
        placeOnCar(o, o.owner, 0.f, 1.f, 0.f);
        return;
    }
    const double lifeMs = o.kind == kItemShield ? kShieldMs : kAngelMs;
    if (o.state == 0 && age(o) > lifeMs) { release(o); return; }
    if (o.state == 2 && age(o) > kGuardPopMs) { release(o); return; }
    placeOnCar(o, o.owner, o.kind == kItemShield ? kShieldLift : 0.f, 1.f, 0.f);
}

// sub 4C2FA0 the handle and sub 4BD5F0 the devil land on every other car a shield eats it
void RaceItems::spawnCurse(int kind, int owner) {
    for (const ItemCar& c : m_cars) {
        if (c.carIndex == owner || c.finished) continue;
        if (m_team && c.team == teamOf(owner)) continue;
        if (absorb(c.carIndex)) continue;
        Obj* o = alloc(kind);
        if (!o) return;
        o->owner = owner;
        o->victim = c.carIndex;
        setModel(*o, kind == kItemDevil ? 1 : 0);
        // sub 4BD5F0 the devil att nif stands on the caster the dmg one on the victim
        if (kind == kItemDevil) setModel2(*o, 0);
        enter(*o, 0);
    }
    sound("item_handle_damage_snd");
}

void RaceItems::updateCurse(Obj& o) {
    if (age(o) > kCurseMs) { release(o); return; }
    if (o.model2 >= 0 && age(o) > 2667.0) o.model2 = -1;
    placeOnCar(o, o.victim, o.kind == kItemHandle ? kHandleDrop : 0.f, 1.f, 0.f);
    if (o.model2 >= 0) placeOnCar2(o, o.owner, 0.f, 1.f);
}

// sub 4CB6E0 the smoke stands 100 ms then its cloud rides the owner and fades at 6000 ms
void RaceItems::spawnSmoke(int owner, float x, float y, float z, float yawDeg) {
    Obj* o = alloc(kItemSmoke);
    if (!o) return;
    o->owner = owner;
    o->pos[0] = x; o->pos[1] = y; o->pos[2] = z + 3.f;
    o->yaw = yawDeg + 90.f;
    setModel(*o, 0);
    enter(*o, 0);
    sound("item_smoke_use_snd");
}

void RaceItems::updateSmoke(Obj& o, float dt) {
    const double ms = age(o);
    if (o.state == 0 && ms > 100.0) { enter(o, 1); setModel(o, 2); o.scale = 1.f; }
    if (o.state == 1) {
        if (ms <= 1000.0) {
            const CarPose p = carPose(o.owner);
            o.pos[0] = p.x; o.pos[1] = p.y; o.pos[2] = p.z;
            o.yaw = p.yawDeg + 180.f;
        }
        if (ms > 6000.0) {
            o.scale *= perFrame(1.032f, dt);
            o.pos[2] -= o.scale - 1.f;
        }
        if (ms > 9000.0) { release(o); return; }
    }
    placeAt(o, o.pos, o.yaw, o.scale);
}

// sub 4D1A50 walks the standings from the rank above the own one up skipping a racer another turtle chases
int RaceItems::useTurtle(int owner) {
    if (!m_active) return -1;
    const ItemCar* me = car(owner);
    int target = -1;
    if (me && me->rank > 0) {
        for (int rank = me->rank - 1; rank >= 0; --rank) {
            const ItemCar* ahead = nullptr;
            for (const ItemCar& c : m_cars) if (c.rank == rank) ahead = &c;
            if (!ahead) continue;
            bool chased = false;
            for (const Obj& o : m_pools[kItemTurtle].objs) if (o.live && o.victim == ahead->carIndex) chased = true;
            if (chased) continue;
            if (m_team && ahead->team == me->team) continue;
            target = ahead->carIndex;
            break;
        }
    }
    spawnTurtle(owner, target);
    return target;
}

void RaceItems::remoteTurtle(int owner, int target) {
    if (!m_active || owner < 0) return;
    spawnTurtle(owner, target);
}

void RaceItems::spawnTurtle(int owner, int target) {
    Obj* o = alloc(kItemTurtle);
    if (!o) return;
    const CarPose p = carPose(owner);
    o->owner = owner;
    o->victim = target;
    o->yaw = p.yawDeg - carGauge(owner) * kGaugeTurn;
    o->target[0] = p.x; o->target[1] = p.y; o->target[2] = p.z;
    o->pos[0] = 0.f; o->pos[1] = 0.f; o->pos[2] = kTurtleHop;
    o->scale = kTurtleStartScale;
    o->budget = kTurtleBudget;
    Vec3 dir;
    math_dir_from_heading_pitch(o->yaw - 90.f, kTurtlePitch, dir);
    o->vel[0] = dir.x * kTurtleSpeed;
    o->vel[1] = dir.y * kTurtleSpeed;
    o->vel[2] = dir.z * kTurtleSpeed;
    setModel(*o, 0);
    enter(*o, 0);
    sound("item_turtle_walk_snd");
}

// sub 4D00C0 0 the toss 2 the chase 100 to 110 the bite 200 the ride 300 the hop off
void RaceItems::updateTurtle(Obj& o, float dt) {
    const CarPose owner = carPose(o.owner);
    switch (o.state) {
    case 0: {
        const float tx = o.vel[0] * o.t + o.pos[0] + owner.x;
        const float ty = o.vel[1] * o.t + o.pos[1] + owner.y;
        const float tz = o.vel[2] * o.t + o.pos[2] - o.t * o.t * kTurtleGravity + owner.z;
        float g = 0.f;
        if (!ground(o, tx, ty, tz, g)) {
            // 0x4D0294 no ground under the toss the chase starts from the throw point
            o.pos[0] = o.target[0]; o.pos[1] = o.target[1]; o.pos[2] = o.target[2];
            enter(o, 2);
            o.bornAt = m_now;
            o.t = 0.f;
            break;
        }
        if (tz < g && o.t > 0.05f) {
            o.pos[0] = tx; o.pos[1] = ty; o.pos[2] = g + kGroundLift;
            enter(o, 2);
            o.bornAt = m_now;
            o.t = 0.f;
            // 0x4D0470 a target far past 300 puts the turtle 299 short of it with a puff where it left
            if (o.victim >= 0) {
                const CarPose t = carPose(o.victim);
                if (math_hypot2d(t.x - tx, t.y - ty) > kTurtleFarJump) {
                    const float away = yawToward(t.x, t.y, tx, ty) - 90.f;
                    o.pos[0] = t.x; o.pos[1] = t.y; o.pos[2] = t.z;
                    along(o.pos[0], o.pos[1], kTurtleFarJump - 1.f, away);
                }
            } else {
                o.pos[0] = owner.x; o.pos[1] = owner.y; o.pos[2] = owner.z;
            }
            break;
        }
        o.t += dt;
        const float at[3] = {tx, ty, tz + kTurtleFlyLift};
        placeAt(o, at, o.yaw, o.scale * kTurtleFlyScale);
        if (o.t > kFlightLimit) release(o);
        return;
    }
    case 2: {
        // the chase heads for the target inside 150 else it follows the line turning a quarter a frame
        float dist = 0.f;
        bool direct = false;
        if (o.victim >= 0) {
            const CarPose t = carPose(o.victim);
            const float dx = t.x - o.pos[0], dy = t.y - o.pos[1], dz = t.z - o.pos[2];
            dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < kTurtleDirect) {
                o.yaw = yawToward(o.pos[0], o.pos[1], t.x, t.y);
                dist = math_hypot2d(dx, dy);
                direct = true;
            }
        } else {
            dist = kTurtleFarJump;
        }
        if (!direct) {
            float next[3];
            if (lineNext(o.pos, next)) {
                const float want = yawToward(o.pos[0], o.pos[1], next[0], next[1]);
                o.yaw += wrap180(want - o.yaw) * (1.f - std::pow(0.75f, dt / kStockFrame));
            }
        }
        // 0x4D0600 the step is the distance over the frames left so the turtle meets its target in 50 frames
        const float frames = dt / kStockFrame;
        float step = dist;
        if (o.budget > 0.f) step = dist * std::min(1.f, frames / o.budget);
        o.budget -= frames;
        along(o.pos[0], o.pos[1], step, o.yaw - 90.f);
        float g = 0.f;
        if (ground(o, o.pos[0], o.pos[1], o.pos[2], g)) o.pos[2] = g;
        const ItemCar* tc = o.victim >= 0 ? car(o.victim) : nullptr;
        if (life(o) > kTurtleChaseMs || (tc && tc->finished) || (o.victim < 0 && life(o) > 3000.0)) {
            o.victim = -1;
            enter(o, 300);
        }
        break;
    }
    case 100:
        enter(o, 101);
        setModel(o, 2);
        break;
    case 101:
        if (age(o) > kTurtleArmMs) {
            bool ridden = false;
            for (const Obj& other : m_pools[kItemTurtle].objs)
                if (&other != &o && other.live && other.victim == o.victim && other.state >= 102 && other.state < 300)
                    ridden = true;
            if (ridden) { release(o); return; }
            enter(o, 102);
        }
        break;
    case 102:
        if (o.victim < 0 || absorb(o.victim)) { release(o); return; }
        effect(o.victim, 500);
        sound("item_turtle_damage_snd");
        enter(o, 110);
        o.bornAt = m_now;
        setModel(o, 1);
        setModel2(o, 3);
        break;
    case 110:
        if (age(o) > kTurtleBiteMs) {
            enter(o, 200);
            sound("item_turtle_caution_snd");
            o.shown = false;
        }
        break;
    case 200: {
        const double hold = isLocal(o.owner) ? kTurtleHoldOwnMs : kTurtleHoldMs;
        const ItemCar* tc = o.victim >= 0 ? car(o.victim) : nullptr;
        if (life(o) > hold - 500.0 || (tc && tc->finished)) enter(o, 300);
        break;
    }
    case 300: {
        enter(o, 301);
        if (o.victim >= 0) {
            // 0x4D0A3A the engine force of 20 straight up hops the car off
            if (m_sim) m_sim->kickCar(o.victim, kTurtleKick);
            const CarPose t = carPose(o.victim);
            o.pos[0] = t.x; o.pos[1] = t.y; o.pos[2] = t.z;
        }
        sound("item_turtle_end_snd");
        o.model2 = -1;
        o.victim = -1;
        setModel(o, 1);
        break;
    }
    case 301:
        if (age(o) > kTurtleEndMs) { release(o); return; }
        break;
    default:
        break;
    }
    if (m_sim) {
        GimmickHazardSlot& h = m_sim->game().hazardTable[static_cast<size_t>(o.slot) % GIMMICK_HAZARD_TABLE_SIZE];
        h.active = o.victim >= 0 ? 1 : 0;
        h.car_index = o.victim;
        h.countdown = o.state;
    }
    if (o.state > 1) o.scale = std::min(kThrownScaleCap, o.scale * perFrame(kThrownGrow, dt));
    if (o.state < 100 || o.victim < 0) {
        const float at[3] = {o.pos[0], o.pos[1], o.pos[2] + kTurtleFlyLift};
        placeAt(o, at, o.yaw, o.scale * kTurtleFlyScale);
    } else {
        placeOnCar(o, o.victim, 0.f, o.scale * kTurtleRideScale, 0.f);
        if (o.model2 >= 0) placeOnCar2(o, o.victim, 0.f, o.scale * kTurtleRideScale);
    }
}

// sub 4CFF70 the chasing turtle grabs its own target inside 10 a car with an effect sends it away
void RaceItems::hitTurtle(Obj& o) {
    if (o.state != 2 || o.victim < 0) return;
    const CarPose p = carPose(o.victim);
    const float dx = p.x - o.pos[0], dy = p.y - o.pos[1], dz = p.z - o.pos[2];
    if (std::sqrt(dx * dx + dy * dy + dz * dz) >= kTurtleHit) return;
    if (effectBusy(o.victim)) {
        o.victim = -1;
        enter(o, 300);
        return;
    }
    enter(o, 100);
    if (isLocal(o.owner) && m_hooks.attackView) m_hooks.attackView(o.victim);
}

void RaceItems::launchHoming(int kind, int owner, int target) {
    if (!m_active || owner < 0) return;
    if (kind == kItemRocket) spawnRocket(owner, target);
    else if (kind == kItemMagnet) spawnMagnet(owner, target);
}

// sub 4C9140 the rocket starts 3 ahead of the shooter on its yaw less the gauge turn
void RaceItems::spawnRocket(int owner, int target) {
    Obj* o = alloc(kItemRocket);
    if (!o) return;
    const CarPose p = carPose(owner);
    const float yaw = p.yawDeg - carGauge(owner) * kGaugeTurn;
    o->owner = owner;
    o->victim = target;
    o->yaw = yaw;
    o->heading = yaw;
    o->pos[0] = p.x; o->pos[1] = p.y; o->pos[2] = p.z;
    along(o->pos[0], o->pos[1], kRocketAhead, yaw - 90.f);
    o->speed = std::max(0.5f, p.speed * 0.021739131f * 60.f);
    o->budget = kRocketBudget;
    o->lift = 1.f;
    setModel(*o, 0);
    enter(*o, 0);
    sound("item_rocket_shot_snd");
}

// sub 4CA2A0 state 0 the flight 100 the hit 101 the boom 102 the wait before the slot frees
void RaceItems::updateRocket(Obj& o, float dt) {
    const float frames = dt / kStockFrame;
    switch (o.state) {
    case 0: {
        int target = o.victim;
        if (target < 0) target = scanCone(o.owner, o.pos, o.heading, kRocketRescan);
        float divisor = 16.f;
        float step = std::min(kRocketSpeedCap, o.speed) * frames;
        o.speed = std::min(kRocketSpeedCap, o.speed + 0.1f * frames);
        if (target >= 0) {
            const CarPose t = carPose(target);
            o.heading = yawToward(o.pos[0], o.pos[1], t.x, t.y);
            const float dist = math_hypot2d(t.x - o.pos[0], t.y - o.pos[1]);
            divisor = std::max(1.f, dist * kRocketTurnScale);
            o.lift *= perFrame(kRocketLiftDecay, dt);
            step = o.budget > 0.f ? dist * std::min(1.f, frames / o.budget) : dist;
        }
        o.yaw += wrap180(o.heading - o.yaw) / divisor * std::min(frames, divisor);
        o.budget -= frames;
        along(o.pos[0], o.pos[1], step, o.yaw - 90.f);
        float g = 0.f;
        if (ground(o, o.pos[0], o.pos[1], o.pos[2], g)) o.pos[2] = g + o.lift;
        const ItemCar* tc = o.victim >= 0 ? car(o.victim) : nullptr;
        if (life(o) > kRocketLifeMs || (tc && tc->finished)) enter(o, 101);
        break;
    }
    case 100:
        if (o.victim >= 0) {
            if (absorb(o.victim)) {
                enter(o, 103);
                o.shown = false;
                break;
            }
            effect(o.victim, 300);
            const CarPose t = carPose(o.victim);
            o.pos[0] = t.x; o.pos[1] = t.y; o.pos[2] = t.z;
            // 0x4CA3F5 the engine force of 50 straight up throws the car
            if (m_sim) m_sim->kickCar(o.victim, kRocketKick);
        }
        enter(o, 101);
        break;
    case 101:
        enter(o, 102);
        setModel(o, 1);
        sound("item_rocket_damage_snd");
        break;
    case 102:
        if (age(o) > kRocketBoomMs) { release(o); return; }
        break;
    case 103:
        release(o);
        return;
    default:
        break;
    }
    if (o.state < 100) {
        float s[16], r[16], t[16], sr[16];
        bx::mtxScale(s, kRocketScale);
        bx::mtxRotateZ(r, o.yaw * kDeg);
        bx::mtxTranslate(t, o.pos[0], o.pos[1], o.pos[2] + 1.f);
        bx::mtxMul(sr, s, r);
        bx::mtxMul(o.world, sr, t);
    } else {
        bx::mtxTranslate(o.world, o.pos[0], o.pos[1], o.pos[2]);
    }
}

// sub 4C93B0 the rocket in flight 500 ms or more takes its own target inside 6
void RaceItems::hitRocket(Obj& o) {
    if (o.state != 0 || o.victim < 0 || life(o) < kRocketArmMs) return;
    const CarPose p = carPose(o.victim);
    const float dx = p.x - o.pos[0], dy = p.y - o.pos[1], dz = p.z - o.pos[2];
    if (std::sqrt(dx * dx + dy * dy + dz * dz) >= kRocketHit) return;
    if (effectBusy(o.victim)) {
        o.victim = -1;
        enter(o, 101);
        return;
    }
    enter(o, 100);
    if (isLocal(o.owner) && m_hooks.attackView) m_hooks.attackView(o.victim);
}

// sub 4C5BD0 the magnet ties the shooter to the target the two nifs sit on the two cars
void RaceItems::spawnMagnet(int owner, int target) {
    if (target < 0) return;
    Obj* o = alloc(kItemMagnet);
    if (!o) return;
    o->owner = owner;
    o->victim = target;
    setModel(*o, 0);
    setModel2(*o, 1);
    enter(*o, 0);
    sound("item_magnet_shot_snd");
}

void RaceItems::updateMagnet(Obj& o) {
    if (o.state == 0) {
        const CarPose a = carPose(o.owner);
        const CarPose b = carPose(o.victim);
        // sub 44E240 the bearing from the target to the shooter
        const float bearing = math_atan2_deg(a.x - b.x, a.y - b.y);
        auto facing = [](const CarPose& from, const CarPose& to) {
            const float off = wrap180(math_atan2_deg(from.x - to.x, from.y - to.y) - from.yawDeg - 90.f);
            return std::fabs(off) < kMagnetCone && math_hypot2d(from.x - to.x, from.y - to.y) < kLockReach;
        };
        if (m_sim) {
            // sub 496B40 pulls the shooter at the bearing plus 270 and the target at plus 90 the two meet
            m_sim->pushCar(o.owner, bearing + 270.f, facing(a, b) ? 1.f : 3.f);
            m_sim->pushCar(o.victim, bearing + 90.f, facing(b, a) ? 0.5f : 1.f);
        }
        const ItemCar* tc = car(o.victim);
        if (age(o) > kMagnetMs || !tc || tc->finished) enter(o, 101);
        o.yaw = bearing + 270.f;
        float r[16], t[16];
        bx::mtxRotateZ(r, o.yaw * kDeg);
        bx::mtxTranslate(t, a.x, a.y, a.z);
        bx::mtxMul(o.world, r, t);
        bx::mtxTranslate(t, b.x, b.y, b.z);
        bx::mtxMul(o.world2, r, t);
        return;
    }
    if (o.state == 101) {
        enter(o, 102);
        o.shown = false;
        o.model2 = -1;
        return;
    }
    if (o.state == 102 && age(o) > kMagnetFreeMs) release(o);
}

// sub 4C7B80 the rabbit slot at the owner the own car rides the carry pool of the port
void RaceItems::spawnRabbit(int kind, int owner, float x, float y, float z, float yawDeg) {
    Obj* o = alloc(kind);
    if (!o) return;
    o->owner = owner;
    o->pos[0] = x; o->pos[1] = y; o->pos[2] = z;
    o->yaw = yawDeg;
    setModel(*o, 0);
    setModel2(*o, 1);
    enter(*o, 0);
    if (isLocal(owner) && m_sim) m_sim->startCarry(owner, kind == kItemBlueRabbit, x, y, z, yawDeg);
    sound("item_rabbit_use_snd");
}

// sub 4C7ED0 the pung puff on the car at the grab the run nif carries the car until the drop
void RaceItems::updateRabbit(Obj& o) {
    const bool blue = o.kind == kItemBlueRabbit;
    if (isLocal(o.owner) && m_sim) {
        const GimmickPoolSlot* slot = m_sim->carrySlot(o.owner, blue);
        const bool carrying = slot && static_cast<int>(slot->state) < 200;
        if (!carrying && o.state == 0) {
            enter(o, 200);
            setModel(o, 0);
            o.model2 = -1;
        }
        if (o.state == 0 && slot) {
            o.pos[0] = slot->x; o.pos[1] = slot->y; o.pos[2] = slot->z;
            o.yaw = slot->yaw_deg;
        }
    } else if (o.state == 0) {
        const CarPose p = carPose(o.owner);
        o.pos[0] = p.x; o.pos[1] = p.y; o.pos[2] = p.z;
        o.yaw = p.yawDeg;
        if (age(o) > kRabbitRemoteMs) { enter(o, 200); setModel(o, 0); o.model2 = -1; }
    }
    if (o.state == 200 && age(o) > 1300.0) { release(o); return; }
    placeOnCar(o, o.owner, 1.f, kRabbitScale, 0.f);
    if (o.model2 >= 0) {
        float s[16], r[16], t[16], sr[16];
        bx::mtxScale(s, kRabbitScale);
        bx::mtxRotateZ(r, o.yaw * kDeg);
        bx::mtxTranslate(t, o.pos[0], o.pos[1], o.pos[2]);
        bx::mtxMul(sr, s, r);
        bx::mtxMul(o.world2, sr, t);
    }
}

// sub 4C8FC0 press the search starts on the shooter with no target yet
void RaceItems::lockPress(int kind, int shooter) {
    Lock& l = m_locks[kind == kItemMagnet ? 1 : 0];
    if (l.phase != 0) return;
    l.phase = 1;
    l.shooter = shooter;
    l.target = -1;
    l.pingAt = m_now - 10.0;
    l.phaseAt = m_now;
    l.aim = 0.f;
    sound(kind == kItemMagnet ? "item_magnet_target_search_snd" : "item_rocket_target_search_snd", 0.7f);
}

void RaceItems::lockAim(int kind, float deltaDeg) {
    Lock& l = m_locks[kind == kItemMagnet ? 1 : 0];
    if (l.phase == 1) l.aim = std::clamp(l.aim + deltaDeg, -kLockAim, kLockAim);
}

// sub 4C8FC0 release with no target drops the search with a target the lock goes to phase 2
int RaceItems::lockRelease(int kind) {
    Lock& l = m_locks[kind == kItemMagnet ? 1 : 0];
    if (l.phase != 1) return l.phase;
    if (l.target < 0) {
        l.phase = 0;
        sound(kind == kItemMagnet ? "item_magnet_target_fail_snd" : "item_rocket_target_fail_snd", 0.7f);
        return 0;
    }
    l.phase = 2;
    l.phaseAt = m_now;
    if (m_hooks.lockPing) m_hooks.lockPing(kind, l.target, 2);
    return 2;
}

void RaceItems::lockCancel(int kind) {
    Lock& l = m_locks[kind == kItemMagnet ? 1 : 0];
    l.phase = 0;
    l.target = -1;
}

int RaceItems::lockPhase(int kind) const { return m_locks[kind == kItemMagnet ? 1 : 0].phase; }
int RaceItems::lockTarget(int kind) const { return m_locks[kind == kItemMagnet ? 1 : 0].target; }

void RaceItems::lockWarning(int kind, int phase) {
    Lock& l = m_locks[kind == kItemMagnet ? 1 : 0];
    l.warnPhase = phase;
    l.warnAt = m_now;
    if (phase != 0) sound(kind == kItemMagnet ? "item_magnet_target_on_snd" : "item_rocket_target_on_snd", 0.8f);
}

// sub 4CA2A0 and sub 4C6950 phase 1 scans and pings phase 2 launches phase 3 holds 1000 ms
void RaceItems::updateLocks() {
    for (int i = 0; i < 2; ++i) {
        Lock& l = m_locks[static_cast<size_t>(i)];
        const int kind = i == 0 ? kItemRocket : kItemMagnet;
        if (l.warnPhase != 0 && m_now - l.warnAt > 3.0) l.warnPhase = 0;
        switch (l.phase) {
        case 1: {
            if (l.shooter < 0) { l.phase = 0; break; }
            const CarPose p = carPose(l.shooter);
            const float from[3] = {p.x, p.y, p.z};
            const int was = l.target;
            l.target = scanCone(l.shooter, from, p.yawDeg + l.aim, kLockReach);
            if (l.target >= 0 && was < 0 && isLocal(l.shooter))
                sound(kind == kItemMagnet ? "item_magnet_target_on_snd" : "item_rocket_target_on_snd", 0.7f);
            if (l.target >= 0 && (m_now - l.pingAt) * 1000.0 > kLockPingMs) {
                l.pingAt = m_now;
                if (m_hooks.lockPing) m_hooks.lockPing(kind, l.target, 1);
            }
            break;
        }
        case 2:
            launchHoming(kind, l.shooter, l.target);
            l.phase = 3;
            l.phaseAt = m_now;
            break;
        case 3:
            if ((m_now - l.phaseAt) * 1000.0 > kLockResetMs) l.phase = 0;
            break;
        default:
            break;
        }
    }
}

// sub 4C9C70 the search frames target01 over the middle then the lock frames target02 over the car
ItemReticle RaceItems::reticle() const {
    ItemReticle r;
    const int tick = static_cast<int>(m_now * 60.0) / 2;
    for (int i = 0; i < 2; ++i) {
        const Lock& l = m_locks[static_cast<size_t>(i)];
        const int kind = i == 0 ? kItemRocket : kItemMagnet;
        const int searchFrames = kind == kItemRocket ? 10 : 2;
        const int lockFrames = kind == kItemRocket ? 3 : 4;
        if (l.phase == 1 && l.shooter >= 0 && isLocal(l.shooter)) {
            r.shown = true;
            r.kind = kind;
            r.carIndex = l.target;
            r.set = l.target >= 0 ? 2 : 1;
            r.frame = l.target >= 0 ? tick % lockFrames : tick % searchFrames;
            return r;
        }
        if (l.warnPhase != 0) {
            r.shown = true;
            r.kind = kind;
            for (const ItemCar& c : m_cars) if (c.local) r.carIndex = c.carIndex;
            r.set = l.warnPhase == 2 ? 2 : 1;
            r.frame = r.set == 2 ? tick % lockFrames : tick % searchFrames;
            return r;
        }
    }
    return r;
}

void RaceItems::update(float dt, const std::vector<ItemCar>& cars) {
    if (!m_active) return;
    m_cars = cars;
    m_now += dt;
    updateLocks();
    for (int kind = 0; kind < kItemKindCount; ++kind) {
        for (Obj& o : m_pools[static_cast<size_t>(kind)].objs) {
            if (!o.live) continue;
            switch (kind) {
            case kItemSpike: case kItemBomb: case kItemDung:
                updateThrown(o, dt);
                if (o.live) hitThrown(o);
                break;
            case kItemStorm: updateStorm(o, dt); break;
            case kItemHive: case kItemIce: case kItemFlash: updatePlaced(o); break;
            case kItemThunder: case kItemHammer: updateStrike(o); break;
            case kItemShield: case kItemAngel: case kItemDevilRed: updateGuard(o); break;
            case kItemHandle: case kItemDevil: updateCurse(o); break;
            case kItemSmoke: updateSmoke(o, dt); break;
            case kItemTurtle:
                updateTurtle(o, dt);
                if (o.live) hitTurtle(o);
                break;
            case kItemRocket:
                updateRocket(o, dt);
                if (o.live) hitRocket(o);
                break;
            case kItemMagnet: updateMagnet(o); break;
            case kItemRabbit: case kItemBlueRabbit: updateRabbit(o); break;
            default: break;
            }
        }
        for (Obj& o : m_victims[static_cast<size_t>(kind)].objs) if (o.live) updateVictim(o);
    }
    if (m_sim) {
        // the freed turtle slots let go of the effect 500 of their car
        for (const Obj& o : m_pools[kItemTurtle].objs) {
            if (o.live) continue;
            GimmickHazardSlot& h = m_sim->game().hazardTable[static_cast<size_t>(o.slot) % GIMMICK_HAZARD_TABLE_SIZE];
            h.active = 0;
            h.car_index = -1;
        }
        // sub 4C31D0 the handle quarters the drift gauge rate sub 4BD8D0 the devil swaps the steering
        for (int kind : {kItemHandle, kItemDevil}) {
            auto& table = kind == kItemHandle ? m_sim->game().slowedTable : m_sim->game().cameraReversedTable;
            for (const Obj& o : m_pools[static_cast<size_t>(kind)].objs) {
                StatusIdSlot& s = table[static_cast<size_t>(o.slot) % STATUS_TABLE_SIZE];
                s.active = o.live ? 1 : 0;
                s.id = o.live ? o.victim : -1;
            }
        }
    }
    // sub 4CDC00 the storm pushes and hits the own car alone online
    for (Obj& o : m_pools[kItemStorm].objs) {
        if (!o.live || o.state != 2) continue;
        for (const ItemCar& c : m_cars) {
            if (!tested(c) || c.carIndex == o.owner || c.finished) continue;
            const CarPose p = carPose(c.carIndex);
            const float dx = p.x - o.pos[0], dy = p.y - o.pos[1], dz = p.z - o.pos[2];
            const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (d >= kStormWind) continue;
            if (d < kStormReach) {
                if (absorb(c.carIndex)) continue;
                victimSlot(kItemStorm, c.carIndex);
                if (c.local && m_hooks.reportHit) m_hooks.reportHit(100);
                if (c.local && m_hooks.attackView) m_hooks.attackView(c.carIndex);
                enter(o, 3);
                break;
            }
            if (m_sim) {
                // the wind pushes the car away from the storm by 36 less the distance times 0 14 times 0 18
                m_sim->pushCar(c.carIndex, yawToward(o.pos[0], o.pos[1], p.x, p.y), (kStormWind - d) * 0.14f * 0.18f);
            }
        }
    }
}

void RaceItems::submit(KnC::Render::SceneRenderer& renderer, std::vector<KnC::Render::PropInstance>& out) {
    if (!m_active) return;
    const float clock = renderer.animation().seconds();
    auto modelOf = [&](Pool& pool, int model) -> const KnC::Render::PropModel* {
        const std::string& rel = pool.nifs[static_cast<size_t>(model)];
        const std::string key = lower(rel);
        auto it = m_models.find(key);
        if (it == m_models.end()) {
            KnC::Render::PropModel loaded;
            const fs::path relPath(rel);
            const std::string dir = findCi(m_itemDir, relPath.parent_path().string());
            const std::string file = dir.empty() ? std::string() : findCi(dir, relPath.filename().string() + ".nif");
            KnC::Render::NifModelRequest request;
            request.nif_path = file;
            request.texture_dir = dir;
            request.play_stopped_controllers = true;
            std::string error;
            if (file.empty() || !KnC::Render::load_prop_model(request, loaded, error)) {
                std::printf("[items] %s failed %s\n", rel.c_str(), file.empty() ? "no file" : error.c_str());
                loaded = KnC::Render::PropModel{};
            } else {
                KnC::Tools::resolve_textures(dir, loaded);
                std::printf("[items] %s %zu parts %zu particle systems\n", rel.c_str(), loaded.parts.size(),
                            loaded.particle_systems.size());
            }
            it = m_models.emplace(key, std::move(loaded)).first;
        }
        if (it->second.parts.empty() && it->second.particle_systems.empty()) return nullptr;
        return &it->second;
    };
    auto place = [&](Pool& pool, Obj& o, int model, bool& restart, const float world[16]) {
        if (model < 0 || model >= static_cast<int>(pool.nifs.size()) || model >= 5) return;
        std::size_t& appended = pool.appended[static_cast<size_t>(o.slot)][static_cast<size_t>(model)];
        if (appended == kNoModel) {
            const KnC::Render::PropModel* m = modelOf(pool, model);
            if (!m) return;
            appended = renderer.append_prop_model(*m);
            restart = true;
        }
        if (restart) {
            renderer.restart_prop_particles(appended, clock);
            restart = false;
        }
        KnC::Render::PropInstance instance;
        instance.model_index = appended;
        instance.layer = KnC::Render::SceneLayer::Props;
        for (int i = 0; i < 16; ++i) instance.world[i] = world[i];
        out.push_back(instance);
    };
    auto emit = [&](Pool& pool, Obj& o) {
        if (!o.live) return;
        if (o.shown) place(pool, o, o.model, o.restart, o.world);
        if (o.model2 >= 0) place(pool, o, o.model2, o.restart2, o.world2);
    };
    for (int kind = 0; kind < kItemKindCount; ++kind) {
        for (Obj& o : m_pools[static_cast<size_t>(kind)].objs) emit(m_pools[static_cast<size_t>(kind)], o);
        for (Obj& o : m_victims[static_cast<size_t>(kind)].objs) emit(m_victims[static_cast<size_t>(kind)], o);
    }
}

}
