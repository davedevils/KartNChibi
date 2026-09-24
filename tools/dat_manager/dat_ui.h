// The window layout on dear imgui the tool bar the list the preview and the details
#pragma once

#include "dat_archive.h"
#include "dat_audio.h"
#include "dat_preview.h"

#include "engine/render/scene_renderer.h"

#include <string>

namespace KnC::Tools {

struct App {
    Archive     archive;
    Listing     listing;
    Preview     preview;
    AudioPlayer audio;
    KnC::Render::SceneRenderer* renderer = nullptr;
    int         selected = -1;
    // Path of the folder picked in the tree empty when a file is picked
    std::string selected_folder;
    bool        tree_view = true;
    bool        headless = false;
    bool        quit = false;
    std::string status;
    std::string last_dir;
    char        search[256] = {};
    // Entry path whose folders the tree opens once then the list scrolls to the selection
    std::string reveal;
    bool        scroll_to_selected = false;
    // Where the scene draws this frame the preview area of the window
    KnC::Render::ViewportRect scene_rect;
};

bool open_game_folder(App& app, const std::string& game_dir);
void select_entry(App& app, int index);
void draw_ui(App& app, int width, int height);

}
