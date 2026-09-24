// The add element dialog and the asset browser both modal
#pragma once

#include "ui_types.h"

namespace KnC::Tools {

void open_add_element_dialog(int element_type);
void open_asset_browser(BrowserTarget target);

// Draws whichever dialog is open must run after the fixed windows
void draw_dialogs();

}
