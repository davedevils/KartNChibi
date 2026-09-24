#include "HelpPopup.h"

#include "app/App.h"
#include "assets/AssetStore.h"

#include <GLFW/glfw3.h>

namespace KnC::Client {

namespace {

// sub 45A220 hardcodes 0x99 0xe8 as this popup position
constexpr float kHelpX = 153.f;
constexpr float kHelpY = 232.f;

}

void HelpPopup::draw(SpriteBatch& batch) {
    const Texture* help = m_app.assets().texture("Popup/Help/help.png");
    if (help && help->valid()) batch.draw(help->handle, kHelpX, kHelpY, static_cast<float>(help->width), static_cast<float>(help->height));
    else batch.fill(kHelpX, kHelpY, 709.f, 301.f, rgba(200, 200, 210, 240));
}

// sub 45A150 Escape or F1 close the sheet
void HelpPopup::onKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_F1) close();
}

// no button on stock sheet so any click closes it
void HelpPopup::onMouseButton(int button, int action, float, float) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) close();
}

void HelpPopup::close() {
    if (m_closed) return;
    m_closed = true;
    m_app.popScreen();
}

}
