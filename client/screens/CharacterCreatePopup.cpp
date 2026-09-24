#include "CharacterCreatePopup.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "net/Utf.h"
#include "ui/MessagePopup.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace KnC::Client {

namespace {

// sub 473650 centres Eng Driver Back art of 413x400 on canvas
constexpr float kBackW = 413.f;
constexpr float kBackH = 400.f;
// sub 473A20 portraits 120x190 at plus 45 47 every 189 names centred on plus 105 244
constexpr float kPicX = 45.f;
constexpr float kPicY = 47.f;
constexpr float kPicW = 120.f;
constexpr float kPicH = 190.f;
constexpr float kPicStep = 189.f;
constexpr float kNameX = 105.f;
constexpr float kNameY = 244.f;
// MSG INPUT NAME on plus 207 284 edit box plus 133 322 of 142 OK at plus 159 360
constexpr float kLabelX = 207.f;
constexpr float kLabelY = 284.f;
constexpr float kInputX = 133.f;
constexpr float kInputY = 322.f;
constexpr float kInputW = 142.f;
constexpr float kOkX = 159.f;
constexpr float kOkY = 360.f;
constexpr float kOkW = 96.f;
constexpr float kOkH = 29.f;
// MSG WAIT box from sub 455430 Popup Information at 339 279
constexpr float kWaitX = 339.f;
constexpr float kWaitY = 279.f;
constexpr float kWaitW = 347.f;
constexpr float kWaitH = 170.f;
constexpr uint32_t kInk = rgba(0, 0, 0, 255);
// counter from sub 473A20 breaks at four rows
constexpr int kMaxPicks = 4;
// text box from sub 473730 holds eleven wide characters
constexpr size_t kNickMax = 11;
constexpr size_t kNickMin = 4;

// marks sub 4E15E0 drops from name before search table 0x703278
bool tabooMark(char16_t c) {
    static const char16_t kMarks[] = u" ~`!@#$%^&*()_-+=\\|{}[]:;\"'<,>.?/";
    for (const char16_t* m = kMarks; *m != 0; ++m)
        if (*m == c) return true;
    return false;
}

std::u16string foldName(const std::u16string& in) {
    std::u16string out;
    for (char16_t c : in) {
        if (tabooMark(c)) continue;
        if (c >= u'A' && c <= u'Z') c = static_cast<char16_t>(c - u'A' + u'a');
        out.push_back(c);
    }
    return out;
}

// def taboo from Data lang utf16 file of 0x4E17F0 one word a line blank lines skipped
const std::vector<std::u16string>& tabooWords(App& app) {
    static std::vector<std::u16string> words;
    static bool loaded = false;
    if (loaded) return words;
    loaded = true;
    std::vector<uint8_t> bytes;
    if (!app.assets().readBytes("Data/" + app.options().language + "/def_taboo.txt", bytes)) {
        std::printf("[create] def taboo not found the name check runs on the length alone\n");
        return words;
    }
    std::u16string line;
    for (size_t at = 0; at + 1 < bytes.size(); at += 2) {
        const char16_t c = static_cast<char16_t>(bytes[at] | (bytes[at + 1] << 8));
        if (c == 0x000A) {
            if (!line.empty()) words.push_back(line);
            line.clear();
            continue;
        }
        if (c == 0x000D) continue;
        line.push_back(c);
    }
    if (!line.empty()) words.push_back(line);
    std::printf("[create] def taboo %zu words\n", words.size());
    return words;
}

}

// hand on any popup of run stops auto answer so a script can walk refusals
static bool s_byHand = false;

CharacterCreatePopup::CharacterCreatePopup(App& app) : m_app(app) {}

float CharacterCreatePopup::backX() const { return std::floor((m_app.canvasWidth() - kBackW) * 0.5f); }
float CharacterCreatePopup::backY() const { return std::floor((m_app.canvasHeight() - kBackH) * 0.5f); }

bool CharacterCreatePopup::tabooHit(App& app, const std::u16string& nickname) {
    // stock keeps list as written so upper case word or a word with a mark never matches
    const std::u16string name = foldName(nickname);
    for (const std::u16string& word : tabooWords(app)) {
        if (word.empty()) continue;
        if (name.find(word) != std::u16string::npos) return true;
    }
    return false;
}

std::vector<uint32_t> CharacterCreatePopup::pickKeys() const {
    std::vector<uint32_t> keys;
    for (const DriverRow& d : m_app.session().catalog().drivers()) {
        if (!d.visible || !d.creationPick) continue;
        keys.push_back(d.key);
        if (static_cast<int>(keys.size()) >= kMaxPicks) break;
    }
    return keys;
}

void CharacterCreatePopup::enter() {
    // sub 473730 picks preview driver at random and clears text box on every open
    const std::vector<uint32_t> keys = pickKeys();
    if (!keys.empty()) m_driverKey = keys[static_cast<size_t>(std::rand()) % keys.size()];
    m_nick = s_byHand ? std::string() : m_app.options().autoCreateCharacter;
    m_app.sound().play("popup_caution_snd", 0.6f);
}

void CharacterCreatePopup::update(float dt) {
    m_time += dt;
    const Options& o = m_app.options();
    if (o.autoCreateCharacter.empty() || s_byHand || m_waiting || m_closed) return;
    // auto run captures popup then answers it
    if (m_app.captureMode() && !m_captured && m_time > 0.6f) {
        m_captured = true;
        m_app.captureStage("create");
        return;
    }
    if (m_time > (m_app.captureMode() ? 1.2f : 0.3f)) confirm();
}

void CharacterCreatePopup::refuse(const char* key) {
    m_closed = true;
    m_app.popScreen();
    App& app = m_app;
    std::printf("[create] refused on the client %s\n", key);
    // stage 2 3 and 5 turn OK of this box into a fresh popup its Cancel quits game
    app.pushScreen(std::make_unique<MessagePopup>(app, app.tr(key), [&app]() { app.openCharacterCreate(); },
                                                  [&app]() { app.quit(); }, true));
}

void CharacterCreatePopup::confirm() {
    if (m_waiting || m_closed) return;
    m_app.click();
    const std::u16string nick = utf8ToU16(m_nick);
    const DriverRow* row = m_driverKey != 0 ? m_app.session().catalog().driver(m_driverKey) : nullptr;
    if (!row) { refuse("MSG_UNKNOWN_ERROR"); return; }
    if (nick.empty()) { refuse("MSG_NEED_NAME"); return; }
    if (nick.size() < kNickMin) { refuse("MSG_INVALID_NICK"); return; }
    if (tabooHit(m_app, nick)) { refuse("MSG_INVALID_NICK"); return; }
    m_waiting = true;
    m_app.session().createCharacter(m_driverKey, nick);
    std::printf("[create] 0x0004 driver %u nickname %s\n", m_driverKey, m_nick.c_str());
}

// first hand on popup stops auto answer and empties name stock box always opens empty
void CharacterCreatePopup::takeHand() {
    if (s_byHand) return;
    s_byHand = true;
    m_nick.clear();
}

void CharacterCreatePopup::onSession(SessionEvent event) {
    if (event != SessionEvent::CharacterCreated || m_closed) return;
    // wait box goes app shows result box from sub 479230
    m_closed = true;
    m_app.popScreen();
}

// sub 473C00 Enter is OK no other key reaches popup
void CharacterCreatePopup::onKey(int key, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (m_waiting) return;
    takeHand();
    if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) { confirm(); return; }
    if (key == GLFW_KEY_BACKSPACE && !m_nick.empty()) {
        size_t n = m_nick.size();
        do { --n; } while (n > 0 && (static_cast<unsigned char>(m_nick[n]) & 0xC0) == 0x80);
        m_nick.resize(n);
    }
}

// edit box mode 0 refuses only percent sign holds eleven wide characters
void CharacterCreatePopup::onChar(unsigned codepoint) {
    if (m_waiting) return;
    takeHand();
    if (codepoint < 32 || codepoint == '%') return;
    if (utf8ToU16(m_nick).size() + (codepoint >= 0x10000 ? 2 : 1) > kNickMax) return;
    appendUtf8(m_nick, codepoint);
}

void CharacterCreatePopup::onMouseMove(float x, float y) {
    const Rect ok = {backX() + kOkX, backY() + kOkY, kOkW, kOkH};
    m_okHover = ok.contains(x, y);
}

// sub 473860 portrait picked on press OK button acts on release
void CharacterCreatePopup::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || m_waiting) return;
    takeHand();
    const Rect ok = {backX() + kOkX, backY() + kOkY, kOkW, kOkH};
    if (action == GLFW_PRESS) {
        m_okPressed = ok.contains(x, y);
        const std::vector<uint32_t> keys = pickKeys();
        for (size_t i = 0; i < keys.size(); ++i) {
            const Rect box = {backX() + kPicX + static_cast<float>(i) * kPicStep, backY() + kPicY, kPicW, kPicH};
            if (box.contains(x, y)) { m_driverKey = keys[i]; m_app.click(); return; }
        }
        return;
    }
    if (action != GLFW_RELEASE) return;
    const bool wasPressed = m_okPressed;
    m_okPressed = false;
    if (wasPressed && ok.contains(x, y)) confirm();
}

void CharacterCreatePopup::draw(SpriteBatch& batch) {
    const FontAtlas& font = m_app.font();
    const FontAtlas& bold = m_app.fontBold();
    AssetStore& assets = m_app.assets();
    if (m_waiting) {
        // MSG WAIT has no button answer closes it
        const Texture* box = assets.texture("Popup/Message/Popup_Information.png");
        if (box && box->valid()) batch.draw(box->handle, kWaitX, kWaitY, kWaitW, kWaitH);
        else batch.fill(kWaitX, kWaitY, kWaitW, kWaitH, rgba(230, 230, 235, 245));
        bold.drawCentered(batch, m_app.tr("MSG_WAIT"), kWaitX + kWaitW * 0.5f, kWaitY + 70.f, 15.f, kInk);
        return;
    }
    const Catalog& cat = m_app.session().catalog();
    const float bx = backX();
    const float by = backY();
    const Texture* back = assets.texture("Popup/RegistDriver/Driver_Back.png");
    if (back && back->valid()) batch.draw(back->handle, bx, by, static_cast<float>(back->width), static_cast<float>(back->height));
    else batch.fill(bx, by, kBackW, kBackH, rgba(200, 200, 205, 240));

    // parts driver asset 04 for pick 03 for rest 03 art is dimmed
    const std::vector<uint32_t> keys = pickKeys();
    for (size_t i = 0; i < keys.size(); ++i) {
        const DriverRow* row = cat.driver(keys[i]);
        if (!row) continue;
        const bool selected = keys[i] == m_driverKey;
        const float x = bx + kPicX + static_cast<float>(i) * kPicStep;
        const Texture* art = assets.texture("Parts/driver_" + row->asset + (selected ? "_04.png" : "_03.png"));
        if (art && art->valid()) batch.draw(art->handle, x, by + kPicY, kPicW, kPicH);
        else font.drawCentered(batch, row->asset, x + kPicW * 0.5f, by + kPicY + 80.f, 14.f, kInk);
        bold.drawCentered(batch, m_app.tr(row->nameKey), bx + kNameX + static_cast<float>(i) * kPicStep, by + kNameY, 14.f, kInk);
    }

    bold.drawCentered(batch, m_app.tr("MSG_INPUT_NAME"), bx + kLabelX, by + kLabelY, 14.f, kInk);
    const bool caret = static_cast<int>(m_time * 2.f) % 2 == 0;
    font.drawClipped(batch, m_nick + (caret ? "|" : ""), bx + kInputX, by + kInputY + 6.f, kInputW, 14.f, kInk);

    const char* suffix = m_okPressed ? "02.png" : m_okHover ? "01.png" : "00.png";
    const Texture* ok = assets.texture(std::string("Buttons/Common_OK_") + suffix);
    if (ok && ok->valid()) batch.draw(ok->handle, bx + kOkX, by + kOkY, kOkW, kOkH);
    else bold.drawCentered(batch, "OK", bx + kOkX + kOkW * 0.5f, by + kOkY + 6.f, 16.f, kInk);
}

}
