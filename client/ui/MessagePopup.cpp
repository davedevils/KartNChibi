#include "MessagePopup.h"

#include "app/App.h"
#include "assets/AssetStore.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <vector>

namespace KnC::Client {

namespace {

// sub 455430 the Information art 347 by 170 at 339 279 the text bold the OK on the bottom row
constexpr float kBoxX = 339.f;
constexpr float kBoxY = 279.f;
constexpr float kBoxW = 347.f;
constexpr float kBoxH = 170.f;
constexpr float kTextX = 22.f;
constexpr float kTextY = 36.f;
constexpr float kTextW = 303.f;
constexpr float kOkX = 125.f;
constexpr float kOkY = 128.f;
// sub 470520 the confirm halves OK at plus 78 Cancel at plus 174 both on 126
constexpr float kHalfOkX = 78.f;
constexpr float kHalfCancelX = 174.f;
constexpr float kHalfY = 126.f;
constexpr float kHalfW = 96.f;
constexpr float kHalfH = 29.f;
constexpr uint32_t kInk = rgba(16, 16, 16, 255);

// wraps on spaces to the width the font gives
std::vector<std::string> wrap(const FontAtlas& font, const std::string& text, float px, float maxWidth) {
    std::vector<std::string> lines;
    std::string line;
    size_t i = 0;
    while (i <= text.size()) {
        const size_t space = text.find(' ', i);
        const std::string word = text.substr(i, space == std::string::npos ? std::string::npos : space - i);
        const std::string probe = line.empty() ? word : line + " " + word;
        if (!line.empty() && font.measure(probe, px) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = probe;
        }
        if (space == std::string::npos) break;
        i = space + 1;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

}

MessagePopup::MessagePopup(App& app, std::string text, std::function<void()> onClose)
    : m_app(app), m_text(std::move(text)), m_onClose(std::move(onClose)) {}

MessagePopup::MessagePopup(App& app, std::string text, std::function<void()> onOk, std::function<void()> onCancel, bool confirm)
    : m_app(app), m_text(std::move(text)), m_onClose(std::move(onCancel)), m_onOk(std::move(onOk)), m_confirm(confirm) {}

void MessagePopup::draw(SpriteBatch& batch) {
    const FontAtlas& font = m_app.fontBold();
    AssetStore& assets = m_app.assets();
    const float x = kBoxX;
    const float y = kBoxY;
    const Texture* back = assets.texture("Popup/Message/Popup_Information.png");
    if (back && back->valid()) batch.draw(back->handle, x, y, kBoxW, kBoxH);
    else batch.fill(x, y, kBoxW, kBoxH, rgba(30, 34, 52, 240));
    const float px = 15.f;
    std::string text = m_text;
    for (size_t at = text.find("\\n"); at != std::string::npos; at = text.find("\\n", at)) text.replace(at, 2, " ");
    const std::vector<std::string> lines = wrap(font, text, px, kTextW);
    float ty = y + kTextY;
    for (const std::string& line : lines) {
        if (ty > y + kOkY - 18.f) break;
        font.drawCentered(batch, line, x + kTextX + kTextW * 0.5f, ty, px, kInk);
        ty += 17.f;
    }
    if (m_confirm) {
        const Texture* ok = assets.texture("Buttons/Common_OK_half_00.png");
        const Texture* cancel = assets.texture("Buttons/Common_Cancel_half_00.png");
        if (ok && ok->valid()) batch.draw(ok->handle, x + kHalfOkX, y + kHalfY, static_cast<float>(ok->width), static_cast<float>(ok->height));
        else font.drawCentered(batch, "OK", x + kHalfOkX + kHalfW * 0.5f, y + kHalfY + 6.f, 14.f, kInk);
        if (cancel && cancel->valid()) batch.draw(cancel->handle, x + kHalfCancelX, y + kHalfY, static_cast<float>(cancel->width), static_cast<float>(cancel->height));
        else font.drawCentered(batch, "Cancel", x + kHalfCancelX + kHalfW * 0.5f, y + kHalfY + 6.f, 14.f, kInk);
        return;
    }
    const Texture* ok = assets.texture("Buttons/Common_OK_00.png");
    if (ok && ok->valid()) batch.draw(ok->handle, x + kOkX, y + kOkY, static_cast<float>(ok->width), static_cast<float>(ok->height));
    else font.drawCentered(batch, "OK", x + kBoxW * 0.5f, y + kOkY + 6.f, 14.f, kInk);
}

void MessagePopup::onKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    if (m_confirm) {
        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) confirmOk();
        else if (key == GLFW_KEY_ESCAPE) close();
        return;
    }
    if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_ESCAPE || key == GLFW_KEY_SPACE) close();
}

void MessagePopup::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    if (!m_confirm) { close(); return; }
    const bool onOk = x >= kBoxX + kHalfOkX && x < kBoxX + kHalfOkX + kHalfW && y >= kBoxY + kHalfY && y < kBoxY + kHalfY + kHalfH;
    const bool onCancel = x >= kBoxX + kHalfCancelX && x < kBoxX + kHalfCancelX + kHalfW && y >= kBoxY + kHalfY && y < kBoxY + kHalfY + kHalfH;
    if (onOk) { m_app.click(); confirmOk(); }
    else if (onCancel) { m_app.click(); close(); }
}

void MessagePopup::confirmOk() {
    if (m_closed) return;
    m_closed = true;
    m_app.popScreen();
    if (m_onOk) m_onOk();
}

void MessagePopup::close() {
    if (m_closed) return;
    m_closed = true;
    m_app.popScreen();
    if (m_onClose) m_onClose();
}

}
