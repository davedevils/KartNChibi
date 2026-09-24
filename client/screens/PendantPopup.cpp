#include "PendantPopup.h"

#include "ShopCommon.h"
#include "app/App.h"
#include "assets/AssetStore.h"
#include "ui/Widgets.h"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <string>

namespace KnC::Client {

namespace {

// sub 46F0F0 the back at 275 141 the close at 470 549
constexpr float kBackX = 275.f;
constexpr float kBackY = 141.f;
constexpr float kBackW = 486.f;
constexpr float kBackH = 445.f;
constexpr Rect kClose = {470.f, 549.f, 96.f, 29.f};
// sub 46EBC0 six by five cells from 302 197 every 74 by 69 each hit 62 by 56
constexpr int kColumns = 6;
constexpr int kCells = 30;
constexpr float kCellX = 302.f;
constexpr float kCellY = 197.f;
constexpr float kCellStepX = 74.f;
constexpr float kCellStepY = 69.f;
constexpr float kCellW = 62.f;
constexpr float kCellH = 56.f;
// sub 46EF50 the detail back 348 278 icon 372 330 name on 520 290 text at 458 324
constexpr float kInfoX = 348.f;
constexpr float kInfoY = 278.f;
constexpr float kInfoIconX = 372.f;
constexpr float kInfoIconY = 330.f;
constexpr float kInfoNameX = 520.f;
constexpr float kInfoNameY = 290.f;
constexpr float kInfoTextX = 458.f;
constexpr float kInfoTextY = 324.f;
constexpr float kInfoTextW = 220.f;
// Install or Remove half at 425 440 the Shop Popup Close half at 521 440
constexpr Rect kInstall = {425.f, 440.f, 96.f, 29.f};
constexpr Rect kInfoClose = {521.f, 440.f, 96.f, 29.f};
constexpr uint32_t kInk = rgba(0, 0, 0, 255);

void sprite(SpriteBatch& batch, const Texture* t, float x, float y, uint32_t tint = rgba(255, 255, 255, 255)) {
    if (t && t->valid()) batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height), tint);
}

// sub 46EBC0 a cell not owned wears pendant close 00 or 01 a dimmed icon without that art
void lockedCell(SpriteBatch& batch, AssetStore& assets, const PendantDef& d, float x, float y, bool hover) {
    char path[64];
    std::snprintf(path, sizeof(path), "Popup/Pendant/pendant_close_%02d.png", hover ? 1 : 0);
    if (const Texture* t = assets.texture(path)) { sprite(batch, t, x, y); return; }
    sprite(batch, assets.texture("Icon/" + d.iconBase + "_00.png"), x, y, rgba(70, 70, 70, hover ? 200 : 150));
}

void button(SpriteBatch& batch, AssetStore& assets, const std::string& art, const Rect& r, bool over) {
    const Texture* t = assets.texture(art + (over ? "01.png" : "00.png"));
    if (!t) t = assets.texture(art + "00.png");
    sprite(batch, t, r.x, r.y);
}

}

PendantPopup::PendantPopup(App& app) : m_app(app) {}

// sub 46F300 the open clears the lock of the equip
void PendantPopup::enter() {
    m_app.session().unlockPendant();
    std::printf("[pendant] box open %zu defs %zu owned worn %u\n", m_app.session().pendantDefs().size(),
                m_app.session().ownedPendants().size(), m_app.session().profile().pendantKey);
}

int PendantPopup::cellAt(float x, float y) const {
    for (int i = 0; i < kCells; ++i) {
        const float cx = kCellX + kCellStepX * static_cast<float>(i % kColumns);
        const float cy = kCellY + kCellStepY * static_cast<float>(i / kColumns);
        if (x >= cx && x < cx + kCellW && y >= cy && y < cy + kCellH) return i;
    }
    return -1;
}

void PendantPopup::draw(SpriteBatch& batch) {
    AssetStore& assets = m_app.assets();
    Session& session = m_app.session();
    const std::vector<PendantDef>& defs = session.pendantDefs();
    const Texture* back = assets.texture("Popup/Pendant/pendant_back.png");
    if (back && back->valid()) sprite(batch, back, kBackX, kBackY);
    else batch.fill(kBackX, kBackY, kBackW, kBackH, rgba(200, 200, 210, 240));

    // the draw walks the visible rows only the owned ones in colour the rest locked
    const int hover = m_detail < 0 ? cellAt(m_mouseX, m_mouseY) : -1;
    int cell = 0;
    for (const PendantDef& d : defs) {
        if (!d.visible) continue;
        if (cell >= kCells) break;
        const float x = kCellX + kCellStepX * static_cast<float>(cell % kColumns);
        const float y = kCellY + kCellStepY * static_cast<float>(cell / kColumns);
        const bool over = cell == hover;
        if (session.ownsPendant(d.key)) sprite(batch, assets.texture("Icon/" + d.iconBase + (over ? "_01.png" : "_00.png")), x, y);
        else lockedCell(batch, assets, d, x, y, over);
        ++cell;
    }
    button(batch, assets, "Buttons/Common_Close_", kClose, kClose.contains(m_mouseX, m_mouseY));

    if (m_detail < 0 || m_detail >= static_cast<int>(defs.size())) return;
    const PendantDef& d = defs[static_cast<size_t>(m_detail)];
    const bool owned = session.ownsPendant(d.key);
    const bool worn = session.profile().pendantKey == d.key;
    sprite(batch, assets.texture("Popup/Pendant/Pendant_Popup_Back_You.png"), kInfoX, kInfoY);
    if (owned) sprite(batch, assets.texture("Icon/" + d.iconBase + "_00.png"), kInfoIconX, kInfoIconY);
    else lockedCell(batch, assets, d, kInfoIconX, kInfoIconY, false);
    m_app.fontBold().drawCentered(batch, m_app.tr(d.nameKey), kInfoNameX, kInfoNameY, 15.f, kInk);
    float ty = kInfoTextY;
    for (const std::string& line : wrapText(m_app.font(), m_app.tr(d.descKey), 13.f, kInfoTextW)) {
        m_app.font().draw(batch, line, kInfoTextX, ty, 13.f, kInk);
        ty += 16.f;
    }
    // Install on an owned pendant not worn Remove on the worn one Close always
    if (owned && !worn) button(batch, assets, "Buttons/Pendant_Install_half_", kInstall, kInstall.contains(m_mouseX, m_mouseY));
    if (owned && worn) button(batch, assets, "Buttons/Pendant_Remove_half_", kInstall, kInstall.contains(m_mouseX, m_mouseY));
    button(batch, assets, "Buttons/Shop_Popup_Close_half_", kInfoClose, kInfoClose.contains(m_mouseX, m_mouseY));
}

void PendantPopup::onKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key != GLFW_KEY_ESCAPE) return;
    if (m_detail >= 0) m_detail = -1;
    else close();
}

void PendantPopup::onMouseMove(float x, float y) {
    m_mouseX = x;
    m_mouseY = y;
}

void PendantPopup::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    Session& session = m_app.session();
    if (m_detail >= 0) {
        const std::vector<PendantDef>& defs = session.pendantDefs();
        if (kInfoClose.contains(x, y)) { m_app.click(); m_detail = -1; return; }
        if (!kInstall.contains(x, y) || m_detail >= static_cast<int>(defs.size())) return;
        // the lock of sub 483A40 keeps every detail button dead until the 0x0123 ack
        if (session.pendantLocked()) return;
        const PendantDef& d = defs[static_cast<size_t>(m_detail)];
        if (!session.ownsPendant(d.key)) return;
        m_app.click();
        const bool worn = session.profile().pendantKey == d.key;
        session.equipPendant(worn ? -1 : static_cast<int32_t>(d.key));
        return;
    }
    if (kClose.contains(x, y)) { m_app.click(); close(); return; }
    // sub 46EDB0 counts every definition hidden or not so the index is the definition order
    const int cell = cellAt(x, y);
    if (cell < 0 || cell >= static_cast<int>(session.pendantDefs().size())) return;
    m_app.click();
    m_detail = cell;
    std::printf("[pendant] detail of key %u\n", session.pendantDefs()[static_cast<size_t>(cell)].key);
}

void PendantPopup::onSession(SessionEvent event) {
    // the ack of 0x0123 swaps Install and Remove on the open detail the draw reads the session
    if (event == SessionEvent::PendantChanged)
        std::printf("[pendant] worn %u lock %d\n", m_app.session().profile().pendantKey, m_app.session().pendantLocked() ? 1 : 0);
}

// sub 46EB70 the close clears the lock too
void PendantPopup::close() {
    if (m_closed) return;
    m_closed = true;
    m_app.session().unlockPendant();
    m_app.popScreen();
}

}
