// The game view with selection drag resize pan and zoom plus the tool strip and status bar
#pragma once

#include <dear-imgui/imgui.h>

namespace KnC::Tools {

constexpr ImGuiWindowFlags kFixedWindow = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

// Window rectangles under the main menu bar for the current frame
struct Layout {
    float sidebar_x = 0.f, sidebar_y = 0.f, sidebar_w = 0.f, sidebar_h = 0.f;
    float canvas_x = 0.f, canvas_y = 0.f, canvas_w = 0.f, canvas_h = 0.f;
    float toolbar_x = 0.f, toolbar_y = 0.f, toolbar_w = 0.f, toolbar_h = 0.f;
    float status_x = 0.f, status_y = 0.f, status_w = 0.f, status_h = 0.f;
};

Layout compute_layout();

void draw_canvas();
void draw_toolbar();
void draw_status_bar();

// Keyboard shortcuts read once per frame when no text field or dialog owns the keys
void handle_shortcuts();

}
