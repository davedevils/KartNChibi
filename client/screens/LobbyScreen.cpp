#include "LobbyScreen.h"
#include "screens/GarageScreen.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "engine/render/scene_renderer.h"
#include "net/Utf.h"
#include "screens/GachaPopup.h"
#include "screens/GhostModeScreen.h"
#include "screens/HelpPopup.h"
#include "screens/MenuPopup.h"
#include "screens/MessengerPopup.h"
#include "ui/MenuFrame.h"
#include "ui/MessagePopup.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace KnC::Client {

namespace {

// stock lobby class sub 408EE0 the top bar at 25 50 the back at 37 88 on the frame
constexpr float kTopX = 25.f;
constexpr float kTopY = 50.f;
constexpr float kBackX = 37.f;
constexpr float kBackY = 88.f;
// the room cards sub 408020 at 438 242 two columns 255 apart three rows 84 apart
constexpr float kCardX = 438.f;
constexpr float kCardY = 242.f;
constexpr float kCardStepX = 255.f;
constexpr float kCardStepY = 84.f;
constexpr float kCardW = 247.f;
constexpr float kCardH = 71.f;
constexpr int kCardsPerPage = 6;
// the chat box sub 4087D0 at 440 553 of 492 by 120 seven lines the inputs on 678
constexpr Rect kChat = {440.f, 553.f, 492.f, 120.f};
constexpr Rect kChatInput = {599.f, 678.f, 320.f, 20.f};
constexpr Rect kWhisperInput = {464.f, 678.f, 118.f, 20.f};
constexpr uint32_t kPageInk = kInkBlack;
// the auto join waits this long for the rows before it picks one
constexpr float kAutoJoinDelay = 1.5f;
// the quick match buttons of the JSON send 0x0064 with the mode in their stock order
constexpr int kQuickMode[4] = {2, 3, 0, 1};

// the stock draw loop shows a row only when value 5 is zero and value 3 is at most 4
bool shownRow(const RoomRow& r) { return r.value[4] == 0 && r.value[2] <= 4; }

const char* modeBadge(uint32_t mode) {
    switch (mode) {
    case 0: return "Lobby/Lobby_Room_I_Single_";
    case 1: return "Lobby/Lobby_Room_I_Team_";
    case 2: return "Lobby/Lobby_Room_S_Single_";
    case 3: return "Lobby/Lobby_Room_S_Team_";
    default: return "Lobby/Lobby_Room_B_";
    }
}

}

void LobbyScreen::enter() {
    loadLayout("ui_state_07_lobby.json");
    AssetStore& assets = m_app.assets();
    // the JSON parks the back the top and the chat icons at the origin the stock places them
    for (const char* id : {"image_0", "image_1", "image_2", "image_3"}) if (Widget* w = find(id)) w->visible = false;
    addMenuFrame(*this, assets, FrameMode::Full, "lobby");
    auto image = [&](const char* id, const char* art, float x, float y, int z) {
        auto img = std::make_unique<ImageWidget>();
        img->type = ElementType::Image;
        img->id = id;
        img->texture = assets.texture(art);
        const float w = img->texture ? static_cast<float>(img->texture->width) : 0.f;
        const float h = img->texture ? static_cast<float>(img->texture->height) : 0.f;
        img->rect = {x, y, w, h};
        img->zIndex = z;
        add(std::move(img));
    };
    image("lobby_top", "Lobby/Lobby_Top.png", kTopX, kTopY, -10);
    // the back is drawn by hand with the preview hole the widget only keeps its spot
    if (Widget* w = find("lobby_top")) w->visible = false;
    addCharPanelWidgets(*this, assets);
    auto button = [&](const char* id, const char* action, const char* art, float x, float y) {
        auto b = std::make_unique<ButtonWidget>();
        b->type = ElementType::Button;
        b->id = id;
        b->action = action;
        b->normal = assets.texture(std::string(art) + "00.png");
        b->hover = assets.texture(std::string(art) + "01.png");
        b->pressed = assets.texture(std::string(art) + "02.png");
        const Texture* size = b->normal ? b->normal : b->hover;
        b->rect = {x, y, size ? static_cast<float>(size->width) : 24.f, size ? static_cast<float>(size->height) : 24.f};
        b->zIndex = 30;
        add(std::move(b));
    };
    button("btn_page_left", "page_left", "Buttons/Common_Page_Left_", 628.f, 500.f);
    button("btn_page_right", "page_right", "Buttons/Common_Page_Right_", 729.f, 500.f);
    // stock capture spots the yellow chat arrow at 435 683 the scroll arrows at 939 on 624 and 651
    button("btn_chat_mode", "chat_mode", "Lobby/Lobby_Chat_All", 435.f, 683.f);
    if (ButtonWidget* b = findAs<ButtonWidget>("btn_chat_mode")) {
        b->normal = assets.texture("Lobby/Lobby_Chat_All.png");
        b->hover = b->normal;
        b->pressed = assets.texture("Lobby/Lobby_Chat_Whisper.png");
        b->rect = {435.f, 683.f, 23.f, 21.f};
    }
    button("btn_chat_up", "chat_up", "Lobby/WaitingRoom_Chat_Up_", 939.f, 624.f);
    button("btn_chat_down", "chat_down", "Lobby/WaitingRoom_Chat_Down_", 939.f, 651.f);

    auto input = std::make_unique<InputWidget>();
    input->type = ElementType::Input;
    input->id = "chat_input";
    input->maxLength = 46;
    input->px = 15.f;
    input->bare = true;
    input->ink = kInkBlack;
    input->rect = kChatInput;
    input->zIndex = 30;
    input->onSubmit = [this]() { sendChat(); };
    m_chatInput = static_cast<InputWidget*>(add(std::move(input)));
    auto whisper = std::make_unique<InputWidget>();
    whisper->type = ElementType::Input;
    whisper->id = "whisper_input";
    whisper->maxLength = 12;
    whisper->px = 15.f;
    whisper->bare = true;
    whisper->ink = kInkBlack;
    whisper->rect = kWhisperInput;
    whisper->zIndex = 30;
    whisper->text = m_app.options().whisperTo;
    whisper->onSubmit = [this]() { if (m_chatInput) focus(m_chatInput); };
    m_whisperInput = static_cast<InputWidget*>(add(std::move(whisper)));
    if (Widget* w = find("button_8")) w->action = "create";
    for (int i = 0; i < 4; ++i) {
        if (Widget* w = find("button_" + std::to_string(4 + i))) w->action = "quick" + std::to_string(kQuickMode[i]);
    }
    m_status.clear();
    m_time = 0.f;
    m_autoSent = false;
    m_saySent = false;
    m_socialOpened = false;
    m_page = 0;
    m_selected = -1;
    m_users = UserListState();
    m_sceneReady = loadCharPreviewScene(m_app, m_view, m_previewWorld);
    m_previewCar = -1;
    m_previewKart.clear();
    m_previewDriver.clear();
    refreshPreview();
    hookUserListTap(m_app, &m_users);
}

void LobbyScreen::leave() {
    m_app.setFrameTap(nullptr);
    KnC::Render::ViewportRect full;
    full.x = 0;
    full.y = 0;
    full.width = m_app.width();
    full.height = m_app.height();
    m_app.renderer().set_viewport(full);
}

// the own kart with the own driver as the garage preview shows them
void LobbyScreen::refreshPreview() {
    if (!m_sceneReady) return;
    const Session& session = m_app.session();
    const Catalog& cat = session.catalog();
    const OwnedKart* kart = nullptr;
    const OwnedCharacter* mine = nullptr;
    if (session.profile().kartInstance >= 0) kart = cat.ownedKart(static_cast<uint32_t>(session.profile().kartInstance));
    if (!kart && !cat.ownedKarts().empty()) kart = &cat.ownedKarts().front();
    if (session.profile().characterInstance >= 0) mine = cat.ownedCharacter(static_cast<uint32_t>(session.profile().characterInstance));
    if (!mine && !cat.ownedCharacters().empty()) mine = &cat.ownedCharacters().front();
    const KartRow* kartDef = kart ? cat.kart(kart->kartKey) : nullptr;
    const DriverRow* driverDef = mine ? cat.driver(mine->driverKey) : nullptr;
    const std::string model = kartDef && !kartDef->model.empty() ? kartDef->model : std::string("Basic_1");
    const std::string asset = driverDef && !driverDef->asset.empty() ? driverDef->asset : std::string("Cosmo");
    if (model == m_previewKart && asset == m_previewDriver && m_previewCar >= 0) return;
    if (m_previewCar >= 0) m_view.removeCar(m_previewCar);
    m_previewKart = model;
    m_previewDriver = asset;
    // the paint goes in with the car so the first drawn frame swaps no body and uploads nothing
    m_previewCar = m_view.addCar(m_app.renderer(), m_app.options().gameDir, model, asset, charPreviewPaint(m_app, model));
    CarPose pose;
    m_view.setPose(m_previewCar, pose, 0.f, 0.f);
}

// a popup drew its own scene over the stage so the preview scene and the kart go up again
void LobbyScreen::sceneLost() {
    m_sceneReady = loadCharPreviewScene(m_app, m_view, m_previewWorld);
    m_previewCar = -1;
    m_previewKart.clear();
    m_previewDriver.clear();
    refreshPreview();
}

bool LobbyScreen::drawScene() {
    if (!m_sceneReady || m_previewCar < 0 || m_users.open) return false;
    drawCharPreview(m_app, m_view, m_previewCar, m_orbit);
    return true;
}

// the wallpaper the common back and the lobby back keep the preview hole the widgets go over them
void LobbyScreen::draw(SpriteBatch& batch) {
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    AssetStore& assets = m_app.assets();
    const Rect hole = m_users.open ? Rect{0.f, 0.f, 0.f, 0.f} : kCharPanelHole;
    drawFrameBack(batch, assets, hole);
    drawTextureWithHole(batch, assets.texture("Lobby/Lobby_Back.png"), kBackX, kBackY, hole);
    const Texture* top = assets.texture("Lobby/Lobby_Top.png");
    if (top && top->valid()) batch.draw(top->handle, kTopX, kTopY, static_cast<float>(top->width), static_cast<float>(top->height));
    for (Widget* w : m_order) if (w->visible && w->zIndex >= 0) w->draw(ctx);
    drawOverlay(ctx);
}

void LobbyScreen::onSession(SessionEvent event) {
    if (event == SessionEvent::InventoryChanged || event == SessionEvent::Profile || event == SessionEvent::LobbyAck) refreshPreview();
}

Rect LobbyScreen::cardRect(int index) const {
    return {kCardX + kCardStepX * static_cast<float>(index % 2), kCardY + kCardStepY * static_cast<float>(index / 2), kCardW, kCardH};
}

void LobbyScreen::openMessenger() {
    m_app.pushScreen(std::make_unique<MessengerPopup>(m_app));
    m_status = "messenger open, 0x0073 polls every 3 s";
}

void LobbyScreen::update(float dt) {
    m_time += dt;
    m_orbit = turnCharPreview(*this, m_orbit, dt);
    Session& session = m_app.session();
    const Options& o = m_app.options();
    if (session.stage() != Stage::Lobby) return;
    // the one chat line of the run goes out first a whisper when a target is named
    if (!o.say.empty() && !m_saySent && m_time > 1.0f) {
        m_saySent = true;
        std::u16string line = utf8ToU16(o.say);
        if (!o.whisperTo.empty()) line = u"/w " + utf8ToU16(o.whisperTo) + u" " + line;
        session.sendChat(line);
        m_status = "chat line sent 0x00B4";
    }
    // the creation run and a chat run end on a picture of the lobby
    if (m_app.captureMode() && o.stopAt == "lobby" && (!o.autoCreateCharacter.empty() || !o.say.empty()) && !m_autoSent && m_time > 2.0f) {
        m_autoSent = true;
        m_app.captureStage("lobby");
        m_app.finishRun();
        return;
    }
    if (m_autoSent) return;
    if (o.stopAt == "friends" && !m_socialOpened && m_time > 1.0f) {
        m_socialOpened = true;
        m_autoSent = true;
        openMessenger();
        return;
    }
    if ((o.stopAt == "missions" || o.stopAt == "mission" || o.autoMission >= 0) && m_time > 0.5f) {
        m_autoSent = true;
        session.openMissionMenu();
        m_status = "auto missions 0x008F";
        return;
    }
    if (o.quickMatch >= 0 && m_time > 0.5f) {
        m_autoSent = true;
        session.quickMatch(o.quickMatch);
        m_status = "auto quick match 0x0064 mode " + std::to_string(o.quickMatch);
        return;
    }
    if (o.stopAt == "licence" && m_time > 0.5f) {
        m_autoSent = true;
        session.openLicense();
        m_status = "auto licence 0x0016";
        return;
    }
    if ((o.stopAt == "carcraft" || o.stopAt == "roomcraft") && m_time > 0.5f) {
        m_autoSent = true;
        m_app.go(o.stopAt);
        return;
    }
    if (o.stopAt == "gacha" && m_time > 0.5f) {
        m_autoSent = true;
        m_app.pushScreen(std::make_unique<GachaPopup>(m_app));
        if (m_app.captureMode()) { m_app.captureStage("gacha"); m_app.finishRun(); }
        return;
    }
    if (o.stopAt == "escmenu" && m_time > 0.5f) {
        m_autoSent = true;
        m_app.pushScreen(std::make_unique<MenuPopup>(m_app));
        if (m_app.captureMode()) { m_app.captureStage("escmenu"); m_app.finishRun(); }
        return;
    }
    if (o.stopAt == "garage" && m_time > 0.5f) {
        m_autoSent = true;
        session.openGarage();
        m_status = "auto garage 0x000F";
        return;
    }
    if ((o.stopAt == "shop" || o.stopAt == "shopitem" || o.autoBuy) && m_time > 0.5f) {
        m_autoSent = true;
        session.openShop();
        m_status = "auto shop 0x0010";
        return;
    }
    const int autoTrack = o.autoRaceTrack > 0 ? o.autoRaceTrack : o.autoCreateTrack;
    if (autoTrack > 0 && m_time > 0.5f) {
        m_autoSent = true;
        const std::u16string nick = session.profile().nickname;
        session.createRoom(nick + u" race", utf8ToU16(o.roomPassword), 8, static_cast<uint32_t>(o.roomMode));
        m_status = "auto create room 0x002D" + std::string(o.roomPassword.empty() ? "" : " with a password");
        return;
    }
    if (o.autoJoin && m_time > kAutoJoinDelay) {
        for (const RoomRow& row : session.rooms()) {
            if (!shownRow(row)) continue;
            if (row.value[1] != 0 && row.value[0] >= row.value[1]) continue;
            // a locked row is joined only when the run carries the password
            if (row.value[3] == 1 && o.roomPassword.empty()) continue;
            m_autoSent = true;
            session.joinRoom(row.roomId, row.value[3] == 1 ? utf8ToU16(o.roomPassword) : u"");
            m_status = "auto join room " + std::to_string(row.roomId);
            return;
        }
        if (m_time > kAutoJoinDelay + 6.f) {
            m_autoSent = true;
            m_status = "auto join found no open room";
        }
    }
}

// sub 409120 Escape opens the Menu box of sub 463A20 F1 the help sheet Enter the chat
void LobbyScreen::onKey(int key, int action, int mods) {
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) { m_app.pushScreen(std::make_unique<MenuPopup>(m_app)); return; }
    if (action == GLFW_PRESS && key == GLFW_KEY_F1) { m_app.pushScreen(std::make_unique<HelpPopup>(m_app)); return; }
    const bool typing = (m_chatInput && m_chatInput->focused) || (m_whisperInput && m_whisperInput->focused);
    if (action == GLFW_PRESS && key == GLFW_KEY_G && !typing) {
        m_app.session().openGarage();
        m_status = "garage 0x000F sent";
        return;
    }
    if (action == GLFW_PRESS && key == GLFW_KEY_S && !typing) {
        m_app.session().openShop();
        m_status = "shop 0x0010 sent";
        return;
    }
    if (action == GLFW_PRESS && key == GLFW_KEY_M && !typing) {
        m_app.session().openMissionMenu();
        m_status = "missions 0x008F sent";
        return;
    }
    if (action == GLFW_PRESS && key == GLFW_KEY_F && !typing) {
        openMessenger();
        return;
    }
    if (action == GLFW_PRESS && (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) && m_chatInput && !m_chatInput->focused) {
        focus(m_chatInput);
        return;
    }
    WidgetScreen::onKey(key, action, mods);
}

// the first click picks a card the second click on the same card joins it like the stock double click
void LobbyScreen::onMouseButton(int button, int action, float x, float y) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        for (int i = 0; i < kCardsPerPage; ++i) {
            if (!cardRect(i).contains(x, y)) continue;
            const size_t shown = static_cast<size_t>(m_page * kCardsPerPage + i);
            if (shown >= m_shownRooms.size()) break;
            if (m_selected == static_cast<int>(shown)) joinRow(shown);
            else m_selected = static_cast<int>(shown);
            return;
        }
    }
    WidgetScreen::onMouseButton(button, action, x, y);
}

void LobbyScreen::joinRow(size_t shownIndex) {
    if (shownIndex >= m_shownRooms.size()) return;
    const uint32_t id = m_shownRooms[shownIndex];
    // a locked row opens the password box a GM joins with an empty one as the 0x002F page says
    for (const RoomRow& row : m_app.session().rooms()) {
        if (row.roomId != id) continue;
        if (row.value[3] == 1 && m_app.session().profile().rolePrivilege <= 1) {
            m_app.pushScreen(std::make_unique<PasswordPopup>(m_app, id));
            m_status = "room " + std::to_string(id) + " is locked";
            return;
        }
    }
    m_app.session().joinRoom(id, u"");
    m_status = "join room " + std::to_string(id) + " sent 0x002F";
}

void LobbyScreen::onAction(const std::string& action, Widget& source) {
    if (action == "create") { m_app.pushScreen(std::make_unique<CreateRoomPopup>(m_app)); return; }
    if (action == "garage") { m_app.session().openGarage(); m_status = "garage 0x000F sent"; return; }
    if (action == "shop") { m_app.session().openShop(); m_status = "shop 0x0010 sent"; return; }
    if (action == "missions") { m_app.session().openMissionMenu(); m_status = "missions 0x008F sent"; return; }
    if (action == "ghost") { m_status = GhostModeScreen::open(m_app) ? "ghost 0x011D sent" : "ghost needs level 10 or a licence"; return; }
    if (action == "messenger") { openMessenger(); return; }
    // the Quit of the bottom bar is case 0xd of sub 42BCE0 the MSG CONFIRM EXIT box
    if (action == "quit") {
        App& app = m_app;
        app.pushScreen(std::make_unique<MessagePopup>(app, app.tr("MSG_CONFIRM_EXIT"), [&app]() { app.quit(); }, nullptr, true));
        return;
    }
    if (action == "lobby") return;
    if (action == "page_left") { if (m_page > 0) --m_page; return; }
    if (action == "page_right") {
        const int pages = (static_cast<int>(m_shownRooms.size()) + kCardsPerPage - 1) / kCardsPerPage;
        if (m_page + 1 < pages) ++m_page;
        return;
    }
    if (action == "chat_mode") {
        if (m_whisperInput) focus(m_whisperInput);
        return;
    }
    if (action == "chat_up" || action == "chat_down") return;
    if (charPanelAction(*this, m_app, m_users, action, &m_orbit)) return;
    if (frameAction(m_app, action)) return;
    if (action.rfind("quick", 0) == 0) {
        const int mode = std::atoi(action.c_str() + 5);
        m_app.session().quickMatch(mode);
        m_status = "quick match 0x0064 mode " + std::to_string(mode) + " sent";
        return;
    }
    m_status = source.id + " is not part of this phase";
}

// the whisper box names a target so the line goes out as the slash w form the server parses
void LobbyScreen::sendChat() {
    if (!m_chatInput || m_chatInput->text.empty()) return;
    std::u16string line = utf8ToU16(m_chatInput->text);
    if (m_whisperInput && !m_whisperInput->text.empty()) line = u"/w " + utf8ToU16(m_whisperInput->text) + u" " + line;
    m_app.session().sendChat(line);
    m_chatInput->text.clear();
}

void LobbyScreen::drawOverlay(DrawContext& ctx) {
    const Session& session = m_app.session();
    AssetStore& assets = m_app.assets();

    // the cards sub 407B50 item art for the item modes speed art for the rest the picked one wears 01
    m_shownRooms.clear();
    for (const RoomRow& row : session.rooms()) if (shownRow(row)) m_shownRooms.push_back(row.roomId);
    const int pages = std::max(1, (static_cast<int>(m_shownRooms.size()) + kCardsPerPage - 1) / kCardsPerPage);
    if (m_page >= pages) m_page = pages - 1;
    for (int i = 0; i < kCardsPerPage; ++i) {
        const size_t shown = static_cast<size_t>(m_page * kCardsPerPage + i);
        if (shown >= m_shownRooms.size()) break;
        const RoomRow* row = nullptr;
        for (const RoomRow& r : session.rooms()) if (r.roomId == m_shownRooms[shown]) row = &r;
        if (!row) continue;
        const Rect r = cardRect(i);
        const bool picked = static_cast<int>(shown) == m_selected;
        const bool playing = row->value[5] != 0;
        const bool speed = row->value[2] == 2 || row->value[2] == 3;
        std::string art = playing ? "Lobby/Lobby_Room_Play.png"
                                  : std::string(speed ? "Lobby/Lobby_Room_Speed_" : "Lobby/Lobby_Room_Item_") + (picked ? "01.png" : "00.png");
        const Texture* back = assets.texture(art);
        if (back && back->valid()) ctx.batch.draw(back->handle, r.x, r.y, static_cast<float>(back->width), static_cast<float>(back->height));
        const Texture* badge = assets.texture(std::string(modeBadge(row->value[2])) + (playing ? "01.png" : "00.png"));
        if (badge && badge->valid()) ctx.batch.draw(badge->handle, r.x + 62.f, r.y + 38.f, static_cast<float>(badge->width), static_cast<float>(badge->height));
        // the 0x002D page says the stock prints the room id plus one the title is bold the count plain
        char head[16];
        std::snprintf(head, sizeof(head), "%03u ", row->roomId + 1);
        ctx.bold.drawClipped(ctx.batch, head + u16ToUtf8(row->name), r.x + 13.f, r.y + 12.f, kCardW - 40.f, 15.f, kInkBlack);
        if (row->value[3] == 1) {
            const Texture* lock = assets.texture("Lobby/Lobby_Room_Lock.png");
            if (lock && lock->valid()) ctx.batch.draw(lock->handle, r.x + 210.f, r.y + 34.f, static_cast<float>(lock->width), static_cast<float>(lock->height));
        }
        char count[16];
        std::snprintf(count, sizeof(count), "%u/%u", row->value[0], row->value[1]);
        drawAligned(ctx, ctx.font, count, r.x + 27.f, r.y + 46.f, 13.f, kInkBlack, Align::Centre);
    }
    if (pages > 1) drawAligned(ctx, ctx.bold, std::to_string(m_page + 1), 690.f, 506.f, 15.f, kPageInk, Align::Centre);

    drawCharPanel(ctx, assets, m_users, &m_app.session());
    // sub 42AB70 the wrench bar of a mode 3 kart at 67 516 under the char preview
    if (!m_users.open && session.profile().kartInstance >= 0) {
        const OwnedKart* worn = session.catalog().ownedKart(static_cast<uint32_t>(session.profile().kartInstance));
        if (worn && worn->expiryKind == 3)
            drawDurability(ctx, assets, 67.f, 516.f, worn->durability, kartDurabilityMax(session.catalog(), *worn));
    }
    drawCharInfo(ctx, assets, session.profile());

    // chat lines bottom up seven lines of the stock box
    ctx.batch.setClip(kChat.x, kChat.y, kChat.w, kChat.h);
    const float lineH = 17.f;
    float y = kChat.y + kChat.h - lineH - 2.f;
    const std::vector<ChatLine>& chat = session.chat();
    for (size_t i = chat.size(); i > 0 && y > kChat.y - lineH; --i) {
        const ChatLine& line = chat[i - 1];
        std::string text;
        uint32_t colour = kInkBlack;
        if (line.type == 0) text = u16ToUtf8(line.sender) + " : " + u16ToUtf8(line.text);
        else if (line.type == 3) { text = u16ToUtf8(line.text); colour = rgba(255, 64, 64, 255); }
        else if (line.type == 4) { text = u16ToUtf8(line.text); colour = rgba(64, 64, 255, 255); }
        // sub 47EC00 prints the resolved key alone the name rides the wire unread
        else if (line.type == 5) { text = m_app.tr(u16ToUtf8(line.text)); colour = rgba(64, 255, 64, 255); }
        else if (line.type == 6) { text = (line.outgoing ? "To " : "From ") + u16ToUtf8(line.sender) + " : " + u16ToUtf8(line.text); colour = rgba(255, 64, 64, 255); }
        else text = u16ToUtf8(line.text);
        ctx.font.drawClipped(ctx.batch, text, kChat.x + 6.f, y, kChat.w - 12.f, 15.f, colour);
        y -= lineH;
    }
    ctx.batch.clearClip();
    if (!m_status.empty() && m_app.options().statusLine) ctx.font.draw(ctx.batch, m_status, 440.f, 706.f, 12.f, kInkGrey);
}

// sub 4634C0 a random def title line as the name no password the count 8 or 16 by mode
CreateRoomPopup::CreateRoomPopup(App& app) : m_app(app) {
    m_name = randomTitle();
    if (m_name.empty()) m_name = u16ToUtf8(m_app.session().profile().nickname) + " room";
    m_password = m_app.options().roomPassword;
    m_private = !m_password.empty();
    m_mode = m_app.options().roomMode >= 0 && m_app.options().roomMode < 4 ? m_app.options().roomMode : 0;
    m_players = (m_mode == 2 || m_mode == 3) ? 16 : 8;
    m_field = m_private ? 1 : 0;
}

// Define lang def title txt utf16 one title per line sub 4E19F0 keeps up to 256
std::string CreateRoomPopup::randomTitle() const {
    std::string raw;
    if (!m_app.assets().readText("Define/" + m_app.options().language + "/def_title.txt", raw)) return std::string();
    if (raw.size() < 4) return std::string();
    std::vector<std::string> titles;
    size_t at = (static_cast<uint8_t>(raw[0]) == 0xFF && static_cast<uint8_t>(raw[1]) == 0xFE) ? 2 : 0;
    std::u16string wide;
    for (; at + 1 < raw.size(); at += 2) wide.push_back(static_cast<char16_t>(static_cast<uint8_t>(raw[at]) | (static_cast<uint8_t>(raw[at + 1]) << 8)));
    std::u16string cur;
    auto flush = [&]() {
        while (!cur.empty() && (cur.back() == u'\r' || cur.back() == u' ')) cur.pop_back();
        if (!cur.empty() && cur.size() <= 31 && titles.size() < 256) titles.push_back(u16ToUtf8(cur));
        cur.clear();
    };
    for (char16_t c : wide) {
        if (c == u'\n') flush();
        else cur.push_back(c);
    }
    flush();
    if (titles.empty()) return std::string();
    return titles[static_cast<size_t>(std::rand()) % titles.size()];
}

// sub 4631F0 back 300 249 arrows plus 132 and 358 on 82 117 152 OK 105 225 Cancel 200 225
void CreateRoomPopup::draw(SpriteBatch& batch) {
    const FontAtlas& font = m_app.font();
    AssetStore& assets = m_app.assets();
    const float bx = 300.f;
    const float by = 249.f;
    const Texture* back = assets.texture("Popup/MakeRoom/CreatRoom_Back.png");
    if (back && back->valid()) batch.draw(back->handle, bx, by, static_cast<float>(back->width), static_cast<float>(back->height));
    else batch.fill(bx, by, 400.f, 263.f, rgba(60, 60, 80, 240));
    auto sprite = [&](const std::string& path, float x, float y) {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
    };
    // sub 462E00 the name edit at plus 138 46 of 200 wide a long title shows its tail
    std::string shown = m_name + (m_field == 0 ? "_" : "");
    size_t cut = 0;
    while (cut < shown.size() && font.measure(shown.substr(cut), 19.f) > 200.f) nextUtf8(shown, cut);
    font.drawClipped(batch, shown.substr(cut), bx + 138.f, by + 48.f, 200.f, 19.f, rgba(14, 130, 231, 255));
    static const char* const kModeArt[5] = {"CreatRoom_I_S", "CreatRoom_I_T", "CreatRoom_S_S", "CreatRoom_S_T", "CreatRoom_B"};
    sprite(std::string("Popup/MakeRoom/") + kModeArt[m_mode] + ".png", bx + 154.f, by + 84.f);
    DrawContext ctx{batch, font, m_app.fontBold()};
    drawAligned(ctx, ctx.bold, std::to_string(m_players), bx + 249.f, by + 121.f, 15.f, kInkDark, Align::Centre);
    sprite(m_private ? "Popup/MakeRoom/CreatRoom_Private.png" : "Popup/MakeRoom/CreatRoom_Public.png", bx + 154.f, by + 153.f);
    if (m_private) {
        std::string stars;
        size_t i = 0;
        while (i < m_password.size()) { nextUtf8(m_password, i); stars += '*'; }
        font.drawClipped(batch, stars + (m_field == 1 ? "_" : ""), bx + 138.f, by + 187.f, 100.f, 19.f, rgba(14, 130, 231, 255));
    }
    for (int row = 0; row < 3; ++row) {
        const float y = by + 82.f + 35.f * static_cast<float>(row);
        const Rect left = {bx + 132.f, y, 24.f, 24.f};
        const Rect right = {bx + 358.f, y, 24.f, 24.f};
        sprite(left.contains(m_mouseX, m_mouseY) ? "Buttons/Common_Page_Left_01.png" : "Buttons/Common_Page_Left_00.png", left.x, left.y);
        sprite(right.contains(m_mouseX, m_mouseY) ? "Buttons/Common_Page_Right_01.png" : "Buttons/Common_Page_Right_00.png", right.x, right.y);
    }
    const Rect ok = {bx + 105.f, by + 225.f, 96.f, 29.f};
    const Rect cancel = {bx + 200.f, by + 225.f, 96.f, 29.f};
    sprite(ok.contains(m_mouseX, m_mouseY) ? "Buttons/Common_OK_half_01.png" : "Buttons/Common_OK_half_00.png", ok.x, ok.y);
    sprite(cancel.contains(m_mouseX, m_mouseY) ? "Buttons/Common_Cancel_half_01.png" : "Buttons/Common_Cancel_half_00.png", cancel.x, cancel.y);
}

// sub 463110 an empty password drops the private flag an empty name shows MSG NEED ROOM NAME
void CreateRoomPopup::confirm() {
    if (m_private && m_password.empty()) m_private = false;
    if (m_name.empty()) {
        m_field = 0;
        m_app.showMessage(m_app.tr("MSG_NEED_ROOM_NAME"));
        return;
    }
    m_app.click();
    m_app.session().createRoom(utf8ToU16(m_name), m_private ? utf8ToU16(m_password) : u"", static_cast<uint32_t>(m_players),
                               static_cast<uint32_t>(m_mode));
    close();
}

void CreateRoomPopup::close() {
    if (m_closed) return;
    m_closed = true;
    m_app.popScreen();
}

// sub 463560 Enter is the OK Escape the Cancel
void CreateRoomPopup::onKey(int key, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (key == GLFW_KEY_ESCAPE) close();
    else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) confirm();
    else if (key == GLFW_KEY_TAB || key == GLFW_KEY_UP || key == GLFW_KEY_DOWN) { if (m_private) m_field = 1 - m_field; }
    else if (key == GLFW_KEY_BACKSPACE) {
        std::string& field = m_field == 0 ? m_name : m_password;
        if (field.empty()) return;
        size_t n = field.size();
        do { --n; } while (n > 0 && (static_cast<unsigned char>(field[n]) & 0xC0) == 0x80);
        field.resize(n);
    }
}

void CreateRoomPopup::onChar(unsigned codepoint) {
    if (codepoint < 32) return;
    std::string& field = m_field == 0 ? m_name : m_password;
    size_t count = 0;
    size_t i = 0;
    while (i < field.size()) { nextUtf8(field, i); ++count; }
    // the 0x002D builder copies at most nine password wchars the name edit takes 40
    if (count >= static_cast<size_t>(m_field == 0 ? 40 : 9)) return;
    appendUtf8(field, codepoint);
}

void CreateRoomPopup::onMouseMove(float x, float y) {
    m_mouseX = x;
    m_mouseY = y;
}

// sub 463590 the mode row resets the count the count row steps by two the last row is privacy
void CreateRoomPopup::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    const float bx = 300.f;
    const float by = 249.f;
    for (int row = 0; row < 3; ++row) {
        const float ry = by + 82.f + 35.f * static_cast<float>(row);
        const Rect left = {bx + 132.f, ry, 24.f, 24.f};
        const Rect right = {bx + 358.f, ry, 24.f, 24.f};
        const int step = left.contains(x, y) ? -1 : right.contains(x, y) ? 1 : 0;
        if (step == 0) continue;
        m_app.click();
        if (row == 0) {
            m_mode = std::clamp(m_mode + step, 0, 3);
            m_players = (m_mode == 2 || m_mode == 3) ? 16 : 8;
        } else if (row == 1) {
            const bool team = m_mode == 1 || m_mode == 3;
            const bool speed = m_mode == 2 || m_mode == 3;
            m_players = std::clamp(m_players + 2 * step, team ? 4 : 2, speed ? 16 : 8);
        } else if (step < 0) {
            m_private = false;
            m_password.clear();
            m_field = 0;
        } else {
            m_private = true;
            m_field = 1;
        }
        return;
    }
    const Rect ok = {bx + 105.f, by + 225.f, 96.f, 29.f};
    const Rect cancel = {bx + 200.f, by + 225.f, 96.f, 29.f};
    if (ok.contains(x, y)) { confirm(); return; }
    if (cancel.contains(x, y)) { m_app.click(); close(); return; }
    const Rect nameBox = {bx + 138.f, by + 44.f, 200.f, 26.f};
    const Rect passBox = {bx + 138.f, by + 183.f, 100.f, 26.f};
    if (nameBox.contains(x, y)) { m_field = 0; return; }
    if (passBox.contains(x, y) && m_private) { m_field = 1; return; }
}

PasswordPopup::PasswordPopup(App& app, uint32_t roomId) : m_app(app), m_roomId(roomId) {
    m_password = m_app.options().roomPassword;
}

// the Pass art of the pak the input sits in its white box the OK under it
void PasswordPopup::draw(SpriteBatch& batch) {
    const FontAtlas& font = m_app.font();
    AssetStore& assets = m_app.assets();
    const float w = m_app.canvasWidth();
    const float h = m_app.canvasHeight();
    const float bx = static_cast<float>(static_cast<int>((w - 353.f) * 0.5f));
    const float by = static_cast<float>(static_cast<int>((h - 177.f) * 0.5f));
    const Texture* back = assets.texture("Popup/Pass/UI_Robby_room_password_back.png");
    if (back && back->valid()) batch.draw(back->handle, bx, by, 353.f, 177.f);
    else batch.fill(bx, by, 353.f, 177.f, rgba(250, 190, 20, 255));
    std::string stars;
    size_t i = 0;
    while (i < m_password.size()) { nextUtf8(m_password, i); stars += '*'; }
    font.drawClipped(batch, stars + "_", bx + 104.f, by + 88.f, 140.f, 15.f, kInkBlack);
    const Texture* ok = assets.texture("Popup/Pass/ok_00.png");
    if (ok && ok->valid()) batch.draw(ok->handle, bx + 146.f, by + 130.f, static_cast<float>(ok->width), static_cast<float>(ok->height));
}

void PasswordPopup::confirm() {
    m_app.click();
    m_app.session().joinRoom(m_roomId, utf8ToU16(m_password));
    close();
}

void PasswordPopup::close() {
    if (m_closed) return;
    m_closed = true;
    m_app.popScreen();
}

void PasswordPopup::onKey(int key, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (key == GLFW_KEY_ESCAPE) close();
    else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) confirm();
    else if (key == GLFW_KEY_BACKSPACE && !m_password.empty()) {
        size_t n = m_password.size();
        do { --n; } while (n > 0 && (static_cast<unsigned char>(m_password[n]) & 0xC0) == 0x80);
        m_password.resize(n);
    }
}

void PasswordPopup::onChar(unsigned codepoint) {
    if (codepoint < 32) return;
    size_t count = 0;
    size_t i = 0;
    while (i < m_password.size()) { nextUtf8(m_password, i); ++count; }
    if (count >= 9) return;
    appendUtf8(m_password, codepoint);
}

void PasswordPopup::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    const float bx = static_cast<float>(static_cast<int>((m_app.canvasWidth() - 353.f) * 0.5f));
    const float by = static_cast<float>(static_cast<int>((m_app.canvasHeight() - 177.f) * 0.5f));
    const Rect ok = {bx + 146.f, by + 130.f, 62.f, 33.f};
    if (ok.contains(x, y)) { confirm(); return; }
    if (x < bx || x > bx + 353.f || y < by || y > by + 177.f) close();
}

}
