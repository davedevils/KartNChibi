// sub 4AE230 drift gauge nifs drawn through gauge camera into gauge rect of hud
#pragma once

#include "RaceHud.h"

#include <cstdint>
#include <string>

namespace KnC::Render { class SceneRenderer; }

namespace KnC::Client {

// sub 4AE590 second camera at -76 -80 380 down z 380x380 rect fov 1 near 0 1 far 10000
class GaugeScene {
public:
    // load nifs append to renderer after track scene up
    bool load(KnC::Render::SceneRenderer& renderer, const std::string& gameDir, bool teamMode);
    void unload() { m_loaded = false; }
    bool loaded() const { return m_loaded; }
    // call after world frame fbW fbH are frame pixels canvasW canvasH the stock canvas stretch as the sprite batch
    void draw(KnC::Render::SceneRenderer& renderer, const HudState& state, float dt, uint16_t fbW, uint16_t fbH,
              float canvasW, float canvasH, bool stretch);

private:
    bool m_loaded = false;
    bool m_team = false;
    int m_red = -1;
    int m_blue = -1;
    int m_spark = -1;
    int m_normalBoost = -1;
    int m_redBoost = -1;
    int m_blueBoost = -1;
    // 0xB08 running seconds of spark clip wrapped at its length
    float m_sparkSeconds = 0.f;
};

}
