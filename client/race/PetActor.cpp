#include "PetActor.h"

#include "TrackData.h"
#include "engine/render/scene_renderer.h"
#include "tools/track_scene/ghost_car.h"

#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace KnC::Client {

namespace {

// 0x48B800 and sub 4A51B0 the pet stands minus 1 along x 0x5A32C8 across 0x5A68CC up
constexpr float kPetBack = 1.f;
constexpr float kPetAcross = 0.9f;
constexpr float kPetUp = 1.6f;
// the race trail grows by speed times 0x5A68D8 a frame over 100 capped 0x5A68D4 in 5 s windows
constexpr float kTrailPerSpeed = 0.0005f;
constexpr float kTrailCap = 3.6f;
constexpr float kTrailSpeedGate = 100.f;
constexpr float kTrailSlowGate = 30.f;
constexpr float kTrailSlowStep = 0.1f;
constexpr double kTrailSeconds = 5.0;
// the side wander steps 0x5A68D0 a frame inside the race or the stand bounds or holds 3 s
constexpr float kSideStep = 0.03f;
constexpr float kRaceSideLow = -2.5f;
constexpr float kRaceSideHigh = 1.6f;
constexpr float kStandSideLow = -2.f;
constexpr float kStandSideHigh = 1.1f;
constexpr double kSideHold = 3.001;
// the bob steps 0x5A05E0 a frame between 0x5A68C4 and one
constexpr float kBobStep = 0.01f;
constexpr float kBobLow = -0.5f;
constexpr float kBobHigh = 1.f;
// minus a quarter in D3DX is plus a quarter in bx
constexpr float kPetTurnRadians = 1.5707963f;
constexpr float kStockFrameHz = 60.f;

}

PetFiles petFiles(const std::string& gameDir, const std::string& modelFolder) {
    PetFiles out;
    if (modelFolder.empty()) return out;
    // driver manager load driver 0x48CE97 builds Pet Body folder body and its kfm beside it
    const std::string root = gameDir + "/Data/Public/Pet/";
    const std::string folder = findEntryCi(root + "Body", modelFolder);
    if (!folder.empty()) out.nif = findEntryCi(folder, "body.nif");
    out.facialDir = findEntryCi(root + "Facial", modelFolder);
    return out;
}

void PetActor::reset(PetHoverKind kind) {
    *this = PetActor();
    m_kind = kind;
}

void PetActor::follow(int sequenceId, float speed) {
    m_sequence = sequenceId;
    m_speed = std::fabs(speed);
}

void PetActor::hover(float frames, double clockSeconds) {
    // the stand of sub 4A51B0 keeps the pet one unit back only the race trails it
    if (m_kind == PetHoverKind::Race) {
        if (m_trailState == 0) {
            m_trailState = 1;
            m_trailAt = clockSeconds;
        } else if (m_trailState == 1) {
            if (m_speed >= kTrailSpeedGate) {
                m_trail = std::min(kTrailCap, m_trail + m_speed * kTrailPerSpeed * frames);
                if (clockSeconds - m_trailAt >= kTrailSeconds) { m_trailState = 2; m_trailAt = clockSeconds; }
            } else {
                m_trailState = 2;
                m_trailAt = clockSeconds;
            }
        } else {
            const float step = m_speed >= kTrailSlowGate ? m_speed * kTrailPerSpeed : kTrailSlowStep;
            m_trail = std::max(0.f, m_trail - step * frames);
            if (clockSeconds - m_trailAt > kTrailSeconds) { m_trailState = 1; m_trailAt = clockSeconds; }
        }
    }
    const float low = m_kind == PetHoverKind::Race ? kRaceSideLow : kStandSideLow;
    const float high = m_kind == PetHoverKind::Race ? kRaceSideHigh : kStandSideHigh;
    if (m_sideState == 0) {
        m_sideState = std::rand() & 3;
        if (m_sideState == 3) m_sideAt = clockSeconds;
    }
    if (m_sideState == 1) {
        m_side += kSideStep * frames;
        if (m_side > high) { m_side = high; m_sideState = 0; }
    } else if (m_sideState == 2) {
        m_side -= kSideStep * frames;
        if (m_side < low) { m_side = low; m_sideState = 0; }
    } else if (m_sideState == 3) {
        if (clockSeconds - m_sideAt >= kSideHold) m_sideState = 0;
    }
    if (m_bobState == 0) {
        m_bob += kBobStep * frames;
        if (m_bob > kBobHigh) { m_bob = kBobHigh; m_bobState = 1; }
    } else {
        m_bob -= kBobStep * frames;
        if (m_bob < kBobLow) { m_bob = kBobLow; m_bobState = 0; }
    }
}

void PetActor::place(const KnC::Tools::GhostDriver& body, std::size_t modelIndex, const float seat[3],
                     const float carWorld[16], float clockSeconds, KnC::Render::CharacterInstance& out) {
    const float dt = m_lastClock < 0.f ? 0.f : std::max(0.f, clockSeconds - m_lastClock);
    m_lastClock = clockSeconds;
    hover(dt * kStockFrameHz, clockSeconds);
    // 0x443F20 a KFM without the driver id keeps the clip it plays
    int wanted = body.clip_of(m_sequence);
    if (wanted < 0) wanted = m_clip >= 0 ? m_clip : body.idle_clip;
    if (wanted != m_clip) {
        m_clip = wanted;
        m_clipStart = clockSeconds;
    }
    float turn[16], lift[16], local[16], world[16];
    bx::mtxRotateZ(turn, kPetTurnRadians);
    bx::mtxTranslate(lift, seat[0] - kPetBack + m_trail, seat[1] + kPetAcross + m_side, seat[2] + kPetUp + m_bob);
    bx::mtxMul(local, turn, lift);
    bx::mtxMul(world, local, carWorld);
    KnC::Tools::ghost_driver_instance(body, modelIndex, world, m_clip, std::max(0.f, clockSeconds - m_clipStart), out);
}

}
