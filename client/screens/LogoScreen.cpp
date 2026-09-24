#include "LogoScreen.h"

#include "app/App.h"
#include "engine/render/map_scene.h"
#include "engine/render/nif_prop_model.h"
#include "engine/render/scene_renderer.h"
#include "tools/track_scene/ghost_car.h"

#include <GLFW/glfw3.h>
#include <bx/math.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace KnC::Client {

namespace {

// sub 411C00 the plate fades in over 300 ms holds a second and fades out over 300 ms
constexpr float kPlateFade = 0.3f;
constexpr float kPlateHold = 1.0f;
constexpr float kPlateSeconds = kPlateFade + kPlateHold + kPlateFade;
// sub 411C00 case 6 waits 800 ms after the second plate before the title stage
constexpr float kPlateTail = 0.8f;
// sub 411FD0 the title fades in over 500 ms then case 2 holds 5499 ms then case 3 a second
constexpr float kIntroFade = 0.5f;
constexpr float kIntroSeconds = kIntroFade + 5.499f + 1.0f;
// sub 4279B0 the camera of the title stage field 0 75 rad near 10 far 10000 aspect 1
constexpr float kIntroFov = 0.75f;
constexpr float kIntroFar = 10000.f;
// sub 4279B0 the scene node is scaled on z and put 1300 units in front of the eye
constexpr float kIntroScaleZ = 1.335f;
constexpr float kIntroDistance = 1300.f;
// the ambient that saturates every emissive shape of the intro the shader clamps the sum at one
constexpr float kIntroAmbient = 4.f;
// the nif of the stock title stage the string at 0x5A05E8
const char* const kIntroNif = "Data/Public/Title/INTRO.nif";
const char* const kIntroDir = "Data/Public/Title";
// the two plates the render module cannot composite the late wallpaper and the blur sheet
const char* const kDroppedPlate[2] = {"Login_THA_00", "background6"};
// block 361 of the nif the wallpaper plate snaps on at this second and holds to the end
constexpr float kLateWallpaper = 6.233f;

// an auto walk or a capture run does not sit through the whole intro
constexpr float kAutoIntroSeconds = 0.4f;

}

void LogoScreen::enter() {
    // the stock logo stage loads the two plates alone the menu art of the JSON is for the editor
    m_stage = Stage::Ogp;
    m_time = 0.f;
    m_intro = 0.f;
    m_left = false;
    auto plate = std::make_unique<ImageWidget>();
    plate->type = ElementType::Image;
    plate->id = "logo_plate";
    plate->texture = m_app.assets().texture("Logo/OGP_ci.png");
    plate->rect = {0.f, 0.f, m_app.canvasWidth(), m_app.canvasHeight()};
    plate->zIndex = 0;
    plate->visible = false;
    add(std::move(plate));
}

void LogoScreen::leave() {
    m_sceneReady = false;
    // the stage drew with no fog every screen after it keeps the fog of the renderer
    if (m_sceneTried) m_app.renderer().set_fog(true);
}

// sub 411C00 the plate is black at the ends of its pair of fades
float LogoScreen::plateAlpha() const {
    if (m_time < kPlateFade) return m_time / kPlateFade;
    if (m_time < kPlateFade + kPlateHold) return 1.f;
    const float out = (m_time - kPlateFade - kPlateHold) / kPlateFade;
    return out >= 1.f ? 0.f : 1.f - out;
}

void LogoScreen::update(float dt) {
    m_frameDt = dt;
    m_time += dt;
    if (m_stage == Stage::Intro) {
        m_intro += dt;
        // a walk that stops here holds the stage a hand run takes the stock seconds
        if (m_app.options().autoWalk && !m_app.autoLeaves("intro")) return;
        const float budget = m_app.autoLeaves("intro") ? kAutoIntroSeconds : kIntroSeconds;
        if (m_intro > budget) leaveToLogin();
        return;
    }
    // an auto walk or a capture run keeps the old short logo so the staged runs do not slow down
    if (m_app.autoLeaves("logo") && m_time > kAutoIntroSeconds) { goToIntro(); return; }
    if (m_time < kPlateSeconds) return;
    if (m_stage == Stage::Ogp) {
        m_stage = Stage::Rnr;
        m_time = 0.f;
        if (ImageWidget* plate = findAs<ImageWidget>("logo_plate"))
            plate->texture = m_app.assets().texture("Logo/RNR_ci.png");
        return;
    }
    if (m_time > kPlateSeconds + kPlateTail) goToIntro();
}

void LogoScreen::goToIntro() {
    if (m_stage == Stage::Intro) return;
    m_stage = Stage::Intro;
    m_intro = 0.f;
    if (Widget* plate = find("logo_plate")) plate->visible = false;
    // stage 2 of sub 404410 starts the menu loop the logo plates play nothing
    m_app.playMenuMusic();
}

void LogoScreen::leaveToLogin() {
    if (m_left) return;
    m_left = true;
    m_app.go("login");
}

bool LogoScreen::loadIntro() {
    using namespace KnC::Render;
    namespace fs = std::filesystem;
    m_sceneReady = false;
    const std::string dir = m_app.options().gameDir;
    if (dir.empty()) return false;
    NifModelRequest request;
    request.nif_path = (fs::path(dir) / kIntroNif).string();
    request.texture_dir = (fs::path(dir) / kIntroDir).string();
    // the stock starts the sequences the stream carries it does not force a stopped controller
    request.play_stopped_controllers = true;
    PropModel model;
    std::string error;
    if (!load_prop_model(request, model, error)) {
        std::printf("[intro] %s failed %s\n", request.nif_path.c_str(), error.c_str());
        return false;
    }
    KnC::Tools::resolve_textures(request.texture_dir, model);
    // KNC INTRO DROP leaves the two plates out again a capture aid against the old frame
    if (std::getenv("KNC_INTRO_DROP") != nullptr) {
        std::vector<KnC::Render::PropPart> kept;
        for (KnC::Render::PropPart& part : model.parts) {
            bool drop = false;
            for (const char* name : kDroppedPlate) drop = drop || part.texture_path.find(name) != std::string::npos;
            if (!drop) kept.push_back(part);
        }
        std::printf("[intro] %zu plate part dropped of %zu\n", model.parts.size() - kept.size(), model.parts.size());
        model.parts = kept;
    }
    // a capture aid one or more part indices kept so a black plate can be found
    if (const char* only = std::getenv("KNC_INTRO_PARTS")) {
        std::vector<KnC::Render::PropPart> kept;
        const std::string list = std::string(",") + only + ",";
        for (size_t i = 0; i < model.parts.size(); ++i) {
            if (list.find("," + std::to_string(i) + ",") == std::string::npos) continue;
            kept.push_back(model.parts[i]);
        }
        std::printf("[intro] KNC INTRO PARTS keeps %zu of %zu parts\n", kept.size(), model.parts.size());
        model.parts = kept;
    }
    MapScene empty;
    // every shape of the intro is authored emissive white so a flat ambient over one saturates it
    empty.sun.enabled = false;
    empty.day_night = flat_day_night(HourColour{0.f, 0.f, 0.f});
    empty.day_night.ambient.fill(HourColour{kIntroAmbient, kIntroAmbient, kIntroAmbient});
    SceneRenderer& renderer = m_app.renderer();
    renderer.upload(empty);
    renderer.set_far_plane(kIntroFar);
    renderer.set_fog(false);
    m_model = renderer.append_prop_model(model);
    renderer.restart_prop_particles(m_model, renderer.animation().seconds());
    std::printf("[intro] %s parts %zu at %.0f ms\n", kIntroNif, model.parts.size(), App::uptimeMs());
    m_sceneReady = true;
    return true;
}

void LogoScreen::sceneLost() {
    if (m_sceneTried && !m_sceneReady) return;
    m_sceneTried = true;
    if (!loadIntro()) std::printf("[intro] the intro nif did not load the stage stays black\n");
}

bool LogoScreen::drawScene() {
    if (m_stage != Stage::Intro) return false;
    if (!m_sceneTried) sceneLost();
    if (!m_sceneReady) return false;
    using namespace KnC::Render;
    SceneRenderer& renderer = m_app.renderer();
    // the node sits 1300 units along plus y with the z scale of the stock call
    PropInstance instance;
    instance.model_index = m_model;
    const float world[16] = {1.f, 0.f, 0.f,           0.f, 0.f, 1.f, 0.f, 0.f,
                             0.f, 0.f, kIntroScaleZ,  0.f, 0.f, kIntroDistance, 0.f, 1.f};
    for (int i = 0; i < 16; ++i) instance.world[i] = world[i];
    renderer.set_appended_prop_instances({instance});
    renderer.animation().advance(m_frameDt);
    // the eye stands at the origin looking along plus y with z up the stock aspect is one
    float view[16];
    bx::mtxLookAt(view, bx::Vec3(0.f, 0.f, 0.f), bx::Vec3(0.f, 1.f, 0.f), bx::Vec3(0.f, 0.f, 1.f),
                  bx::Handedness::Right);
    renderer.set_field_of_view(kIntroFov, kIntroFov);
    const float eye[3] = {0.f, 0.f, 0.f};
    renderer.draw(view, eye);
    return true;
}

void LogoScreen::onKey(int key, int action, int mods) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) { m_app.quit(); return; }
    WidgetScreen::onKey(key, action, mods);
    // the stock has no skip key ours takes any key so a hand is never held on the plates
    if (m_stage == Stage::Intro) leaveToLogin();
    else goToIntro();
}

void LogoScreen::onMouseButton(int button, int action, float x, float y) {
    WidgetScreen::onMouseButton(button, action, x, y);
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    if (m_stage == Stage::Intro) leaveToLogin();
    else goToIntro();
}

void LogoScreen::onAction(const std::string& action, Widget&) {
    if (action == "ui_close") { m_app.quit(); return; }
    leaveToLogin();
}

// the plates fade through a black sheet as the stock fade does the intro draws in 3D
void LogoScreen::drawOverlay(DrawContext& ctx) {
    if (m_stage == Stage::Intro) {
        // block 361 of the nif snaps the wallpaper plate on at six seconds the overlay backs it up when dropped
        if (m_intro >= kLateWallpaper && std::getenv("KNC_INTRO_DROP") != nullptr) {
            if (const Texture* wall = m_app.assets().texture("Login/Login01.png"))
                ctx.batch.draw(wall->handle, 0.f, 0.f, m_app.canvasWidth(), m_app.canvasHeight());
        }
        const float fade = m_intro < kIntroFade ? 1.f - m_intro / kIntroFade : 0.f;
        if (fade > 0.f)
            ctx.batch.fill(0.f, 0.f, m_app.canvasWidth(), m_app.canvasHeight(),
                           rgba(0, 0, 0, static_cast<uint8_t>(fade * 255.f)));
        return;
    }
    const ImageWidget* plate = findAs<ImageWidget>("logo_plate");
    if (!plate || !plate->texture) return;
    const uint8_t alpha = static_cast<uint8_t>(std::clamp(plateAlpha(), 0.f, 1.f) * 255.f);
    if (alpha == 0) return;
    ctx.batch.draw(plate->texture->handle, 0.f, 0.f, m_app.canvasWidth(), m_app.canvasHeight(),
                   rgba(255, 255, 255, alpha));
}

}
