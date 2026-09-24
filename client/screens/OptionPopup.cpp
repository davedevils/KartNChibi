#include "OptionPopup.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "ui/MenuFrame.h"
#include "ui/Widgets.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace KnC::Client {

namespace {

// sub 46D480 origin 0x164 0xa1 the tabs at plus 23 and 121 on 42
constexpr float kGameX = 356.f;
constexpr float kGameY = 161.f;
// sub 45B8D0 origin 0x153 0x83
constexpr float kInputX = 339.f;
constexpr float kInputY = 131.f;
constexpr float kBackW = 289.f;
constexpr float kBackH = 442.f;
constexpr Rect kTabGraphic = {23.f, 42.f, 99.f, 36.f};
constexpr Rect kTabSound = {121.f, 42.f, 99.f, 36.f};
// the bottom row OK half at plus 50 Cancel half at plus 145 both on 405 Default on 359
constexpr Rect kOk = {50.f, 405.f, 96.f, 29.f};
constexpr Rect kCancel = {145.f, 405.f, 96.f, 29.f};
constexpr Rect kDefaultGame = {25.f, 359.f, 86.f, 26.f};
constexpr Rect kDefaultInput = {26.f, 359.f, 86.f, 26.f};
constexpr float kArrowLeftX = 28.f;
constexpr float kArrowRightX = 237.f;
constexpr float kArrowSize = 24.f;
constexpr float kCheckSize = 12.f;
// graphic checks at plus 36 on 101 124 147 sound mutes at plus 31 on 117 198 279
constexpr float kGraphicCheckX = 36.f;
constexpr float kGraphicCheckY[3] = {101.f, 124.f, 147.f};
constexpr float kSoundCheckX = 31.f;
constexpr float kSoundCheckY[3] = {117.f, 198.f, 279.f};
// the resolution and quality plates at plus 48 on 204 and 277 their arrows one pixel higher
constexpr float kPlateX = 48.f;
constexpr float kResolutionY = 204.f;
constexpr float kQualityY = 277.f;
// the sound bars 13 wide every 15 from plus 56 on 139 220 302 their arrows one pixel higher
constexpr float kBarX = 56.f;
constexpr float kBarStep = 15.f;
constexpr float kBarY[3] = {139.f, 220.f, 302.f};
constexpr int kBarMax = 12;
// sub 45B3C0 key sheet at plus 27 112 rows from 117 every 27 name on 203 box 157 to 250
constexpr float kKeySheetX = 27.f;
constexpr float kKeySheetY = 112.f;
constexpr float kRowY = 117.f;
constexpr float kRowStep = 27.f;
constexpr float kRowBoxX = 157.f;
constexpr float kRowBoxRight = 250.f;
constexpr float kRowTextX = 203.f;
constexpr float kPadUseX = 31.f;
constexpr float kPadUseY = 95.f;
constexpr Rect kPadCheck = {38.f, 101.f, 12.f, 12.f};
// the edited row blinks 100 ms on and 150 ms off
constexpr float kBlinkOn = 0.1f;
constexpr float kBlinkOff = 0.15f;

void sprite(SpriteBatch& batch, AssetStore& assets, const std::string& path, float x, float y) {
    const Texture* t = assets.texture(path);
    if (t && t->valid()) batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
}

// a two state button by its art the hover art while the mouse is over it
void button(SpriteBatch& batch, AssetStore& assets, const std::string& art, const Rect& r, bool over) {
    const Texture* t = assets.texture(art + (over ? "01.png" : "00.png"));
    if (!t) t = assets.texture(art + "00.png");
    if (t && t->valid()) batch.draw(t->handle, r.x, r.y, static_cast<float>(t->width), static_cast<float>(t->height));
}

Rect at(float ox, float oy, const Rect& r) { return {ox + r.x, oy + r.y, r.w, r.h}; }

int roundBars(float volume) {
    return std::clamp(static_cast<int>(volume * 12.f + 0.5f), 0, kBarMax);
}

}

GameOptionPopup::GameOptionPopup(App& app) : m_app(app), m_edit(app.gameOptions()) {
    barsFromVolumes();
}

void GameOptionPopup::barsFromVolumes() {
    m_bars[0] = roundBars(m_edit.bgm);
    m_bars[1] = roundBars(m_edit.effect);
    m_bars[2] = roundBars(m_edit.car);
}

// sub 448C30 on every arrow and every mute the three channels follow the edit before the OK
void GameOptionPopup::applyBgm() {
    GameOptions& live = m_app.gameOptions();
    live.bgm = m_edit.bgm;
    live.bgmOff = m_edit.bgmOff;
    live.effect = m_edit.effect;
    live.effectOff = m_edit.effectOff;
    live.car = m_edit.car;
    live.carOff = m_edit.carOff;
    m_app.applySoundOptions();
}

void GameOptionPopup::draw(SpriteBatch& batch) {
    AssetStore& assets = m_app.assets();
    const float ox = kGameX;
    const float oy = kGameY;
    const Texture* back = assets.texture(m_tab == 0 ? "Popup/Option/Option_Graphic_Back.png" : "Popup/Option/Option_Sound_Back.png");
    if (back && back->valid()) batch.draw(back->handle, ox, oy, static_cast<float>(back->width), static_cast<float>(back->height));
    else batch.fill(ox, oy, kBackW, kBackH, rgba(200, 200, 210, 240));
    const Rect tabG = at(ox, oy, kTabGraphic);
    const Rect tabS = at(ox, oy, kTabSound);
    // the open tab is drawn last so its white plate sits over the neighbour
    if (m_tab == 0) button(batch, assets, "Popup/Option/Option_Tab_Sound_", tabS, tabS.contains(m_mouseX, m_mouseY));
    else button(batch, assets, "Popup/Option/Option_Tab_Graphic_", tabG, tabG.contains(m_mouseX, m_mouseY));
    if (m_tab == 0) button(batch, assets, "Popup/Option/Option_Tab_Graphic_", tabG, true);
    else button(batch, assets, "Popup/Option/Option_Tab_Sound_", tabS, true);
    if (m_tab == 0) {
        const bool checks[3] = {m_edit.windowMode > 0.f, m_edit.randomInvite > 0.f, m_edit.motionBlur > 0.f};
        for (int i = 0; i < 3; ++i) {
            if (checks[i]) sprite(batch, assets, "Popup/Option/Common_back_Check.png", ox + kGraphicCheckX, oy + kGraphicCheckY[i]);
        }
        const int wide = m_edit.wideMode > 0.f ? 1 : 0;
        sprite(batch, assets, wide ? "Popup/Option/Option_Resolution_Wide.png" : "Popup/Option/Option_Resolution_Nomal.png", ox + kPlateX, oy + kResolutionY);
        const int detail = std::clamp(static_cast<int>(m_edit.detail), 0, 2);
        static const char* const kQuality[3] = {"Popup/Option/Option_Quality_Low.png", "Popup/Option/Option_Quality_Medium.png", "Popup/Option/Option_Quality_High.png"};
        sprite(batch, assets, kQuality[detail], ox + kPlateX, oy + kQualityY);
        for (float y : {kResolutionY - 1.f, kQualityY - 1.f}) {
            const Rect l = {ox + kArrowLeftX, oy + y, kArrowSize, kArrowSize};
            const Rect r = {ox + kArrowRightX, oy + y, kArrowSize, kArrowSize};
            button(batch, assets, "Buttons/Common_Page_Left_", l, l.contains(m_mouseX, m_mouseY));
            button(batch, assets, "Buttons/Common_Page_Right_", r, r.contains(m_mouseX, m_mouseY));
        }
    } else {
        const float offs[3] = {m_edit.bgmOff, m_edit.effectOff, m_edit.carOff};
        for (int i = 0; i < 3; ++i) {
            if (offs[i] > 0.f) sprite(batch, assets, "Popup/Option/Common_back_Check.png", ox + kSoundCheckX, oy + kSoundCheckY[i]);
            for (int b = 0; b < m_bars[i]; ++b) sprite(batch, assets, "Popup/Option/Option_Sound_Bar.png", ox + kBarX + kBarStep * static_cast<float>(b), oy + kBarY[i]);
            const Rect l = {ox + kArrowLeftX, oy + kBarY[i] - 1.f, kArrowSize, kArrowSize};
            const Rect r = {ox + kArrowRightX, oy + kBarY[i] - 1.f, kArrowSize, kArrowSize};
            button(batch, assets, "Buttons/Common_Page_Left_", l, l.contains(m_mouseX, m_mouseY));
            button(batch, assets, "Buttons/Common_Page_Right_", r, r.contains(m_mouseX, m_mouseY));
        }
    }
    const Rect def = at(ox, oy, kDefaultGame);
    const Rect ok = at(ox, oy, kOk);
    const Rect cancel = at(ox, oy, kCancel);
    button(batch, assets, "Popup/Option/Common_Default_", def, def.contains(m_mouseX, m_mouseY));
    button(batch, assets, "Buttons/Common_OK_half_", ok, ok.contains(m_mouseX, m_mouseY));
    button(batch, assets, "Buttons/Common_Cancel_half_", cancel, cancel.contains(m_mouseX, m_mouseY));
}

// sub 46DB40 Enter is the OK Escape the Cancel
void GameOptionPopup::onKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) confirm();
    else if (key == GLFW_KEY_ESCAPE) cancel();
}

void GameOptionPopup::onMouseMove(float x, float y) {
    m_mouseX = x;
    m_mouseY = y;
}

// sub 46DB70 the buttons and sub 46CAB0 the tabs and the checks
void GameOptionPopup::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    const float ox = kGameX;
    const float oy = kGameY;
    if (at(ox, oy, kTabGraphic).contains(x, y)) { m_app.click(); m_tab = 0; return; }
    if (at(ox, oy, kTabSound).contains(x, y)) { m_app.click(); m_tab = 1; return; }
    if (at(ox, oy, kOk).contains(x, y)) { m_app.click(); confirm(); return; }
    if (at(ox, oy, kCancel).contains(x, y)) { m_app.click(); cancel(); return; }
    if (at(ox, oy, kDefaultGame).contains(x, y)) {
        m_app.click();
        if (m_tab == 0) m_edit.defaultGraphic();
        else { m_edit.defaultSound(); barsFromVolumes(); applyBgm(); }
        return;
    }
    if (m_tab == 0) {
        for (int i = 0; i < 3; ++i) {
            const Rect c = {ox + kGraphicCheckX, oy + kGraphicCheckY[i], kCheckSize, kCheckSize};
            if (!c.contains(x, y)) continue;
            m_app.click();
            float& v = i == 0 ? m_edit.windowMode : i == 1 ? m_edit.randomInvite : m_edit.motionBlur;
            v = v > 0.f ? 0.f : 1.f;
            return;
        }
        for (int row = 0; row < 2; ++row) {
            const float ry = oy + (row == 0 ? kResolutionY : kQualityY) - 1.f;
            const Rect l = {ox + kArrowLeftX, ry, kArrowSize, kArrowSize};
            const Rect r = {ox + kArrowRightX, ry, kArrowSize, kArrowSize};
            const int step = l.contains(x, y) ? -1 : r.contains(x, y) ? 1 : 0;
            if (step == 0) continue;
            m_app.click();
            // the wide mode walks 0 and 1 the detail 0 to 2 both stop at their ends
            if (row == 0) m_edit.wideMode = static_cast<float>(std::clamp(static_cast<int>(m_edit.wideMode) + step, 0, 1));
            else m_edit.detail = static_cast<float>(std::clamp(static_cast<int>(m_edit.detail) + step, 0, 2));
            return;
        }
        return;
    }
    for (int i = 0; i < 3; ++i) {
        const Rect c = {ox + kSoundCheckX, oy + kSoundCheckY[i], kCheckSize, kCheckSize};
        if (c.contains(x, y)) {
            m_app.click();
            float& v = i == 0 ? m_edit.bgmOff : i == 1 ? m_edit.effectOff : m_edit.carOff;
            v = v > 0.f ? 0.f : 1.f;
            applyBgm();
            return;
        }
        const Rect l = {ox + kArrowLeftX, oy + kBarY[i] - 1.f, kArrowSize, kArrowSize};
        const Rect r = {ox + kArrowRightX, oy + kBarY[i] - 1.f, kArrowSize, kArrowSize};
        const int step = l.contains(x, y) ? -1 : r.contains(x, y) ? 1 : 0;
        if (step == 0) continue;
        m_app.click();
        m_bars[i] = std::clamp(m_bars[i] + step, 0, kBarMax);
        const float twelfth = static_cast<float>(m_bars[i]) / 12.f;
        // sub 46DB70 bgm and kart take the twelfth the effects eight tenths and a fifth of it
        if (i == 0) m_edit.bgm = twelfth;
        else if (i == 1) m_edit.effect = m_bars[i] == 0 ? 0.f : std::clamp(twelfth * 0.2f + 0.8f, 0.f, 1.f);
        else m_edit.car = twelfth;
        applyBgm();
        return;
    }
}

// sub 46CE40 the ini is written the 0x0130 report carries the deny invitation flag then the box closes
void GameOptionPopup::confirm() {
    if (m_closed) return;
    m_app.gameOptions() = m_edit;
    if (!m_app.gameOptions().save()) std::printf("[options] cannot write %s\n", GameOptions::fileName());
    else std::printf("[options] %s written\n", GameOptions::fileName());
    m_app.applySoundOptions();
    m_app.applyGraphicOptions();
    m_app.session().sendOptionReport(m_edit.randomInvite);
    std::printf("[options] graphic window %s wide %s detail %d motion blur %s deny invite %s\n",
                m_edit.windowMode > 0.f ? "on" : "off", m_edit.wideMode > 0.f ? "on" : "off", m_app.detailLevel(),
                m_edit.motionBlur > 0.f ? "on" : "off", m_edit.randomInvite > 0.f ? "on" : "off");
    close();
}

// sub 46D320 the ini is read back so every edit is undone
void GameOptionPopup::cancel() {
    if (m_closed) return;
    GameOptions back = m_app.gameOptions();
    back.load();
    m_app.gameOptions() = back;
    m_app.applySoundOptions();
    close();
}

void GameOptionPopup::close() {
    if (m_closed) return;
    m_closed = true;
    m_app.popScreen();
}

ControlOptionPopup::ControlOptionPopup(App& app) : m_app(app), m_edit(app.bindings()) {}

void ControlOptionPopup::update(float dt) {
    m_blink += dt;
    if (m_blinkOn && m_blink >= kBlinkOn) { m_blinkOn = false; m_blink = 0.f; }
    else if (!m_blinkOn && m_blink >= kBlinkOff) { m_blinkOn = true; m_blink = 0.f; }
}

void ControlOptionPopup::draw(SpriteBatch& batch) {
    AssetStore& assets = m_app.assets();
    const float ox = kInputX;
    const float oy = kInputY;
    const Texture* back = assets.texture("Popup/Input/Input_Back.png");
    if (back && back->valid()) batch.draw(back->handle, ox, oy, static_cast<float>(back->width), static_cast<float>(back->height));
    else batch.fill(ox, oy, kBackW, kBackH, rgba(200, 200, 210, 240));
    sprite(batch, assets, "Popup/Input/Input_Key.png", ox + kKeySheetX, oy + kKeySheetY);
    const Rect tabK = at(ox, oy, kTabGraphic);
    const Rect tabP = at(ox, oy, kTabSound);
    if (m_tab == 0) button(batch, assets, "Popup/Input/Input_Tab_Pad_", tabP, tabP.contains(m_mouseX, m_mouseY));
    else button(batch, assets, "Popup/Input/Input_Tab_Key_", tabK, tabK.contains(m_mouseX, m_mouseY));
    if (m_tab == 0) button(batch, assets, "Popup/Input/Input_Tab_Key_", tabK, true);
    else button(batch, assets, "Popup/Input/Input_Tab_Pad_", tabP, true);
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    if (m_tab == 1) {
        sprite(batch, assets, "Popup/Input/Input_PadUse.png", ox + kPadUseX, oy + kPadUseY);
        if (m_edit.joystick) sprite(batch, assets, "Popup/Input/Common_back_Check.png", ox + kPadCheck.x, oy + kPadCheck.y);
    }
    for (int i = 0; i < 8; ++i) {
        // the pad tab skips the left and right rows the stick gives them
        if (m_tab == 1 && (i == 2 || i == 3)) continue;
        const float y = oy + kRowY + kRowStep * static_cast<float>(i);
        if (m_editRow == i) {
            if (m_blinkOn) sprite(batch, assets, "Popup/Input/UI_keyset_key_back.png", ox + kRowBoxX, y);
            continue;
        }
        char pad[8];
        std::snprintf(pad, sizeof(pad), "b%02d", m_edit.pad[i] + 1);
        const std::string text = m_tab == 0 ? InputBindings::keyName(m_edit.keys[i]) : std::string(pad);
        // the stock text call centres on plus 203 the middle of the white box
        drawAligned(ctx, ctx.bold, text, ox + kRowTextX, y + 3.f, 15.f, kInkDark, Align::Centre);
    }
    const Rect def = at(ox, oy, kDefaultInput);
    const Rect ok = at(ox, oy, kOk);
    const Rect cancel = at(ox, oy, kCancel);
    button(batch, assets, "Popup/Input/Common_Degault_", def, def.contains(m_mouseX, m_mouseY));
    button(batch, assets, "Buttons/Common_OK_half_", ok, ok.contains(m_mouseX, m_mouseY));
    button(batch, assets, "Buttons/Common_Cancel_half_", cancel, cancel.contains(m_mouseX, m_mouseY));
}

// sub 45BBC0 Enter is the OK when no row waits Escape stops the wait or cancels the box
void ControlOptionPopup::onKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    if (m_editRow >= 0) {
        if (key == GLFW_KEY_ESCAPE) { m_editRow = -1; return; }
        // sub 45B3C0 the first key the stock table names lands on the row
        const int vk = InputBindings::vkFromGlfw(key);
        if (vk < 0 || !InputBindings::keyAllowed(vk)) return;
        if (m_tab == 0) m_edit.keys[m_editRow] = vk;
        m_editRow = -1;
        return;
    }
    if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) confirm();
    else if (key == GLFW_KEY_ESCAPE) cancel();
}

void ControlOptionPopup::onMouseMove(float x, float y) {
    m_mouseX = x;
    m_mouseY = y;
}

// sub 45BD60 the three buttons sub 45BC10 the tabs the pad check and the row pick
void ControlOptionPopup::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    const float ox = kInputX;
    const float oy = kInputY;
    if (at(ox, oy, kTabGraphic).contains(x, y)) { m_app.click(); m_tab = 0; m_editRow = -1; return; }
    if (at(ox, oy, kTabSound).contains(x, y)) { m_app.click(); m_tab = 1; m_editRow = -1; return; }
    if (at(ox, oy, kOk).contains(x, y)) { m_app.click(); confirm(); return; }
    if (at(ox, oy, kCancel).contains(x, y)) { m_app.click(); cancel(); return; }
    if (at(ox, oy, kDefaultInput).contains(x, y)) { m_app.click(); m_edit.defaults(); m_editRow = -1; return; }
    if (m_tab == 1 && at(ox, oy, kPadCheck).contains(x, y)) {
        m_app.click();
        // the stock asks the joystick device first ours has none so the flag stays off with its message
        if (!m_edit.joystick) m_app.showMessage(m_app.tr("MSG_DEVICE_JOYSTICK_FAIL"));
        m_edit.joystick = false;
        return;
    }
    for (int i = 0; i < 8; ++i) {
        if (m_tab == 1 && (i == 2 || i == 3)) continue;
        const float ry = oy + kRowY + kRowStep * static_cast<float>(i);
        if (x > ox + kRowBoxX && x < ox + kRowBoxRight && y > ry && y < ry + 23.f) {
            m_editRow = i;
            m_blinkOn = true;
            m_blink = 0.f;
            return;
        }
    }
}

// sub 45B7E0 the ini is written then the box closes
void ControlOptionPopup::confirm() {
    if (m_closed) return;
    m_app.bindings() = m_edit;
    if (!m_app.bindings().save()) std::printf("[options] cannot write %s\n", InputBindings::fileName());
    else std::printf("[options] %s written\n", InputBindings::fileName());
    close();
}

// sub 45B830 the ini is read back
void ControlOptionPopup::cancel() {
    if (m_closed) return;
    InputBindings back = m_app.bindings();
    back.load();
    m_app.bindings() = back;
    close();
}

void ControlOptionPopup::close() {
    if (m_closed) return;
    m_closed = true;
    m_app.popScreen();
}

}
