#include "PodiumScene.h"

#include "engine/render/nif_prop_model.h"
#include "engine/render/scene_renderer.h"

#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace KnC::Client {

namespace {

constexpr float kDegToRad = 3.14159265f / 180.f;

// FUN 0048A120 winer 01 is team podium winer 02 is single under Data Public
const char* kSinglePodium = "Effect/Podium/winer_02.nif";
const char* kTeamPodium = "Effect/Podium/winer_01.nif";

// podium yaw is mean start heading minus 90 object turns by minus yaw plus 180
constexpr float kPodiumYawOffset = -90.f;
constexpr float kObjectYawTurn = 180.f;
// 0x5A66F0 170 minus 90 karts face mean heading minus 10
constexpr float kKartYawOffset = 170.f - 90.f;

// world 0x92F8 drop heights per rank fifth serves every rank past three
constexpr float kSingleDrop[5] = {60.f, 80.f, 100.f, 120.f, 120.f};
constexpr float kTeamDrop[5] = {80.f, 80.f, 80.f, 80.f, 100.f};
// FUN 00401D90 state 2030 waits 0x5DC then lowers five heights by 0x59F450 per second
constexpr float kDropDelay = 1.5f;
constexpr float kDropSpeed = 50.f;

// FUN 0048A3E0 seats off podium in stock world axes x forward y across z up
constexpr float kSingleSeat[3][3] = {{0.084f, 0.023f, 6.53f}, {0.084f, 6.14f, 4.68f}, {0.084f, -6.14f, 3.74f}};
constexpr float kTeamSeat[4][3] = {{0.084f, -6.6f, 2.8f}, {0.084f, -2.2f, 2.8f}, {0.084f, 2.2f, 2.8f}, {0.084f, 6.6f, 2.8f}};
// ranks past three stand behind at minus 6 across four spots half unit up
constexpr float kOthersX = -6.f;
constexpr float kOthersY[4] = {6.936f, 2.339f, -2.258f, -6.855f};
constexpr float kOthersZ = 0.5f;

// camera update mode 8 eye and look off podium 0x5A3264 0x5A3260 0x5A325C 0x5A3258 0x5A3254 0x5A15F0
constexpr float kEyeOffset[3] = {-35.4f, 10.3f, 9.f};
constexpr float kLookOffset[3] = {-15.7f, 4.7f, 8.f};
// camera fov update 0x43E850 mode 8 takes 0 65 radians
constexpr float kPodiumFov = 0.65f;

// FUN 00486DF0 stage clock in seconds since load 0x641 0xA8D 0xC1D 0xDAD 0xF3D 0xE75
constexpr float kStageAt[6] = {1.601f, 2.701f, 3.101f, 3.501f, 3.901f, 3.701f};

}

void PodiumScene::turned(float dx, float dy, float out[2]) const {
    // same turn as car matrix heading row 0 is cos minus sin row 1 is sin cos
    const float h = m_headingDeg * kDegToRad;
    out[0] = dx * std::cos(h) + dy * std::sin(h);
    out[1] = -dx * std::sin(h) + dy * std::cos(h);
}

bool PodiumScene::load(RaceView& view, KnC::Render::SceneRenderer& renderer, const std::string& gameDir,
                       const RaceWorld& world, bool teamMode, int finisherCount) {
    m_loaded = false;
    m_team = teamMode;
    m_finishers = finisherCount;
    m_stage = 0;
    m_seconds = 0.f;
    const auto& rows = world.scene.start_rows;
    if (rows.size() < 2) {
        std::printf("[podium] the track has %zu start rows the podium needs two\n", rows.size());
        return false;
    }
    // FUN 0048A120 midpoint of first and last row mean heading minus 90
    const auto& first = rows.front();
    const auto& last = rows.back();
    m_pos[0] = (first.x + last.x) * 0.5f;
    m_pos[1] = (first.y + last.y) * 0.5f;
    m_pos[2] = (first.z + last.z) * 0.5f;
    m_headingDeg = (first.heading + last.heading) * 0.5f;
    m_yawDeg = m_headingDeg + kPodiumYawOffset;
    for (int i = 0; i < 5; ++i) m_drop[i] = teamMode ? kTeamDrop[i] : kSingleDrop[i];

    namespace fs = std::filesystem;
    const fs::path nif = fs::path(gameDir) / "Data" / "Public" / (teamMode ? kTeamPodium : kSinglePodium);
    KnC::Render::NifModelRequest request;
    request.nif_path = nif.string();
    request.texture_dir = nif.parent_path().string();
    request.play_stopped_controllers = true;
    KnC::Render::PropModel model;
    std::string error;
    if (!KnC::Render::load_prop_model(request, model, error)) {
        std::printf("[podium] %s failed %s\n", request.nif_path.c_str(), error.c_str());
        return false;
    }
    KnC::Tools::resolve_textures(request.texture_dir, model);
    // WIN sign and wings fly in on animated nodes ribbons and confetti sit beside
    size_t animated = 0;
    for (const KnC::Render::PropPart& part : model.parts) animated += part.animation.node >= 0 ? 1 : 0;
    m_propHandle = view.addProp(renderer, model);
    // FUN 00444790 rotate is RotationZ of minus yaw plus 180 then bx places by that
    float rotate[16], translate[16], worldMatrix[16];
    bx::mtxRotateZ(rotate, (m_yawDeg + kObjectYawTurn) * kDegToRad);
    bx::mtxTranslate(translate, m_pos[0], m_pos[1], m_pos[2]);
    bx::mtxMul(worldMatrix, rotate, translate);
    view.placeProp(m_propHandle, worldMatrix, true);
    std::printf("[podium] %s at %.1f %.1f %.1f yaw %.1f parts %zu animated %zu particles %zu finishers %d\n",
                nif.string().c_str(), m_pos[0], m_pos[1], m_pos[2], m_yawDeg, model.parts.size(), animated,
                model.particle_systems.size(), finisherCount);
    m_loaded = true;
    return true;
}

bool PodiumScene::seat(int rank, int slot, CarPose& pose) const {
    if (!m_loaded || rank < 0) return false;
    float dx = 0.f, dy = 0.f, dz = 0.f;
    int dropIndex = rank < 3 ? rank : 4;
    if (m_team) {
        if (rank > 3) return false;
        dx = kTeamSeat[rank][0]; dy = kTeamSeat[rank][1]; dz = kTeamSeat[rank][2];
        dropIndex = rank < 3 ? rank : 4;
    } else if (rank < 3) {
        dx = kSingleSeat[rank][0]; dy = kSingleSeat[rank][1]; dz = kSingleSeat[rank][2];
    } else {
        // stock keeps four spots behind podium fifth and later racer stays put
        if (slot < 0 || slot > 3) return false;
        dx = kOthersX; dy = kOthersY[slot]; dz = kOthersZ;
    }
    float across[2];
    turned(dx, dy, across);
    pose.x = m_pos[0] + across[0];
    pose.y = m_pos[1] + across[1];
    pose.z = m_pos[2] + dz + m_drop[dropIndex];
    pose.yawDeg = m_yawDeg + kKartYawOffset;
    pose.speed = 0.f;
    pose.speedKmh = 0.f;
    pose.hasBody = false;
    pose.hasWheels = false;
    pose.driftSlipDeg = 0.f;
    pose.driftGauge = 0.f;
    pose.driftState = 0;
    pose.boosting = false;
    pose.reversing = false;
    pose.turnState = 0;
    pose.steerAverage = 0.f;
    return true;
}

void PodiumScene::update(float dt, std::vector<PodiumEvent>& out) {
    if (!m_loaded) return;
    m_seconds += dt;
    // state 2030 of FUN 00401D90 heights fall once wait is over
    if (m_seconds > kDropDelay) {
        for (float& height : m_drop) {
            height -= dt * kDropSpeed;
            if (height < 0.f) height = 0.f;
        }
    }
    // stage chain of FUN 00486DF0 a stage past finisher count jumps to win clips
    bool advanced = true;
    while (advanced) {
        advanced = false;
        switch (m_stage) {
        case 0:
            if (m_seconds < kStageAt[0]) break;
            out.push_back(PodiumEvent::DropStart);
            m_stage = m_team ? 2 : (m_finishers >= 1 ? 1 : 4);
            advanced = true;
            break;
        case 1:
            if (m_seconds < kStageAt[1]) break;
            out.push_back(PodiumEvent::Landing);
            m_stage = m_finishers >= 2 ? 2 : 4;
            advanced = true;
            break;
        case 2:
            if (m_seconds < kStageAt[2]) break;
            out.push_back(PodiumEvent::Landing);
            m_stage = m_team ? 4 : (m_finishers >= 3 ? 3 : 4);
            advanced = true;
            break;
        case 3:
            if (m_seconds < kStageAt[3]) break;
            out.push_back(PodiumEvent::Landing);
            m_stage = 4;
            advanced = true;
            break;
        case 4:
            out.push_back(PodiumEvent::WinClips);
            if (m_finishers > 3) {
                if (m_seconds < kStageAt[4]) { m_stage = 40; break; }
                out.push_back(PodiumEvent::OthersLanding);
            }
            m_stage = 5;
            advanced = true;
            break;
        case 40:
            // win clips played others still fall stage waits for their thump
            if (m_seconds < kStageAt[4]) break;
            out.push_back(PodiumEvent::OthersLanding);
            m_stage = 5;
            advanced = true;
            break;
        case 5:
            if (m_seconds < kStageAt[5]) break;
            out.push_back(PodiumEvent::Ceremony);
            m_stage = 6;
            break;
        default:
            break;
        }
    }
}

void PodiumScene::camera(float eye[3], float look[3], float& fovRadians) const {
    float across[2];
    turned(kEyeOffset[0], kEyeOffset[1], across);
    eye[0] = m_pos[0] + across[0];
    eye[1] = m_pos[1] + across[1];
    eye[2] = m_pos[2] + kEyeOffset[2];
    turned(kLookOffset[0], kLookOffset[1], across);
    look[0] = m_pos[0] + across[0];
    look[1] = m_pos[1] + across[1];
    look[2] = m_pos[2] + kLookOffset[2];
    fovRadians = kPodiumFov;
}

}
