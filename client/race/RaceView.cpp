#include "RaceView.h"

#include "RaceEffects.h"
#include "engine/render/nif_prop_model.h"
#include "engine/render/scene_renderer.h"
#include "tools/replay/ghost_replay.h"

#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace KnC::Client {

using namespace KnC::Render;
using namespace KnC::Tools;

namespace {

constexpr float kDegToRad = 3.14159265f / 180.f;
constexpr float kRadToDeg = 180.f / 3.14159265f;

// camera update 0x43F040 mode 1 heading eases a quarter per frame 0x5A32D4 a thirty second before green
constexpr float kCamHeadingBlend = 0.25f;
constexpr float kCamHeadingBlendStart = 0.03125f;
// 0x5A2494 and 0x5A32D8 heading moves only past a tenth of a degree
constexpr float kCamHeadingDeadzone = 0.1f;
// 0x5A164C plus zero global 0x2EB0238 then 0x5A15EC times one minus zero 0x2EB0228
constexpr float kCamDriftSwing = 0.6f + 0.2f;
// 0x43E850 field of view 0x5A3218 base 0x5A3214 cap 0x5A321C per speed unit 0x5A3220 zoom 0x5A2494 offset
constexpr float kCamFovBase = 1.05f;
constexpr float kCamFovMax = 1.8f;
constexpr float kCamFovPerSpeed = 0.003f;
constexpr float kCamFovZoomScale = 0.15f;
constexpr float kCamFovOffset = 0.1f;
// camera 0x1F0 boost zoom stays zero in this build no writer sets its state 1
constexpr float kCamZoom = 0.f;
// 0x5A32CC distance loses 12 per radian of field 0x5A32C8 then nine tenths 0x5A164C zoom share
constexpr float kCamDistanceFovScale = 12.f;
constexpr float kCamDistanceScale = 0.9f;
constexpr float kCamDistanceZoomScale = 0.6f;
// 0x5A18B0 car pitch times 2 5 comes off camera pitch capped at 0x5A32C4 35 degrees
constexpr float kCamPitchPerCarPitch = 2.5f;
constexpr float kCamPitchTermCap = 35.f;
// row stat 16 of every shipped row look point height above car
constexpr float kCamLookHeightDefault = 3.5f;
// eye eases by a half and its height by an eighth per frame before green
constexpr float kCamStartEaseXY = 2.f;
constexpr float kCamStartEaseZ = 8.f;
// stock ran camera once per rendered frame at 60 frames a second
constexpr float kStockFrameSeconds = 1.f / 60.f;
// camera init 0x4408D0 frustum right edge is tan of fov times this aspect term over 2
constexpr float kStockAspectPivot = 1.7780f;
constexpr float kStockAspectBase = 1.35f;
// driver place on car 0x48B800 pet stands minus 1 along x 0x5A32C8 across 0x5A68CC up
constexpr float kPetBack = 1.f;
constexpr float kPetAcross = 0.9f;
constexpr float kPetUp = 1.6f;
// trail grows by speed times 0x5A68D8 a frame over 100 a second capped 0x5A68D4
constexpr float kPetTrailPerSpeed = 0.0005f;
constexpr float kPetTrailCap = 3.6f;
constexpr float kPetTrailSpeedGate = 100.f;
constexpr float kPetTrailSlowGate = 30.f;
constexpr float kPetTrailSlowStep = 0.1f;
constexpr double kPetTrailSeconds = 5.0;
// side wander steps 0x5A68D0 a frame between 0x5A68C8 and 0x5A68CC or holds 3 s
constexpr float kPetSideStep = 0.03f;
constexpr float kPetSideLow = -2.5f;
constexpr float kPetSideHigh = 1.6f;
constexpr double kPetSideHold = 3.001;
// bob steps 0x5A05E0 a frame between 0x5A68C4 and one
constexpr float kPetBobStep = 0.01f;
constexpr float kPetBobLow = -0.5f;
constexpr float kPetBobHigh = 1.f;
// pet node turns by minus a quarter D3DX which is plus a quarter in bx
constexpr float kPetTurnRadians = 1.5707963f;
constexpr float kStockFrameHz = 60.f;

// sub 4D3570 four weather nifs under Data Public
const char* const kRainSheetNif = "Effect/rain_01.nif";
const char* const kRainSplashNif = "Effect/rain_02.nif";
const char* const kThunderNif = "Effect/thunder_01.nif";
const char* const kSnowSheetNif = "Effect/snow_a01_car.nif";
// 0x4D3007 rain sheet 5 ahead of eye and 5 up 0x4D2FA8 snow 8 ahead and 10 up
constexpr float kWeatherRainAhead = 5.f;
constexpr float kWeatherRainLift = 5.f;
constexpr float kWeatherSnowAhead = 8.f;
constexpr float kWeatherSnowLift = 10.f;
// emitter gate of rain 01 nif runs 3 33 s so sheet restarts on that window
constexpr float kWeatherRainWindow = 3.3f;
// 0x4D30C8 splash lands inside 2 5 units of car 0 1 over ground shows 0x172 ms
constexpr float kWeatherSplashReach = 2.5f;
constexpr float kWeatherSplashLift = 0.1f;
constexpr float kWeatherSplashShow = 0.37f;
// 0x4D3230 thunder waits 8 s picks 8 to 17 units ahead plus minus 5 across shows 0x2F8 ms
constexpr float kWeatherThunderGap = 8.f;
constexpr float kWeatherThunderNear = 8.f;
constexpr float kWeatherThunderSpread = 10.f;
constexpr float kWeatherThunderSide = 5.f;
constexpr float kWeatherThunderLift = 3.f;
constexpr float kWeatherThunderShow = 0.76f;
// sub 4D34D0 veil alpha for two modes and 0x12C window a thunder keeps it clear
constexpr uint8_t kWeatherRainVeil = 0x60;
constexpr uint8_t kWeatherSnowVeil = 0x30;
constexpr float kWeatherFlashSeconds = 0.3f;
// ground probe starts this far over point it looks for
constexpr float kWeatherProbeLift = 5.f;
// camera mode 9 at 0x44007C heading yaw minus 90 plus 15 eye 6 away 3 up
constexpr float kFinishHeadingOffset = 15.f;
constexpr float kFinishDistance = 6.f;
constexpr float kFinishEyeHeight = 3.f;
// 0x5A3214 reused as finish look height above car
constexpr float kFinishLookHeight = 1.8f;
// 0x5A3240 shake decays by this per frame 0x5A8418 jitter is rand under shake times this
constexpr float kShakeDecay = 0.84f;
constexpr float kShakeJitter = 0.0001f;
// car visual update 0x48E6A0 ground pitch and roll of remote car ease by 0x5A32AC an eighth
constexpr float kRemoteLeanBlend = 0.125f;
// input poll keyboard 0x497D9A local car picks lean clip over 10 units a second
constexpr float kLocalLeanSpeedGate = 10.f;

// horizontal field stock engine derives from vertical one and window aspect
float stockHorizontalField(float fov, const SceneRenderer& renderer) {
    const ViewportRect& vp = renderer.viewport();
    const float aspect = vp.height > 0 ? static_cast<float>(vp.width) / static_cast<float>(vp.height) : 4.f / 3.f;
    return fov * ((aspect - kStockAspectPivot) * 0.5f + kStockAspectBase);
}

float wrapSigned180(float deg) {
    while (deg > 180.f) deg -= 360.f;
    while (deg < -180.f) deg += 360.f;
    return deg;
}

// per frame blend of stock turned into one for a frame of dt seconds
float frameBlend(float perFrame, float dt) {
    const float frames = dt > 0.f ? dt / kStockFrameSeconds : 1.f;
    return 1.f - std::pow(1.f - perFrame, frames);
}

// car placed at pose port matrix when it has one else yaw minus slip turn
void carWorld(const CarPose& pose, float out[16]) {
    if (pose.hasBody) {
        for (int i = 0; i < 16; ++i) out[i] = pose.body[i];
        return;
    }
    // car visual update 0x48E8F0 RotationZ of minus yaw plus slip bx mtxRotateZ of plus that
    float rotate[16];
    bx::mtxRotateZ(rotate, (pose.yawDeg - pose.driftSlipDeg) * kDegToRad);
    float translate[16];
    bx::mtxTranslate(translate, pose.x, pose.y, pose.z);
    bx::mtxMul(out, rotate, translate);
}

// no appended model yet track without an item box keeps this
constexpr std::size_t kNoAppendedModel = static_cast<std::size_t>(-1);

// car model key from kart and paint plain body under empty paint
std::string carModelKey(const std::string& model, const std::string& paint) {
    return paint.empty() ? model : model + "@" + paint;
}

GhostPose ghostPoseOf(const CarPose& pose) {
    GhostPose g;
    g.pos[0] = pose.x; g.pos[1] = pose.y; g.pos[2] = pose.z;
    g.yawDeg = pose.yawDeg;
    g.boosting = pose.boosting;
    g.boostKind = pose.boostKind;
    g.reversing = pose.reversing;
    g.driftState = pose.driftState;
    g.miniTurboStage = pose.miniTurboStage;
    g.turnState = pose.turnState;
    return g;
}

}

RaceEffects& raceEffects() { static RaceEffects effects; return effects; }

void ChaseCamera::update(const CarPose& car, float dt) {
    // 0x43F486 heading target is yaw plus 90 minus 0 8 times raw drift gauge
    const float targetHeading = car.yawDeg + 90.f - kCamDriftSwing * car.driftGauge;
    if (!valid) headingDeg = targetHeading;
    const float delta = wrapSigned180(targetHeading - headingDeg);
    if (delta > kCamHeadingDeadzone || delta < -kCamHeadingDeadzone) {
        const float perFrame = car.greenLight ? kCamHeadingBlend : kCamHeadingBlendStart;
        headingDeg += delta * frameBlend(perFrame, dt);
    }

    // 0x43E850 field widens with speed boost zoom of this build stays zero
    float fov = ((kCamZoom - 1.f) * kCamFovZoomScale + 1.f) * car.speed * kCamFovPerSpeed + kCamFovBase - kCamFovOffset;
    fov = std::clamp(fov, kCamFovBase, kCamFovMax);
    // 0x43E958 licence mode 0xD keeps base field distance stays at least 8 4 0x5A32C0
    if (licenceMode) fov = kCamFovBase;
    fovRadians = fov;

    // 0x43F4FB distance shrinks as field widens then nine tenths of it
    const float distance =
        (kCamZoom * kCamDistanceZoomScale + (std::max(car.camDistance, licenceMode ? 8.4f : 0.f) - (fov - kCamFovBase) * kCamDistanceFovScale)) *
        kCamDistanceScale;

    // 0x43F54D car pitch times 2 5 clamped 0 to 35 comes off row pitch
    const float pitchTerm = std::clamp(car.pitchDeg * kCamPitchPerCarPitch, 0.f, kCamPitchTermCap);
    const float pitch = (car.camPitchDeg - pitchTerm) * kDegToRad;

    // math dir from heading pitch 0x44DD50 gives sin h cos p cos h cos p sin p behind car
    const float h = headingDeg * kDegToRad;
    const float dir[3] = {std::sin(h) * std::cos(pitch), std::cos(h) * std::cos(pitch), std::sin(pitch)};
    const float targetEye[3] = {car.x + dir[0] * distance, car.y + dir[1] * distance, car.z + dir[2] * distance};
    // row stat 16 under one is a bad row stock 3 5 keeps sky in frame
    const float lookHeight = car.camHeight < 1.f ? kCamLookHeightDefault : car.camHeight;
    const float targetLook[3] = {car.x, car.y, car.z + lookHeight};

    if (!valid || car.greenLight) {
        for (int i = 0; i < 3; ++i) eye[i] = targetEye[i];
    } else {
        // 0x43F9D3 light states 2 and 6 ease eye by a half and its height by an eighth
        const float blendXY = frameBlend(1.f / kCamStartEaseXY, dt);
        const float blendZ = frameBlend(1.f / kCamStartEaseZ, dt);
        eye[0] += (targetEye[0] - eye[0]) * blendXY;
        eye[1] += (targetEye[1] - eye[1]) * blendXY;
        eye[2] += (targetEye[2] - eye[2]) * blendZ;
    }
    for (int i = 0; i < 3; ++i) look[i] = targetLook[i];
    decayShake(dt);
    valid = true;
}

void ChaseCamera::finish(const CarPose& car, float dt) {
    // 0x4400A3 heading from FUN 0044DF00 gives sin h cos h like chase direction
    headingDeg = car.yawDeg - 90.f + kFinishHeadingOffset;
    const float h = headingDeg * kDegToRad;
    eye[0] = car.x + std::sin(h) * kFinishDistance;
    eye[1] = car.y + std::cos(h) * kFinishDistance;
    eye[2] = car.z + kFinishEyeHeight;
    look[0] = car.x;
    look[1] = car.y;
    look[2] = car.z + kFinishLookHeight;
    // camera fov update 0x43E850 leaves mode 9 on base field
    fovRadians = kCamFovBase;
    decayShake(dt);
    valid = true;
}

void ChaseCamera::place(const float eyeIn[3], const float lookIn[3], float fov, float dt) {
    for (int i = 0; i < 3; ++i) { eye[i] = eyeIn[i]; look[i] = lookIn[i]; }
    fovRadians = fov;
    headingDeg = std::atan2(eye[0] - look[0], eye[1] - look[1]) * kRadToDeg;
    decayShake(dt);
    valid = true;
}

// 0x44069A shake times 0 84 per frame under one rests at one
void ChaseCamera::decayShake(float dt) {
    const float frames = dt > 0.f ? dt / kStockFrameSeconds : 1.f;
    shake *= std::pow(kShakeDecay, frames);
    if (shake < 1.f) shake = 0.f;
}

void ChaseCamera::view(float out[16], float outEye[3]) const {
    for (int i = 0; i < 3; ++i) outEye[i] = eye[i];
    float at[3] = {look[0], look[1], look[2]};
    // camera shake jitter 0x43EAF0 rand under shake times 0 0001 on each look axis
    if (shake >= 1.f) {
        const int range = static_cast<int>(shake);
        for (float& axis : at) axis += static_cast<float>(std::rand() % range) * kShakeJitter;
    }
    bx::mtxLookAt(out, bx::Vec3(eye[0], eye[1], eye[2]), bx::Vec3(at[0], at[1], at[2]),
                  bx::Vec3(0.f, 0.f, 1.f), bx::Handedness::Right);
}

bool RaceView::load(SceneRenderer& renderer, RaceWorld& world) {
    renderer.upload(world.scene.scene);
    const MapScene& scene = world.scene.scene;
    const float extent[3] = {scene.bounds_max[0] - scene.bounds_min[0], scene.bounds_max[1] - scene.bounds_min[1],
                             scene.bounds_max[2] - scene.bounds_min[2]};
    const float radius = 0.5f * std::sqrt(extent[0] * extent[0] + extent[1] * extent[1] + extent[2] * extent[2]);
    renderer.set_far_plane(std::max(kClientFarPlane, radius * 2.2f));
    m_carModels.clear();
    m_driverModels.clear();
    m_cars.clear();
    m_props.clear();
    m_itemBoxes.clear();
    m_itemBoxModel = kNoAppendedModel;
    m_collision = &world.scene.collision;
    m_camera.reset();
    m_characterCount = scene.character_models.size();
    m_loaded = true;
    // question mark boxes of track model turns on its own controllers
    if (world.scene.has_item_box && !world.scene.item_boxes.empty()) {
        m_itemBoxModel = renderer.append_prop_model(world.scene.item_box_model);
        m_itemBoxes.reserve(world.scene.item_boxes.size());
        for (const KnC::Tools::TrackItemBox& box : world.scene.item_boxes) {
            ItemBoxSpot spot;
            for (int axis = 0; axis < 3; ++axis) spot.position[axis] = box.position[axis];
            m_itemBoxes.push_back(spot);
        }
        std::printf("[view] %zu item boxes on the track\n", m_itemBoxes.size());
    }
    raceEffects().reset(raceGameDirOf(world.files.trackDir));
    return true;
}

void RaceView::hideItemBox(size_t index, float respawnAt) {
    if (index >= m_itemBoxes.size()) return;
    m_itemBoxes[index].hidden = true;
    m_itemBoxes[index].respawnAt = respawnAt;
}

void RaceView::refreshItemBoxes(float clockSeconds) {
    for (ItemBoxSpot& box : m_itemBoxes)
        if (box.hidden && clockSeconds >= box.respawnAt) box.hidden = false;
}

void RaceView::setNightSky(SceneRenderer& renderer, bool night) {
    // track ships sky nif first and sky night nif second track without one keeps day
    const int phase = night && renderer.sky_phase_count() > 1 ? 1 : 0;
    renderer.set_sky_phase(phase);
}

// sub 4D3570 weather class loads its nifs once mode picks which of them show
bool RaceView::setWeather(SceneRenderer& renderer, const std::string& gameDir, RaceWeather weather) {
    m_weather = weather;
    m_rainSheet = m_rainSplash = m_thunderProp = m_snowSheet = -1;
    m_splashState = m_thunderState = 0;
    m_splashAge = m_thunderAge = m_rainAge = 0.f;
    m_thunderFired = false;
    if (!m_loaded || (weather != RaceWeather::Rain && weather != RaceWeather::Snow)) return true;
    auto add = [&](const char* name) -> int {
        namespace fs = std::filesystem;
        const fs::path nif = fs::path(gameDir) / "Data" / "Public" / name;
        NifModelRequest request;
        request.nif_path = nif.string();
        request.texture_dir = nif.parent_path().string();
        request.play_stopped_controllers = true;
        PropModel model;
        std::string error;
        if (!load_prop_model(request, model, error)) {
            std::printf("[weather] %s failed %s\n", request.nif_path.c_str(), error.c_str());
            return -1;
        }
        resolve_textures(request.texture_dir, model);
        return addProp(renderer, model);
    };
    if (weather == RaceWeather::Rain) {
        m_rainSheet = add(kRainSheetNif);
        m_rainSplash = add(kRainSplashNif);
        m_thunderProp = add(kThunderNif);
    } else {
        m_snowSheet = add(kSnowSheetNif);
    }
    std::printf("[weather] mode %d rain %d splash %d thunder %d snow %d\n", static_cast<int>(weather), m_rainSheet,
                m_rainSplash, m_thunderProp, m_snowSheet);
    return true;
}

// xorshift for placements exe rolls rand port keeps its own so a run repeats
float RaceView::weatherRoll(float span) {
    m_weatherRng ^= m_weatherRng << 13;
    m_weatherRng ^= m_weatherRng >> 17;
    m_weatherRng ^= m_weatherRng << 5;
    return static_cast<float>(m_weatherRng % 1000u) * 0.001f * span;
}

float RaceView::weatherJitter() {
    return weatherRoll(2.f) - 1.f;
}

// sub 4D2F10 places sheet splash and thunder every frame off camera and car
void RaceView::updateWeather(SceneRenderer& renderer, float dt, const float carPos[3]) {
    if (m_weather != RaceWeather::Rain && m_weather != RaceWeather::Snow) return;
    if (!m_camera.valid) return;
    // camera world matrix row 0 is view direction exe puts sheet along it
    float forward[3] = {m_camera.look[0] - m_camera.eye[0], m_camera.look[1] - m_camera.eye[1],
                        m_camera.look[2] - m_camera.eye[2]};
    const float len = std::sqrt(forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2]);
    if (len < 0.001f) return;
    for (int i = 0; i < 3; ++i) forward[i] /= len;
    float turn[16], place[16];
    bx::mtxRotateZ(turn, -m_camera.headingDeg * kDegToRad);
    auto put = [&](int handle, const float at[3]) {
        if (handle < 0) return;
        for (int i = 0; i < 16; ++i) place[i] = turn[i];
        place[12] = at[0];
        place[13] = at[1];
        place[14] = at[2];
        placeProp(handle, place, true);
    };
    auto hide = [&](int handle) {
        if (handle < 0) return;
        m_props[static_cast<size_t>(handle)].shown = false;
    };
    if (m_weather == RaceWeather::Snow) {
        const float at[3] = {m_camera.eye[0] + forward[0] * kWeatherSnowAhead,
                             m_camera.eye[1] + forward[1] * kWeatherSnowAhead,
                             m_camera.eye[2] + forward[2] * kWeatherSnowAhead + kWeatherSnowLift};
        put(m_snowSheet, at);
        return;
    }
    const float sheet[3] = {m_camera.eye[0] + forward[0] * kWeatherRainAhead,
                            m_camera.eye[1] + forward[1] * kWeatherRainAhead,
                            m_camera.eye[2] + forward[2] * kWeatherRainAhead + kWeatherRainLift};
    put(m_rainSheet, sheet);
    m_rainAge += dt;
    if (m_rainSheet >= 0 && m_rainAge >= kWeatherRainWindow) {
        m_rainAge = 0.f;
        restartProp(renderer, m_rainSheet);
    }
    // 0x4D30C8 splash pops inside 2 5 units of car on ground shows 370 ms
    m_splashAge += dt;
    if (m_splashState == 0) {
        float at[3] = {carPos[0] + weatherJitter() * kWeatherSplashReach,
                       carPos[1] + weatherJitter() * kWeatherSplashReach, carPos[2]};
        float ground = at[2];
        if (m_collision && KnC::Kart::Client::world_locate_piece_by_height(m_weatherProbe, *m_collision, at[0], at[1],
                                                                          at[2] + kWeatherProbeLift, &ground, nullptr))
            at[2] = ground + kWeatherSplashLift;
        put(m_rainSplash, at);
        if (m_rainSplash >= 0) restartProp(renderer, m_rainSplash);
        m_splashState = 1;
        m_splashAge = 0.f;
    } else if (m_splashAge > kWeatherSplashShow) {
        hide(m_rainSplash);
        m_splashState = 0;
        m_splashAge = 0.f;
    }
    // 0x4D3230 thunder strikes 8 to 17 units ahead once last one is 8 s old shows 760 ms
    m_thunderAge += dt;
    if (m_thunderState == 0) {
        if (m_thunderAge <= kWeatherThunderGap) return;
        const float reach = kWeatherThunderNear + weatherRoll(kWeatherThunderSpread);
        m_thunderPos[0] = m_camera.eye[0] + forward[0] * reach + weatherJitter() * kWeatherThunderSide;
        m_thunderPos[1] = m_camera.eye[1] + forward[1] * reach + weatherJitter() * kWeatherThunderSide;
        m_thunderPos[2] = m_camera.eye[2] + forward[2] * reach;
        float ground = m_thunderPos[2];
        if (m_collision && KnC::Kart::Client::world_locate_piece_by_height(m_weatherProbe, *m_collision, m_thunderPos[0],
                                                                          m_thunderPos[1],
                                                                          m_thunderPos[2] + kWeatherProbeLift, &ground,
                                                                          nullptr))
            m_thunderPos[2] = ground + kWeatherThunderLift;
        put(m_thunderProp, m_thunderPos);
        if (m_thunderProp >= 0) restartProp(renderer, m_thunderProp);
        m_thunderState = 1;
        m_thunderAge = 0.f;
        m_thunderFired = true;
    } else {
        put(m_thunderProp, m_thunderPos);
        if (m_thunderAge > kWeatherThunderShow) {
            hide(m_thunderProp);
            m_thunderState = 0;
            m_thunderAge = 0.f;
        }
    }
}

// sub 4D34D0 rain veils frame at 0x60 snow at 0x30 fresh thunder clears it 300 ms
uint8_t RaceView::weatherVeil() const {
    if (m_weather == RaceWeather::Snow) return kWeatherSnowVeil;
    // sub 4D1BE0 night mode carries same 0x60 veil with no class behind it
    if (m_weather == RaceWeather::Night) return kWeatherRainVeil;
    if (m_weather != RaceWeather::Rain) return 0;
    if (m_thunderState == 1 && m_thunderAge <= kWeatherFlashSeconds) return 0;
    return kWeatherRainVeil;
}

bool RaceView::takeThunder() {
    const bool fired = m_thunderFired;
    m_thunderFired = false;
    return fired;
}

bool RaceView::loadEmpty(SceneRenderer& renderer) {
    MapScene empty;
    // no tile means no sun and a dark backdrop so preview lights itself
    empty.sun.direction[0] = -0.42f;
    empty.sun.direction[1] = 0.46f;
    empty.sun.direction[2] = -0.78f;
    empty.sun.colour = HourColour{1.f, 0.98f, 0.94f};
    empty.sun.enabled = true;
    empty.day_night = flat_day_night(HourColour{0.56f, 0.61f, 0.70f});
    empty.day_night.ambient.fill(HourColour{0.85f, 0.85f, 0.90f});
    renderer.upload(empty);
    renderer.set_far_plane(kClientFarPlane);
    m_carModels.clear();
    m_driverModels.clear();
    m_cars.clear();
    m_props.clear();
    m_itemBoxes.clear();
    m_itemBoxModel = kNoAppendedModel;
    m_collision = nullptr;
    m_camera.reset();
    m_characterCount = 0;
    m_loaded = true;
    raceEffects().reset(std::string());
    return true;
}

RaceView::CarModel& RaceView::carModel(SceneRenderer& renderer, const std::string& gameDir, const std::string& model,
                                       const std::string& paint) {
    const std::string key = carModelKey(model, paint);
    auto it = m_carModels.find(key);
    if (it != m_carModels.end()) return it->second;
    CarModel& cm = m_carModels[key];
    std::string nif = kartBodyNif(gameDir, model);
    if (nif.empty()) {
        std::printf("[view] no body nif for kart %s the Basic 1 body stands in\n", model.c_str());
        nif = kartBodyNif(gameDir, "Basic_1");
    }
    std::string error;
    // paint names BodyColor folder of body Car Body High model BodyColor paint 0x49123D
    if (nif.empty() || !load_ghost_car(nif, cm.car, error, paint)) {
        std::printf("[view] kart %s failed %s\n", model.c_str(), error.c_str());
        return cm;
    }
    cm.firstModelIndex = renderer.append_prop_model(cm.car.body);
    for (const PropModel& wheel : cm.car.wheels) renderer.append_prop_model(wheel);
    cm.valid = true;
    std::printf("[view] kart %s paint %s %zu wheels from %s\n", model.c_str(), paint.empty() ? "none" : paint.c_str(),
                cm.car.wheels.size(), nif.c_str());
    return cm;
}

void RaceView::setCarPaint(SceneRenderer& renderer, int handle, const std::string& paint) {
    if (handle < 0 || handle >= static_cast<int>(m_cars.size())) return;
    CarVisual& v = m_cars[static_cast<size_t>(handle)];
    if (!v.alive || v.paint == paint) return;
    CarModel& cm = carModel(renderer, m_gameDir, v.kartName, paint);
    if (!cm.valid) return;
    v.paint = paint;
    v.kartModel = carModelKey(v.kartName, paint);
}

const std::string& RaceView::kartModelOf(int handle) const {
    static const std::string none;
    if (handle < 0 || handle >= static_cast<int>(m_cars.size())) return none;
    return m_cars[static_cast<size_t>(handle)].kartName;
}

RaceView::DriverModel& RaceView::driverModel(SceneRenderer& renderer, const std::string& gameDir,
                                             const std::string& asset, const std::string& chassis) {
    const std::string key = asset + "@" + chassis;
    auto it = m_driverModels.find(key);
    if (it != m_driverModels.end()) return it->second;
    DriverModel& dm = m_driverModels[key];
    std::string nif = driverBodyNif(gameDir, asset);
    if (nif.empty()) {
        std::printf("[view] no body nif for driver %s Cosmo stands in\n", asset.c_str());
        nif = driverBodyNif(gameDir, "Cosmo");
    }
    std::string error;
    if (nif.empty() || !load_ghost_driver(nif, chassis, dm.driver, error)) {
        std::printf("[view] driver %s failed %s\n", asset.c_str(), error.c_str());
        return dm;
    }
    dm.modelIndex = m_characterCount++;
    renderer.append_character_model(dm.driver.model);
    dm.valid = true;
    std::printf("[view] driver %s %zu clips seat %s %.3f %.3f %.3f\n", asset.c_str(), dm.driver.model.rig.clips.size(),
                dm.driver.seat_found ? "from the ini" : "at the car origin", dm.driver.seat[0], dm.driver.seat[1],
                dm.driver.seat[2]);
    return dm;
}

int RaceView::addCar(SceneRenderer& renderer, const std::string& gameDir, const std::string& kartModel,
                     const std::string& driverAsset, const std::string& paint) {
    if (!m_loaded) return -1;
    m_gameDir = gameDir;
    CarModel& cm = carModel(renderer, gameDir, kartModel, paint);
    std::string chassis = kartModel;
    if (cm.valid) {
        const std::string nif = kartBodyNif(gameDir, kartModel);
        const size_t slash = nif.find_last_of("/\\");
        if (slash != std::string::npos) {
            const std::string dir = nif.substr(0, slash);
            const size_t up = dir.find_last_of("/\\");
            chassis = up == std::string::npos ? dir : dir.substr(up + 1);
        }
    }
    driverModel(renderer, gameDir, driverAsset, chassis);
    CarVisual v;
    v.kartModel = carModelKey(kartModel, cm.valid ? paint : std::string());
    v.kartName = kartModel;
    v.paint = cm.valid ? paint : std::string();
    v.driverAsset = driverAsset + "@" + chassis;
    v.alive = true;
    m_cars.push_back(v);
    return static_cast<int>(m_cars.size() - 1);
}

RaceView::PetModel& RaceView::petModel(SceneRenderer& renderer, const std::string& petNif,
                                       const std::string& facialDir) {
    auto it = m_petModels.find(petNif);
    if (it != m_petModels.end()) return it->second;
    PetModel& pm = m_petModels[petNif];
    std::string error;
    if (!load_ghost_driver(petNif, std::string(), pm.driver, error)) {
        std::printf("[view] pet %s failed %s\n", petNif.c_str(), error.c_str());
        return pm;
    }
    // face sheet of a pet sits under Pet Facial not beside its body
    if (!facialDir.empty()) resolve_textures(facialDir, pm.driver.model);
    pm.modelIndex = m_characterCount++;
    renderer.append_character_model(pm.driver.model);
    pm.valid = true;
    std::printf("[view] pet %s %zu clips idle %d\n", petNif.c_str(), pm.driver.model.rig.clips.size(), pm.driver.idle_clip);
    return pm;
}

void RaceView::setCarPet(SceneRenderer& renderer, int handle, const std::string& petNif, const std::string& facialDir) {
    if (handle < 0 || handle >= static_cast<int>(m_cars.size())) return;
    CarVisual& v = m_cars[static_cast<size_t>(handle)];
    v.petModel.clear();
    v.hover = PetHover();
    if (petNif.empty()) return;
    if (petModel(renderer, petNif, facialDir).valid) v.petModel = petNif;
}

// driver place on car 0x48B800 pet trails at speed wanders across and bobs per 60 Hz frame
void RaceView::hoverPet(CarVisual& v, const CarPose& pose, float dt, double clockSeconds) {
    PetHover& h = v.hover;
    const float frames = dt * kStockFrameHz;
    const float speed = std::fabs(pose.speed);
    if (h.trailState == 0) {
        h.trailState = 1;
        h.trailAt = clockSeconds;
    } else if (h.trailState == 1) {
        if (speed >= kPetTrailSpeedGate) {
            h.trail = std::min(kPetTrailCap, h.trail + speed * kPetTrailPerSpeed * frames);
            if (clockSeconds - h.trailAt >= kPetTrailSeconds) { h.trailState = 2; h.trailAt = clockSeconds; }
        } else {
            h.trailState = 2;
            h.trailAt = clockSeconds;
        }
    } else {
        const float step = speed >= kPetTrailSlowGate ? speed * kPetTrailPerSpeed : kPetTrailSlowStep;
        h.trail = std::max(0.f, h.trail - step * frames);
        if (clockSeconds - h.trailAt > kPetTrailSeconds) { h.trailState = 1; h.trailAt = clockSeconds; }
    }
    if (h.sideState == 0) {
        const int roll = std::rand() & 3;
        h.sideState = roll;
        if (roll == 3) h.sideAt = clockSeconds;
    }
    if (h.sideState == 1) {
        h.side += kPetSideStep * frames;
        if (h.side > kPetSideHigh) { h.side = kPetSideHigh; h.sideState = 0; }
    } else if (h.sideState == 2) {
        h.side -= kPetSideStep * frames;
        if (h.side < kPetSideLow) { h.side = kPetSideLow; h.sideState = 0; }
    } else if (h.sideState == 3) {
        if (clockSeconds - h.sideAt >= kPetSideHold) h.sideState = 0;
    }
    if (h.bobState == 0) {
        h.bob += kPetBobStep * frames;
        if (h.bob > kPetBobHigh) { h.bob = kPetBobHigh; h.bobState = 1; }
    } else {
        h.bob -= kPetBobStep * frames;
        if (h.bob < kPetBobLow) { h.bob = kPetBobLow; h.bobState = 0; }
    }
}

void RaceView::removeCar(int handle) {
    if (handle < 0 || handle >= static_cast<int>(m_cars.size())) return;
    m_cars[static_cast<size_t>(handle)].alive = false;
    raceEffects().remove(handle);
}

// KNC VIEW TEST DRIFT paints full right drift on local car so a capture shows lean path
void applyDriftTest(CarPose& pose) {
    static const bool wanted = std::getenv("KNC_VIEW_TEST_DRIFT") != nullptr;
    if (!wanted || !pose.hasBody) return;
    pose.driftGauge = -45.f;
    pose.driftState = 2;
    pose.turnState = 2;
    pose.driftSlipDeg = -1.62f * 45.f;
    pose.wheelSteer = -0.5236f;
    // lean of car effect lean update stat 8 times gauge size and stat 9 times gauge
    const float lift = 0.52f * 45.f * kDegToRad;
    const float lean = 0.52f * 45.f * kDegToRad;
    float slipZ[16], liftY[16], leanX[16], yawZ[16], rotate[16], translate[16], tmp[16];
    bx::mtxRotateZ(yawZ, pose.yawDeg * kDegToRad);
    bx::mtxRotateZ(slipZ, -pose.driftSlipDeg * kDegToRad);
    bx::mtxRotateY(liftY, -lift);
    bx::mtxRotateX(leanX, -lean);
    // D3DX lean times slip times body in row order lean first
    bx::mtxMul(tmp, leanX, liftY);
    bx::mtxMul(rotate, tmp, slipZ);
    bx::mtxMul(tmp, rotate, yawZ);
    bx::mtxTranslate(translate, pose.x, pose.y, pose.z);
    bx::mtxMul(pose.body, tmp, translate);
}

void RaceView::setPose(int handle, const CarPose& given, float dt, float clockSeconds) {
    if (handle < 0 || handle >= static_cast<int>(m_cars.size())) return;
    CarVisual& v = m_cars[static_cast<size_t>(handle)];
    if (!v.alive) return;
    CarPose pose = given;
    applyDriftTest(pose);
    carWorld(pose, v.world);
    // remote car has no port matrix ground under its wheels tilts it like stock
    if (!pose.hasBody) remoteLean(v, pose, dt);
    if (!v.petModel.empty()) hoverPet(v, pose, dt, clockSeconds);
    float moved[3] = {0.f, 0.f, 0.f};
    if (v.prevValid) {
        moved[0] = pose.x - v.prevPos[0];
        moved[1] = pose.y - v.prevPos[1];
        moved[2] = pose.z - v.prevPos[2];
    }
    v.prevPos[0] = pose.x; v.prevPos[1] = pose.y; v.prevPos[2] = pose.z;
    v.prevValid = true;
    auto cm = m_carModels.find(v.kartModel);
    if (cm != m_carModels.end() && cm->second.valid) {
        if (pose.hasWheels) {
            ghost_wheels_from_physics(cm->second.car, pose.wheelSpin, pose.wheelSteer, v.wheels);
        } else {
            // remote car rolls by distance and turns front pair by eased lean of port
            ghost_wheels_advance(cm->second.car, v.world, moved, dt, pose.turnState, v.wheels);
            v.wheels.steer_degrees = pose.steerAverage * kRadToDeg;
        }
    }
    // race effects follow pose then ground shake lifts wheels
    raceEffects().update(handle, pose, cm != m_carModels.end() && cm->second.valid ? &cm->second.car : nullptr,
                         v.world, dt, clockSeconds);
    raceEffects().shakeWheels(handle, v.wheels);
    auto dm = m_driverModels.find(v.driverAsset);
    if (dm != m_driverModels.end() && dm->second.valid) {
        // local car keeps lean clip for a speed over 10 remote rule gates at 3
        const float speed = pose.hasBody && pose.speed <= kLocalLeanSpeedGate ? 0.f : pose.speed;
        int wanted = ghost_driver_clip(dm->second.driver, ghostPoseOf(pose), speed);
        // podium forces win or lose sequence on driver
        if (v.forcedClip >= 0) {
            const int forced = dm->second.driver.clip_of(v.forcedClip);
            if (forced >= 0) wanted = forced;
        }
        if (wanted != v.clip) {
            v.clip = wanted;
            v.clipStart = clockSeconds;
        }
        v.clipSeconds = std::max(0.f, clockSeconds - v.clipStart);
    }
}

void RaceView::drawOrbit(SceneRenderer& renderer, const CarPose& target, float orbitDeg, float distance,
                         float height, float dt) {
    if (!m_loaded) return;
    renderer.animation().advance(dt);
    submit(renderer);
    const float angle = orbitDeg * kDegToRad;
    const float eye[3] = {target.x + std::cos(angle) * distance, target.y + std::sin(angle) * distance,
                          target.z + height};
    float view[16];
    bx::mtxLookAt(view, bx::Vec3(eye[0], eye[1], eye[2]), bx::Vec3(target.x, target.y, target.z + 0.6f),
                  bx::Vec3(0.f, 0.f, 1.f), bx::Handedness::Right);
    // preview keeps stock base field 1 05 and stock horizontal rule
    renderer.set_field_of_view(kCamFovBase, stockHorizontalField(kCamFovBase, renderer));
    for (int i = 0; i < 16; ++i) m_lastView[i] = view[i];
    for (int i = 0; i < 3; ++i) m_lastEye[i] = eye[i];
    renderer.draw(view, eye);
}

void RaceView::submit(SceneRenderer& renderer) {
    std::vector<PropInstance> props;
    std::vector<CharacterInstance> riders;
    for (const CarVisual& v : m_cars) {
        if (!v.alive) continue;
        auto cm = m_carModels.find(v.kartModel);
        if (cm != m_carModels.end() && cm->second.valid) {
            ghost_car_instances(cm->second.car, cm->second.firstModelIndex, v.world, v.wheels, props);
        }
        auto dm = m_driverModels.find(v.driverAsset);
        if (dm != m_driverModels.end() && dm->second.valid) {
            CharacterInstance rider;
            ghost_driver_instance(dm->second.driver, dm->second.modelIndex, v.world, v.clip < 0 ? -1 : v.clip,
                                  v.clipSeconds, rider);
            riders.push_back(rider);
            // pet beside driver seat turned a quarter hovering on its idle clip
            auto pm = v.petModel.empty() ? m_petModels.end() : m_petModels.find(v.petModel);
            if (pm != m_petModels.end() && pm->second.valid) {
                const float* seat = dm->second.driver.seat;
                float turn[16], lift[16], local[16], world[16];
                bx::mtxRotateZ(turn, kPetTurnRadians);
                bx::mtxTranslate(lift, seat[0] - kPetBack + v.hover.trail, seat[1] + kPetAcross + v.hover.side,
                                 seat[2] + kPetUp + v.hover.bob);
                bx::mtxMul(local, turn, lift);
                bx::mtxMul(world, local, v.world);
                CharacterInstance pet;
                ghost_driver_instance(pm->second.driver, pm->second.modelIndex, world, pm->second.driver.idle_clip,
                                      renderer.animation().seconds(), pet);
                riders.push_back(pet);
            }
        }
    }
    // every item box not held right now model turns on its own controllers
    if (m_itemBoxModel != kNoAppendedModel) {
        for (const ItemBoxSpot& box : m_itemBoxes) {
            if (box.hidden) continue;
            PropInstance instance;
            instance.model_index = m_itemBoxModel;
            instance.layer = SceneLayer::Props;
            bx::mtxTranslate(instance.world, box.position[0], box.position[1], box.position[2]);
            props.push_back(instance);
        }
    }
    // podium and any other prop placed beside cars
    for (const ExtraProp& extra : m_props) {
        if (!extra.shown) continue;
        PropInstance instance;
        instance.model_index = extra.modelIndex;
        for (int i = 0; i < 16; ++i) instance.world[i] = extra.world[i];
        props.push_back(instance);
    }
    // effect placements of every car after karts sparks flames dust shadow
    raceEffects().submit(renderer, props, m_camera.valid ? m_camera.eye : nullptr);
    renderer.set_appended_prop_instances(props);
    renderer.update_character_instances(riders);
}

void RaceView::draw(SceneRenderer& renderer, const CarPose& given, float dt) {
    if (!m_loaded) return;
    renderer.animation().advance(dt);
    submit(renderer);
    CarPose chase = given;
    applyDriftTest(chase);
    m_camera.update(chase, dt);
    float view[16];
    float eye[3];
    m_camera.view(view, eye);
    if (std::getenv("KNC_CAMERA_LOG") != nullptr) {
        static int frames = 0;
        if (++frames % 25 == 0) {
            const float bodyYaw = std::atan2(-chase.body[1], chase.body[0]) * kRadToDeg;
            std::printf("[cam] body row0 %.2f %.2f %.2f row1 %.2f %.2f %.2f row2 %.2f %.2f %.2f yaw of row0 %.1f\n",
                        chase.body[0], chase.body[1], chase.body[2], chase.body[4], chase.body[5], chase.body[6],
                        chase.body[8], chase.body[9], chase.body[10], bodyYaw);
        }
        if (frames % 25 == 0)
            std::printf("[cam] car %.1f %.1f %.1f yaw %.1f pitch %.2f gauge %.1f slip %.1f speed %.1f head %.1f fov %.3f eye %.1f %.1f %.1f look %.1f %.1f %.1f body %d\n",
                        chase.x, chase.y, chase.z, chase.yawDeg, chase.pitchDeg, chase.driftGauge, chase.driftSlipDeg,
                        chase.speed, m_camera.headingDeg, m_camera.fovRadians, eye[0], eye[1], eye[2],
                        m_camera.look[0], m_camera.look[1], m_camera.look[2], chase.hasBody ? 1 : 0);
    }
    present(renderer, m_camera.fovRadians);
}

void RaceView::drawFinish(SceneRenderer& renderer, const CarPose& own, float dt) {
    if (!m_loaded) return;
    renderer.animation().advance(dt);
    submit(renderer);
    m_camera.finish(own, dt);
    present(renderer, m_camera.fovRadians);
}

void RaceView::drawFixed(SceneRenderer& renderer, const float eye[3], const float look[3], float fovRadians,
                         float dt) {
    if (!m_loaded) return;
    renderer.animation().advance(dt);
    submit(renderer);
    m_camera.place(eye, look, fovRadians, dt);
    present(renderer, fovRadians);
}

void RaceView::present(SceneRenderer& renderer, float fovRadians) {
    // lens of frame vertical field of camera horizontal one by stock rule
    renderer.set_field_of_view(fovRadians, stockHorizontalField(fovRadians, renderer));
    m_camera.view(m_lastView, m_lastEye);
    renderer.draw(m_lastView, m_lastEye);
}

void RaceView::lastView(float out[16], float outEye[3]) const {
    for (int i = 0; i < 16; ++i) out[i] = m_lastView[i];
    for (int i = 0; i < 3; ++i) outEye[i] = m_lastEye[i];
}

int RaceView::addProp(SceneRenderer& renderer, const PropModel& model) {
    if (!m_loaded) return -1;
    ExtraProp extra;
    extra.modelIndex = renderer.append_prop_model(model);
    // emitters of prop start now podium confetti falls from its first frame
    renderer.restart_prop_particles(extra.modelIndex, renderer.animation().seconds());
    m_props.push_back(extra);
    return static_cast<int>(m_props.size() - 1);
}

void RaceView::placeProp(int handle, const float world[16], bool shown) {
    if (handle < 0 || handle >= static_cast<int>(m_props.size())) return;
    ExtraProp& extra = m_props[static_cast<size_t>(handle)];
    for (int i = 0; i < 16; ++i) extra.world[i] = world[i];
    extra.shown = shown;
}

void RaceView::restartProp(SceneRenderer& renderer, int handle) {
    if (handle < 0 || handle >= static_cast<int>(m_props.size())) return;
    renderer.restart_prop_particles(m_props[static_cast<size_t>(handle)].modelIndex, renderer.animation().seconds());
}

void RaceView::setDriverClip(int handle, int sequenceId) {
    if (handle < 0 || handle >= static_cast<int>(m_cars.size())) return;
    m_cars[static_cast<size_t>(handle)].forcedClip = sequenceId;
}

void RaceView::remoteLean(CarVisual& v, const CarPose& pose, float dt) {
    auto cm = m_carModels.find(v.kartModel);
    if (!m_collision || cm == m_carModels.end() || !cm->second.valid || cm->second.car.wheel_local.size() < 4) return;
    // 0x48E948 each O WHEEL point through yaw matrix then ground height under it
    float wheel[4][3];
    for (int i = 0; i < 4; ++i) {
        const float* local = cm->second.car.wheel_local[static_cast<size_t>(i)].data();
        const float p[3] = {local[12], local[13], local[14]};
        for (int axis = 0; axis < 3; ++axis)
            wheel[i][axis] = p[0] * v.world[axis] + p[1] * v.world[4 + axis] + p[2] * v.world[8 + axis] + v.world[12 + axis];
        float ground = wheel[i][2];
        if (KnC::Kart::Client::world_locate_piece_by_height(v.probe, *m_collision, wheel[i][0], wheel[i][1],
                                                            wheel[i][2], &ground, nullptr))
            wheel[i][2] = ground;
    }
    // 0x48EAE4 pair averages front 0 1 rear 2 3 left 0 2 right 1 3 flat gives 0
    const float frontX = (wheel[0][0] + wheel[1][0]) * 0.5f, frontY = (wheel[0][1] + wheel[1][1]) * 0.5f;
    const float rearX = (wheel[2][0] + wheel[3][0]) * 0.5f, rearY = (wheel[2][1] + wheel[3][1]) * 0.5f;
    const float frontZ = (wheel[0][2] + wheel[1][2]) * 0.5f, rearZ = (wheel[2][2] + wheel[3][2]) * 0.5f;
    const float leftZ = (wheel[0][2] + wheel[2][2]) * 0.5f, rightZ = (wheel[1][2] + wheel[3][2]) * 0.5f;
    const float horizontal = std::hypot(rearY - frontY, rearX - frontX);
    if (horizontal < 0.01f) return;
    const float pitchTarget = std::atan2(horizontal, rearZ - frontZ) * kRadToDeg - 90.f;
    const float rollTarget = std::atan2(horizontal, rightZ - leftZ) * kRadToDeg - 90.f;
    const float blend = frameBlend(kRemoteLeanBlend, dt);
    v.pitchDeg += wrapSigned180(pitchTarget - v.pitchDeg) * blend;
    v.rollDeg += wrapSigned180(rollTarget - v.rollDeg) * blend;
    // 0x48ECAF RotationYawPitchRoll yaw pitch pitch minus roll then yaw and place
    float leanX[16], leanY[16], lean[16], rotate[16], translate[16], tmp[16];
    bx::mtxRotateX(leanX, v.rollDeg * kDegToRad);
    bx::mtxRotateY(leanY, -v.pitchDeg * kDegToRad);
    bx::mtxMul(lean, leanX, leanY);
    bx::mtxRotateZ(rotate, (pose.yawDeg - pose.driftSlipDeg) * kDegToRad);
    bx::mtxTranslate(translate, pose.x, pose.y, pose.z);
    bx::mtxMul(tmp, lean, rotate);
    bx::mtxMul(v.world, tmp, translate);
}

}
