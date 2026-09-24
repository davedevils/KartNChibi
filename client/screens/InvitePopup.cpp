#include "InvitePopup.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "net/Utf.h"
#include "ui/MenuFrame.h"
#include "ui/Widgets.h"

#include <GLFW/glfw3.h>

namespace KnC::Client {

namespace {

// back 353x177 centred per json accept decline 131 wide x plus40 or plus182 y plus130
constexpr float kBoxW = 353.f;
constexpr float kBoxH = 177.f;
constexpr float kBoxX = 335.f;
constexpr float kBoxY = 295.f;
constexpr Rect kAccept = {kBoxX + 40.f, kBoxY + 130.f, 131.f, 28.f};
constexpr Rect kDecline = {kBoxX + 182.f, kBoxY + 130.f, 131.f, 28.f};

}

void InvitePopup::draw(SpriteBatch& batch) {
    AssetStore& assets = m_app.assets();
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    // exe names Invite folder but pak only ships SmallTalk twin art
    const Texture* back = assets.texture("Popup/Invite/UI_popup_information_back.png");
    if (!back) back = assets.texture("Popup/SmallTalk/UI_popup_information_back.png");
    if (back && back->valid()) batch.draw(back->handle, kBoxX, kBoxY, kBoxW, kBoxH);
    else batch.fill(kBoxX, kBoxY, kBoxW, kBoxH, rgba(230, 230, 236, 245));
    const std::string who = u16ToUtf8(m_invite.inviter);
    drawAligned(ctx, ctx.bold, who + " invites you to room " + std::to_string(m_invite.roomId + 1), kBoxX + kBoxW * 0.5f, kBoxY + 58.f, 15.f, kInkDark, Align::Centre);
    drawAligned(ctx, ctx.bold, "Do you accept the invitation?", kBoxX + kBoxW * 0.5f, kBoxY + 80.f, 15.f, kInkDark, Align::Centre);
    auto button = [&](const char* art, const Rect& r, const char* label) {
        const bool over = r.contains(m_mouseX, m_mouseY);
        const Texture* t = assets.texture(std::string(art) + (over ? "01.png" : "00.png"));
        if (!t) t = assets.texture(std::string(art) + "00.png");
        if (t && t->valid()) batch.draw(t->handle, r.x, r.y, static_cast<float>(t->width), static_cast<float>(t->height));
        else ctx.bold.drawCentered(batch, label, r.x + r.w * 0.5f, r.y + 8.f, 15.f, kInkDark);
    };
    button("Popup/Invite/bt_accept_", kAccept, "Accept");
    button("Popup/Invite/bt_decline_", kDecline, "Decline");
}

void InvitePopup::onKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) accept();
    else if (key == GLFW_KEY_ESCAPE) decline();
}

void InvitePopup::onMouseMove(float x, float y) {
    m_mouseX = x;
    m_mouseY = y;
}

void InvitePopup::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    if (kAccept.contains(x, y)) { m_app.click(); accept(); }
    else if (kDecline.contains(x, y)) { m_app.click(); decline(); }
}

void InvitePopup::accept() {
    if (m_closed) return;
    m_app.session().joinRoom(m_invite.roomId, m_invite.password);
    close();
}

// answer code 1 means decline
void InvitePopup::decline() {
    if (m_closed) return;
    m_app.session().answerInvite(m_invite.replyKey, 1);
    close();
}

void InvitePopup::close() {
    if (m_closed) return;
    m_closed = true;
    m_app.popScreen();
}

}
