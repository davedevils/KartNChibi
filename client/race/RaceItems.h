// the item objects of a race the stock managers one pool per kind with their nifs motion and hits
#pragma once

#include "RaceSim.h"
#include "TrackData.h"

#include "engine/render/map_scene.h"
#include "games/kart/physics/client/world_collision.h"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace KnC::Render { class SceneRenderer; }

namespace KnC::Client {

// held item ids of the 0x0049 grant the 0x0047 spawn kind and the icon table
enum ItemKind : int {
    kItemBooster = 0, kItemBigBooster = 1, kItemSpike = 2, kItemStorm = 3, kItemThunder = 4, kItemHandle = 5,
    kItemTurtle = 6, kItemRabbit = 7, kItemShield = 8, kItemSmoke = 9, kItemRocket = 10, kItemHive = 11,
    kItemAngel = 12, kItemBlueRabbit = 13, kItemIce = 14, kItemFlash = 15, kItemMagnet = 16, kItemHammer = 17,
    kItemBomb = 18, kItemDung = 19, kItemDevil = 20, kItemDevilRed = 21, kItemKindCount = 22
};

// one racer as the items see it the local one is the car the camera follows
struct ItemCar {
    int carIndex = -1;
    uint32_t playerId = 0;
    bool local = false;
    // zero based standings rank of 0x0045 minus one before the first row
    int rank = -1;
    int team = 0;
    // car 0x3714 the stock item tests skip a car that finished
    bool finished = false;
};

// what the items ask of the race screen sounds hit reports and the two preview windows
struct ItemHooks {
    std::function<void(const char* sound, float volume)> sound;
    // the own car took a hit of a local only kind its code goes out on C2S 0x0069
    std::function<void(int code)> reportHit;
    // sub 4A9AC0 the top right attack view on a car for three seconds
    std::function<void(int carIndex)> attackView;
    // sub 4AB570 the event view mode 3 on a car mode 4 on an item kind and slot
    std::function<void(int mode, int a, int b)> eventView;
    // sub 4C0740 the dung splats over the screen of the own car
    std::function<void()> dungSplat;
    // effect 1100 of the flash sub 43D7E0 mode 3 the white veil over the own screen
    std::function<void()> flashBlind;
    // sub 481520 the lock phase of the own rocket or magnet on a car C2S 0x0057 online
    std::function<void(int kind, int targetCar, int phase)> lockPing;
};

// the aim marker of sub 4C9C70 and sub 4C5E20 over a car or over the middle while the search runs
struct ItemReticle {
    bool shown = false;
    // 10 rocket 16 magnet
    int kind = 0;
    // 1 the search frames target01 2 the lock frames target02
    int set = 1;
    int frame = 0;
    // the car the marker sits on minus one for the middle of the screen
    int carIndex = -1;
};

class RaceItems {
public:
    // a fresh race the item nifs under Data Public Item offline is the stock no socket branch
    void begin(const std::string& gameDir, RaceWorld& world, RaceSim& sim, const ItemHooks& hooks, bool teamMode,
               bool offline);
    void end();
    bool active() const { return m_active; }
    // FUN 004B0570 the stock roll table 0 of 0x5EBE50 by racer count and zero based rank minus one for nothing
    static int roll(int racers, int rank, bool heldRabbit, bool teamMode, uint32_t& seed);
    // sub 47A110 the S2C 0x0047 spawn of a kind by its owner car and the four floats of the frame
    void spawn(int kind, int ownerCar, float x, float y, float z, float yawDeg);
    // S2C 0x0069 700 and 1000 put the hive and the ice on the car
    void remoteHit(int carIndex, int code);
    // sub 4D1A50 the own turtle takes the racer ahead in the standings the target car or minus one
    int useTurtle(int owner);
    // S2C 0x005C the turtle of another racer
    void remoteTurtle(int owner, int target);
    // sub 4CA1A0 sub 4C6910 the rocket or the magnet of a shooter on a target
    void launchHoming(int kind, int owner, int target);
    // sub 4C8FC0 a press starts the search the release locks the target or drops the search
    void lockPress(int kind, int shooter);
    int lockRelease(int kind);
    void lockCancel(int kind);
    // sub 4CA2A0 the steer keys turn the search 10 degrees a press up to 60 either side
    void lockAim(int kind, float deltaDeg);
    int lockPhase(int kind) const;
    int lockTarget(int kind) const;
    // S2C 0x0057 a lock of another racer on the own car
    void lockWarning(int kind, int phase);
    ItemReticle reticle() const;
    // the race frame the item objects move and hit the cars of the list
    void update(float dt, const std::vector<ItemCar>& cars);
    // the item nifs of the live objects appended once per pool slot and placed every frame
    void submit(KnC::Render::SceneRenderer& renderer, std::vector<KnC::Render::PropInstance>& out);
    // true while an item shields the car sub 4B7DD0 the shield and the angel
    bool shielded(int carIndex) const;
    // sub 4CEFF0 sub 4B9F80 the caster holds a live one of the kind already
    bool casting(int kind, int carIndex) const;
    // sub 4BD8D0 a devil rides the car its steering keys swap
    bool cursed(int kind, int carIndex) const;
    // live objects of a kind for the event view camera and the proof scripts
    int liveCount(int kind) const;
    bool slotPosition(int kind, int slot, float out[3]) const;
    // the owner and the target car of a slot for the event view camera
    bool slotCars(int kind, int slot, int& owner, int& target) const;
    // sub 4C25E0 the hammer squash of a car one is none
    float squash(int carIndex) const;

private:
    // one item object of a pool the state machine of its manager and the nif it shows
    struct Obj {
        bool live = false;
        int kind = -1;
        int slot = 0;
        int owner = -1;
        int victim = -1;
        int state = 0;
        double stateAt = 0.0;
        double bornAt = 0.0;
        float t = 0.f;
        float pos[3] = {0.f, 0.f, 0.f};
        float vel[3] = {0.f, 0.f, 0.f};
        float target[3] = {0.f, 0.f, 0.f};
        float yaw = 0.f;
        float heading = 0.f;
        float scale = 1.f;
        float sink = 1.f;
        float speed = 0.f;
        float lift = 0.f;
        float budget = 0.f;
        int model = 0;
        // a second nif shown beside the main one minus one for none
        int model2 = -1;
        bool restart = true;
        bool restart2 = true;
        bool shown = true;
        bool armed = false;
        float world[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
        float world2[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
        KnC::Kart::Client::BspQuery probe;
    };
    // the nifs of a kind in slot order and one appended copy per pool slot per nif
    struct Pool {
        std::vector<std::string> nifs;
        std::vector<Obj> objs;
        std::vector<std::array<std::size_t, 5>> appended;
    };
    // the search and lock globals of the rocket at 0x2EFFF40 and of the magnet at 0x2EF7BD8
    struct Lock {
        int phase = 0;
        int shooter = -1;
        int target = -1;
        double pingAt = -10.0;
        double phaseAt = 0.0;
        float aim = 0.f;
        // the lock of another racer on the own car and when it came
        int warnPhase = 0;
        double warnAt = -10.0;
    };

    Obj* alloc(int kind);
    void release(Obj& o);
    void setModel(Obj& o, int model);
    void setModel2(Obj& o, int model);
    void enter(Obj& o, int state);
    double age(const Obj& o) const { return (m_now - o.stateAt) * 1000.0; }
    double life(const Obj& o) const { return (m_now - o.bornAt) * 1000.0; }
    CarPose carPose(int carIndex) const;
    float carGauge(int carIndex) const;
    const ItemCar* car(int carIndex) const;
    bool isLocal(int carIndex) const;
    // the stock tests every car without a socket and the own car alone online
    bool tested(const ItemCar& c) const { return m_offline || c.local; }
    int leader() const;
    bool ground(Obj& o, float x, float y, float z, float& out);
    // sub 4B7DD0 the shield or the angel takes the hit and pops
    bool absorb(int carIndex);
    void effect(int carIndex, int code);
    bool effectBusy(int carIndex) const;
    void carMatrix(int carIndex, float out[16]) const;
    void placeThrown(Obj& o);
    void placeOnCar(Obj& o, int carIndex, float lift, float scale, float turnDeg);
    void placeOnCar2(Obj& o, int carIndex, float lift, float scale);
    void placeAt(Obj& o, const float at[3], float yawDeg, float scale);
    void sound(const char* name, float volume = 1.f);
    // sub 489E60 the racing line point past the reach from a car where no other racer stands
    bool lineAhead(int ownerCar, float reach, float out[3]) const;
    // sub 489970 sub 489D50 the nearest racing line point and the one after it
    bool lineNext(const float at[3], float out[3]) const;
    int teamOf(int carIndex) const;
    // sub 499CB0 the nearest racer inside 30 degrees of a heading and a reach
    int scanCone(int shooter, const float from[3], float headingDeg, float reach) const;

    void spawnThrown(int kind, int owner, float x, float y, float z, float yawDeg);
    void updateThrown(Obj& o, float dt);
    void hitThrown(Obj& o);
    void spawnStorm(int owner, float x, float y, float z, float yawDeg);
    void updateStorm(Obj& o, float dt);
    void spawnPlaced(int kind, int owner, float yawDeg);
    void updatePlaced(Obj& o);
    void victimSlot(int kind, int carIndex);
    void updateVictim(Obj& o);
    void spawnStrike(int kind, int owner);
    void updateStrike(Obj& o);
    void spawnGuard(int kind, int carIndex);
    void updateGuard(Obj& o);
    void spawnCurse(int kind, int owner);
    void updateCurse(Obj& o);
    void spawnSmoke(int owner, float x, float y, float z, float yawDeg);
    void updateSmoke(Obj& o, float dt);
    // sub 4CFCF0 sub 4D00C0 the turtle toss chase bite and ride on its target
    void spawnTurtle(int owner, int target);
    void updateTurtle(Obj& o, float dt);
    void hitTurtle(Obj& o);
    // sub 4C9140 sub 4CA2A0 the rocket flies to its target
    void spawnRocket(int owner, int target);
    void updateRocket(Obj& o, float dt);
    void hitRocket(Obj& o);
    // sub 4C5BD0 sub 4C6950 the magnet pulls the two cars together
    void spawnMagnet(int owner, int target);
    void updateMagnet(Obj& o);
    // sub 4C7B80 sub 4C7ED0 the rabbit carries its car along the line
    void spawnRabbit(int kind, int owner, float x, float y, float z, float yawDeg);
    void updateRabbit(Obj& o);
    void updateLocks();

    std::map<std::string, KnC::Render::PropModel> m_models;
    std::array<Pool, kItemKindCount> m_pools;
    // the hit visual of the storm the hive and the ice on a victim car
    std::array<Pool, kItemKindCount> m_victims;
    // index 0 the rocket index 1 the magnet
    std::array<Lock, 2> m_locks;
    std::string m_itemDir;
    RaceWorld* m_world = nullptr;
    RaceSim* m_sim = nullptr;
    ItemHooks m_hooks;
    std::vector<ItemCar> m_cars;
    // the hammer squash per car index and when it began
    std::map<int, double> m_squash;
    double m_now = 0.0;
    uint32_t m_rng = 0x2545F491u;
    bool m_active = false;
    bool m_team = false;
    bool m_offline = false;
};

}
