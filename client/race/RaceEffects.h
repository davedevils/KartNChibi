// stock car effects one nif pool per car per effect on kart dummies
#pragma once

#include "RaceSim.h"

#include "engine/render/map_scene.h"
#include "tools/track_scene/ghost_car.h"

#include <atomic>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace KnC::Render { class SceneRenderer; }

namespace KnC::Client {

// game dir of track folder is Data Public World theme track five folders up
std::string raceGameDirOf(const std::string& trackDir);

class RaceEffects {
public:
    // forgets every car and pool scene was replaced game dir finds effect nifs
    void reset(const std::string& gameDir);
    // once per car per frame pose with hasSignals uses port flags else guesses landing from z history
    void update(int handle, const CarPose& pose, const KnC::Tools::GhostCar* car, const float world[16],
                float dt, float clock);
    // port signals for a car override update guesses when caller has them
    void setSignals(int handle, const CarEffectSignals& signals);
    // 0x0069 effect code landed on car code 1000 plays small impact sprite 0x496066
    void hit(int handle, int code, float clock);
    // plate and antenna item of car model names from 0x00C2 part rows
    void setLook(int handle, const std::string& plateModel, const std::string& antModel);
    // the posed O NAME and O ANT of the body the plate and antenna ride its bounce like child nodes
    void followDummies(int handle, const float name[16], const float ant[16]);
    void remove(int handle);
    // ground shake of wheels on car state call after wheel update
    void shakeWheels(int handle, KnC::Tools::GhostWheelState& wheels) const;
    // loads missing nifs appends effects after kart ones eye places impact sprite quarter way to car 0x4981B0
    void submit(KnC::Render::SceneRenderer& renderer, std::vector<KnC::Render::PropInstance>& out,
                const float eye[3] = nullptr);
    // parses every effect nif off main thread during load stop flag aborts early if race exits first
    static std::map<std::string, KnC::Render::PropModel> parseAll(const std::string& gameDir,
                                                                  const std::atomic<bool>* stop = nullptr);
    // takes parsed models after reset first shown then appended only
    void adopt(std::map<std::string, KnC::Render::PropModel>&& models);
    // a parseAll thread runs so an effect waits for adopt instead of a parse on the frame thread
    void awaitParse() { m_awaitingParse = true; }

private:
    // one effect per car exe slot list from car visual update same order
    enum Slot {
        kDrift1L, kDrift1R, kDrift2L, kDrift2R, kTurboL, kTurboR, kTurboWind, kSlips, kLand,
        kSmogL, kSmogR, kDustL, kDustR, kShadow, kPlate, kAnt, kHit, kSlotCount
    };
    struct Pool {
        // appended model index in renderer none until effect first shows
        std::size_t model = static_cast<std::size_t>(-1);
        std::string nif;
        bool on = false;
        bool restart = false;
        float world[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    };
    struct Car {
        bool alive = false;
        bool local = false;
        CarPose pose;
        CarEffectSignals signals;
        bool signalsGiven = false;
        const KnC::Tools::GhostCar* car = nullptr;
        float world[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
        float clock = 0.f;
        float landAt = -100.f;
        float hitAt = -100.f;
        // impact sprite tier 0 is big sheet 1 is small one
        int hitTier = 1;
        bool wasBoosting = false;
        bool wasDusting[2] = {false, false};
        bool wasLanded = false;
        // landing guess without port flag peak while rising then stop of fall
        float prevZ = 0.f;
        float prevVz = 0.f;
        float peakZ = 0.f;
        bool flying = false;
        bool prevValid = false;
        float testClock = 0.f;
        std::string plateModel;
        std::string antModel;
        // impact age from last signals smaller value means new bump
        int lastImpactAge = -1;
        // dust values exe writes per frame one drive per rear wheel shared with renderer
        std::shared_ptr<KnC::Render::ParticleDrive> dustDrive[2];
        // appended model per nif for this car dust swaps nif with ground kind
        std::map<std::string, std::size_t> appended;
        Pool pools[kSlotCount];
    };

    void decide(Car& c, float dt);
    void place(Car& c);
    // impact sprite for a tier restarts unless one started within last tenth
    void impact(Car& c, int tier, float clock);
    bool ensureModel(KnC::Render::SceneRenderer& renderer, Car& c, Slot slot);
    const KnC::Render::PropModel* loadNif(const std::string& path);
    std::string effectNif(const char* name) const;
    std::string partNif(const char* folder, const std::string& model) const;
    // folder walk for a name once per race decide pass asks every frame
    std::string cachedPath(const std::string& key, const std::string& dir, const std::string& file) const;

    std::map<int, Car> m_cars;
    // every loaded nif by path one copy appended per car that shows it
    std::map<std::string, KnC::Render::PropModel> m_models;
    // resolved file per effect name empty when folder lacks it
    mutable std::map<std::string, std::string> m_paths;
    std::string m_gameDir;
    bool m_awaitingParse = false;
    // camera eye from last submit impact sprite sits between it and car
    float m_eye[3] = {0.f, 0.f, 0.f};
    bool m_eyeValid = false;
    bool m_test = false;
    // KNC FX TEST 0 loops every effect number holds one phase for capture
    int m_testPhase = 0;
    // KNC FX SHOT png path to write once sparks and flame show together cleared after shot
    std::string m_shotPath;
    int m_shotFrames = 0;
};

// race effects for every car one set per process race view resets it with scene
RaceEffects& raceEffects();

}
