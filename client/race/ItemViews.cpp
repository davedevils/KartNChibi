#include "ItemViews.h"

#include "RaceItems.h"

#include "engine/render/scene_renderer.h"
#include "games/kart/physics/client/boost.h"
#include "games/kart/physics/client/math_helpers.h"

#include <bx/math.h>

#include <algorithm>
#include <cmath>

namespace KnC::Client {

using KnC::Kart::Client::Vec3;
using KnC::Kart::Client::math_atan2_deg;
using KnC::Kart::Client::math_dir_from_heading_pitch;

namespace {

constexpr float kStockFrame = 1.f / 60.f;
constexpr float kLookEase = 0.25f;
// sub 4A9EA0 the attack rect 732 0 1024 245 fov 0 72 aspect 4 by 3 3000 ms
constexpr float kAttackRect[4] = {732.f, 0.f, 292.f, 245.f};
constexpr float kAttackFov = 0.72f;
constexpr float kAttackAspect = 4.f / 3.f;
constexpr float kAttackFar = 10000.f;
constexpr double kAttackMs = 3000.0;
constexpr float kAttackDistance = 15.f;
constexpr float kAttackEyeLift = 1.8f;
constexpr float kAttackLookLift = 0.4f;
constexpr float kAttackPitch = 12.f;
// sub 4AD3C0 the event rect 762 331 928 482 sub 4AB570 sets fov 1 02 far 1000
constexpr float kEventRect[4] = {762.f, 331.f, 166.f, 151.f};
constexpr float kEventFov = 1.02f;
constexpr float kEventAspect = 1.0993f;
constexpr float kEventFar = 1000.f;
constexpr double kEventCarMs = 5000.0;
constexpr float kEventCarLookLift = 0.6f;
// sub 4AB880 mode 4 the eye turns off the line from the reference car to the item by the turn
struct Shot {
    int kind;
    float turn;
    float pitch;
    float distance;
    float eyeLift;
    float lookLift;
    double ms;
};
constexpr Shot kShots[] = {
    {kItemSpike, 180.f, 12.f, 24.f, 2.4f, 2.f, 5000.0},
    {kItemHive, 180.f, 12.f, 24.f, 2.4f, 2.f, 5000.0},
    {kItemIce, 180.f, 12.f, 30.f, 2.4f, 1.f, 5000.0},
    {kItemFlash, 180.f, 10.f, 40.f, 2.f, 8.f, 1500.0},
    {kItemThunder, 210.f, 3.f, 16.f, 3.f, 2.f, 6000.0},
    {kItemHammer, 210.f, 3.f, 16.f, 3.f, 2.f, 6000.0},
};
// sub 4AB880 kind 3 the storm eye 40 out at 40 degrees for 12000 ms
constexpr float kStormPitch = 40.f;
constexpr float kStormDistance = 40.f;
constexpr float kStormEyeLift = 2.f;
constexpr float kStormLookLift = 8.f;
constexpr double kStormMs = 12000.0;

void eyeFrom(const float at[3], float heading, float pitch, float distance, float lift, float out[3]) {
    Vec3 dir;
    math_dir_from_heading_pitch(heading, pitch, dir);
    out[0] = at[0] + dir.x * distance;
    out[1] = at[1] + dir.y * distance;
    out[2] = at[2] + dir.z * distance + lift;
}

KnC::Render::HudScene sceneIn(const float rect[4], uint16_t fbW, uint16_t fbH, float canvasW, float canvasH) {
    KnC::Render::HudScene scene;
    const float scale = std::min(static_cast<float>(fbW) / canvasW, static_cast<float>(fbH) / canvasH);
    const float offsetX = std::floor((static_cast<float>(fbW) - canvasW * scale) * 0.5f);
    const float offsetY = std::floor((static_cast<float>(fbH) - canvasH * scale) * 0.5f);
    const float left = offsetX + rect[0] * scale;
    const float top = offsetY + rect[1] * scale;
    scene.x = static_cast<uint16_t>(std::max(0.f, std::floor(left)));
    scene.y = static_cast<uint16_t>(std::max(0.f, std::floor(top)));
    scene.width = static_cast<uint16_t>(std::max(0.f, std::min(std::floor(rect[2] * scale), static_cast<float>(fbW) - left)));
    scene.height = static_cast<uint16_t>(std::max(0.f, std::min(std::floor(rect[3] * scale), static_cast<float>(fbH) - top)));
    return scene;
}

}

void ItemViews::reset() {
    m_attack = View{};
    m_event = View{};
    m_now = 0.0;
}

void ItemViews::startAttack(int carIndex, const CarPose& pose) {
    if (m_attack.active || carIndex < 0) return;
    m_attack = View{};
    m_attack.active = true;
    m_attack.car = carIndex;
    m_attack.startedAt = m_now;
    m_attack.heading = pose.yawDeg - 90.f;
    m_attack.pitch = pose.pitchDeg + kAttackPitch;
}

void ItemViews::startEvent(int mode, int a, int b) {
    if (m_event.active) return;
    m_event = View{};
    m_event.active = true;
    m_event.mode = mode;
    m_event.a = a;
    m_event.b = b;
    m_event.startedAt = m_now;
}

void ItemViews::ease(View& v, const float look[3], float dt) const {
    if (v.first) {
        for (int i = 0; i < 3; ++i) v.look[i] = look[i];
        v.first = false;
        return;
    }
    const float k = 1.f - std::pow(1.f - kLookEase, dt / kStockFrame);
    for (int i = 0; i < 3; ++i) v.look[i] += (look[i] - v.look[i]) * k;
}

void ItemViews::tick(View& v, float dt) const {
    v.frameClock += dt;
    if (v.frameClock >= 2.f * kStockFrame) {
        v.frameClock = 0.f;
        v.frame = v.frame == 0 ? 1 : 0;
    }
}

void ItemViews::update(float dt, const RaceSim& sim, const RaceItems& items) {
    m_now += dt;
    if (m_attack.active) {
        const CarPose p = sim.pose(m_attack.car);
        const float at[3] = {p.x, p.y, p.z};
        eyeFrom(at, m_attack.heading, m_attack.pitch, kAttackDistance, kAttackEyeLift, m_attack.eye);
        const float look[3] = {p.x, p.y, p.z + kAttackLookLift};
        ease(m_attack, look, dt);
        tick(m_attack, dt);
        if ((m_now - m_attack.startedAt) * 1000.0 > kAttackMs) m_attack.active = false;
    }
    if (!m_event.active) return;
    const double ms = (m_now - m_event.startedAt) * 1000.0;
    tick(m_event, dt);
    if (m_event.mode == 3) {
        // mode 3 the eye 15 in front of the car 12 degrees over its pitch 5000 ms
        const CarPose p = sim.pose(m_event.a);
        const float at[3] = {p.x, p.y, p.z};
        eyeFrom(at, p.yawDeg - 90.f, p.pitchDeg + kAttackPitch, kAttackDistance, kAttackEyeLift, m_event.eye);
        const float look[3] = {p.x, p.y, p.z + kEventCarLookLift};
        ease(m_event, look, dt);
        if (ms > kEventCarMs) m_event.active = false;
        return;
    }
    if (m_event.mode != 4) {
        m_event.active = false;
        return;
    }
    const int kind = m_event.a;
    const int slot = m_event.b;
    int owner = -1, victim = -1;
    float item[3];
    if (!items.slotCars(kind, slot, owner, victim) || !items.slotPosition(kind, slot, item)) {
        m_event.active = false;
        return;
    }
    if (kind == kItemStorm) {
        const CarPose o = sim.pose(owner);
        const float want = math_atan2_deg(item[0] - o.x, item[1] - o.y);
        if (m_event.first) m_event.heading = want;
        float turn = want - m_event.heading;
        while (turn > 180.f) turn -= 360.f;
        while (turn < -180.f) turn += 360.f;
        m_event.heading += turn * (1.f - std::pow(1.f - 1.f / 128.f, dt / kStockFrame));
        eyeFrom(item, m_event.heading + 90.f, kStormPitch, kStormDistance, kStormEyeLift, m_event.eye);
        const float look[3] = {item[0], item[1], item[2] + kStormLookLift};
        ease(m_event, look, dt);
        if (ms > kStormMs) m_event.active = false;
        return;
    }
    for (const Shot& shot : kShots) {
        if (shot.kind != kind) continue;
        float subject[3] = {item[0], item[1], item[2]};
        float reference[3];
        const CarPose o = sim.pose(owner);
        reference[0] = o.x; reference[1] = o.y; reference[2] = o.z;
        if (kind == kItemThunder || kind == kItemHammer) {
            if (victim < 0) { m_event.active = false; return; }
            const CarPose v = sim.pose(victim);
            subject[0] = v.x; subject[1] = v.y; subject[2] = v.z;
        }
        const float heading = math_atan2_deg(subject[0] - reference[0], subject[1] - reference[1]) + shot.turn;
        eyeFrom(subject, heading, shot.pitch, shot.distance, shot.eyeLift, m_event.eye);
        const float look[3] = {subject[0], subject[1], subject[2] + shot.lookLift};
        ease(m_event, look, dt);
        if (ms > shot.ms) m_event.active = false;
        return;
    }
    m_event.active = false;
}

void ItemViews::draw(KnC::Render::SceneRenderer& renderer, uint16_t fbW, uint16_t fbH, float canvasW,
                     float canvasH) const {
    auto render = [&](const View& v, const float rect[4], float fov, float aspect, float far) {
        KnC::Render::HudScene scene = sceneIn(rect, fbW, fbH, canvasW, canvasH);
        bx::mtxLookAt(scene.view, bx::Vec3(v.eye[0], v.eye[1], v.eye[2]), bx::Vec3(v.look[0], v.look[1], v.look[2]),
                      bx::Vec3(0.f, 0.f, 1.f), bx::Handedness::Right);
        for (int i = 0; i < 3; ++i) scene.eye[i] = v.eye[i];
        scene.fov_vertical = fov;
        scene.fov_horizontal = 2.f * std::atan(std::tan(fov * 0.5f) * aspect);
        scene.far_plane = far;
        renderer.draw_world_inset(scene);
    };
    if (m_attack.active && !m_attack.first) render(m_attack, kAttackRect, kAttackFov, kAttackAspect, kAttackFar);
    if (m_event.active && !m_event.first) render(m_event, kEventRect, kEventFov, kEventAspect, kEventFar);
}

}
