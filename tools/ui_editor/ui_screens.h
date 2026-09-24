// Screen setup the config file and the undo history
#pragma once

#include "ui_types.h"

#include <string>

namespace KnC::Tools {

void        save_config();
std::string load_config();

// Fills one state by hand when no JSON file exists for it
void init_screen_fallback(int state);

// Loads every known state from JSON and falls back where a file is missing
void load_all_screens();

bool reload_screen(int state);

// Switches the game folder and rebuilds every texture the screens hold
void reopen_game_folder(const std::string& game_path);

}
