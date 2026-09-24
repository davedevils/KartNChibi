// Opens one NIF with bgfx drag orbit wheel zoom Escape quits skinned or clip NIF plays via character loader
#include "engine/render/light_rig.h"
#include "engine/render/nif_character_model.h"
#include "engine/render/nif_prop_model.h"
#include "engine/render/png_screenshot.h"
#include "engine/render/scene_renderer.h"

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
#include <cctype>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace KnC::Render;

namespace {

struct Options {
    std::string nif_path;
    std::string texture_dir;
    std::string screenshot_path;
    // KF clips bound on the skeleton the first one plays
    std::vector<std::string> clip_paths;
    int      frames = 3;
    uint16_t width = 1280;
    uint16_t height = 720;
    int      hour = 12;
    bool     cel = false;
    bool     glow = true;
    bool     sun = true;
};

bool parse(int argc, char** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--screenshot" && has_value) out.screenshot_path = argv[++i];
        else if (arg == "--textures" && has_value) out.texture_dir = argv[++i];
        else if (arg == "--clip" && has_value) out.clip_paths.push_back(argv[++i]);
        else if (arg == "--frames" && has_value) out.frames = std::atoi(argv[++i]);
        else if (arg == "--hour" && has_value) out.hour = std::atoi(argv[++i]);
        else if (arg == "--size" && has_value) {
            int w = 0, h = 0;
            if (std::sscanf(argv[++i], "%dx%d", &w, &h) != 2 || w <= 0 || h <= 0) return false;
            out.width = static_cast<uint16_t>(w);
            out.height = static_cast<uint16_t>(h);
        }
        else if (arg == "--cel") out.cel = true;
        else if (arg == "--no-glow") out.glow = false;
        else if (arg == "--no-sun") out.sun = false;
        else if (out.nif_path.empty() && arg[0] != '-') out.nif_path = arg;
        else return false;
    }
    return !out.nif_path.empty();
}

// Z up like the client yaw 0 stands on the minus X side looking along plus X
class OrbitCamera {
public:
    void frame_bound(const ModelBound& bound) {
        for (int axis = 0; axis < 3; ++axis) target_[axis] = bound.center[axis];
        const float radius = bound.radius > 0.f ? bound.radius : 1.f;
        distance_ = radius * 2.6f;
        closest_ = radius * 0.05f;
    }
    void turn(float yaw, float pitch) {
        yaw_ += yaw;
        pitch_ = std::clamp(pitch_ + pitch, -1.53f, 1.53f);
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
    float distance_ = 1.f;
    float closest_ = 0.01f;
};

struct Mouse {
    double x = 0, y = 0;
    float wheel = 0.f;
};

void on_scroll(GLFWwindow* window, double, double y) {
    static_cast<Mouse*>(glfwGetWindowUserPointer(window))->wheel += static_cast<float>(y);
}

std::string lower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// A car ships its texture under BODYCOLOR per colour a track under the map Texture High and Low folders
std::string find_texture(const std::filesystem::path& root, const std::string& wanted) {
    namespace fs = std::filesystem;
    std::error_code ignored;
    if (wanted.empty() || fs::exists(wanted, ignored)) return wanted;
    const std::string name = lower(fs::path(wanted).filename().string());
    std::string found;
    for (const fs::path& base : {root, root.parent_path()}) {
        for (const auto& entry : fs::recursive_directory_iterator(base, ignored)) {
            if (!entry.is_regular_file(ignored)) continue;
            if (lower(entry.path().filename().string()) != name) continue;
            const std::string path = entry.path().string();
            if (found.empty() || lower(path).find("high") != std::string::npos) found = path;
        }
        if (!found.empty()) return found;
    }
    return wanted;
}

void resolve_textures(const std::filesystem::path& root, PropModel& model) {
    for (PropPart& part : model.parts) {
        part.texture_path = find_texture(root, part.texture_path);
        if (!part.environment.texture.empty())
            part.environment.texture = find_texture(root, part.environment.texture);
    }
    for (ParticleSystemDefinition& system : model.particle_systems)
        system.texture_path = find_texture(root, system.texture_path);
}

void resolve_textures(const std::filesystem::path& root, CharacterModel& model) {
    for (SkinnedPart& part : model.parts) part.texture_path = find_texture(root, part.texture_path);
}

// Sphere on the rest pose box the camera frames it like a prop bound
ModelBound character_bound(const CharacterModel& model) {
    ModelBound bound;
    bound.radius = 0.f;
    for (int axis = 0; axis < 3; ++axis) {
        bound.center[axis] = 0.5f * (model.bounds_min[axis] + model.bounds_max[axis]);
        const float reach = 0.5f * (model.bounds_max[axis] - model.bounds_min[axis]);
        bound.radius += reach * reach;
    }
    bound.radius = std::sqrt(bound.radius);
    return bound;
}

// One character with its first clip playing or a prop the scene holds either
bool build_scene(const KnC::NifScene& nif, const Options& options, const std::string& texture_dir,
                 MapScene& scene, ModelBound& bound) {
    std::string error;
    if (nif_has_skin(nif) || !options.clip_paths.empty()) {
        CharacterModelRequest request;
        request.nif_path = options.nif_path;
        request.texture_dir = texture_dir;
        for (const std::string& clip : options.clip_paths) {
            CharacterClipRequest wanted;
            wanted.kf_path = clip;
            request.clips.push_back(wanted);
        }
        CharacterModel character;
        if (!build_character_model(nif, request, character, error)) {
            std::cerr << "model_viewer: " << error << "\n";
            return false;
        }
        resolve_textures(texture_dir, character);
        bound = character_bound(character);
        std::size_t slots = 0;
        for (const BonePalette& palette : character.rig.palettes) slots += palette.nodes.size();
        std::cout << "[viewer] " << character.name << ": " << character.parts.size()
                  << " skinned parts, " << slots << " palette slots, "
                  << character.rig.skeleton.nodes.size() << " skeleton nodes, "
                  << character.rig.clips.size() << " clips";
        if (!character.rig.clips.empty())
            std::cout << ", playing '" << character.rig.clips.front().motion.name << "' "
                      << character.rig.clips.front().motion.duration() << " s";
        std::cout << ", radius " << bound.radius << "\n";
        scene.tile_id = character.name;
        CharacterInstance instance;
        instance.model_index = 0;
        instance.clip = character.rig.clips.empty() ? -1 : 0;
        scene.character_models.push_back(std::move(character));
        scene.character_instances.push_back(instance);
        return true;
    }
    NifModelRequest request;
    request.nif_path = options.nif_path;
    request.texture_dir = texture_dir;
    PropModel model;
    build_prop_model(nif, request, model);
    resolve_textures(texture_dir, model);
    bound = model.bound;
    std::cout << "[viewer] " << model.name << ": " << model.parts.size() << " parts, "
              << model.particle_systems.size() << " particle systems, radius "
              << model.bound.radius << "\n";
    scene.tile_id = model.name;
    scene.prop_models.push_back(std::move(model));
    PropInstance instance;
    instance.model_index = 0;
    instance.layer = SceneLayer::Props;
    scene.prop_instances.push_back(instance);
    return true;
}

// Walks up from the NIF to the Data folder and names Public World Light nif there
std::string find_light_rig(const std::filesystem::path& nif_dir) {
    namespace fs = std::filesystem;
    std::error_code ignored;
    for (fs::path dir = nif_dir; !dir.empty(); dir = dir.parent_path()) {
        const fs::path candidate = dir / "Public" / "World" / "Light.nif";
        if (fs::exists(candidate, ignored)) return candidate.string();
        if (dir == dir.parent_path()) break;
    }
    return std::string();
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

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse(argc, argv, options)) {
        std::cerr << "usage: model_viewer <file.nif> [--screenshot out.png] [--frames N] "
                     "[--textures <dir>] [--clip <anim.kf>] [--size WxH] [--hour 0..23] [--cel] "
                     "[--no-glow] [--no-sun]\n";
        return 2;
    }

    KnC::NifScene nif;
    std::string error;
    if (!KnC::read_nif_scene(options.nif_path, nif, error)) {
        std::cerr << "model_viewer: " << error << "\n";
        return 1;
    }
    const std::filesystem::path nif_dir = std::filesystem::path(options.nif_path).parent_path();
    const std::string texture_dir =
        options.texture_dir.empty() ? nif_dir.string() : options.texture_dir;
    MapScene scene;
    ModelBound bound;
    if (!build_scene(nif, options, texture_dir, scene, bound)) return 1;

    if (!glfwInit()) {
        std::cerr << "model_viewer: glfw init failed\n";
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, options.screenshot_path.empty() ? GLFW_TRUE : GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(options.width, options.height, "KnC scene viewer",
                                          nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "model_viewer: window failed\n";
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

    for (int axis = 0; axis < 3; ++axis) {
        scene.bounds_min[axis] = bound.center[axis] - bound.radius;
        scene.bounds_max[axis] = bound.center[axis] + bound.radius;
    }
    if (options.sun) {
        const std::string rig_path = find_light_rig(nif_dir);
        LightRig rig;
        std::string rig_error;
        if (rig_path.empty()) {
            std::cerr << "[viewer] no Light.nif above " << nif_dir.string() << ", ambient only\n";
        } else if (!load_light_rig(rig_path, rig, rig_error)) {
            std::cerr << "[viewer] " << rig_error << ", ambient only\n";
        } else {
            apply_light_rig(rig, scene);
            std::cout << "[viewer] light rig " << rig_path << " ambient " << rig.ambient.red << " "
                      << rig.ambient.green << " " << rig.ambient.blue << " sun " << rig.sun.colour.red
                      << " " << rig.sun.colour.green << " " << rig.sun.colour.blue << " along "
                      << rig.sun.direction[0] << " " << rig.sun.direction[1] << " "
                      << rig.sun.direction[2] << "\n";
        }
    }
    renderer.upload(scene);
    renderer.set_far_plane(std::max(kClientFarPlane, bound.radius * 8.f));
    renderer.set_cel_shading(options.cel);
    renderer.set_glow(options.glow);

    OrbitCamera camera;
    camera.frame_bound(bound);
    glfwGetCursorPos(window, &mouse.x, &mouse.y);
    auto previous = std::chrono::steady_clock::now();
    int frame = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
        const auto now = std::chrono::steady_clock::now();
        const float seconds = std::chrono::duration<float>(now - previous).count();
        previous = now;
        renderer.animation().advance(seconds);

        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (width > 0 && height > 0 &&
            (width != renderer.viewport().width || height != renderer.viewport().height))
            renderer.resize(static_cast<uint16_t>(width), static_cast<uint16_t>(height));

        double x = 0, y = 0;
        glfwGetCursorPos(window, &x, &y);
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
            camera.turn(static_cast<float>(x - mouse.x) * 0.005f,
                        static_cast<float>(mouse.y - y) * 0.005f);
        mouse.x = x;
        mouse.y = y;
        if (mouse.wheel != 0.f) {
            camera.dolly(std::pow(0.85f, mouse.wheel));
            mouse.wheel = 0.f;
        }

        float view[16];
        float eye[3];
        camera.view(view, eye);
        renderer.draw(view, eye);
        if (!options.screenshot_path.empty() && frame == options.frames)
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, options.screenshot_path.c_str());
        bgfx::frame();
        ++frame;
        if (!options.screenshot_path.empty() && frame > options.frames + 2) break;
    }

    renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    if (!options.screenshot_path.empty()) {
        if (!screenshot.wrote_file()) {
            std::cerr << "[viewer] no screenshot was written\n";
            return 1;
        }
        std::cout << "[viewer] wrote " << options.screenshot_path
                  << (screenshot.wrote_one_colour() ? " (one flat colour)" : "") << "\n";
    }
    return 0;
}
