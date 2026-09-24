// back row of Input ini camera update 0x43F040 turns eye to rear while held
#pragma once

#include "RaceSim.h"

#include <cmath>

namespace KnC::Client {

// eye sits ahead of nose over driver look point behind kart
inline void lookBackCamera(const CarPose& car, float eye[3], float look[3]) {
    const float rad = car.yawDeg * 3.14159265f / 180.f;
    const float fx = -std::cos(rad);
    const float fy = std::sin(rad);
    eye[0] = car.x + fx * 3.5f;
    eye[1] = car.y + fy * 3.5f;
    eye[2] = car.z + 4.f;
    look[0] = car.x - fx * 15.f;
    look[1] = car.y - fy * 15.f;
    look[2] = car.z + 1.f;
}

// look back fov matches stock chase camera rest fov
constexpr float kLookBackFieldRadians = 1.05f;

}
