#include "GachaPopup.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "engine/formats/nif_reader.h"
#include "engine/render/nif_prop_model.h"
#include "engine/render/scene_renderer.h"
#include "net/Session.h"
#include "screens/ShopCommon.h"
#include "tools/track_scene/ghost_car.h"
#include "ui/MenuFrame.h"
#include "ui/Widgets.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace KnC::Client {

namespace {

// sub 4576B0 back at 185 120 on capture play at plus 256 463 close at plus 498 474
constexpr float kBoxX = 185.f;
constexpr float kBoxY = 120.f;
constexpr float kBoxW = 653.f;
constexpr float kBoxH = 530.f;
constexpr Rect kPlay = {kBoxX + 256.f, kBoxY + 463.f, 131.f, 46.f};
constexpr Rect kClose = {kBoxX + 498.f, kBoxY + 474.f, 96.f, 27.f};
// two lotto tabs at plus 649 on 86 and 262 astro one open at rest
constexpr Rect kTabAstro = {kBoxX + 649.f, kBoxY + 86.f, 143.f, 152.f};
constexpr Rect kTabGold = {kBoxX + 649.f, kBoxY + 262.f, 143.f, 152.f};
// coin icon and its count at top left of window on capture
constexpr float kCoinX = 240.f;
constexpr float kCoinY = 172.f;
constexpr float kCountX = 316.f;
constexpr float kCountY = 195.f;
// sub 4576B0 machine view rect at plus 42 50 of 564x404 stock 3D window
constexpr Rect kMachine = {kBoxX + 42.f, kBoxY + 50.f, 564.f, 404.f};
// astro ticket base key 2000 gold one 2001 gacha handler load
constexpr uint32_t kTicketAstro = 2000;
constexpr uint32_t kTicketGold = 2001;
// prize shows at 9 and three quarter seconds box takes a new roll at 12
constexpr float kPrizeAt = 9.75f;
constexpr float kRollDone = 12.01f;
// NiCamera from gacha camera 01 has frustum top 0 311 twice its arc tangent
constexpr float kMachineFov = 0.6031f;
// sub 4571D0 draws with Camera01 eye half a unit to left of its path
constexpr float kEyeShiftX = -0.5f;
// NiCamera frustum runs 1 to 5000 far side of star box sits about 900 out
constexpr float kMachineFarPlane = 2000.f;

// sub 4576B0 four machines tab picks pair and rare flag picks second of it
const char* const kMachineNif[4] = {
    "gacha_machinenormal_02", "gacha_machinerare_01", "gacha_machinenormal_02_1", "gacha_machinerare_01_1",
};
const char* const kCameraNif = "gacha_camera_01";
const char* const kMachineDir = "Data/Public/Image/Popup/Gacha/Machine";

// sub 4571D0 fires eleven sound cues off table 0x5E4480 at these seconds
struct Cue {
    float at;
    const char* snd;
};
const Cue kCues[11] = {
    {0.6f, "gacha_coin_snd"}, {3.55f, "gacha_trans_snd"}, {3.8f, "gacha_jump_snd"},
    {4.0f, "gacha_jump_snd"}, {4.21f, "gacha_jump_snd"}, {5.1f, "gacha_wall_snd"},
    {5.46f, "gacha_wall_snd"}, {5.86f, "gacha_wall_snd"}, {7.23f, "gacha_wall_snd"},
    {7.93f, "gacha_wall_snd"}, {8.66f, "gacha_result_snd"},
};

// prize icon and name off category same lookups detail panel sub 456E60 uses
void resolvePrize(App& app, uint32_t category, uint32_t baseKey, std::string& icon, std::string& label) {
    const Catalog& cat = app.session().catalog();
    if (category == 0) {
        if (const DriverRow* row = cat.driver(baseKey)) { icon = driverIcon(*row, true); label = app.tr(row->nameKey); }
    } else if (category == 1) {
        if (const KartRow* row = cat.kart(baseKey)) { icon = kartIcon(*row, true); label = app.tr(row->nameKey); }
    } else if (category == 2) {
        if (const ItemRow* row = cat.item(baseKey)) {
            icon = pickIcon(app.assets(), itemIcon(*row, true), "Parts/item_" + row->nameKey + "_01.png");
            label = app.tr(row->nameKey);
        }
    } else if (category == 3) {
        if (const PartRow* row = cat.part(baseKey)) { icon = partIcon(*row, true); label = app.tr(row->nameKey); }
    }
}

}

// linear between two keys around t clamped at both ends as stock samples its linear keys
void GachaPopup::CameraTrack::sample(float t, float out[3]) const {
    out[0] = out[1] = out[2] = 0.f;
    if (times.empty() || values.size() < times.size() * 3) return;
    if (t <= times.front()) { for (int i = 0; i < 3; ++i) out[i] = values[static_cast<size_t>(i)]; return; }
    const size_t last = times.size() - 1;
    if (t >= times[last]) { for (int i = 0; i < 3; ++i) out[i] = values[last * 3 + static_cast<size_t>(i)]; return; }
    size_t k = 0;
    while (k + 1 < times.size() && times[k + 1] <= t) ++k;
    const float span = times[k + 1] - times[k];
    const float f = span > 0.f ? (t - times[k]) / span : 0.f;
    for (int i = 0; i < 3; ++i) {
        const float a = values[k * 3 + static_cast<size_t>(i)];
        const float b = values[(k + 1) * 3 + static_cast<size_t>(i)];
        out[i] = a + (b - a) * f;
    }
}

const OwnedItem* GachaPopup::ticketRow(int tab) const {
    const uint32_t key = tab == 0 ? kTicketAstro : kTicketGold;
    for (const OwnedItem& item : m_app.session().catalog().ownedItems())
        if (item.itemKey == key && item.active != 0 && item.periodValue > 0) return &item;
    return nullptr;
}

// echoed ticket row from answer lands in catalogue so count here is live one
int GachaPopup::ticketsFor(int tab) const {
    const OwnedItem* row = ticketRow(tab);
    return row ? static_cast<int>(row->periodValue) : 0;
}

// Play button sub 456910 sends 0x00ED with owned ticket row then machine plays
void GachaPopup::play() {
    if (m_state == State::Spinning) return;
    if (m_state == State::Prize && m_time < kRollDone) return;
    const OwnedItem* ticket = ticketRow(m_tab);
    if (!ticket) { m_status = "no ticket on this lotto"; return; }
    m_app.session().rollGacha(*ticket);
    m_state = State::Spinning;
    m_time = 0.f;
    m_cue = 0;
    m_haveResult = false;
    m_rareFlag = 0;
    // sub 456980 every machine and camera go back to their first frame
    if (m_sceneReady) for (int h : m_machine) m_view.restartProp(m_app.renderer(), h);
    m_status = "0x00ED sent tab " + std::to_string(m_tab);
}

// answer landed on session prize and rare flag pick icon and machine
void GachaPopup::onSession(SessionEvent event) {
    if (event != SessionEvent::GachaResult) return;
    const GachaResult& r = m_app.session().gachaResult();
    if (!r.valid) return;
    m_haveResult = true;
    m_rareFlag = r.rareFlag;
    m_prizeCategory = r.category;
    m_prizeBaseKey = r.baseKey;
    std::printf("[gacha] result category %u base %u rare %u tickets left %d\n", r.category, r.baseKey, r.rareFlag, ticketsFor(m_tab));
}

void GachaPopup::tickCues() {
    while (m_cue < 11 && m_time >= kCues[m_cue].at) {
        m_app.sound().play(kCues[m_cue].snd);
        ++m_cue;
    }
}

void GachaPopup::update(float dt) {
    m_frameDt = dt;
    if (m_state == State::Idle) return;
    // sub 4571D0 clock stops past twelve seconds machine holds its last frame
    if (m_time < kRollDone) m_time = std::min(m_time + dt, kRollDone);
    if (m_state == State::Spinning) {
        tickCues();
        if (m_time >= kPrizeAt) m_state = State::Prize;
    }
}

int GachaPopup::machineIndex() const {
    return m_tab * 2 + (m_haveResult && m_rareFlag != 0 ? 1 : 0);
}

// four machine nifs beside their textures on an empty scene then two paths from camera nif
bool GachaPopup::loadMachine() {
    using namespace KnC::Render;
    namespace fs = std::filesystem;
    m_sceneReady = false;
    for (int& h : m_machine) h = -1;
    const fs::path dir = fs::path(m_app.options().gameDir) / kMachineDir;
    if (!m_view.loadEmpty(m_app.renderer())) return false;
    // star box radius is 550 camera stands 336 out far plane of 240 cuts both
    m_app.renderer().set_far_plane(kMachineFarPlane);
    for (int i = 0; i < 4; ++i) {
        NifModelRequest request;
        request.nif_path = (dir / (std::string(kMachineNif[i]) + ".nif")).string();
        request.texture_dir = dir.string();
        PropModel model;
        std::string error;
        if (!load_prop_model(request, model, error)) {
            std::printf("[gacha] %s failed %s\n", request.nif_path.c_str(), error.c_str());
            return false;
        }
        KnC::Tools::resolve_textures(request.texture_dir, model);
        m_machine[i] = m_view.addProp(m_app.renderer(), model);
        const float identity[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
        m_view.placeProp(m_machine[i], identity, false);
        std::printf("[gacha] %s parts %zu particle systems %zu\n", kMachineNif[i], model.parts.size(), model.particle_systems.size());
    }
    // Camera01 rides a NiPosData path and looks at Camera01 Target which rides a NiTransformData path
    KnC::NifScene scene;
    std::string error;
    const std::string camera = (dir / (std::string(kCameraNif) + ".nif")).string();
    if (!KnC::read_nif_scene(camera, scene, error)) {
        std::printf("[gacha] %s failed %s\n", camera.c_str(), error.c_str());
        return false;
    }
    m_eyePath = CameraTrack();
    m_lookPath = CameraTrack();
    for (const KnC::NifBlock& block : scene.blocks) {
        if (!block.animation) continue;
        if (block.type == "NiPosData" && block.animation->channel.components == 3) {
            m_eyePath.times = block.animation->channel.times;
            m_eyePath.values = block.animation->channel.values;
        } else if (block.type == "NiTransformData" && block.animation->translations.components == 3) {
            m_lookPath.times = block.animation->translations.times;
            m_lookPath.values = block.animation->translations.values;
        }
    }
    if (m_eyePath.times.empty() || m_lookPath.times.empty()) {
        std::printf("[gacha] the camera nif carries no path eye %zu keys look %zu keys\n", m_eyePath.times.size(), m_lookPath.times.size());
        return false;
    }
    std::printf("[gacha] camera path eye %zu keys look %zu keys\n", m_eyePath.times.size(), m_lookPath.times.size());
    m_sceneReady = true;
    return true;
}

// another screen holds renderer so machine scene loads again failed load not retried
void GachaPopup::sceneLost() {
    if (m_sceneTried && !m_sceneReady) return;
    m_sceneTried = true;
    if (!loadMachine()) std::printf("[gacha] the machine window stays flat the nifs did not load\n");
}

// machine window lobby sprites under box stay out of it while scene draws
bool GachaPopup::sceneHole(float& x, float& y, float& w, float& h) const {
    if (!m_sceneReady) return false;
    x = kMachine.x; y = kMachine.y; w = kMachine.w; h = kMachine.h;
    return true;
}

// machine window in framebuffer pixels then camera from sub 4571D0 at machine clock
bool GachaPopup::drawScene() {
    // base screen with no scene of its own never hands renderer over so load starts here
    if (!m_sceneTried) sceneLost();
    if (!m_sceneReady) return false;
    // canvas rect in framebuffer pixels letterboxed or stretched as frame is
    float px[4];
    m_app.canvasToPixels(kMachine.x, kMachine.y, kMachine.w, kMachine.h, px);
    KnC::Render::ViewportRect box;
    box.x = static_cast<uint16_t>(px[0]);
    box.y = static_cast<uint16_t>(px[1]);
    box.width = static_cast<uint16_t>(px[2]);
    box.height = static_cast<uint16_t>(px[3]);
    const KnC::Render::ViewportRect& now = m_app.renderer().viewport();
    if (now.x != box.x || now.y != box.y || now.width != box.width || now.height != box.height)
        m_app.renderer().set_viewport(box);
    const int shown = machineIndex();
    const float identity[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    for (int i = 0; i < 4; ++i) m_view.placeProp(m_machine[i], identity, i == shown);
    float eye[3], look[3];
    m_eyePath.sample(m_time, eye);
    m_lookPath.sample(m_time, look);
    eye[0] += kEyeShiftX;
    // idle box holds its first frame so scene clock stands still until play press
    m_view.drawFixed(m_app.renderer(), eye, look, kMachineFov, m_state == State::Idle ? 0.f : m_frameDt);
    return true;
}

void GachaPopup::draw(SpriteBatch& batch) {
    AssetStore& assets = m_app.assets();
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    auto sprite = [&](const std::string& path, float x, float y) -> const Texture* {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
        return t;
    };
    // back is opaque grey in window stock draws over it ours keeps a hole
    const Texture* back = assets.texture("Popup/Gacha/Gacha_Back.png");
    if (back && back->valid()) drawTextureWithHole(batch, back, kBoxX, kBoxY, m_sceneReady ? kMachine : Rect{0.f, 0.f, 0.f, 0.f});
    else batch.fill(kBoxX, kBoxY, kBoxW, kBoxH, rgba(200, 200, 210, 240));
    // dark blue star field stands in for machine window when nifs are missing
    if (!m_sceneReady) batch.fill(kMachine.x, kMachine.y, kMachine.w, kMachine.h, rgba(36, 40, 122, 255));
    // coin count for open lotto astro ticket base 2000 gold one 2001
    sprite("Popup/Gacha/UI_shop_gacha_coin.png", kCoinX, kCoinY);
    const int tickets = ticketsFor(m_tab);
    char digit[64];
    std::snprintf(digit, sizeof(digit), "Popup/Gacha/UI_shop_gacha_num_%d.png", std::min(std::max(tickets, 0), 9));
    if (!sprite(digit, kCountX, kCountY)) ctx.bold.draw(batch, std::to_string(tickets), kCountX, kCountY, 20.f, rgba(255, 96, 32, 255));
    sprite(m_tab == 0 ? "Popup/Gacha/Gacha_tab_C_01.png" : "Popup/Gacha/Gacha_tab_C_00.png", kTabAstro.x, kTabAstro.y);
    sprite(m_tab == 1 ? "Popup/Gacha/Gacha_tab_P_01.png" : "Popup/Gacha/Gacha_tab_P_00.png", kTabGold.x, kTabGold.y);

    const float cx = kMachine.x + kMachine.w * 0.5f;
    const float cy = kMachine.y + kMachine.h * 0.5f;
    if (m_state == State::Spinning && !m_sceneReady) {
        // without machine nifs a 2D coin turns so roll shows some motion
        const Texture* coin = assets.texture("Popup/Gacha/UI_shop_gacha_coin.png");
        if (coin && coin->valid()) {
            const float wobble = std::sin(m_time * 8.f) * 24.f;
            batch.draw(coin->handle, cx - static_cast<float>(coin->width) * 0.5f + wobble, cy - static_cast<float>(coin->height) * 0.5f,
                       static_cast<float>(coin->width), static_cast<float>(coin->height));
        }
        drawAligned(ctx, ctx.bold, "Rolling ...", cx, cy + 60.f, 20.f, kInkWhite, Align::Centre);
    } else if (m_state == State::Prize) {
        // sub 456E60 draws prize over machine with MSG GACHA GET
        std::string icon, label;
        resolvePrize(m_app, m_prizeCategory, m_prizeBaseKey, icon, label);
        const Texture* pic = icon.empty() ? nullptr : assets.texture(icon);
        if (pic && pic->valid()) batch.draw(pic->handle, cx - 64.f, cy - 74.f, 128.f, 128.f);
        drawAligned(ctx, ctx.bold, label.empty() ? m_app.tr("MSG_GACHA_GET") : label, cx, cy + 64.f, 18.f, kInkWhite, Align::Centre);
        if (!label.empty()) drawAligned(ctx, ctx.font, m_app.tr("MSG_GACHA_GET"), cx, cy + 88.f, 14.f, rgba(255, 220, 96, 255), Align::Centre);
    }

    const bool overPlay = kPlay.contains(m_mouseX, m_mouseY);
    if (!sprite(overPlay ? "Popup/Gacha/UI_shop_gacha_play_01.png" : "Popup/Gacha/UI_shop_gacha_play_00.png", kPlay.x, kPlay.y))
        ctx.bold.drawCentered(batch, "START", kPlay.x + kPlay.w * 0.5f, kPlay.y + 14.f, 18.f, kInkWhite);
    const bool overClose = kClose.contains(m_mouseX, m_mouseY);
    if (!sprite(overClose ? "Buttons/Common_Close_01.png" : "Buttons/Common_Close_00.png", kClose.x, kClose.y))
        ctx.bold.drawCentered(batch, "Close", kClose.x + kClose.w * 0.5f, kClose.y + 7.f, 15.f, kInkDark);
    if (!m_status.empty() && m_app.options().statusLine) ctx.font.draw(batch, m_status, kBoxX + 10.f, kBoxY + kBoxH - 18.f, 12.f, kInkGrey);
}

void GachaPopup::onKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) close();
    else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) { m_app.click(); play(); }
}

void GachaPopup::onMouseMove(float x, float y) {
    m_mouseX = x;
    m_mouseY = y;
}

void GachaPopup::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    if (kClose.contains(x, y)) { m_app.click(); close(); return; }
    // sub 456830 tabs answer only while no machine plays
    if (m_state != State::Spinning && kTabAstro.contains(x, y)) { m_app.click(); m_tab = 0; m_state = State::Idle; m_time = 0.f; return; }
    if (m_state != State::Spinning && kTabGold.contains(x, y)) { m_app.click(); m_tab = 1; m_state = State::Idle; m_time = 0.f; return; }
    if (kPlay.contains(x, y)) { m_app.click(); play(); return; }
    if (x < kBoxX || x > kBoxX + kBoxW || y < kBoxY || y > kBoxY + kBoxH) close();
}

// machine viewport goes back to whole window base screen sets its own on next draw
void GachaPopup::leave() {
    KnC::Render::ViewportRect full;
    full.x = 0;
    full.y = 0;
    full.width = m_app.width();
    full.height = m_app.height();
    m_app.renderer().set_viewport(full);
}

void GachaPopup::close() {
    if (m_closed) return;
    m_closed = true;
    m_app.popScreen();
}

}
