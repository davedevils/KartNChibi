// Opens one track folder with the bgfx renderer and drives a ghost car NIF along a recording
#include "engine/render/png_screenshot.h"
#include "engine/render/scene_renderer.h"
#include "tools/replay/ghost_replay.h"
#include "tools/track_scene/ghost_car.h"
#include "tools/track_scene/track_scene.h"

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace KnC::Render;
using namespace KnC::Tools;

namespace {

constexpr float kDegToRad = 3.14159265f / 180.f;
constexpr float kPitchLimit = 1.53f;

struct Options {
    std::string track_dir;
    std::string screenshot_path;
    int      frames = 3;
    uint16_t width = 1280;
    uint16_t height = 720;
    int      hour = 12;
    bool     sun = true;
    bool     collision = true;
    bool     markers = true;
    bool     geometry = false;
    std::string ghost_path;   // KCGR file or raw 28 byte samples car nif is body NIF driven along the ghost Basic 1 when empty
    std::string car_nif;
    std::string driver_nif;   // driver body NIF seated on the car Cosmo when empty false leaves the car empty
    bool     driver = true;
    float    seek = 0.f;      // playback start in seconds camera follows the ghost from behind
    bool     chase = false;
    int      pose_test = 0;   // forces driver state 1 left 2 right 3 back 4 turbo camera turns around ghost at garage distance
    bool     orbit = false;
    float    orbit_deg = 0.f; // where 0 is behind car 90 left side moves driver off seat in car space a depth test aid
    float    driver_offset[3] = {0.f, 0.f, 0.f};
};

bool parse(int argc, char** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--screenshot" && has_value) out.screenshot_path = argv[++i];
        else if (arg == "--frames" && has_value) out.frames = std::atoi(argv[++i]);
        else if (arg == "--hour" && has_value) out.hour = std::atoi(argv[++i]);
        else if (arg == "--size" && has_value) {
            int w = 0, h = 0;
            if (std::sscanf(argv[++i], "%dx%d", &w, &h) != 2 || w <= 0 || h <= 0) return false;
            out.width = static_cast<uint16_t>(w);
            out.height = static_cast<uint16_t>(h);
        }
        else if (arg == "--no-sun") out.sun = false;
        else if (arg == "--no-col") out.collision = false;
        else if (arg == "--no-markers") out.markers = false;
        else if (arg == "--geometry") out.geometry = true;
        else if (arg == "--ghost" && has_value) out.ghost_path = argv[++i];
        else if (arg == "--car" && has_value) out.car_nif = argv[++i];
        else if (arg == "--driver" && has_value) out.driver_nif = argv[++i];
        else if (arg == "--no-driver") out.driver = false;
        else if (arg == "--seek" && has_value) out.seek = static_cast<float>(std::atof(argv[++i]));
        else if (arg == "--chase") out.chase = true;
        else if (arg == "--orbit" && has_value) { out.orbit = true; out.orbit_deg = static_cast<float>(std::atof(argv[++i])); }
        else if (arg == "--driver-offset" && i + 3 < argc) {
            for (int axis = 0; axis < 3; ++axis) out.driver_offset[axis] = static_cast<float>(std::atof(argv[++i]));
        }
        else if (arg == "--pose-test" && has_value) out.pose_test = std::atoi(argv[++i]);
        else if (out.track_dir.empty() && arg[0] != '-') out.track_dir = arg;
        else return false;
    }
    return !out.track_dir.empty();
}

// Position and look direction WASD moves it the right button turns it
class FreeCamera {
public:
    void reset(float x, float y, float z, float heading_degrees) {
        position_[0] = x; position_[1] = y; position_[2] = z;
        // Heading zero drives toward minus x and grows clockwise same axis as the marker box
        yaw_ = 3.14159265f - heading_degrees * kDegToRad;
        // A downward tilt keeps the track surface in frame on a flat start
        pitch_ = -0.3f;
    }
    void look(float yaw_delta, float pitch_delta) {
        yaw_ += yaw_delta;
        pitch_ = std::clamp(pitch_ + pitch_delta, -kPitchLimit, kPitchLimit);
    }
    void move(float forward_amount, float right_amount, float up_amount) {
        float fwd[3];
        forward(fwd);
        float right[3] = {fwd[1], -fwd[0], 0.f};
        const float length = std::sqrt(right[0] * right[0] + right[1] * right[1]);
        if (length > 1e-5f) { right[0] /= length; right[1] /= length; }
        for (int axis = 0; axis < 3; ++axis)
            position_[axis] += fwd[axis] * forward_amount + right[axis] * right_amount;
        position_[2] += up_amount;
    }
    void forward(float out[3]) const {
        out[0] = std::cos(pitch_) * std::cos(yaw_);
        out[1] = std::cos(pitch_) * std::sin(yaw_);
        out[2] = std::sin(pitch_);
    }
    void view(float out[16], float eye[3]) const {
        float fwd[3];
        forward(fwd);
        const float target[3] = {position_[0] + fwd[0], position_[1] + fwd[1], position_[2] + fwd[2]};
        for (int axis = 0; axis < 3; ++axis) eye[axis] = position_[axis];
        bx::mtxLookAt(out, bx::Vec3(position_[0], position_[1], position_[2]),
                      bx::Vec3(target[0], target[1], target[2]), bx::Vec3(0.f, 0.f, 1.f), bx::Handedness::Right);
    }
    const float* position() const { return position_; }

private:
    float position_[3] = {0.f, 0.f, 0.f};
    float yaw_ = 0.f;
    float pitch_ = 0.f;
};

// Turns around a fixed point the left button drags it the wheel dollies it
class OrbitCamera {
public:
    void frame(const float centre[3], float radius) {
        for (int axis = 0; axis < 3; ++axis) target_[axis] = centre[axis];
        distance_ = std::max(radius * 2.2f, 5.f);
        closest_ = std::max(radius * 0.05f, 0.5f);
    }
    void turn(float yaw_delta, float pitch_delta) {
        yaw_ += yaw_delta;
        pitch_ = std::clamp(pitch_ + pitch_delta, -kPitchLimit, kPitchLimit);
    }
    void dolly(float factor) { distance_ = std::max(closest_, distance_ * factor); }
    void view(float out[16], float eye[3]) const {
        const float flat = std::cos(pitch_);
        eye[0] = target_[0] - distance_ * flat * std::cos(yaw_);
        eye[1] = target_[1] - distance_ * flat * std::sin(yaw_);
        eye[2] = target_[2] - distance_ * std::sin(pitch_);
        bx::mtxLookAt(out, bx::Vec3(eye[0], eye[1], eye[2]),
                      bx::Vec3(target_[0], target_[1], target_[2]), bx::Vec3(0.f, 0.f, 1.f), bx::Handedness::Right);
    }

private:
    float target_[3] = {0.f, 0.f, 0.f};
    float yaw_ = 0.6f;
    float pitch_ = -0.35f;
    float distance_ = 20.f;
    float closest_ = 1.f;
};

struct Mouse {
    double x = 0, y = 0;
    float wheel = 0.f;
};

void on_scroll(GLFWwindow* window, double, double y) {
    static_cast<Mouse*>(glfwGetWindowUserPointer(window))->wheel += static_cast<float>(y);
}

void* native_handle(GLFWwindow* window) {
#if defined(_WIN32)
    return glfwGetWin32Window(window);
#elif defined(__APPLE__)
    return glfwGetCocoaWindow(window);
#else
    return reinterpret_cast<void*>(glfwGetX11Window(window));
#endif
}

// True only the frame a key goes from up to down
bool just_pressed(GLFWwindow* window, int key, bool& previous) {
    const bool down = glfwGetKey(window, key) == GLFW_PRESS;
    const bool pressed = down && !previous;
    previous = down;
    return pressed;
}

// Appended marker instances with their model index moved into the renderer own pool
std::vector<PropInstance> append_marker_models(SceneRenderer& renderer, const TrackScene& track) {
    std::vector<size_t> bases;
    bases.reserve(track.marker_models.size());
    for (const PropModel& model : track.marker_models) bases.push_back(renderer.append_prop_model(model));
    std::vector<PropInstance> instances = track.marker_instances;
    for (PropInstance& instance : instances) instance.model_index = bases[instance.model_index];
    return instances;
}

// Walks up from the track folder to Data and names the Basic 1 body there
std::string default_car_nif(const std::string& track_dir) {
    namespace fs = std::filesystem;
    std::error_code ignored;
    fs::path dir = fs::absolute(track_dir, ignored);
    for (int depth = 0; depth < 8 && !dir.empty(); ++depth) {
        const fs::path candidate = dir / "Public" / "Car" / "Body" / "High" / "Basic_1" / "BODY.nif";
        if (fs::exists(candidate, ignored)) return candidate.string();
        if (dir == dir.parent_path()) break;
        dir = dir.parent_path();
    }
    return "";
}

// Walks up from the track folder to Data and names the Cosmo driver body there
std::string default_driver_nif(const std::string& track_dir) {
    namespace fs = std::filesystem;
    std::error_code ignored;
    fs::path dir = fs::absolute(track_dir, ignored);
    for (int depth = 0; depth < 8 && !dir.empty(); ++depth) {
        const fs::path candidate = dir / "Public" / "Driver" / "Body" / "High" / "Cosmo" / "body.nif";
        if (fs::exists(candidate, ignored)) return candidate.string();
        if (dir == dir.parent_path()) break;
        dir = dir.parent_path();
    }
    return "";
}

// The chassis section of the driver pos ini is the car body folder name
std::string chassis_of(const std::string& car_nif) {
    return std::filesystem::path(car_nif).parent_path().filename().string();
}

// The ghost car placed at the pose the yaw turns it around the up axis
void ghost_world(const GhostPose& pose, float out[16]) {
    float rotate[16];
    // Nose lands on minus cos A sin A since bx mtxRotateZ of A turns row vectors by minus A
    bx::mtxRotateZ(rotate, pose.yawDeg * kDegToRad);
    float translate[16];
    bx::mtxTranslate(translate, pose.pos[0], pose.pos[1], pose.pos[2]);
    bx::mtxMul(out, rotate, translate);
}

// Overrides the recorded state so a clip switch can be checked on a run that never drifts
void apply_pose_test(int state, GhostPose& pose, float& speed) {
    if (state == 0) return;
    pose.reversing = state == 3;
    pose.boosting = state == 4;
    pose.turnState = state == 1 || state == 2 ? state : 0;
    if (state == 1 || state == 2) speed = std::max(speed, 10.f);
}

// Sits behind and above the ghost car and looks at it
void chase_view(const GhostPose& pose, float out[16], float eye[3]) {
    // Same forward axis as the free camera reset for one heading value
    const float yaw = 3.14159265f - pose.yawDeg * kDegToRad;
    const float fwd[3] = {std::cos(yaw), std::sin(yaw), 0.f};
    eye[0] = pose.pos[0] - fwd[0] * 9.f;
    eye[1] = pose.pos[1] - fwd[1] * 9.f;
    eye[2] = pose.pos[2] + 3.5f;
    bx::mtxLookAt(out, bx::Vec3(eye[0], eye[1], eye[2]),
                  bx::Vec3(pose.pos[0], pose.pos[1], pose.pos[2] + 1.f), bx::Vec3(0.f, 0.f, 1.f), bx::Handedness::Right);
}

// The garage preview camera 5 2 out 1 9 up turning around the car on the given angle
void orbit_view(const GhostPose& pose, float degrees, float out[16], float eye[3]) {
    // Angle zero stands behind the tail the tail lies on plus x of the body turned by the yaw
    const float angle = (degrees - pose.yawDeg) * kDegToRad;
    eye[0] = pose.pos[0] + std::cos(angle) * 5.2f;
    eye[1] = pose.pos[1] + std::sin(angle) * 5.2f;
    eye[2] = pose.pos[2] + 1.9f;
    bx::mtxLookAt(out, bx::Vec3(eye[0], eye[1], eye[2]),
                  bx::Vec3(pose.pos[0], pose.pos[1], pose.pos[2] + 0.6f), bx::Vec3(0.f, 0.f, 1.f), bx::Handedness::Right);
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse(argc, argv, options)) {
        std::cerr << "usage: map_viewer <track folder> [--screenshot out.png] [--frames N] "
                     "[--size WxH] [--no-sun] [--no-col] [--no-markers] [--geometry] [--hour 0..23]\n"
                     "       [--ghost run.ghost] [--car BODY.nif] [--driver body.nif] [--no-driver]\n"
                     "       [--seek seconds] [--chase] [--pose-test 1..4]\n";
        return 2;
    }

    TrackSceneRequest request;
    request.track_dir = options.track_dir;
    request.load_sun = options.sun;
    request.load_collision = options.collision;
    request.load_markers = options.markers;
    request.load_geometry = options.geometry;

    TrackScene track;
    std::string error;
    if (!load_track_scene(request, track, error)) {
        std::cerr << "[map] " << error << "\n";
        return 1;
    }
    size_t prop_triangles = 0;
    for (const PropModel& model : track.scene.prop_models)
        for (const PropPart& part : model.parts) prop_triangles += part.indices.size() / 3;
    std::cout << "[map] " << track.scene.tile_id << ": " << track.scene.prop_models.size()
              << " prop models, " << track.scene.prop_instances.size() << " instances, "
              << prop_triangles << " triangles, " << track.start_rows.size()
              << " start rows, " << track.marker_instances.size() << " markers\n";

    // The ghost run and the car that drives it loaded before any window shows
    GhostRecording recording;
    GhostCar ghost_car;
    GhostDriver ghost_driver;
    bool has_driver = false;
    std::unique_ptr<GhostPlayback> playback;
    if (!options.ghost_path.empty()) {
        if (!load_ghost_file(options.ghost_path, recording, error)) {
            std::cerr << "[map] " << error << "\n";
            return 1;
        }
        const std::string car_nif =
            options.car_nif.empty() ? default_car_nif(options.track_dir) : options.car_nif;
        if (car_nif.empty()) {
            std::cerr << "[map] no car NIF found above the track, pass --car\n";
            return 1;
        }
        if (!load_ghost_car(car_nif, ghost_car, error)) {
            std::cerr << "[map] " << error << "\n";
            return 1;
        }
        playback = std::make_unique<GhostPlayback>(recording);
        playback->seek(options.seek);
        std::cout << "[map] ghost " << recording.name << ": " << recording.samples.size()
                  << " samples, " << playback->durationSeconds() << " s, car " << car_nif << " with "
                  << ghost_car.wheels.size() << " wheels\n";
        if (options.driver) {
            const std::string driver_nif =
                options.driver_nif.empty() ? default_driver_nif(options.track_dir) : options.driver_nif;
            if (driver_nif.empty()) {
                std::cerr << "[map] no driver NIF found above the track, pass --driver, car stays empty\n";
            } else if (!load_ghost_driver(driver_nif, chassis_of(car_nif), ghost_driver, error)) {
                std::cerr << "[map] " << error << ", car stays empty\n";
            } else {
                has_driver = true;
                std::cout << "[map] driver " << ghost_driver.model.name << " "
                          << ghost_driver.model.parts.size() << " parts, seat "
                          << ghost_driver.seat[0] << " " << ghost_driver.seat[1] << " "
                          << ghost_driver.seat[2]
                          << (ghost_driver.seat_found ? " from driver_pos ini [" + chassis_of(car_nif) + "]"
                                                      : " car origin, no ini row")
                          << ", " << ghost_driver.model.rig.clips.size() << " clips";
                for (const CharacterClip& clip : ghost_driver.model.rig.clips)
                    std::cout << " " << clip.sequence_id << ":" << clip.motion.name;
                std::cout << (ghost_driver.idle_clip < 0 ? ", no idle, rest pose" : "") << "\n";
            }
        }
    }

    if (!glfwInit()) {
        std::cerr << "[map] glfw init failed\n";
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, options.screenshot_path.empty() ? GLFW_TRUE : GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(options.width, options.height, "KnC map viewer",
                                          nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "[map] window failed\n";
        return 1;
    }
    Mouse mouse;
    glfwSetWindowUserPointer(window, &mouse);
    glfwSetScrollCallback(window, on_scroll);

    PngScreenshotCallback screenshot;
    SceneRenderer renderer;
    RendererSetup setup;
    setup.native_window = native_handle(window);
    setup.width = options.width;
    setup.height = options.height;
    setup.callback = &screenshot;
    setup.hour = options.hour;
    setup.vsync = options.screenshot_path.empty();
    if (!renderer.init(setup)) return 1;

    renderer.upload(track.scene);
    const float extent[3] = {track.scene.bounds_max[0] - track.scene.bounds_min[0],
                             track.scene.bounds_max[1] - track.scene.bounds_min[1],
                             track.scene.bounds_max[2] - track.scene.bounds_min[2]};
    const float radius = 0.5f * std::sqrt(extent[0] * extent[0] + extent[1] * extent[1] +
                                          extent[2] * extent[2]);
    renderer.set_far_plane(std::max(kClientFarPlane, radius * 2.2f));

    std::vector<PropInstance> marker_instances;
    bool markers_visible = options.markers && !track.marker_models.empty();
    if (!track.marker_models.empty()) {
        marker_instances = append_marker_models(renderer, track);
        if (markers_visible) renderer.set_appended_prop_instances(marker_instances);
    }

    size_t car_model_index = 0;
    size_t driver_model_index = 0;
    if (playback) {
        car_model_index = renderer.append_prop_model(ghost_car.body);
        for (const PropModel& wheel : ghost_car.wheels) renderer.append_prop_model(wheel);
        for (const PropModel& piece : ghost_car.pieces) renderer.append_prop_model(piece);
        if (has_driver) {
            driver_model_index = track.scene.character_models.size();
            renderer.append_character_model(ghost_driver.model);
        }
    }
    bool ghost_playing = true;
    bool chase = options.chase && playback != nullptr;
    GhostPose ghost_pose;
    // Speed from the pose deltas the clip rule gates the lean side on it
    float ghost_speed = 0.f;
    float ghost_prev_pos[3] = {0.f, 0.f, 0.f};
    bool  ghost_prev_valid = false;
    GhostWheelState ghost_wheels;
    // The clip playing and the clock second it started so its time runs from zero
    int   driver_clip = -1;
    float driver_clip_start = 0.f;

    float start_x = 0.f, start_y = 0.f, start_z = 5.f, start_heading = 0.f;
    if (!track.start_rows.empty()) {
        start_x = track.start_rows[0].x;
        start_y = track.start_rows[0].y;
        start_z = track.start_rows[0].z + 4.f;
        start_heading = track.start_rows[0].heading;
    } else {
        start_x = (track.scene.bounds_min[0] + track.scene.bounds_max[0]) * 0.5f;
        start_y = (track.scene.bounds_min[1] + track.scene.bounds_max[1]) * 0.5f;
        start_z = track.scene.bounds_max[2] + radius * 0.4f;
    }

    FreeCamera free_camera;
    free_camera.reset(start_x, start_y, start_z, start_heading);
    OrbitCamera orbit_camera;
    const float centre[3] = {(track.scene.bounds_min[0] + track.scene.bounds_max[0]) * 0.5f,
                             (track.scene.bounds_min[1] + track.scene.bounds_max[1]) * 0.5f,
                             (track.scene.bounds_min[2] + track.scene.bounds_max[2]) * 0.5f};
    orbit_camera.frame(centre, radius);
    bool use_orbit = false;

    bool prev1 = false, prev2 = false, prev3 = false, prev4 = false, prev5 = false;
    bool prevF = false, prevG = false, prevR = false;
    bool prevP = false, prevC = false, prevHome = false, prevComma = false, prevPeriod = false;

    glfwGetCursorPos(window, &mouse.x, &mouse.y);
    auto previous_time = std::chrono::steady_clock::now();
    int frame = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - previous_time).count();
        previous_time = now;
        renderer.animation().advance(dt);

        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (width > 0 && height > 0 &&
            (width != renderer.viewport().width || height != renderer.viewport().height))
            renderer.resize(static_cast<uint16_t>(width), static_cast<uint16_t>(height));

        double x = 0, y = 0;
        glfwGetCursorPos(window, &x, &y);
        const float delta_x = static_cast<float>(x - mouse.x);
        const float delta_y = static_cast<float>(y - mouse.y);
        mouse.x = x;
        mouse.y = y;

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            use_orbit = false;
            free_camera.look(delta_x * 0.003f, -delta_y * 0.003f);
        }
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            use_orbit = true;
            orbit_camera.turn(delta_x * 0.005f, -delta_y * 0.005f);
        }
        if (mouse.wheel != 0.f) {
            orbit_camera.dolly(std::pow(0.85f, mouse.wheel));
            mouse.wheel = 0.f;
        }

        const float speed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 4.f : 1.f) *
                            20.f * dt;
        float forward_amount = 0.f, right_amount = 0.f, up_amount = 0.f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) forward_amount += speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) forward_amount -= speed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) right_amount -= speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) right_amount += speed;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) up_amount += speed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) up_amount -= speed;
        if (forward_amount != 0.f || right_amount != 0.f || up_amount != 0.f) {
            use_orbit = false;
            free_camera.move(forward_amount, right_amount, up_amount);
        }

        if (just_pressed(window, GLFW_KEY_R, prevR)) {
            free_camera.reset(start_x, start_y, start_z, start_heading);
            use_orbit = false;
        }
        if (just_pressed(window, GLFW_KEY_1, prev1))
            renderer.set_visible(SceneLayer::Terrain, !renderer.visible(SceneLayer::Terrain));
        if (just_pressed(window, GLFW_KEY_2, prev2))
            renderer.set_visible(SceneLayer::Props, !renderer.visible(SceneLayer::Props));
        if (just_pressed(window, GLFW_KEY_3, prev3))
            renderer.set_visible(SceneLayer::Sky, !renderer.visible(SceneLayer::Sky));
        if (just_pressed(window, GLFW_KEY_4, prev4))
            renderer.set_visible(SceneLayer::Collision, !renderer.visible(SceneLayer::Collision));
        if (just_pressed(window, GLFW_KEY_5, prev5) && !marker_instances.empty()) {
            markers_visible = !markers_visible;
            renderer.set_appended_prop_instances(markers_visible ? marker_instances
                                                                  : std::vector<PropInstance>{});
        }
        if (just_pressed(window, GLFW_KEY_F, prevF)) renderer.set_fog(!renderer.fog_enabled());
        if (just_pressed(window, GLFW_KEY_G, prevG)) renderer.set_glow(!renderer.glow_enabled());

        if (playback) {
            if (just_pressed(window, GLFW_KEY_P, prevP)) ghost_playing = !ghost_playing;
            if (just_pressed(window, GLFW_KEY_C, prevC)) chase = !chase;
            // A jump in time is not a distance driven the wheels and the speed start again
            if (just_pressed(window, GLFW_KEY_HOME, prevHome)) {
                playback->restart();
                ghost_prev_valid = false;
            }
            if (just_pressed(window, GLFW_KEY_COMMA, prevComma)) {
                playback->seek(std::max(0.f, playback->elapsedSeconds() - 2.f));
                ghost_prev_valid = false;
            }
            if (just_pressed(window, GLFW_KEY_PERIOD, prevPeriod)) {
                playback->seek(playback->elapsedSeconds() + 2.f);
                ghost_prev_valid = false;
            }
            if (ghost_playing) playback->advance(dt);
            ghost_pose = playback->pose();
            float car_world[16];
            ghost_world(ghost_pose, car_world);
            // Distance moved since the last frame rolls the wheels and gives the speed
            float moved[3] = {0.f, 0.f, 0.f};
            if (ghost_prev_valid)
                for (int axis = 0; axis < 3; ++axis) moved[axis] = ghost_pose.pos[axis] - ghost_prev_pos[axis];
            for (int axis = 0; axis < 3; ++axis) ghost_prev_pos[axis] = ghost_pose.pos[axis];
            ghost_prev_valid = true;
            if (dt > 1e-4f && ghost_playing)
                ghost_speed = std::sqrt(moved[0] * moved[0] + moved[1] * moved[1] + moved[2] * moved[2]) / dt;
            apply_pose_test(options.pose_test, ghost_pose, ghost_speed);
            ghost_wheels_advance(ghost_car, car_world, moved, ghost_playing ? dt : 0.f, ghost_pose.turnState, ghost_wheels);
            // The car rides with the markers in the appended pool rebuilt every frame
            std::vector<PropInstance> appended =
                markers_visible ? marker_instances : std::vector<PropInstance>{};
            ghost_car_instances(ghost_car, car_model_index, car_world, ghost_wheels, appended);
            renderer.set_appended_prop_instances(appended);
            if (has_driver) {
                // The clip follows the pose its time restarts only when the clip changes
                const int wanted = ghost_driver_clip(ghost_driver, ghost_pose, ghost_speed);
                if (wanted != driver_clip) {
                    driver_clip = wanted;
                    driver_clip_start = renderer.animation().seconds();
                    if (driver_clip >= 0)
                        std::cout << "[map] driver clip " << ghost_driver.model.rig.clips[driver_clip].motion.name
                                  << " at " << playback->elapsedSeconds() << " s\n";
                }
                std::vector<CharacterInstance> riders(1);
                float rider_world[16];
                float offset[16];
                bx::mtxTranslate(offset, options.driver_offset[0], options.driver_offset[1], options.driver_offset[2]);
                bx::mtxMul(rider_world, offset, car_world);
                ghost_driver_instance(ghost_driver, driver_model_index, rider_world, driver_clip,
                                      std::max(0.f, renderer.animation().seconds() - driver_clip_start),
                                      riders[0]);
                renderer.update_character_instances(riders);
            }
        }

        float view[16];
        float eye[3];
        if (playback && chase) chase_view(ghost_pose, view, eye);
        else if (playback && options.orbit) orbit_view(ghost_pose, options.orbit_deg, view, eye);
        else if (use_orbit) orbit_camera.view(view, eye);
        else free_camera.view(view, eye);
        renderer.draw(view, eye);
        if (!options.screenshot_path.empty() && frame == options.frames)
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, options.screenshot_path.c_str());
        bgfx::frame();

        char title[200];
        if (playback)
            std::snprintf(title, sizeof(title), "KnC map viewer  ghost %.1f / %.1f s%s  x %.1f y %.1f z %.1f",
                          playback->elapsedSeconds(), playback->durationSeconds(),
                          ghost_playing ? "" : " paused", eye[0], eye[1], eye[2]);
        else
            std::snprintf(title, sizeof(title), "KnC map viewer  x %.1f y %.1f z %.1f", eye[0], eye[1],
                          eye[2]);
        glfwSetWindowTitle(window, title);

        ++frame;
        if (!options.screenshot_path.empty() && frame > options.frames + 2) break;
    }

    renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    if (!options.screenshot_path.empty()) {
        if (!screenshot.wrote_file()) {
            std::cerr << "[map] no screenshot was written\n";
            return 1;
        }
        std::cout << "[map] wrote " << options.screenshot_path
                  << (screenshot.wrote_one_colour() ? " (one flat colour)" : "") << "\n";
    }
    return 0;
}
