#include "ChannelScreen.h"

#include "app/App.h"
#include "net/Utf.h"
#include "screens/HelpPopup.h"
#include "screens/MenuPopup.h"
#include "ui/MenuFrame.h"
#include "ui/MessagePopup.h"

#include <GLFW/glfw3.h>

#include <algorithm>

namespace KnC::Client {

namespace {

// stock channel class sub 424B00 back at 113 86 bars at 609 in three tiers
constexpr float kBackX = 113.f;
constexpr float kBackY = 86.f;
constexpr float kBarX = 609.f;
constexpr float kBarStep = 27.f;
constexpr float kBoxTop[3] = {204.f, 343.f, 482.f};
constexpr int kBarsPerTier = 4;
const char* const kBarAsset[3] = {"ChannelSelect/channel", "ChannelSelect/channelad", "ChannelSelect/channelms"};
// OK button of stock notice box at 463 657
constexpr float kOkX = 463.f;
constexpr float kOkY = 657.f;

}

void ChannelScreen::enter() {
    loadLayout("ui_state_03_channel.json");
    // JSON parks back at 115 108 stock draws it at 113 86 on menu frame
    if (Widget* w = find("channel_back")) { w->rect.x = kBackX; w->rect.y = kBackY; w->zIndex = 5; }
    if (Widget* w = find("bg_wallpaper")) w->visible = false;
    addMenuFrame(*this, m_app.assets(), FrameMode::Full, "channel");
    if (Widget* w = find("frame_channel")) w->visible = false;
    m_rows.clear();
    m_selected = -1;
    m_time = 0.f;
    m_left = false;
    const std::vector<ChannelRow>& channels = m_app.session().channels();
    int used[3] = {0, 0, 0};
    for (size_t i = 0; i < channels.size(); ++i) {
        const int tier = static_cast<int>(std::min<uint32_t>(channels[i].tier, 2));
        if (used[tier] >= kBarsPerTier) continue;
        const int slot = used[tier]++;
        auto btn = std::make_unique<ButtonWidget>();
        btn->type = ElementType::Button;
        btn->id = "channel_" + std::to_string(i);
        const std::string base = std::string(kBarAsset[tier]) + std::to_string(slot + 1) + "-1_";
        btn->normal = m_app.assets().texture(base + "00.PNG");
        btn->hover = m_app.assets().texture(base + "01.PNG");
        btn->pressed = m_app.assets().texture(base + "02.PNG");
        const Texture* size = btn->normal ? btn->normal : btn->hover;
        btn->rect = {kBarX, kBoxTop[tier] + slot * kBarStep, size ? static_cast<float>(size->width) : 227.f,
                     size ? static_cast<float>(size->height) : 21.f};
        btn->zIndex = 30;
        const int index = static_cast<int>(i);
        btn->onClick = [this, index]() { pick(index); };
        m_rows.push_back({index, static_cast<ButtonWidget*>(add(std::move(btn)))});
    }
    auto ok = std::make_unique<ButtonWidget>();
    ok->type = ElementType::Button;
    ok->id = "btn_ok";
    ok->action = "ok";
    ok->normal = m_app.assets().texture("Buttons/Common_OK_00.png");
    ok->hover = m_app.assets().texture("Buttons/Common_OK_01.png");
    ok->pressed = m_app.assets().texture("Buttons/Common_OK_02.png");
    ok->rect = {kOkX, kOkY, 96.f, 29.f};
    ok->zIndex = 30;
    // stock capture shows no OK under notice button stays for keyboard path
    ok->visible = false;
    add(std::move(ok));
    if (!channels.empty()) m_selected = 0;
}

void ChannelScreen::update(float dt) {
    m_time += dt;
    if (m_app.autoLeaves("channel") && !m_left && m_time > 0.3f) pick(m_selected);
}

// sub 424460 Escape opens Menu box F1 help sheet Enter takes pick
void ChannelScreen::onKey(int key, int action, int mods) {
    if (action == GLFW_PRESS && (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)) { pick(m_selected); return; }
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) { m_app.pushScreen(std::make_unique<MenuPopup>(m_app)); return; }
    if (action == GLFW_PRESS && key == GLFW_KEY_F1) { m_app.pushScreen(std::make_unique<HelpPopup>(m_app)); return; }
    WidgetScreen::onKey(key, action, mods);
}

void ChannelScreen::onAction(const std::string& action, Widget&) {
    if (action == "ok") { pick(m_selected); return; }
    // frame gear and quit from bottom bar stage 4 router keeps other buttons quiet
    if (action == "quit") {
        App& app = m_app;
        app.pushScreen(std::make_unique<MessagePopup>(app, app.tr("MSG_CONFIRM_EXIT"), [&app]() { app.quit(); }, nullptr, true));
        return;
    }
    if (action == "option") frameAction(m_app, action);
}

void ChannelScreen::pick(int index) {
    if (m_left) return;
    const std::vector<ChannelRow>& channels = m_app.session().channels();
    // 0x0018 channel field is row id or minus one when no row
    int32_t id = -1;
    if (index >= 0 && index < static_cast<int>(channels.size())) {
        id = static_cast<int32_t>(channels[index].id);
        m_selected = index;
    }
    m_app.session().selectChannel(id);
    m_left = true;
    m_app.go("menu");
}

void ChannelScreen::drawOverlay(DrawContext& ctx) {
    const std::vector<ChannelRow>& channels = m_app.session().channels();
    // bar art carries its own label stock capture prints no population and no pick wash on it
    (void)channels;
    const std::string notice = u16ToUtf8(m_app.session().notice());
    if (!notice.empty() && m_app.options().statusLine) ctx.font.drawClipped(ctx.batch, notice, 140.f, 600.f, 300.f, 13.f, rgba(40, 30, 10, 255));
}

}
