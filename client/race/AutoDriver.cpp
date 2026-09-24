#include "AutoDriver.h"

#include <cmath>

namespace KnC::Client {

using KnC::Kart::Client::CheckpointPoint;
using KnC::Kart::Client::InputFlags;

namespace {

constexpr float kPi = 3.14159265f;
// a point counts as reached inside this distance then the next one is the target
constexpr float kReachUnits = 22.f;
// no steer inside this heading error
constexpr float kDeadDeg = 3.f;
// a long turn at speed takes the drift key
constexpr float kDriftDeg = 24.f;
constexpr float kDriftKmh = 55.f;
// a hairpin at speed takes the brake
constexpr float kBrakeDeg = 65.f;
constexpr float kBrakeKmh = 60.f;
// the tick length the stuck timer counts
constexpr float kTickSeconds = 0.02f;

float wrapDeg(float d) {
    while (d > 180.f) d -= 360.f;
    while (d < -180.f) d += 360.f;
    return d;
}

}

void AutoDriver::reset(const std::vector<CheckpointPoint>& line) {
    m_line = line;
    m_index = 0;
    m_lastError = 0.f;
    m_reverseUntil = 0.f;
    m_stuckSeconds = 0.f;
}

void AutoDriver::start(float x, float y, float yawDeg) {
    if (m_line.empty()) return;
    // forward is minus cos A sin A the same rule the grid heading follows
    const float rad = yawDeg * kPi / 180.f;
    const float dirx = -std::cos(rad), diry = std::sin(rad);
    size_t best = m_line.size();
    float bestDist = 1e30f;
    for (size_t i = 0; i < m_line.size(); ++i) {
        const float dx = m_line[i].x - x, dy = m_line[i].y - y;
        if (dx * dirx + dy * diry <= 0.f) continue;
        const float d = dx * dx + dy * dy;
        if (d < bestDist) { bestDist = d; best = i; }
    }
    if (best == m_line.size()) {
        for (size_t i = 0; i < m_line.size(); ++i) {
            const float dx = m_line[i].x - x, dy = m_line[i].y - y;
            const float d = dx * dx + dy * dy;
            if (d < bestDist) { bestDist = d; best = i; }
        }
    }
    m_index = static_cast<int>(best % m_line.size());
}

void AutoDriver::drive(const CarPose& pose, InputFlags& flags, bool& driftHeld) {
    flags = InputFlags();
    driftHeld = false;
    if (m_line.empty()) return;
    const size_t n = m_line.size();
    // step past every point already reached the line loops
    for (int guard = 0; guard < 4; ++guard) {
        const CheckpointPoint& t = m_line[static_cast<size_t>(m_index) % n];
        const float dx = t.x - pose.x, dy = t.y - pose.y;
        if (std::sqrt(dx * dx + dy * dy) > kReachUnits) break;
        m_index = static_cast<int>((static_cast<size_t>(m_index) + 1) % n);
    }
    const CheckpointPoint& target = m_line[static_cast<size_t>(m_index) % n];
    const float dx = target.x - pose.x, dy = target.y - pose.y;
    const float wantDeg = std::atan2(dy, -dx) * 180.f / kPi;
    const float error = wrapDeg(wantDeg - pose.yawDeg);
    m_lastError = error;

    // a kart that stopped against a wall backs off for a moment then tries again
    if (pose.speedKmh < 4.f) m_stuckSeconds += kTickSeconds;
    else m_stuckSeconds = 0.f;
    if (m_stuckSeconds > 2.5f) { m_reverseUntil = 1.2f; m_stuckSeconds = 0.f; }
    if (m_reverseUntil > 0.f) {
        m_reverseUntil -= kTickSeconds;
        flags.brake = 1;
        if (error > kDeadDeg) flags.steerLeft = 1;
        else if (error < -kDeadDeg) flags.steerRight = 1;
        return;
    }

    flags.accel = 1;
    if (error > kDeadDeg) flags.steerRight = 1;
    else if (error < -kDeadDeg) flags.steerLeft = 1;
    const float mag = std::fabs(error);
    if (mag > kBrakeDeg && pose.speedKmh > kBrakeKmh) {
        flags.accel = 0;
        flags.brake = 1;
    }
    if (mag > kDriftDeg && pose.speedKmh > kDriftKmh) driftHeld = true;
}

}
