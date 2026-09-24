// Pak and Data folder images decoded into bgfx textures for the canvas
#pragma once

#include "ui_types.h"

#include <string>
#include <vector>

namespace KnC::Tools {

// Opens the pak files of the game folder then lists the image assets
bool open_game_folder(const std::string& game_path);

void scan_available_assets();

// A cached texture for the asset path or null when nothing decodes
const LoadedTexture* load_texture(const std::string& path);

void resolve_element_textures(std::vector<UIElement>& elements);

// Drops the short prefix so the file keeps the client convention
std::string short_asset_path(const std::string& path);

void destroy_all_textures();

}
