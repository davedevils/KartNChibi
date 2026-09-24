#include "GaugeScene.h"

#include "engine/render/nif_prop_model.h"
#include "engine/render/scene_renderer.h"
#include "tools/track_scene/ghost_car.h"

#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace KnC::Client {

namespace {

// sub 4AE590 the six nifs under Data Public
const char* const kRedGauge = "Effect/Guage/red_gauge.nif";
const char* const kBlueGauge = "Effect/Guage/blue_gauge.nif";
const char* const kSpark = "Effect/Guage/spark.nif";
const char* const kNormalBoost = "Effect/Guage/normal_boost.nif";
const char* const kRedBoost = "Effect/Guage/red_boost_get.nif";
const char* const kBlueBoost = "Effect/Guage/blue_boost_get.nif";

// 0x4AE7F1 the rect is width minus 380 height minus 383 to width and height minus 3
constexpr float kRectLeft = 380.f;
constexpr float kRectTop = 383.f;
constexpr float kRectSize = 380.f;
// 0x4AE8D3 the eye the look point and the up of the gauge camera
constexpr float kEye[3] = {-76.f, -80.f, 380.f};
constexpr float kLook[3] = {-76.f, -80.f, 0.f};
constexpr float kUp[3] = {-1.f, 0.f, 0.f};
// 0x4AE8B5 the lens 1 rad the near 0 1 the far 10000 the aspect 1
constexpr float kFov = 1.f;
constexpr float kNear = 0.1f;
constexpr float kFar = 10000.f;
// 0x5A6E90 the fill posed at the band value times this 127 is the whole five second clip
constexpr float kFillToSeconds = 0.039370079f;
// 0x5A6E70 the spark clip wraps at 0 13 s
constexpr float kSparkWrap = 0.13f;
// 0x5A6E88 the dummy of the gauge nif the spark rides
const char* const kSparkNode = "O_POS";

int loadOne(KnC::Render::SceneRenderer& renderer, const std::string& gameDir, const char* name) {
    namespace fs = std::filesystem;
    const fs::path nif = fs::path(gameDir) / "Data" / "Public" / name;
    KnC::Render::NifModelRequest request;
    request.nif_path = nif.string();
    request.texture_dir = nif.parent_path().string();
    request.play_stopped_controllers = true;
    KnC::Render::PropModel model;
    std::string error;
    if (!KnC::Render::load_prop_model(request, model, error)) {
        std::printf("[gauge] %s failed %s\n", request.nif_path.c_str(), error.c_str());
        return -1;
    }
    KnC::Tools::resolve_textures(request.texture_dir, model);
    // the hud scene carries its own flat light the map ambient never reaches the gauge art
    model.lit_by_map_ambient = false;
    return static_cast<int>(renderer.append_prop_model(model));
}

}

bool GaugeScene::load(KnC::Render::SceneRenderer& renderer, const std::string& gameDir, bool teamMode) {
    m_loaded = false;
    m_team = teamMode;
    m_sparkSeconds = 0.f;
    m_red = loadOne(renderer, gameDir, kRedGauge);
    m_blue = teamMode ? loadOne(renderer, gameDir, kBlueGauge) : -1;
    m_spark = loadOne(renderer, gameDir, kSpark);
    m_normalBoost = loadOne(renderer, gameDir, kNormalBoost);
    m_redBoost = loadOne(renderer, gameDir, kRedBoost);
    m_blueBoost = loadOne(renderer, gameDir, kBlueBoost);
    m_loaded = m_red >= 0;
    std::printf("[gauge] red %d blue %d spark %d boost %d %d %d\n", m_red, m_blue, m_spark, m_normalBoost, m_redBoost,
                m_blueBoost);
    return m_loaded;
}

void GaugeScene::draw(KnC::Render::SceneRenderer& renderer, const HudState& state, float dt, uint16_t fbW,
                      uint16_t fbH, float canvasW, float canvasH, bool stretch) {
    if (!m_loaded || state.driftGauge < 0.f) return;
    KnC::Render::HudScene scene;
    // same canvas to frame mapping as the sprite batch so the band sits on its frame art
    const float fit = std::min(static_cast<float>(fbW) / canvasW, static_cast<float>(fbH) / canvasH);
    const float scaleX = stretch ? static_cast<float>(fbW) / canvasW : fit;
    const float scaleY = stretch ? static_cast<float>(fbH) / canvasH : fit;
    const float offsetX = std::floor((static_cast<float>(fbW) - canvasW * scaleX) * 0.5f);
    const float offsetY = std::floor((static_cast<float>(fbH) - canvasH * scaleY) * 0.5f);
    const float left = offsetX + (canvasW - kRectLeft) * scaleX;
    const float top = offsetY + (canvasH - kRectTop) * scaleY;
    scene.x = static_cast<uint16_t>(std::max(0.f, std::floor(left)));
    scene.y = static_cast<uint16_t>(std::max(0.f, std::floor(top)));
    scene.width = static_cast<uint16_t>(std::min(std::floor(kRectSize * scaleX), static_cast<float>(fbW) - left));
    scene.height = static_cast<uint16_t>(std::min(std::floor(kRectSize * scaleY), static_cast<float>(fbH) - top));
    bx::mtxLookAt(scene.view, bx::Vec3(kEye[0], kEye[1], kEye[2]), bx::Vec3(kLook[0], kLook[1], kLook[2]),
                  bx::Vec3(kUp[0], kUp[1], kUp[2]), bx::Handedness::Right);
    for (int i = 0; i < 3; ++i) scene.eye[i] = kEye[i];
    scene.fov_vertical = kFov;
    scene.fov_horizontal = kFov;
    scene.near_plane = kNear;
    scene.far_plane = kFar;
    scene.ambient = KnC::Render::HourColour{1.f, 1.f, 1.f};
    auto place = [&](int model, const float world[16]) {
        if (model < 0) return;
        KnC::Render::PropInstance instance;
        instance.model_index = static_cast<size_t>(model);
        if (world != nullptr)
            for (int i = 0; i < 16; ++i) instance.world[i] = world[i];
        scene.instances.push_back(instance);
    };
    // the two bands posed at their fill the blue one only in the team modes
    renderer.pose_prop_model_at(static_cast<size_t>(m_red), state.driftGauge * kFillToSeconds);
    place(m_red, nullptr);
    if (m_blue >= 0 && state.driftGaugeBlue >= 0.f) {
        renderer.pose_prop_model_at(static_cast<size_t>(m_blue), state.driftGaugeBlue * kFillToSeconds);
        place(m_blue, nullptr);
    }
    // this 0x85C the spark at the O POS dummy of the band while the charge runs
    m_sparkSeconds += dt;
    if (m_sparkSeconds > kSparkWrap) m_sparkSeconds = 0.f;
    if (state.driftGaugeSpark && m_spark >= 0) {
        float at[16];
        if (renderer.prop_node_matrix(static_cast<size_t>(m_red), kSparkNode, at)) {
            float world[16];
            bx::mtxTranslate(world, at[12], at[13], at[14]);
            renderer.pose_prop_model_at(static_cast<size_t>(m_spark), m_sparkSeconds);
            place(m_spark, world);
        }
    }
    // boost get sheets play 1 7 s from full band plain one stands the rest of time
    if (state.gaugeFlash == 1 && m_redBoost >= 0) {
        renderer.pose_prop_model_at(static_cast<size_t>(m_redBoost), static_cast<float>(state.gaugeFlashAge));
        place(m_redBoost, nullptr);
    } else if (state.gaugeFlash == 2 && m_blueBoost >= 0) {
        renderer.pose_prop_model_at(static_cast<size_t>(m_blueBoost), static_cast<float>(state.gaugeFlashAge));
        place(m_blueBoost, nullptr);
    } else if (m_normalBoost >= 0) {
        renderer.pose_prop_model_at(static_cast<size_t>(m_normalBoost), 0.f);
        place(m_normalBoost, nullptr);
    }
    renderer.draw_hud_scene(scene);
}

}
