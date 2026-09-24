// a line follower on the follow 01 ini points it steers the port with the six flags
#pragma once

#include "RaceSim.h"

#include "games/kart/physics/client/input.h"

#include <vector>

namespace KnC::Client {

class AutoDriver {
public:
    void reset(const std::vector<KnC::Kart::Client::CheckpointPoint>& line);
    // starts on the point ahead of the grid the nearest overall can sit behind and turns the kart around
    void start(float x, float y, float yawDeg);
    // the flags for this tick from the pose of the local car
    void drive(const CarPose& pose, KnC::Kart::Client::InputFlags& flags, bool& driftHeld);
    bool ready() const { return !m_line.empty(); }
    int targetIndex() const { return m_index; }
    float lastErrorDeg() const { return m_lastError; }

private:
    std::vector<KnC::Kart::Client::CheckpointPoint> m_line;
    int m_index = 0;
    float m_lastError = 0.f;
    float m_reverseUntil = 0.f;
    float m_stuckSeconds = 0.f;
};

}
