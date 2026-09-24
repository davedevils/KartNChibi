// podium scene for race end winner 01 or 02 sits at grid midpoint camera mode 8
#pragma once

#include "RaceSim.h"
#include "RaceView.h"
#include "TrackData.h"

#include <string>
#include <vector>

namespace KnC::Render { class SceneRenderer; }

namespace KnC::Client {

// podium clock event this frame drives sound and camera
enum class PodiumEvent { DropStart, Landing, OthersLanding, WinClips, Ceremony };

class PodiumScene {
public:
    // load podium nif for mode place between first and last start row
    bool load(RaceView& view, KnC::Render::SceneRenderer& renderer, const std::string& gameDir,
              const RaceWorld& world, bool teamMode, int finisherCount);
    // world pose for finisher rank 0 is first slot is index past rank three
    bool seat(int rank, int slot, CarPose& pose) const;
    // advance drop and stage clock frame events land in out
    void update(float dt, std::vector<PodiumEvent>& out);
    // camera mode 8 eye and look off podium plus fov radians
    void camera(float eye[3], float look[3], float& fovRadians) const;
    float seconds() const { return m_seconds; }
    bool loaded() const { return m_loaded; }
    void reset() { m_loaded = false; }

private:
    // offset from stock world axes turned into heading of this grid
    void turned(float dx, float dy, float out[2]) const;

    bool m_loaded = false;
    bool m_team = false;
    int m_finishers = 0;
    float m_pos[3] = {0.f, 0.f, 0.f};
    float m_yawDeg = 0.f;
    float m_headingDeg = 0.f;
    // world 0x92F8 five drop heights fall at 50 per second beginning at one and a half seconds
    float m_drop[5] = {0.f, 0.f, 0.f, 0.f, 0.f};
    // world 0x9320 stage from FUN 00486DF0 seconds counts time since load
    int m_stage = 0;
    float m_seconds = 0.f;
    int m_propHandle = -1;
};

}
