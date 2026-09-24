#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace KnC::Render {

// position normal tint UV 0 to 1 per layer zero normal pre shaded
struct SceneVertex {
    float    x = 0.f;
    float    y = 0.f;
    float    z = 0.f;
    float    normal_x = 0.f;
    float    normal_y = 0.f;
    float    normal_z = 0.f;
    uint32_t abgr = 0xffffffffu;
    float    u = 0.f;
    float    v = 0.f;
};

// NIF color four floats r first packed vertex diffuse for bgfx
inline uint32_t nif_colour_abgr(const float colour[4]) {
    const auto channel = [](float value) {
        return static_cast<uint32_t>(std::lround(std::clamp(value, 0.f, 1.f) * 255.f));
    };
    return (channel(colour[3]) << 24) | (channel(colour[2]) << 16) |
           (channel(colour[1]) << 8) | channel(colour[0]);
}

}
