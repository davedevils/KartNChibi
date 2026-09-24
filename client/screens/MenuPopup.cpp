#include "MenuPopup.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "screens/HelpPopup.h"
#include "screens/OptionPopup.h"
#include "ui/MessagePopup.h"
#include "ui/Widgets.h"

#include <GLFW/glfw3.h>

namespace KnC::Client {

namespace {

// sub 463A20 back at 0x155 0xee buttons at plus 24 on 51 87 123 159 close at 97 204
constexpr float kBoxX = 341.f;
constexpr float kBoxY = 238.f;
constexpr float kBoxW = 287.f;
constexpr float kBoxH = 240.f;

struct MenuButton {
    const char* art;
    float x;
    float y;
    float w;
    float h;
    const char* action;
};

const MenuButton kButtons[] = {
    {"Popup/Menu/Menu_Help_", 24.f, 51.f, 239.f, 27.f, "help"},
    {"Popup/Menu/Menu_GameSetting_", 24.f, 87.f, 239.f, 27.f, "game"},
    {"Popup/Menu/Menu_ControlSetting_", 24.f, 123.f, 239.f, 27.f, "control"},
    {"Popup/Menu/Menu_End_", 24.f, 159.f, 239.f, 27.f, "end"},
    {"Buttons/Common_Close_", 97.f, 204.f, 96.f, 27.f, "close"},
};

}

void MenuPopup::draw(SpriteBatch& batch) {
    AssetStore& assets = m_app.assets();
    const Texture* back = assets.texture("Popup/Menu/Menu_Back.png");
    if (back && back->valid()) batch.draw(back->handle, kBoxX, kBoxY, static_cast<float>(back->width), static_cast<float>(back->height));
    else batch.fill(kBoxX, kBoxY, kBoxW, kBoxH, rgba(200, 200, 210, 240));
    for (const MenuButton& b : kButtons) {
        const Rect r = {kBoxX + b.x, kBoxY + b.y, b.w, b.h};
        const bool over = r.contains(m_mouseX, m_mouseY);
        const Texture* t = assets.texture(std::string(b.art) + (over ? "01.png" : "00.png"));
        if (!t) t = assets.texture(std::string(b.art) + "00.png");
        if (t && t->valid()) batch.draw(t->handle, r.x, r.y, static_cast<float>(t->width), static_cast<float>(t->height));
    }
}

// sub 463880 Enter and Escape close the box
void MenuPopup::onKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) close();
}

void MenuPopup::onMouseMove(float x, float y) {
    m_mouseX = x;
    m_mouseY = y;
}

// sub 4638F0 every item closes the box first then opens its own sheet the End asks MSG CONFIRM EXIT
void MenuPopup::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    for (const MenuButton& b : kButtons) {
        const Rect r = {kBoxX + b.x, kBoxY + b.y, b.w, b.h};
        if (!r.contains(x, y)) continue;
        m_app.click();
        const std::string act = b.action;
        close();
        if (act == "help") m_app.pushScreen(std::make_unique<HelpPopup>(m_app));
        else if (act == "game") m_app.pushScreen(std::make_unique<GameOptionPopup>(m_app));
        else if (act == "control") m_app.pushScreen(std::make_unique<ControlOptionPopup>(m_app));
        else if (act == "end") {
            App& app = m_app;
            app.pushScreen(std::make_unique<MessagePopup>(app, app.tr("MSG_CONFIRM_EXIT"), [&app]() { app.quit(); }, nullptr, true));
        }
        return;
    }
}

void MenuPopup::close() {
    if (m_closed) return;
    m_closed = true;
    m_app.popScreen();
}

}
