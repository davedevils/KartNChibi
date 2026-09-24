// Reads and writes the client UI JSON files
#pragma once

#include "ui_types.h"

#include <string>

namespace KnC::Tools {

// The JSON file that holds one state searched in the known folders
std::string json_path_for_state(int state);

bool load_screen_from_json(UIScreen& screen, const std::string& path);

// The file name the Save command writes into the working folder
std::string default_export_name(const UIScreen& screen, int state);

bool export_screen_json(const UIScreen& screen, int state, const std::string& path);

}
