#include "LensFlare.h"

#include "RaceView.h"
#include "engine/render/nif_prop_model.h"

#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace KnC::Client {

namespace {

// 0x4D2B99 the size of each lens then its place along the flare line
constexpr float kLensSize[6] = {2.0f, 1.6f, 1.0f, 1.0f, 2.0f, 1.5f};
constexpr float kLensAlong[6] = {0.2f, 0.4f, 0.8f, 1.2f, 1.4f, 1.6f};
// 0x5A3298 the line runs from 20 units out on the sun ray to 20 units out on the view ray
constexpr float kReach = 20.f;
// 0x5A322C a lens stands at the sun end plus half the line times its place times 0 8
constexpr float kAlongScale = 0.8f;
// 0x5A15EC and 0x5A32B0 a lens is its size times 0 2 wide and 1 2 of that high
constexpr float kSizeScale = 0.2f;
constexpr float kHeightScale = 1.2f;
// 0x5A1648 and 0x5A7AA8 strength is 100 minus the squared half line times 0 0125
constexpr float kStrengthBase = 100.f;
constexpr float kStrengthScale = 0.0125f;
// 0x5A164C 0x5A7AB0 and 0x5A6A68 the glare starts past 0 6 at 150 per unit capped at 60
constexpr float kGlareFrom = 0.6f;
constexpr float kGlarePerUnit = 150.f;
constexpr float kGlareCap = 60.f;

bool normalised(float v[3]) {
    const float length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length < 1e-4f) return false;
    for (int axis = 0; axis < 3; ++axis) v[axis] /= length;
    return true;
}

}

bool LensFlare::load(RaceView& view, KnC::Render::SceneRenderer& renderer, const std::string& gameDir,
                     const float point[3]) {
    m_loaded = false;
    m_strength = 0.f;
    for (int& prop : m_props) prop = -1;
    if (point[2] < 0.f) return false;
    namespace fs = std::filesystem;
    for (int lens = 0; lens < kLenses; ++lens) {
        const fs::path nif = fs::path(gameDir) / "Data" / "Public" / "Effect" / ("LENS" + std::to_string(lens + 1) + ".nif");
        KnC::Render::NifModelRequest request;
        request.nif_path = nif.string();
        request.texture_dir = nif.parent_path().string();
        KnC::Render::PropModel model;
        std::string error;
        if (!KnC::Render::load_prop_model(request, model, error)) {
            std::printf("[weather] %s failed %s\n", request.nif_path.c_str(), error.c_str());
            return false;
        }
        KnC::Tools::resolve_textures(request.texture_dir, model);
        m_props[lens] = view.addProp(renderer, model);
    }
    for (int axis = 0; axis < 3; ++axis) m_point[axis] = point[axis];
    m_loaded = true;
    std::printf("[weather] lens flare aimed at %.1f %.1f %.1f\n", m_point[0], m_point[1], m_point[2]);
    return true;
}

// the stock turns each sheet by a RotationY of the view the lens nodes are billboards so the facing wins
void LensFlare::update(RaceView& view) {
    if (!m_loaded) return;
    const ChaseCamera& camera = view.camera();
    if (!camera.valid) return;
    float toView[3], toSun[3];
    for (int axis = 0; axis < 3; ++axis) {
        toView[axis] = camera.look[axis] - camera.eye[axis];
        toSun[axis] = m_point[axis] - camera.eye[axis];
    }
    if (!normalised(toView) || !normalised(toSun)) return;
    float sunEnd[3], half[3];
    for (int axis = 0; axis < 3; ++axis) {
        sunEnd[axis] = camera.eye[axis] + toSun[axis] * kReach;
        half[axis] = (toView[axis] - toSun[axis]) * kReach * 0.5f;
    }
    const float squared = half[0] * half[0] + half[1] * half[1] + half[2] * half[2];
    m_strength = std::clamp((kStrengthBase - squared) * kStrengthScale, 0.f, 1.f);
    const bool shown = m_strength > 0.f;
    for (int lens = 0; lens < kLenses; ++lens) {
        const float size = kLensSize[lens] * kSizeScale;
        float world[16];
        bx::mtxScale(world, size, size * kHeightScale, size);
        for (int axis = 0; axis < 3; ++axis)
            world[12 + axis] = sunEnd[axis] + half[axis] * kLensAlong[lens] * kAlongScale;
        view.placeProp(m_props[lens], world, shown);
    }
}

void LensFlare::hide(RaceView& view) {
    float rest[16];
    bx::mtxIdentity(rest);
    for (int prop : m_props) view.placeProp(prop, rest, false);
    m_strength = 0.f;
}

uint8_t LensFlare::glare() const {
    if (!m_loaded || m_strength <= kGlareFrom) return 0;
    return static_cast<uint8_t>(std::clamp((m_strength - kGlareFrom) * kGlarePerUnit, 0.f, kGlareCap));
}

}
