// KnC UI layout editor on bgfx glfw and dear imgui
#include "imgui_bgfx.h"
#include "ui_assets.h"
#include "ui_canvas.h"
#include "ui_dialogs.h"
#include "ui_json.h"
#include "ui_screens.h"
#include "ui_screenshot.h"
#include "ui_sidebar.h"

#include "tinyfiledialogs/tinyfiledialogs.h"

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

namespace {

struct Options {
    std::string game_path;
    std::string ui_dir;
    std::string json_path;
    std::string export_path;
    std::string screenshot_path;
    std::string language;
    int state = -1;
    int frames = 3;
    uint16_t width = 1280;
    uint16_t height = 800;
};

bool parse(int argc, char** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--game" && has_value) out.game_path = argv[++i];
        else if (arg == "--ui-dir" && has_value) out.ui_dir = argv[++i];
        else if (arg == "--json" && has_value) out.json_path = argv[++i];
        else if (arg == "--export" && has_value) out.export_path = argv[++i];
        else if (arg == "--screenshot" && has_value) out.screenshot_path = argv[++i];
        else if (arg == "--lang" && has_value) out.language = argv[++i];
        else if (arg == "--state" && has_value) out.state = std::atoi(argv[++i]);
        else if (arg == "--frames" && has_value) out.frames = std::atoi(argv[++i]);
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

// The game folder comes from the command line the ini file or a folder dialog
bool pick_game_folder(const Options& options, bool headless) {
    if (!options.game_path.empty()) {
        g_editor.gamePath = options.game_path;
        return true;
    }
    g_editor.gamePath = load_config();
    if (!g_editor.gamePath.empty() || headless) return !g_editor.gamePath.empty();
    const char* path = tinyfd_selectFolderDialog("Select KnC Game Folder (with pak001.dat)", "");
    if (path == nullptr) return false;
    g_editor.gamePath = path;
    save_config();
    return true;
}

void load_named_json(const std::string& path) {
    UIScreen probe;
    if (!load_screen_from_json(probe, path)) return;
    const int state = probe.state >= 0 && probe.state < STATE_COUNT ? probe.state : g_editor.currentState;
    g_editor.screens[state] = probe;
    g_editor.screens[state].state = state;
    g_editor.currentState = state;
}

}

int main(int argc, char** argv) {
    Options options;
    if (!parse(argc, argv, options)) {
        std::cerr << "usage: ui_editor [--game <client folder>] [--ui-dir <json folder>] [--json <file>] [--state N]\n"
                     "                 [--lang Eng] [--export out.json] [--screenshot out.png] [--frames N] [--size WxH]\n";
        return 2;
    }
    const bool headless = !options.screenshot_path.empty() || !options.export_path.empty();
    const bool show_window = options.screenshot_path.empty() && options.export_path.empty();
    std::printf("=== KnC UI Editor (bgfx) ===\n");

    if (!glfwInit()) {
        std::cerr << "ui_editor: glfw init failed\n";
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, show_window ? GLFW_TRUE : GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(options.width, options.height, "KnC UI Editor", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "ui_editor: window failed\n";
        glfwTerminate();
        return 1;
    }

    UiScreenshotCallback screenshot;
    const uint32_t reset_flags = show_window ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;
    bgfx::Init parameters;
    parameters.type = bgfx::RendererType::Count;
    parameters.resolution.width = options.width;
    parameters.resolution.height = options.height;
    parameters.resolution.reset = reset_flags;
    parameters.platformData.nwh = native_handle(window);
    parameters.callback = &screenshot;
    if (!bgfx::init(parameters)) {
        std::cerr << "ui_editor: bgfx init found no usable backend\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    std::printf("[render] backend %s\n", bgfx::getRendererName(bgfx::getRendererType()));
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x19191eff, 1.f, 0);

    if (!imgui_bgfx_create(window, 15.f)) {
        std::cerr << "ui_editor: imgui setup failed\n";
        bgfx::shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    if (!options.language.empty()) g_editor.language = options.language;
    g_editor.uiDir = options.ui_dir;
    if (pick_game_folder(options, headless)) {
        std::printf("[INIT] game folder %s\n", g_editor.gamePath.c_str());
        open_game_folder(g_editor.gamePath);
    } else {
        std::printf("[INIT] no game folder the canvas shows placeholders\n");
    }
    load_all_screens();
    if (!options.json_path.empty()) load_named_json(options.json_path);
    if (options.state >= 0 && options.state < STATE_COUNT) g_editor.currentState = options.state;
    std::printf("[INIT] editor ready on %s\n", state_name(g_editor.currentState));

    int exit_code = 0;
    if (!options.export_path.empty()) {
        if (!export_screen_json(current_screen(), g_editor.currentState, options.export_path)) exit_code = 1;
    }

    uint16_t width = options.width;
    uint16_t height = options.height;
    auto previous = std::chrono::steady_clock::now();
    int frame = 0;
    const bool run_frames = show_window || !options.screenshot_path.empty();
    while (run_frames && !glfwWindowShouldClose(window) && !g_editor.quitRequested) {
        glfwPollEvents();
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - previous).count();
        previous = now;

        int fb_width = 0, fb_height = 0;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 && (fb_width != width || fb_height != height)) {
            width = static_cast<uint16_t>(fb_width);
            height = static_cast<uint16_t>(fb_height);
            bgfx::reset(width, height, reset_flags);
        }
        bgfx::setViewRect(0, 0, 0, width, height);
        bgfx::touch(0);

        imgui_bgfx_begin_frame(width, height, dt);
        handle_shortcuts();
        draw_menu_bar();
        draw_sidebar();
        draw_toolbar();
        draw_canvas();
        draw_status_bar();
        draw_dialogs();
        imgui_bgfx_end_frame(1);

        if (!options.screenshot_path.empty() && frame == options.frames)
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, options.screenshot_path.c_str());
        bgfx::frame();
        ++frame;
        if (!options.screenshot_path.empty() && frame > options.frames + 2) break;
    }

    destroy_all_textures();
    imgui_bgfx_destroy();
    bgfx::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    if (!options.screenshot_path.empty()) {
        if (!screenshot.wrote_file()) {
            std::cerr << "[ui_editor] no screenshot was written\n";
            return 1;
        }
        std::cout << "[ui_editor] wrote " << options.screenshot_path
                  << (screenshot.wrote_one_colour() ? " (one flat colour)" : "") << "\n";
    }
    return exit_code;
}
