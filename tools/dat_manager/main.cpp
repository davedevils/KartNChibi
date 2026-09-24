// KnC dat manager browses the pak archives of a game folder with a preview on bgfx and dear imgui
#include "dat_ui.h"
#include "imgui_bgfx.h"

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

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace KnC::Tools;
using namespace KnC::Render;

namespace {

constexpr bgfx::ViewId kImGuiView = 9;

struct Options {
    std::string game_dir;
    std::string screenshot_path;
    std::string open_path;
    int      frames = 3;
    uint16_t width = 1500;
    uint16_t height = 950;
    // Orbit yaw and pitch in radians for a capture the default is the reset view
    bool  view_set = false;
    float view_yaw = 0.f;
    float view_pitch = 0.f;
};

bool parse(int argc, char** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--game" && has_value) out.game_dir = argv[++i];
        else if (arg == "--screenshot" && has_value) out.screenshot_path = argv[++i];
        else if (arg == "--open" && has_value) out.open_path = argv[++i];
        else if (arg == "--frames" && has_value) out.frames = std::atoi(argv[++i]);
        else if (arg == "--view" && has_value) {
            if (std::sscanf(argv[++i], "%f,%f", &out.view_yaw, &out.view_pitch) != 2) return false;
            out.view_set = true;
        }
        else if (arg == "--size" && has_value) {
            int w = 0, h = 0;
            if (std::sscanf(argv[++i], "%dx%d", &w, &h) != 2 || w <= 0 || h <= 0) return false;
            out.width = static_cast<uint16_t>(w);
            out.height = static_cast<uint16_t>(h);
        }
        else return false;
    }
    return true;
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

bool same_rect(const ViewportRect& a, const ViewportRect& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

}

int main(int argc, char** argv) {
    Options options;
    if (!parse(argc, argv, options)) {
        std::cerr << "usage: dat_manager [--game <client folder>] [--open <entry path>] "
                     "[--screenshot out.png] [--frames N] [--size WxH]\n";
        return 2;
    }
    const bool headless = !options.screenshot_path.empty();
    std::printf("=== KnC DAT Manager (bgfx) ===\n");

    if (!glfwInit()) {
        std::cerr << "dat_manager: glfw init failed\n";
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, headless ? GLFW_FALSE : GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(options.width, options.height, "KnC DAT Manager", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "dat_manager: window failed\n";
        glfwTerminate();
        return 1;
    }

    PngScreenshotCallback screenshot;
    SceneRenderer renderer;
    RendererSetup setup;
    setup.native_window = native_handle(window);
    setup.width = options.width;
    setup.height = options.height;
    setup.callback = &screenshot;
    setup.vsync = !headless;
    if (!renderer.init(setup)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    if (!imgui_bgfx_create(window, 15.f)) {
        std::cerr << "dat_manager: imgui setup failed\n";
        renderer.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    App app;
    app.renderer = &renderer;
    app.headless = headless;
    if (!headless) app.audio.init();
    app.preview.attach(&renderer, &app.archive, &app.audio);

    const std::string game_dir = options.game_dir.empty() ? load_game_dir_setting() : options.game_dir;
    if (!game_dir.empty()) open_game_folder(app, game_dir);
    else app.status = "Pick the game folder with Change Folder";
    int exit_code = 0;
    if (!options.open_path.empty()) {
        const int index = app.archive.find_path(options.open_path);
        if (index < 0) {
            std::cerr << "dat_manager: no entry " << options.open_path << " in the pak\n";
            exit_code = 1;
        } else {
            select_entry(app, index);
            app.reveal = Archive::clean_path(app.archive.entries()[index].path);
            app.scroll_to_selected = true;
            if (options.view_set) app.preview.camera().set_angles(options.view_yaw, options.view_pitch);
        }
    }

    uint16_t width = options.width;
    uint16_t height = options.height;
    auto previous = std::chrono::steady_clock::now();
    int frame = 0;
    while (exit_code == 0 && !glfwWindowShouldClose(window) && !app.quit) {
        glfwPollEvents();
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - previous).count();
        previous = now;

        int fb_width = 0, fb_height = 0;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 && (fb_width != width || fb_height != height)) {
            width = static_cast<uint16_t>(fb_width);
            height = static_cast<uint16_t>(fb_height);
            renderer.resize(width, height);
        }

        imgui_bgfx_begin_frame(width, height, dt);
        draw_ui(app, width, height);

        if (!same_rect(app.scene_rect, renderer.viewport())) renderer.set_viewport(app.scene_rect);
        // A capture steps the clock one sixtieth per frame so N frames hold N sixtieths of motion
        renderer.animation().advance(headless ? 1.f / 60.f : dt);
        float view[16];
        float eye[3];
        app.preview.camera().view(view, eye);
        renderer.draw(view, eye);
        imgui_bgfx_end_frame(kImGuiView);

        if (headless && frame == options.frames)
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, options.screenshot_path.c_str());
        bgfx::frame();
        ++frame;
        if (headless && frame > options.frames + 2) break;
    }

    app.audio.shutdown();
    app.preview.shutdown();
    imgui_bgfx_destroy();
    renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    if (headless && exit_code == 0) {
        if (!screenshot.wrote_file()) {
            std::cerr << "[dat_manager] no screenshot was written\n";
            return 1;
        }
        std::cout << "[dat_manager] wrote " << options.screenshot_path
                  << (screenshot.wrote_one_colour() ? " (one flat colour)" : "") << "\n";
    }
    return exit_code;
}
