// sub 4D2A30 4D25E0 4D2950 the sun flare of the clear weather six Effect lens nifs and a white glare
#pragma once

#include <cstdint>
#include <string>

namespace KnC::Render { class SceneRenderer; }

namespace KnC::Client {

class RaceView;

class LensFlare {
public:
    // 0x4D2A30 loads Effect lens1 to lens6 a point with a negative z turns the flare off
    bool load(RaceView& view, KnC::Render::SceneRenderer& renderer, const std::string& gameDir, const float point[3]);
    // 0x4D25E0 places the six sheets on the line from the sun point to the view axis 20 units out
    void update(RaceView& view);
    // hides the six sheets the podium has no flare
    void hide(RaceView& view);
    // 0x4D2950 white veil alpha 0 to 60 once the strength passes 0 6
    uint8_t glare() const;
    bool loaded() const { return m_loaded; }
    float strength() const { return m_strength; }

private:
    static constexpr int kLenses = 6;
    int m_props[kLenses] = {-1, -1, -1, -1, -1, -1};
    float m_point[3] = {0.f, 0.f, 0.f};
    // 0x428 of the flare object 1 at 26 degrees from the sun or nearer 0 past 60
    float m_strength = 0.f;
    bool m_loaded = false;
};

}
