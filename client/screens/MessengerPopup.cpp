#include "MessengerPopup.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "net/Utf.h"
#include "screens/ShopCommon.h"

#include <GLFW/glfw3.h>

#include <cmath>

#include <cstdio>

namespace KnC::Client {

namespace {

// stock messenger the 418 by 577 list back centred tabs on plus 83 rows from plus 131 every 25
constexpr float kPanelW = 418.f;
constexpr float kPanelH = 577.f;
// the stock capture puts Friend Inbox Block and the invite icon tab on plus 23 121 219 317
constexpr float kTabY = 83.f;
constexpr float kTabX[4] = {23.f, 121.f, 219.f, 318.f};
// the tab order of the art friend note block invite the invite art carries our request rows
constexpr int kTabOfArt[4] = {0, 2, 3, 1};
// the per row bin of the block tab sits at plus 0x152 on the row line
constexpr float kBlockDelX = 338.f;
// the six items of the row menu of FUN 00464A50 at the click plus six and these y steps
constexpr float kMenuDX = 6.f;
constexpr float kMenuY[6] = {8.f, 27.f, 46.f, 65.f, 84.f, 103.f};
constexpr float kMenuW = 96.f;
constexpr float kMenuH = 19.f;
constexpr float kRowX = 65.f;
constexpr float kRowY = 131.f;
constexpr float kRowH = 25.f;
constexpr int kRowsShown = 14;
constexpr float kActionX = 278.f;
// stock capture spots the add note and search icons on one row at plus 19 62 104 on 42
constexpr float kSideX = 19.f;
constexpr float kSideY = 42.f;
constexpr float kSideStep = 43.f;
// the OK button of the capture sits at plus 149 541
constexpr float kOkX = 149.f;
constexpr float kOkY = 541.f;
// the note body and the compose box sit on the Information popup art centred on the screen
constexpr float kInfoW = 347.f;
constexpr float kInfoH = 170.f;
constexpr uint32_t kInk = rgba(16, 16, 16, 255);
constexpr uint32_t kOnline = rgba(0, 0, 255, 255);
constexpr uint32_t kOffline = rgba(96, 96, 96, 255);
// the 0x0073 poll period of the stock messenger
constexpr float kPollSeconds = 3.f;
// our server reads one verb per socket read so the verbs go one at a time
constexpr float kVerbGap = 0.6f;

const char* codeKey(int32_t code) {
    static const char* const kKeys[] = {"MSG_FRIEND_ADD", "MSG_FRIEND_ADD_OK", "MSG_FRIEND_DUP", "MSG_FRIEND_MAX",
                                        "MSG_FRIEND_ACCEPTER_MAX", "MSG_FRIEND_NOUSER", "MSG_FRIEND_DEL", "MSG_NOTE_INPUTUSER",
                                        "MSG_NOTE_INPUTNOTE", "MSG_NOTE_NOUSER", "MSG_NOTE_SEND", "MSG_NOTE_NOSEND"};
    // sub 47B3C0 code 12 queues the request and shows no dialog
    if (code >= 0 && code < 12) return kKeys[code];
    return "";
}

}

MessengerPopup::MessengerPopup(App& app) : m_app(app) {}

float MessengerPopup::panelX() const { return static_cast<float>(static_cast<int>((m_app.canvasWidth() - kPanelW) * 0.5f)); }
float MessengerPopup::panelY() const { return static_cast<float>(static_cast<int>((m_app.canvasHeight() - kPanelH) * 0.5f)); }

void MessengerPopup::enter() {
    const Options& o = m_app.options();
    if (o.acceptFriends) m_tab = Tab::Requests;
    m_pollAt = 0.f;
    m_nextVerbAt = 0.5f;
}

int MessengerPopup::rowCount() const {
    const Session& s = m_app.session();
    switch (m_tab) {
    case Tab::Friends: return static_cast<int>(s.friends().size());
    case Tab::Requests: return static_cast<int>(s.friendRequests().size());
    case Tab::Notes: return static_cast<int>(s.notes().size());
    case Tab::Blocks: return static_cast<int>(s.blocks().size());
    }
    return 0;
}

// the friend tab prints Blocked after a name that stands in the 0x0079 list as swprintf 0x5A53F8 does
bool MessengerPopup::isBlocked(uint32_t playerId) const {
    for (const BlockRow& b : m_app.session().blocks()) if (b.playerId == playerId) return true;
    return false;
}

// the block item of the row menu sends 0x007A with the name of the picked friend
void MessengerPopup::blockAction() {
    Session& session = m_app.session();
    if (m_menuRow < 0 || m_menuRow >= static_cast<int>(session.friends().size())) return;
    const FriendRow& f = session.friends()[static_cast<size_t>(m_menuRow)];
    session.addBlock(f.name);
    m_status = "0x007A block " + u16ToUtf8(f.name);
}

// the user info item of the row menu sends 0x0072 with the name sub 4822D0
void MessengerPopup::userInfoAction() {
    Session& session = m_app.session();
    if (m_menuRow < 0 || m_menuRow >= static_cast<int>(session.friends().size())) return;
    const FriendRow& f = session.friends()[static_cast<size_t>(m_menuRow)];
    session.requestUserInfo(f.name);
    m_cardOpen = true;
    m_status = "0x0072 user info " + u16ToUtf8(f.name);
}

// the invite item of the row menu sends 0x006C with the name sub 481CD0 the room must be open
void MessengerPopup::inviteAction() {
    Session& session = m_app.session();
    if (m_menuRow < 0 || m_menuRow >= static_cast<int>(session.friends().size())) return;
    const FriendRow& f = session.friends()[static_cast<size_t>(m_menuRow)];
    session.inviteToRoom(f.name);
    m_status = "0x006C invite " + u16ToUtf8(f.name);
}

void MessengerPopup::openRowMenu(float x, float y, int row) {
    m_menuOpen = true;
    m_menuX = x;
    m_menuY = y;
    m_menuRow = row;
}

// the six rows of the menu only the note the delete and the block have a verb on our wire
bool MessengerPopup::menuClick(float x, float y) {
    if (!m_menuOpen) return false;
    m_menuOpen = false;
    for (int i = 0; i < 6; ++i) {
        const Rect r = {m_menuX + kMenuDX, m_menuY + kMenuY[i], kMenuW, kMenuH};
        if (!r.contains(x, y)) continue;
        m_app.click();
        m_selected = m_menuRow;
        // FUN 00467A70 case 1 the six ids 0xc user info 0xd invite 0xe note 9 chat 0xa delete 0xb block
        switch (i) {
        case 0: userInfoAction(); break;
        case 1: inviteAction(); break;
        case 2: m_tab = Tab::Notes; primaryAction(); break;
        case 4: m_tab = Tab::Friends; deleteAction(); break;
        case 5: blockAction(); break;
        default: m_status = "the chat item opens the stock smalltalk window which is not in"; break;
        }
        return true;
    }
    return true;
}

void MessengerPopup::selectRow(int row) {
    if (row < 0 || row >= rowCount()) return;
    m_selected = row;
    if (m_tab == Tab::Notes) {
        const NoteRow& n = m_app.session().notes()[static_cast<size_t>(row)];
        if (!n.read) m_app.session().markNoteRead(n.noteId);
    }
}

void MessengerPopup::update(float dt) {
    m_time += dt;
    Session& session = m_app.session();
    if (m_time >= m_pollAt) {
        m_pollAt = m_time + kPollSeconds;
        session.pollFriendStatus();
    }
    const Options& o = m_app.options();
    if (m_autoDone || m_time < m_nextVerbAt) return;
    if (!o.addFriend.empty() && m_status.empty()) {
        session.addFriend(utf8ToU16(o.addFriend));
        m_status = "0x006F sent for " + o.addFriend;
        m_nextVerbAt = m_time + 2.f;
        return;
    }
    if (!o.note.empty() && !o.whisperTo.empty() && !m_noteSent) {
        m_noteSent = true;
        m_tab = Tab::Notes;
        session.sendNote(utf8ToU16(o.whisperTo), utf8ToU16(o.note));
        m_status = "0x0081 sent to " + o.whisperTo;
        m_nextVerbAt = m_time + 2.f;
        return;
    }
    if (o.acceptFriends && !session.friendRequests().empty()) {
        const FriendRequestRow& r = session.friendRequests().front();
        m_status = "0x0070 accept " + u16ToUtf8(r.name);
        session.acceptFriend(r.playerId);
        m_nextVerbAt = m_time + kVerbGap;
        return;
    }
    if (o.acceptFriends && m_tab == Tab::Requests) m_tab = Tab::Friends;
    if (m_app.captureMode() && !m_captured && m_time > 2.5f) {
        m_captured = true;
        // a real run with an inbox opens the first note the sample keeps the Friend tab as the stock
        if (!session.notes().empty() && o.addFriend.empty() && !o.acceptFriends && o.note.empty() && !m_app.sampleMode()) {
            m_tab = Tab::Notes;
            selectRow(0);
        }
        m_app.captureStage("friends");
        if (o.stopAt == "friends") { m_autoDone = true; m_app.finishRun(); }
    }
}

void MessengerPopup::onSession(SessionEvent event) {
    if (event != SessionEvent::SocialChanged) return;
    const int32_t code = m_app.session().lastSocialCode();
    if (code >= 0 && codeKey(code)[0] != '\0') {
        // the table lines break on the two character backslash n the status line is one line
        std::string text = m_app.tr(codeKey(code));
        for (size_t at = text.find("\\n"); at != std::string::npos; at = text.find("\\n", at)) text.replace(at, 2, " ");
        m_status = text + " (" + std::to_string(code) + ")";
    }
    if (m_selected >= rowCount()) m_selected = rowCount() > 0 ? rowCount() - 1 : 0;
}

void MessengerPopup::primaryAction() {
    Session& session = m_app.session();
    switch (m_tab) {
    case Tab::Friends:
        m_input = Input::AddFriend;
        m_text.clear();
        m_status = m_app.tr("MSG_FRIEND_ADD");
        break;
    case Tab::Requests:
        if (m_selected < rowCount()) {
            const FriendRequestRow& r = session.friendRequests()[static_cast<size_t>(m_selected)];
            session.acceptFriend(r.playerId);
            m_status = "0x0070 accept " + u16ToUtf8(r.name);
        }
        break;
    case Tab::Notes:
        m_input = Input::NoteTo;
        m_text.clear();
        m_noteTo.clear();
        m_status = m_app.tr("MSG_NOTE_INPUTUSER");
        break;
    case Tab::Blocks:
        // the stock has no typed add here the block comes off the friend row menu
        m_status = "pick a friend row and use the block item of its menu";
        break;
    }
}

void MessengerPopup::deleteAction() {
    Session& session = m_app.session();
    if (m_selected >= rowCount()) return;
    switch (m_tab) {
    case Tab::Friends: {
        const FriendRow& f = session.friends()[static_cast<size_t>(m_selected)];
        session.deleteFriend(f.playerId);
        m_status = "0x0074 delete " + u16ToUtf8(f.name);
        break;
    }
    case Tab::Requests: {
        const FriendRequestRow& r = session.friendRequests()[static_cast<size_t>(m_selected)];
        session.rejectFriend(r.playerId);
        m_status = "0x0071 reject " + u16ToUtf8(r.name);
        break;
    }
    case Tab::Notes: {
        const NoteRow& n = session.notes()[static_cast<size_t>(m_selected)];
        session.deleteNote(n.noteId);
        m_status = "0x0085 delete note " + std::to_string(n.noteId);
        break;
    }
    case Tab::Blocks: {
        const BlockRow& b = session.blocks()[static_cast<size_t>(m_selected)];
        session.removeBlock(b.playerId);
        m_status = "0x007B unblock " + u16ToUtf8(b.name);
        break;
    }
    }
}

void MessengerPopup::submitInput() {
    Session& session = m_app.session();
    if (m_input == Input::AddFriend) {
        if (m_text.empty()) return;
        session.addFriend(utf8ToU16(m_text));
        m_status = "0x006F sent for " + m_text;
        m_input = Input::None;
    } else if (m_input == Input::NoteTo) {
        if (m_text.empty()) return;
        m_noteTo = m_text;
        m_text.clear();
        m_input = Input::NoteBody;
        m_status = "note body then Enter sends 0x0081";
    } else if (m_input == Input::NoteBody) {
        if (m_text.empty()) return;
        session.sendNote(utf8ToU16(m_noteTo), utf8ToU16(m_text));
        m_status = "0x0081 sent to " + m_noteTo;
        m_input = Input::None;
        m_text.clear();
    }
}

void MessengerPopup::close() {
    if (m_closed) return;
    m_closed = true;
    m_app.popScreen();
}

void MessengerPopup::onKey(int key, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (m_input != Input::None) {
        if (key == GLFW_KEY_ESCAPE) { m_input = Input::None; m_text.clear(); return; }
        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) { submitInput(); return; }
        if (key == GLFW_KEY_BACKSPACE && !m_text.empty()) {
            size_t n = m_text.size();
            do { --n; } while (n > 0 && (static_cast<unsigned char>(m_text[n]) & 0xC0) == 0x80);
            m_text.resize(n);
        }
        return;
    }
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) { close(); return; }
    if (key == GLFW_KEY_TAB) { m_tab = static_cast<Tab>((static_cast<int>(m_tab) + 1) % 4); m_selected = 0; m_menuOpen = false; return; }
    if (key == GLFW_KEY_UP) { selectRow(m_selected - 1); return; }
    if (key == GLFW_KEY_DOWN) { selectRow(m_selected + 1); return; }
    if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_A) { primaryAction(); return; }
    if (key == GLFW_KEY_DELETE || key == GLFW_KEY_D) { deleteAction(); return; }
    if (key == GLFW_KEY_N) {
        if (m_tab == Tab::Friends && m_selected < rowCount()) {
            m_noteTo = u16ToUtf8(m_app.session().friends()[static_cast<size_t>(m_selected)].name);
            m_text.clear();
            m_input = Input::NoteBody;
            m_status = "note to " + m_noteTo + ", Enter sends 0x0081";
        }
    }
}

void MessengerPopup::onChar(unsigned codepoint) {
    if (m_input == Input::None || codepoint < 32) return;
    size_t count = 0;
    size_t i = 0;
    while (i < m_text.size()) { nextUtf8(m_text, i); ++count; }
    const size_t cap = m_input == Input::NoteBody ? 159 : 12;
    if (count >= cap) return;
    appendUtf8(m_text, codepoint);
}

void MessengerPopup::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    const float px = panelX();
    const float py = panelY();
    if (menuClick(x, y)) return;
    for (int t = 0; t < 4; ++t) {
        const Rect tab = {px + kTabX[t], py + kTabY, t == 3 ? 75.f : 99.f, 36.f};
        if (!tab.contains(x, y)) continue;
        m_app.click();
        m_tab = static_cast<Tab>(kTabOfArt[t]);
        m_selected = 0;
        return;
    }
    // the bin of a block row sits on the row line the stock registers one button per row
    if (m_tab == Tab::Blocks) {
        for (int i = 0; i < rowCount() && i < kRowsShown; ++i) {
            const Rect bin = {px + kBlockDelX, py + kRowY + kRowH * static_cast<float>(i), 28.f, 22.f};
            if (!bin.contains(x, y)) continue;
            m_app.click();
            m_selected = i;
            deleteAction();
            return;
        }
    }
    const Rect list = {px + kRowX, py + kRowY, 192.f, kRowH * static_cast<float>(kRowsShown)};
    if (list.contains(x, y)) {
        const int row = static_cast<int>((y - list.y) / kRowH);
        selectRow(row);
        // a click on a friend row opens the stock six item menu under the cursor
        if (m_tab == Tab::Friends && row >= 0 && row < rowCount()) openRowMenu(x, y, row);
        return;
    }
    if (m_tab == Tab::Requests && m_selected < rowCount()) {
        const float ry = py + kRowY + kRowH * static_cast<float>(m_selected);
        const Rect accept = {px + kActionX, ry - 5.f, 58.f, 24.f};
        const Rect reject = {px + kActionX + 60.f, ry - 5.f, 58.f, 24.f};
        if (accept.contains(x, y)) { m_app.click(); primaryAction(); return; }
        if (reject.contains(x, y)) { m_app.click(); deleteAction(); return; }
    }
    const Rect plus = {px + kSideX, py + kSideY, 28.f, 34.f};
    const Rect note = {px + kSideX + kSideStep, py + kSideY, 28.f, 34.f};
    const Rect del = {px + kSideX + kSideStep * 2.f, py + kSideY, 28.f, 34.f};
    const Rect ok = {px + kOkX, py + kOkY, 96.f, 29.f};
    if (plus.contains(x, y)) { m_app.click(); primaryAction(); return; }
    if (del.contains(x, y)) { m_app.click(); deleteAction(); return; }
    if (note.contains(x, y)) { m_app.click(); m_tab = Tab::Notes; primaryAction(); return; }
    if (ok.contains(x, y)) { m_app.click(); close(); return; }
    if (x < px || x > px + kPanelW || y < py || y > py + kPanelH) close();
}

// the list back the tabs the name plates the side verbs then the note or the compose box
void MessengerPopup::draw(SpriteBatch& batch) {
    const FontAtlas& font = m_app.font();
    AssetStore& assets = m_app.assets();
    Session& session = m_app.session();
    const float px = panelX();
    const float py = panelY();
    auto sprite = [&](const std::string& path, float x, float y) -> const Texture* {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
        return t;
    };
    if (!sprite(m_tab == Tab::Notes ? "Popup/Messenger/Messenger_NoteList_Back.png" : "Popup/Messenger/Messenger_List_Back.png", px, py))
        batch.fill(px, py, kPanelW, kPanelH, rgba(246, 246, 250, 245));

    const char* const kTabArt[4] = {"Popup/Messenger/Messenger_Tab_Friend_", "Popup/Messenger/Messenger_Tab_Note_",
                                    "Popup/Messenger/Messenger_Tab_block_", "Popup/Messenger/Messenger_Tab_inviteFriend_"};
    for (int t = 0; t < 4; ++t) {
        const bool on = static_cast<int>(m_tab) == kTabOfArt[t];
        if (!sprite(std::string(kTabArt[t]) + (on ? "02.png" : "00.png"), px + kTabX[t], py + kTabY)) {
            batch.fill(px + kTabX[t], py + kTabY, 99.f, 36.f, on ? rgba(255, 220, 90, 255) : rgba(200, 200, 210, 255));
        }
        if (t == 3 && !session.friendRequests().empty())
            font.draw(batch, std::to_string(session.friendRequests().size()), px + kTabX[t] + 60.f, py + kTabY + 2.f, 12.f, rgba(220, 40, 40, 255));
    }

    const int count = rowCount();
    for (int i = 0; i < count && i < kRowsShown; ++i) {
        const float y = py + kRowY + static_cast<float>(i) * kRowH;
        sprite("Popup/Messenger/Messenger_FriendName.png", px + kRowX, y);
        if (i == m_selected) batch.fill(px + kRowX, y, 192.f, 22.f, rgba(255, 220, 90, 70));
        if (m_tab == Tab::Friends) {
            const FriendRow& f = session.friends()[static_cast<size_t>(i)];
            const bool online = f.statusA >= 0;
            const std::string label = u16ToUtf8(f.name) + (isBlocked(f.playerId) ? " (Blocked)" : "");
            font.drawClipped(batch, label, px + kRowX + 8.f, y + 4.f, 176.f, 15.f, online ? kOnline : kOffline);
        } else if (m_tab == Tab::Blocks) {
            const BlockRow& b = session.blocks()[static_cast<size_t>(i)];
            font.drawClipped(batch, u16ToUtf8(b.name), px + kRowX + 8.f, y + 4.f, 176.f, 15.f, kInk);
            sprite("Popup/Messenger/Messenger_Del_00.png", px + kBlockDelX, y);
        } else if (m_tab == Tab::Requests) {
            const FriendRequestRow& r = session.friendRequests()[static_cast<size_t>(i)];
            font.drawClipped(batch, u16ToUtf8(r.name), px + kRowX + 8.f, y + 4.f, 176.f, 15.f, kInk);
            if (i == m_selected) {
                sprite("Popup/Messenger/Messenger_Tab_Aceiter_00.png", px + kActionX, y - 5.f);
                sprite("Popup/Messenger/Messenger_Tab_Recusar_00.png", px + kActionX + 60.f, y - 5.f);
            }
        } else {
            const NoteRow& n = session.notes()[static_cast<size_t>(i)];
            font.drawClipped(batch, u16ToUtf8(n.sender) + "  " + u16ToUtf8(n.sortKey1), px + kRowX + 8.f, y + 4.f, 176.f, 15.f, n.read ? kOffline : kInk);
        }
    }
    sprite("Popup/Messenger/Messenger_Plus_00.png", px + kSideX, py + kSideY);
    sprite("Popup/Messenger/Messenger_Note_00.png", px + kSideX + kSideStep, py + kSideY);
    sprite("Popup/Messenger/Messenger_Del_00.png", px + kSideX + kSideStep * 2.f, py + kSideY);

    // the selected note shows on the NoteShow plate under the rows the compose box on the info art
    if (m_input == Input::None && m_tab == Tab::Notes && m_selected < count) {
        const NoteRow& n = session.notes()[static_cast<size_t>(m_selected)];
        const float sy = py + kRowY + kRowH * static_cast<float>(kRowsShown) + 10.f;
        sprite("Popup/Messenger/Messenger_NoteShow.png", px + kRowX, sy);
        const std::vector<std::string> lines = wrapText(font, u16ToUtf8(n.body), 14.f, 270.f);
        float ly = sy + 6.f;
        for (const std::string& line : lines) {
            if (ly > sy + 40.f) break;
            font.draw(batch, line, px + kRowX + 8.f, ly, 14.f, kInk);
            ly += 16.f;
        }
    }
    if (m_input != Input::None) {
        const float ix = std::floor((m_app.canvasWidth() - kInfoW) * 0.5f);
        const float iy = std::floor((m_app.canvasHeight() - kInfoH) * 0.5f);
        if (!sprite("Popup/Message/Popup_Information.png", ix, iy)) batch.fill(ix, iy, kInfoW, kInfoH, rgba(220, 220, 230, 255));
        const std::string labelText = m_input == Input::AddFriend ? m_app.tr("MSG_FRIEND_ADD")
                                    : m_input == Input::NoteTo ? m_app.tr("MSG_NOTE_INPUTUSER") : "To " + m_noteTo;
        const std::vector<std::string> lines = wrapText(font, labelText, 14.f, kInfoW - 44.f);
        float ly = iy + 40.f;
        for (const std::string& line : lines) { if (ly > iy + 90.f) break; font.draw(batch, line, ix + 22.f, ly, 14.f, kInk); ly += 16.f; }
        sprite("Popup/Message/Popup_Input.png", ix + 22.f, iy + 100.f);
        font.drawClipped(batch, m_text + "_", ix + 28.f, iy + 104.f, 290.f, 15.f, kInk);
    }
    // the 0x0072 card of the user info item on the plate under the rows as the stock panel
    if (m_cardOpen && m_input == Input::None && !m_menuOpen) {
        const UserInfoCard& card = session.userInfoCard();
        const float sy = py + kRowY + kRowH * static_cast<float>(kRowsShown) + 10.f;
        sprite("Popup/Messenger/Messenger_NoteShow.png", px + kRowX, sy);
        if (!card.valid) {
            font.draw(batch, "waiting for the 0x0072 answer", px + kRowX + 8.f, sy + 5.f, 14.f, kInk);
        } else {
            // the plate has two rows of white boxes name and level on the first the exp on the second
            char level[32];
            std::snprintf(level, sizeof(level), "Lv %u", card.level);
            char exp[64];
            std::snprintf(exp, sizeof(exp), "exp %u / %u", card.exp, card.expNext);
            font.drawClipped(batch, u16ToUtf8(card.name), px + kRowX + 8.f, sy + 5.f, 180.f, 14.f, kInk);
            font.draw(batch, level, px + kRowX + 208.f, sy + 5.f, 14.f, kInk);
            font.drawClipped(batch, exp, px + kRowX + 8.f, sy + 33.f, 180.f, 13.f, kInk);
            // sub 465040 the worn pendant of blob 0x64 its name or MSG PENDANT NOEQUIP the icon on the right
            const PendantDef* worn = card.pendant > 0 ? session.pendantDef(static_cast<uint32_t>(card.pendant)) : nullptr;
            font.drawClipped(batch, worn ? m_app.tr(worn->nameKey) : m_app.tr("MSG_PENDANT_NOEQUIP"), px + kRowX + 208.f,
                             sy + 33.f, 110.f, 13.f, kInk);
            const std::string icon = worn ? "Icon/" + worn->iconBase + "_s.png" : "CharInfo/Common_Char_Pendant_00.png";
            const Texture* t = m_app.assets().texture(icon);
            if (t && t->valid()) batch.draw(t->handle, px + kRowX + 318.f, sy + 8.f, 30.f, 30.f * static_cast<float>(t->height) / static_cast<float>(t->width));
        }
    }
    // the row menu of the friend tab over the list the last item is the block verb
    if (m_menuOpen) {
        const char* const kMenuArt[6] = {"Popup/Messenger/Messenger_chat_user_00.png", "Popup/Messenger/Messenger_chat_invite_00.png",
                                         "Popup/Messenger/Messenger_chat_note_00.png", "Popup/Messenger/Messenger_chat_chatting_00.png",
                                         "Popup/Messenger/Messenger_chat_delete_00.png", "Popup/Messenger/Messenger_chat_block_00.png"};
        const char* const kMenuWord[6] = {"User Info", "Invite", "Note", "Chat", "Delete", "Block"};
        for (int i = 0; i < 6; ++i) {
            const float mx = m_menuX + kMenuDX;
            const float my = m_menuY + kMenuY[i];
            if (!sprite(kMenuArt[i], mx, my)) {
                batch.fill(mx, my, kMenuW, kMenuH, rgba(250, 250, 252, 240));
                font.draw(batch, kMenuWord[i], mx + 6.f, my + 3.f, 13.f, kInk);
            }
        }
    }
    sprite("Popup/Messenger/Common_OK1_00.png", px + kOkX, py + kOkY);
    if (!m_status.empty() && m_app.options().statusLine) font.drawClipped(batch, m_status, px + 24.f, py + kPanelH - 24.f, kPanelW - 48.f, 12.f, kInk);
}

}
