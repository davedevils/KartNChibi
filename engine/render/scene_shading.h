#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace KnC::Render {

// sun direction for ground Lambert view and prop hulls
constexpr float kLightDirection[3] = {0.42f, -0.56f, 0.71f};
constexpr float kAmbientLight = 0.30f;

struct Rgba {
    int red = 255;
    int green = 255;
    int blue = 255;
    int alpha = 255;
};

// Lambert shade term of surface normal need not be unit
inline float lambert_shade(const float normal[3]) {
    const float length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] +
                                   normal[2] * normal[2]);
    if (length <= 0.f) return 1.0f;
    return (normal[0] * kLightDirection[0] + normal[1] * kLightDirection[1] +
            normal[2] * kLightDirection[2]) / length;
}

inline uint32_t shaded_abgr(const Rgba& colour, float light) {
    const float lit = std::min(1.0f, std::max(kAmbientLight, light));
    const auto channel = [lit](int value) {
        return static_cast<uint32_t>(std::lround(static_cast<float>(value) * lit));
    };
    return (static_cast<uint32_t>(colour.alpha) << 24) | (channel(colour.blue) << 16) |
           (channel(colour.green) << 8) | channel(colour.red);
}

}
