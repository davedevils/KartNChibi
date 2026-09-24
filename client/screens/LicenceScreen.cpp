#include "LicenceScreen.h"
#include "MissionScreen.h"
#include "race/LookBack.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "engine/render/map_scene.h"
#include "engine/render/scene_renderer.h"
#include "games/kart/physics/client/boost.h"
#include "net/Session.h"
#include "screens/GhostModeScreen.h"
#include "screens/ShopCommon.h"
#include "ui/CharPanel.h"
#include "ui/MenuFrame.h"

#include <GLFW/glfw3.h>
#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace KnC::Client {

using KnC::Kart::Client::InputFlags;

namespace {

// sub 437190 origin 31 85 top at 25 50 back at 37 88
constexpr float kOriginX = 31.f;
constexpr float kOriginY = 85.f;
constexpr float kTopX = 25.f;
constexpr float kTopY = 50.f;
constexpr float kBackX = 37.f;
constexpr float kBackY = 88.f;
// medals plus 35 on 139 226 313 tiles at plus 282 every 198 on 160 bonus at plus 224 386
constexpr float kMedalX = 35.f;
constexpr float kMedalY[3] = {139.f, 226.f, 313.f};
constexpr float kTileX = 282.f;
constexpr float kTileStep = 198.f;
constexpr float kTileY = 160.f;
constexpr float kBonusX = 224.f;
constexpr float kBonusY = 386.f;
constexpr float kTileW = 118.f;
constexpr float kTileH = 145.f;
// sub 437820 clear stamp at tile plus 16 47 big lock plus 6 25 medal lock plus 2 3
constexpr float kClearDx = 16.f;
constexpr float kClearDy = 47.f;
constexpr float kLockDx = 6.f;
constexpr float kLockDy = 25.f;
constexpr float kMedalLockDx = 2.f;
constexpr float kMedalLockDy = 3.f;
// advanced medal wants grade 1 and level 10 master medal grade 2 and level 40
constexpr int kAdvancedLevel = 10;
constexpr int kMasterLevel = 40;
// picked state FUN 00436F10 start plus 33 140 back plus 33 480 sheet plus 187 143
constexpr float kStartDx = 33.f;
constexpr float kStartDy = 140.f;
constexpr float kBackDx = 33.f;
constexpr float kBackDy = 480.f;
constexpr float kBoardDx = 187.f;
constexpr float kBoardDy = 143.f;
// minimap plus 199 399 info sheet plus 505 162 preview rect plus 160 60 of 340 by 350
constexpr float kMinimapDx = 199.f;
constexpr float kMinimapDy = 399.f;
constexpr float kInfoDx = 505.f;
constexpr float kInfoDy = 162.f;
constexpr Rect kPreviewRect = {kOriginX + 160.f, kOriginY + 60.f, 340.f, 350.f};
// blue box of license back 003 kart shows over it 10 px inside sheet corner
constexpr Rect kPreviewHole = {kOriginX + kBoardDx + 10.f, kOriginY + kBoardDy + 10.f, 265.f, 225.f};

const char* const kMedalArt[3] = {"LicenseMenu/license_rookie_", "LicenseMenu/license_advanced_", "LicenseMenu/license_master_"};

float tileX(int i) { return i < 3 ? kOriginX + kTileX + kTileStep * static_cast<float>(i) : kOriginX + kBonusX; }
float tileY(int i) { return i < 3 ? kOriginY + kTileY : kOriginY + kBonusY; }

bool autoLicence() {
    const char* env = std::getenv("KNC_AUTO_LICENCE");
    return env && env[0] != '\0';
}

// test index from KNC AUTO LICENCE key of tier picked on entry
int autoLicenceTest() {
    const char* env = std::getenv("KNC_AUTO_LICENCE");
    if (!env || env[0] == '\0') return -1;
    const int v = std::atoi(env);
    return v < 0 ? 0 : v;
}

// sheet png as disk path renderer reads textures from files pak copy written out
std::string boardPath(App& app) {
    const std::string onDisk = app.options().gameDir + "/Data/Public/Image/LicenseMenu/license_back_003.PNG";
    std::error_code ignored;
    if (std::filesystem::exists(onDisk, ignored)) return onDisk;
    std::vector<uint8_t> bytes;
    if (!app.assets().readBytes("Image/LicenseMenu/license_back_003.PNG", bytes) || bytes.empty()) return std::string();
    const std::filesystem::path out = std::filesystem::temp_directory_path(ignored) / "knc_client_licence_board.png";
    std::ofstream file(out, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return out.string();
}

}

// sub 42BCE0 ghost and mission buttons answer only past level 9 or with licence grade
bool LicenceScreen::stageGateOpen(const App& app) {
    const Profile& p = const_cast<App&>(app).session().profile();
    return p.level > 9 || p.band > 0;
}

// rookie on grade 0 or level under 10 advanced on grade 1 under 40 master on grade 2 from 40
int LicenceScreen::startTier() const {
    const Profile& profile = m_app.session().profile();
    const int grade = profile.band;
    const int level = profile.level;
    if (grade == 0 || level < kAdvancedLevel) return 0;
    if (grade == 1 && level < kMasterLevel) return 1;
    if (grade == 2 && level >= kMasterLevel) return 2;
    return grade - 1;
}

bool LicenceScreen::tierOpen(int tier) const {
    const Profile& profile = m_app.session().profile();
    if (tier <= 0) return true;
    if (tier == 1) return profile.band >= 1 && profile.level >= kAdvancedLevel;
    return profile.band >= 2 && profile.level >= kMasterLevel;
}

bool LicenceScreen::tilePickable(int test) const {
    if (test == 0) return true;
    return m_app.session().licenceState(static_cast<uint32_t>(m_tier * 10 + test - 1)) != nullptr;
}

void LicenceScreen::applyTierArt() {
    AssetStore& assets = m_app.assets();
    for (int i = 0; i < 4; ++i) {
        ButtonWidget* b = findAs<ButtonWidget>("btn_tile" + std::to_string(i));
        if (!b) continue;
        char art[64];
        std::snprintf(art, sizeof(art), "LicenseMenu/license_%d_%d_", m_tier, i);
        b->normal = assets.texture(std::string(art) + "00.png");
        b->hover = assets.texture(std::string(art) + "01.png");
        b->pressed = assets.texture(std::string(art) + "02.png");
        const Texture* size = b->normal ? b->normal : b->hover;
        b->rect = {tileX(i), tileY(i), size ? static_cast<float>(size->width) : kTileW, size ? static_cast<float>(size->height) : kTileH};
    }
}

void LicenceScreen::showPickedWidgets(bool picked) {
    for (int i = 0; i < 4; ++i)
        if (Widget* w = find("btn_tile" + std::to_string(i))) w->visible = !picked;
    for (int t = 0; t < 3; ++t)
        if (Widget* w = find("btn_tier" + std::to_string(t))) w->visible = !picked;
    if (Widget* w = find("btn_start")) w->visible = picked;
    if (Widget* w = find("btn_back")) w->visible = picked;
}

void LicenceScreen::enter() {
    AssetStore& assets = m_app.assets();
    addMenuFrame(*this, assets, FrameMode::Full, "tutorial");
    auto button = [&](const std::string& id, const std::string& action, const std::string& art, float x, float y, float w, float h) {
        auto b = std::make_unique<ButtonWidget>();
        b->type = ElementType::Button;
        b->id = id;
        b->action = action;
        b->normal = assets.texture(art + "00.png");
        b->hover = assets.texture(art + "01.png");
        b->pressed = assets.texture(art + "02.png");
        const Texture* size = b->normal ? b->normal : b->hover;
        b->rect = {x, y, size ? static_cast<float>(size->width) : w, size ? static_cast<float>(size->height) : h};
        b->zIndex = 30;
        add(std::move(b));
    };
    for (int t = 0; t < 3; ++t)
        button("btn_tier" + std::to_string(t), "tier" + std::to_string(t), kMedalArt[t], kOriginX + kMedalX, kOriginY + kMedalY[t], 80.f, 80.f);
    m_tier = startTier();
    for (int i = 0; i < 4; ++i) {
        char art[64];
        std::snprintf(art, sizeof(art), "LicenseMenu/license_%d_%d_", m_tier, i);
        button("btn_tile" + std::to_string(i), "tile" + std::to_string(i), art, tileX(i), tileY(i), kTileW, kTileH);
    }
    // FUN 00437190 creates start and back buttons at picked state coords
    button("btn_start", "start", "LicenseMenu/license_start_", kOriginX + kStartDx, kOriginY + kStartDy, 90.f, 87.f);
    button("btn_back", "unpick", "LicenseMenu/license_back_", kOriginX + kBackDx, kOriginY + kBackDy, 90.f, 87.f);
    m_time = 0.f;
    m_captured = false;
    m_capturedPick = false;
    m_autoStarted = false;
    m_pick = -1;
    m_startSent = false;
    m_status.clear();
    showPickedWidgets(false);
    m_app.playMenuMusic();
}

void LicenceScreen::leave() {
    KnC::Render::ViewportRect full;
    full.x = 0;
    full.y = 0;
    full.width = m_app.width();
    full.height = m_app.height();
    m_app.renderer().set_viewport(full);
}

// blue box slice of sheet drawn as quad before fixed camera then own kart
void LicenceScreen::loadPreview() {
    if (m_sceneReady) return;
    // sub 4375FD opens the char panel camera on this rect the sheet sits at its board spot
    const Rect sheet = {kOriginX + kBoardDx, kOriginY + kBoardDy, 708.f, 420.f};
    m_sceneReady = loadCharPreviewSceneFor(m_app, m_view, m_previewWorld, kPreviewRect, boardPath(m_app), sheet);
    if (!m_sceneReady) return;
    const Session& session = m_app.session();
    const Catalog& cat = session.catalog();
    const KartRow* kart = cat.kart(session.myKartKey());
    const DriverRow* driver = cat.driver(session.myDriverKey());
    const std::string model = kart && !kart->model.empty() ? kart->model : std::string("Basic_1");
    const std::string asset = driver && !driver->asset.empty() ? driver->asset : std::string("Cosmo");
    // paint loads with car so first frame swaps no body uploads nothing
    m_previewCar = m_view.addCar(m_app.renderer(), m_app.options().gameDir, model, asset, charPreviewPaint(m_app, model));
    CarPose pose;
    m_view.setPose(m_previewCar, pose, 0.f, 0.f);
}

// race above or a popup drew its own scene so preview reloads on next pick
void LicenceScreen::sceneLost() {
    m_sceneReady = false;
    m_previewCar = -1;
    if (m_pick >= 0) loadPreview();
}

bool LicenceScreen::drawScene() {
    if (m_pick < 0 || !m_sceneReady || m_previewCar < 0) return false;
    drawCharPreview(m_app, m_view, m_previewCar, 0.f, kPreviewRect);
    return true;
}

void LicenceScreen::update(float dt) {
    m_time += dt;
    if (m_app.captureMode() && !m_captured && m_time > 1.2f) {
        m_captured = true;
        m_app.captureStage("licence");
        // scripted run takes its own shots and ends on its quit line
        if (m_app.options().stopAt == "licence" && !autoLicence() && !m_app.scripted()) m_app.finishRun();
    }
    // KNC AUTO LICENCE picks test of open tier then presses start once picked frame is shot
    if (autoLicence() && !m_autoStarted && m_time > 1.6f) {
        const int test = std::min(autoLicenceTest(), 3);
        // login burst from our server carries no 0x00A2 rows second 0x0016 brings them
        if (m_pick < 0 && !tilePickable(test) && !m_rowsAsked) {
            m_rowsAsked = true;
            m_app.session().openLicense();
            m_status = "0x0016 sent again for the 0x00A2 rows";
        }
        if (m_pick < 0 && tilePickable(test)) {
            m_pick = test;
            loadPreview();
            showPickedWidgets(true);
            m_status = "auto picked test " + std::to_string(m_tier * 10 + m_pick);
        }
        if (m_pick >= 0 && m_time > 2.6f) {
            if (m_app.captureMode() && !m_capturedPick) {
                m_capturedPick = true;
                m_app.captureStage("licence_pick");
            }
            if (m_time > 3.4f) {
                m_autoStarted = true;
                sendStart();
            }
        }
    }
}

void LicenceScreen::draw(SpriteBatch& batch) {
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    AssetStore& assets = m_app.assets();
    auto sprite = [&](const char* path, float x, float y) {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
    };
    const bool picked = m_pick >= 0 && m_sceneReady && m_previewCar >= 0;
    const Rect hole = picked ? kPreviewHole : Rect{0.f, 0.f, 0.f, 0.f};
    if (m_pick >= 0) {
        // FUN 00436F10 picked state art hole shows 3D preview
        drawFrameBack(batch, assets, hole);
        drawTextureWithHole(batch, assets.texture("LicenseMenu/license_background_01.PNG"), kBackX, kBackY, hole);
        sprite("LicenseMenu/license_Top.png", kTopX, kTopY);
        drawTextureWithHole(batch, assets.texture("LicenseMenu/license_back_003.PNG"), kOriginX + kBoardDx, kOriginY + kBoardDy, hole);
        char art[64];
        std::snprintf(art, sizeof(art), "LicenseMenu/license_minimap_%d_%d.png", m_tier, m_pick);
        sprite(art, kOriginX + kMinimapDx, kOriginY + kMinimapDy);
        std::snprintf(art, sizeof(art), "LicenseMenu/license_info_%d_%d.png", m_tier, m_pick);
        sprite(art, kOriginX + kInfoDx, kOriginY + kInfoDy);
    } else {
        for (Widget* w : m_order) if (w->visible && w->zIndex < 0) w->draw(ctx);
        sprite("LicenseMenu/license_background_00.PNG", kBackX, kBackY);
        sprite("LicenseMenu/license_Top.png", kTopX, kTopY);
    }
    // DMV banner and bonus tree words are part of back art
    for (Widget* w : m_order) if (w->visible && w->zIndex >= 0) w->draw(ctx);
    drawOverlay(ctx);
}

// sub 437820 medal locks by grade and level per test clear stamp or lock past first open
void LicenceScreen::drawOverlay(DrawContext& ctx) {
    AssetStore& assets = m_app.assets();
    auto sprite = [&](const char* path, float x, float y) {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) ctx.batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
    };
    if (m_pick < 0) {
        for (int t = 1; t < 3; ++t) {
            if (tierOpen(t)) continue;
            sprite("LicenseMenu/license_lock.PNG", kOriginX + kMedalX + kMedalLockDx, kOriginY + kMedalY[t] + kMedalLockDy);
        }
        const Session& session = m_app.session();
        bool openSeen = false;
        for (int i = 0; i < 4; ++i) {
            const LicenceProgressRow* row = session.licenceState(static_cast<uint32_t>(m_tier * 10 + i));
            if (row && row->passed == 1) {
                sprite("LicenseMenu/Common_Clear.png", tileX(i) + kClearDx, tileY(i) + kClearDy);
                continue;
            }
            if (row) continue;
            // first test with no row is open one every later one wears lock
            if (!openSeen) { openSeen = true; continue; }
            sprite("LicenseMenu/license_Bonus_00.PNG", tileX(i) + kLockDx, tileY(i) + kLockDy);
        }
    }
    if (!m_status.empty() && m_app.options().statusLine) ctx.font.draw(ctx.batch, m_status, 40.f, 706.f, 12.f, kInkGrey);
}

void LicenceScreen::onKey(int key, int action, int mods) {
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        if (m_pick >= 0) { m_pick = -1; showPickedWidgets(false); return; }
        exitToLobby();
        return;
    }
    if (action == GLFW_PRESS && (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) && m_pick >= 0) { sendStart(); return; }
    WidgetScreen::onKey(key, action, mods);
}

void LicenceScreen::sendStart() {
    if (m_pick < 0 || m_startSent) return;
    m_startSent = true;
    m_app.session().startLicenceTest();
    m_status = "0x0062 sent for test " + std::to_string(m_tier * 10 + m_pick);
}

void LicenceScreen::onAction(const std::string& action, Widget& source) {
    if (action == "lobby" || action == "channel") { exitToLobby(); return; }
    if (action == "quit") { m_app.quit(); return; }
    if (action == "start") { sendStart(); return; }
    if (action == "unpick") { m_pick = -1; showPickedWidgets(false); return; }
    if (action == "missions") {
        if (stageGateOpen(m_app)) { m_app.session().openMissionMenu(); m_status = "missions 0x008F sent"; }
        return;
    }
    if (action == "ghost") {
        if (stageGateOpen(m_app)) GhostModeScreen::open(m_app);
        return;
    }
    if (action == "tutorial") return;
    if (action.rfind("tier", 0) == 0) {
        const int tier = action[4] - '0';
        // locked medal answers nothing open one swaps tiles to its tests
        if (!tierOpen(tier)) return;
        m_tier = tier;
        m_pick = -1;
        applyTierArt();
        m_status = "tier " + std::to_string(tier);
        return;
    }
    if (action.rfind("tile", 0) == 0) {
        const int test = action[4] - '0';
        if (!tilePickable(test)) { m_status = "test " + std::to_string(test) + " is locked"; return; }
        m_pick = test;
        loadPreview();
        showPickedWidgets(true);
        m_status = "licence test " + std::to_string(m_tier * 10 + m_pick) + " picked";
        return;
    }
    if (frameAction(m_app, action)) return;
    m_status = source.id + " is not part of this phase";
}

void LicenceScreen::onSession(SessionEvent event) {
    if (event == SessionEvent::Disconnected) m_status = "disconnected";
    // empty 0x0062 ack from sub 479580 opens stage 13 on picked key
    if (event == SessionEvent::LicenceTestAck && m_startSent && m_pick >= 0) {
        m_startSent = false;
        m_app.pushScreen(std::make_unique<LicenceRunScreen>(m_app, static_cast<uint32_t>(m_tier * 10 + m_pick)));
    }
    // race above popped on this ack menu shows fresh rows
    if (event == SessionEvent::LicenseAck) {
        m_pick = -1;
        m_startSent = false;
        showPickedWidgets(false);
        applyTierArt();
        m_app.playMenuMusic();
        // scripted run ends with its own quit so last actions get their frames
        if (autoLicence() && m_autoStarted && m_app.captureMode() && !m_app.scripted()) m_app.finishRun();
    }
}

// stock leaves licence menu with lobby request 0x0012
void LicenceScreen::exitToLobby() {
    m_app.session().openLobby();
    m_status = "0x0012 sent";
}

namespace {

// FUN 0041B860 time limits by key 130000 default 125000 for most tests 60000 for 22 70000 for 23
double licenceLimitMs(uint32_t key) {
    switch (key) {
    case 1: case 2: case 3: case 10: case 11: case 12: case 13: case 20: case 21: return 125000.0;
    case 22: return 60000.0;
    case 23: return 70000.0;
    default: return 130000.0;
    }
}

// intro board 59 104 text 439 219 wide 355 gold 869 242 exp 869 264
constexpr float kBoardX = 59.f;
constexpr float kBoardY = 104.f;
constexpr float kBoardTextX = 439.f;
constexpr float kBoardTextY = 219.f;
constexpr float kBoardTextW = 355.f;
constexpr float kBoardGoldX = 869.f;
constexpr float kBoardGoldY = 242.f;
constexpr float kBoardExpY = 264.f;
// FUN 0041B020 skip button zone 884 to 954 on 339 to 404
constexpr Rect kSkipZone = {884.f, 339.f, 70.f, 65.f};
// sub 4B2690 mode 2 240 ms pre roll then one digit per second GO releases car
constexpr float kCountdownPreroll = 0.24f;
constexpr float kCountdownSeconds = 3.24f;
// win clip runs 200 frames lose clip 120 frames at 60 a second
constexpr float kPassSeconds = 200.f / 60.f;
constexpr float kFailSeconds = 120.f / 60.f;
// licence clear popup at 163 15 enter zone at plus 510 360 numbers at 338
constexpr float kClearX = 163.f;
constexpr float kClearY = 15.f;
constexpr Rect kClearEnter = {kClearX + 510.f, kClearY + 360.f, 70.f, 60.f};
constexpr float kClearExpX = 485.f;
constexpr float kClearGoldX = 615.f;
constexpr float kClearNumberY = 338.f;
// timer shows 3s after countdown starts run capture 12s in
constexpr float kTimerDelay = 3.f;
constexpr float kRunCaptureSeconds = 12.f;
// gauge update 0x4ADC20 grows by 3 2 per second 0x5A6E6C hands booster at 127 0x5A6E60
constexpr float kGaugeRate = 3.2f;
constexpr float kGaugeFull = 127.f;
// licence camera of stage 0xD min distance 8 4 scale 0 9 pitch 37 look height 3 5
constexpr float kLicenceCamDistance = 8.4f;
constexpr float kLicenceCamScale = 0.9f;
constexpr float kLicenceCamPitch = 37.f;
constexpr float kLicenceCamHeight = 3.5f;
// gauge board sits at width minus 264 height minus 230 needle turns around plus 151 109
constexpr float kGaugeRight = 264.f;
constexpr float kGaugeBottom = 230.f;
constexpr float kGaugePivotX = 151.f;
constexpr float kGaugePivotY = 109.f;
// arc measured on board png art inner radius 72 outer 104 needle sweep 4 38 rad
constexpr float kGaugeInner = 72.f;
constexpr float kGaugeOuter = 104.f;
constexpr float kGaugeStart = -3.14f;
constexpr float kGaugeSweep = 4.38f;
constexpr int kGaugeSteps = 24;

}

LicenceRunScreen::LicenceRunScreen(App& app, uint32_t licenceKey) : m_app(app), m_key(licenceKey) {}

LicenceRunScreen::~LicenceRunScreen() = default;

std::string LicenceRunScreen::worldFolder() const {
    const uint32_t k = m_key;
    if (k < 10) return "License_01";
    if (k == 12) return "License_02/nif_jump";
    if (k == 10 || k == 11 || k == 13) return "License_02/nif";
    if (k == 20 || k == 23) return "License_03/01";
    if (k == 21 || k == 22) return "License_03/02";
    char name[32];
    std::snprintf(name, sizeof(name), "License_%02u", k / 10 + 1);
    return name;
}

void LicenceRunScreen::enter() {
    // menu below kept a small preview viewport this track needs whole window
    KnC::Render::ViewportRect full;
    full.x = 0;
    full.y = 0;
    full.width = m_app.width();
    full.height = m_app.height();
    m_app.renderer().set_viewport(full);
    m_autoDrive = autoLicence();
    m_limitMs = licenceLimitMs(m_key);
    // 0x4AE230 and 0x4ADC20 open drift gauge on licence keys 2 and 13 only
    m_gaugeTest = m_key == 2 || m_key == 13;
    // FUN 0041D710 and siblings count limit down rookie driving test FUN 0041F870 does not
    m_limitCounts = m_key != 0 && m_key != 1;
    m_status = "loading World/License/" + worldFolder();
    m_app.playMusic("tutorial_bgm");
    if (!loadWorld()) return;
    spawnLocal();
    m_phase = Phase::Intro;
    m_phaseTime = 0.f;
}

void LicenceRunScreen::leave() {
    m_app.sound().stopEngine();
}

bool LicenceRunScreen::loadWorld() {
    TrackFiles files;
    files.themeFolder = "License";
    files.trackFolder = worldFolder();
    files.laps = 1;
    const std::string world = m_app.options().gameDir + "/Data/Public/World";
    files.mapDir = findEntryCi(world, "License");
    if (files.mapDir.empty()) { m_status = "no World/License folder"; std::printf("[licence] %s\n", m_status.c_str()); return false; }
    std::string dir = files.mapDir;
    std::string rest = files.trackFolder;
    while (!rest.empty()) {
        const size_t slash = rest.find('/');
        const std::string part = slash == std::string::npos ? rest : rest.substr(0, slash);
        rest = slash == std::string::npos ? std::string() : rest.substr(slash + 1);
        dir = findEntryCi(dir, part);
        if (dir.empty()) { m_status = "no World/License/" + files.trackFolder; std::printf("[licence] %s\n", m_status.c_str()); return false; }
    }
    files.trackDir = dir;
    for (char& c : files.mapDir) if (c == '\\') c = '/';
    for (char& c : files.trackDir) if (c == '\\') c = '/';
    std::string error;
    if (!loadRaceWorld(files, m_world, error)) { m_status = "world load failed: " + error; std::printf("[licence] %s\n", m_status.c_str()); return false; }
    if (!m_sim.init(m_world, m_app.session().profile().playerId, error)) { m_status = "sim init failed: " + error; return false; }
    if (const OwnedPet* worn = m_app.session().catalog().equippedPet()) m_sim.setEquippedPet(worn->petKey);
    m_view.load(m_app.renderer(), m_world);
    m_view.camera().licenceMode = true;
    m_auto.reset(m_sim.line());
    m_worldLoaded = true;
    orderMinimapQuad();
    std::printf("[licence] %s loaded racing line %zu points checkpoints %d\n", files.trackFolder.c_str(), m_sim.line().size(),
                m_world.checkpointCount);
    return true;
}

void LicenceRunScreen::orderMinimapQuad() {
    MinimapData& map = m_world.minimap;
    if (!map.valid) return;
    float cx = 0.f, cy = 0.f;
    for (int i = 0; i < 4; ++i) { cx += map.cornerX[i]; cy += map.cornerY[i]; }
    cx *= 0.25f;
    cy *= 0.25f;
    int order[4] = {0, 1, 2, 3};
    float angle[4];
    for (int i = 0; i < 4; ++i) angle[i] = std::atan2(map.cornerY[i] - cy, map.cornerX[i] - cx);
    for (int i = 0; i < 3; ++i)
        for (int j = i + 1; j < 4; ++j)
            if (angle[order[j]] < angle[order[i]]) std::swap(order[i], order[j]);
    bool same = true;
    for (int i = 0; i < 4; ++i) if (order[i] != i) same = false;
    if (same) return;
    MinimapData sorted = map;
    for (int i = 0; i < 4; ++i) {
        sorted.cornerX[i] = map.cornerX[order[i]];
        sorted.cornerY[i] = map.cornerY[order[i]];
        sorted.cornerU[i] = map.cornerU[order[i]];
        sorted.cornerV[i] = map.cornerV[order[i]];
    }
    map = sorted;
    std::printf("[licence] minimap corners reordered %d %d %d %d so the quad closes\n", order[0], order[1], order[2],
                order[3]);
}

void LicenceRunScreen::spawnLocal() {
    Session& session = m_app.session();
    const auto& rows = m_world.scene.start_rows;
    float x = 0.f, y = 0.f, z = 1.f, yaw = 0.f;
    if (!rows.empty()) { x = rows[0].x; y = rows[0].y; z = rows[0].z; yaw = rows[0].heading; }
    const KartRow* kart = session.catalog().kart(session.myKartKey());
    const DriverRow* driver = session.catalog().driver(session.myDriverKey());
    const std::string model = kart && !kart->model.empty() ? kart->model : "Basic_1";
    const std::string asset = driver && !driver->asset.empty() ? driver->asset : "Cosmo";
    CarSetup setup;
    setup.playerId = session.profile().playerId;
    if (kart) { setup.stats = kart->stats; setup.vehicleKind = static_cast<int>(kart->vehicleKind); }
    setup.carFile = kartCarFile(m_app.options().gameDir, model);
    setup.x = x; setup.y = y; setup.z = z; setup.yawDeg = yaw;
    std::string error;
    if (!m_sim.spawnLocal(setup, error)) { m_status = "local spawn failed: " + error; std::printf("[licence] %s\n", m_status.c_str()); return; }
    m_viewHandle = m_view.addCar(m_app.renderer(), m_app.options().gameDir, model, asset);
    m_auto.start(x, y, yaw);
    m_chase = m_sim.pose(m_sim.localIndex());
    m_lastFace = -1;
    m_startSeen = false;
    m_midSeen = false;
    m_itemUses = 0;
    m_boosts = 0;
    m_gauge = 0.f;
    m_slotItem = -1;
    m_slotAge = -1.f;
    m_boosterUsed = false;
}

// 0x4ADC20 gated by stage 0xD only keys 2 and 13
void LicenceRunScreen::updateDriftGauge(float dt, bool drifting) {
    if (!m_gaugeTest || !m_sim.hasLocal()) return;
    const CarPose pose = localPose();
    if (drifting) {
        // rate doubles while boost runs doubles again on booster test key 2
        float rate = kGaugeRate;
        if (pose.boosting) rate *= 2.f;
        if (m_key % 10 == 2) rate *= 2.f;
        m_gauge += rate * dt;
    }
    if (m_gauge < kGaugeFull) return;
    m_gauge = 0.f;
    // FUN 004AECD0 kind 0 puts booster in slot blinks pickup banner
    m_slotItem = 0;
    m_slotAge = 0.f;
    m_app.sound().play("itembox_get_snd");
    std::printf("[licence] drift gauge full the booster lands in the slot\n");
}

// FUN 004AEFA0 case 0 clears item slot after firing
void LicenceRunScreen::useBooster() {
    if (m_slotItem != 0 || !m_sim.hasLocal()) return;
    m_slotItem = -1;
    m_boosterUsed = true;
    KnC::Kart::Client::car_boost_start(m_sim.game(), m_sim.localIndex(), 1, m_sim.nowMs());
    m_app.sound().play("boost_item_snd");
    std::printf("[licence] booster fired\n");
}

// retry FUN 0041C700 fail box OK rebuilds stage from intro board
void LicenceRunScreen::restart() {
    m_view.removeCar(m_viewHandle);
    m_viewHandle = -1;
    std::string error;
    m_sim.init(m_world, m_app.session().profile().playerId, error);
    if (const OwnedPet* worn = m_app.session().catalog().equippedPet()) m_sim.setEquippedPet(worn->petKey);
    m_auto.reset(m_sim.line());
    spawnLocal();
    m_view.camera().reset();
    m_view.camera().licenceMode = true;
    m_phase = Phase::Intro;
    m_phaseTime = 0.f;
    m_runClock = 0.f;
    m_limitMs = licenceLimitMs(m_key);
    m_cueStage = -1;
    m_gauge = 0.f;
    m_slotItem = -1;
    m_slotAge = -1.f;
    m_boosterUsed = false;
    m_app.sound().stopEngine();
}

CarPose LicenceRunScreen::localPose() const {
    if (!m_sim.hasLocal()) return m_chase;
    return m_sim.pose(m_sim.localIndex());
}

void LicenceRunScreen::beginCountdown() {
    if (m_phase != Phase::Intro) return;
    m_phase = Phase::Countdown;
    m_phaseTime = 0.f;
    m_cueStage = -1;
}

// FUN 0041B800 GO of countdown lifts ground flag stamps run
void LicenceRunScreen::release() {
    m_phase = Phase::Running;
    m_phaseTime = 0.f;
    m_runClock = 0.f;
    m_sim.setGreenLight(true);
    m_sim.setSessionRunning(true);
    m_app.sound().play("countdown_go_snd");
    m_status = "the test runs";
}

void LicenceRunScreen::pass() {
    if (m_phase != Phase::Running) return;
    m_phase = Phase::Passed;
    m_phaseTime = 0.f;
    m_sim.setFinished(m_app.session().profile().playerId);
    m_view.setDriverClip(m_viewHandle, KnC::Tools::kDriverSeqWin);
    m_app.sound().stopEngine();
    m_app.sound().play("goal_finish_snd");
    m_status = "passed";
}

void LicenceRunScreen::fail() {
    if (m_phase != Phase::Running) return;
    m_phase = Phase::Failed;
    m_phaseTime = 0.f;
    m_sim.setFinished(m_app.session().profile().playerId);
    m_view.setDriverClip(m_viewHandle, KnC::Tools::kDriverSeqLose);
    m_app.sound().stopEngine();
    m_status = "failed";
}

// FUN 0041F770 START face then CHECK 004 then START again passes driving test
bool LicenceRunScreen::goalReached() {
    if (!m_startSeen || !m_midSeen) return false;
    switch (m_key % 10) {
    case 1: return m_itemUses >= 3;
    // FUN 00420950 wants booster of full gauge fired mini turbo keeps auto run alive
    case 2: return m_boosterUsed || m_boosts >= 1;
    case 3: return m_boosts >= 3;
    default: return true;
    }
}

void LicenceRunScreen::watchFaces() {
    const int face = m_sim.localCheckpointFace();
    if (face == m_lastFace) return;
    m_lastFace = face;
    if (face < 0) return;
    const int mid = std::max(1, m_world.checkpointCount / 2);
    if (face == 0 && !m_startSeen) { m_startSeen = true; return; }
    if (m_startSeen && face == mid) m_midSeen = true;
    if (face == 0 && m_startSeen && m_midSeen) {
        if (goalReached()) pass();
        else { m_startSeen = false; m_midSeen = false; }
    }
}

// FUN 0041F870 phase 7 C2S 0x00A3 with key and two echoes passed row shows board again
void LicenceRunScreen::submit() {
    m_phase = Phase::Submit;
    m_phaseTime = 0.f;
    const LicenceProgressRow* row = m_app.session().licenceState(m_key);
    if (row && row->passed > 0) {
        m_resultLanded = true;
        m_resultFresh = false;
        m_resultPassed = 1;
        m_phase = Phase::Result;
        m_status = "already passed the board shows MSG_TUTORIAL_SUCC";
        return;
    }
    // two echoes from test def plus 2C and plus 34 server recomputes from its table
    const LicenceTestDef* def = m_app.session().catalog().licenceTest(m_key);
    m_app.session().submitLicenceTest(m_key, def ? def->testParam : 0, def ? def->rewardGold : 0);
    m_status = "0x00A3 sent for key " + std::to_string(m_key);
}

// popup OK and exit box OK go back to licence menu with 0x0016 FUN 00483950
void LicenceRunScreen::leaveToMenu() {
    m_app.sound().stopEngine();
    m_app.session().openLicense();
    m_status = "0x0016 sent";
}

// 0x0016 ack lands on menu under this run so run pops off
void LicenceRunScreen::onSession(SessionEvent event) {
    if (event == SessionEvent::LicenseAck) m_app.popScreen();
    if (event == SessionEvent::LobbyAck) m_app.popScreen();
    // tutorial complete recv 0x47C3C0 progress row landed clear board shows
    if (event == SessionEvent::LicenceTestResult) {
        const LicenceTestResult& r = m_app.session().licenceTestResult();
        if (!r.valid || r.key != m_key) return;
        std::printf("[licence] 0x00A3 key %u passed %u\n", r.key, r.passed);
        m_resultLanded = true;
        m_resultFresh = true;
        m_resultPassed = r.passed;
        if (m_phase == Phase::Submit) { m_phase = Phase::Result; m_phaseTime = 0.f; }
    }
}

void LicenceRunScreen::update(float dt) {
    m_time += dt;
    m_frameDt = dt;
    m_phaseTime += dt;
    if (!m_worldLoaded) return;
    if (m_phase == Phase::Intro) {
        if (m_app.captureMode() && !m_capturedIntro && m_phaseTime > 0.8f) {
            m_capturedIntro = true;
            m_app.captureStage("licence_intro");
        }
        if (m_autoDrive && m_phaseTime > 1.5f) beginCountdown();
    }
    if (m_phase == Phase::Countdown) {
        const float left = kCountdownSeconds - m_phaseTime;
        const int digit = m_phaseTime < kCountdownPreroll ? 0 : left > 2.f ? 3 : left > 1.f ? 2 : left > 0.f ? 1 : 0;
        if (digit > 0 && digit != m_cueStage) {
            m_cueStage = digit;
            m_app.sound().play("countdown_ready_snd");
        }
        if (m_phaseTime >= kCountdownSeconds) release();
    }
    const bool driving = m_phase == Phase::Running;
    if (driving) {
        m_runClock += dt;
        if (m_limitCounts && m_phaseTime > kTimerDelay) {
            m_limitMs -= static_cast<double>(dt) * 1000.0;
            if (m_limitMs <= 0.0) { m_limitMs = 0.0; fail(); }
        }
        if (m_app.captureMode() && !m_capturedRun && m_runClock > kRunCaptureSeconds) {
            m_capturedRun = true;
            m_app.captureStage("licence_race");
        }
    }
    if (m_phase == Phase::Passed && m_phaseTime >= kPassSeconds) submit();
    if (m_phase == Phase::Failed && m_phaseTime >= kFailSeconds) { m_phase = Phase::FailBox; m_phaseTime = 0.f; }
    if (m_phase == Phase::Result && m_app.captureMode() && !m_capturedBoard && m_phaseTime > 0.8f) {
        m_capturedBoard = true;
        m_app.captureStage("licence_pass");
    }
    if (m_phase == Phase::Result && m_autoDrive && m_phaseTime > 2.5f) leaveToMenu();

    InputFlags flags;
    bool drift = false;
    bool item = false;
    if (m_sim.hasLocal()) {
        if (driving) {
            if (m_autoDrive) {
                m_auto.drive(localPose(), flags, drift);
                // auto run taps item key every five seconds so item test counts uses
                item = std::fmod(m_runClock, 5.f) < 0.1f;
                // booster tests want mini turbo drift pulse every ten seconds until count reached
                const int wanted = m_key % 10 == 2 ? 1 : m_key % 10 == 3 ? 3 : 0;
                const float pulse = std::fmod(m_runClock, 10.f);
                if (m_boosts < wanted && localPose().speedKmh > 70.f && pulse < 1.6f) {
                    if (pulse < 0.7f) { drift = true; flags.steerLeft = 1; flags.steerRight = 0; }
                    else if (pulse < 0.8f) { drift = false; flags.accel = 0; }
                    else { drift = true; flags.steerLeft = 0; flags.steerRight = 1; flags.accel = 1; }
                }
            } else {
                flags.accel = m_app.raceKeyDown(RaceKey::Up) ? 1 : 0;
                flags.brake = m_app.raceKeyDown(RaceKey::Down) ? 1 : 0;
                flags.steerLeft = m_app.raceKeyDown(RaceKey::Left) ? 1 : 0;
                flags.steerRight = m_app.raceKeyDown(RaceKey::Right) ? 1 : 0;
                drift = m_app.raceKeyDown(RaceKey::Drift);
                item = m_app.raceKeyDown(RaceKey::Item);
            }
        } else if (m_phase >= Phase::Passed) {
            flags.brake = 1;
        }
        const bool accelPressed = flags.accel != 0 && !m_accelWas;
        m_accelWas = flags.accel != 0;
        if (item && !m_itemWas) {
            ++m_itemUses;
            useBooster();
        }
        m_itemWas = item;
        m_sim.setLocalInput(flags, drift, accelPressed);
    }
    if (driving) {
        // gauge reads car drift state matching stage 0xD branch of 0x4ADC20
        const bool sliding = m_sim.hasLocal() && localPose().driftState != 0;
        updateDriftGauge(dt, sliding);
        if (m_slotAge >= 0.f) m_slotAge += dt;
    }
    const int ticks = m_sim.advance(dt);
    if (ticks > 0 && m_sim.hasLocal() && driving) {
        const CarPose now = localPose();
        if (now.boosting && !m_chase.boosting) ++m_boosts;
        watchFaces();
    }
    const float clock = m_app.renderer().animation().seconds();
    if (m_viewHandle >= 0 && m_sim.hasLocal()) m_view.setPose(m_viewHandle, m_sim.pose(m_sim.localIndex()), dt, clock);
    if (m_sim.hasLocal()) m_chase = localPose();
    // camera update 0x43F040 in stage 0xD clamps distance to 8 4 after scale pitch 37 height 3 5
    if (m_chase.camDistance * kLicenceCamScale < kLicenceCamDistance)
        m_chase.camDistance = kLicenceCamDistance / kLicenceCamScale;
    m_chase.camPitchDeg = kLicenceCamPitch;
    m_chase.camHeight = kLicenceCamHeight;
    if (driving || m_phase == Phase::Countdown) {
        const CarPose pose = localPose();
        m_app.sound().engine("accel_my_snd_03", 0.6f + std::min(pose.speedKmh, 200.f) / 200.f * 1.2f, 0.5f);
    } else {
        m_app.sound().stopEngine();
    }
}

bool LicenceRunScreen::drawScene() {
    if (!m_worldLoaded || !m_view.loaded()) return false;
    if (m_phase == Phase::Running && m_app.raceKeyDown(RaceKey::Back)) {
        float eye[3], look[3];
        lookBackCamera(m_chase, eye, look);
        m_view.drawFixed(m_app.renderer(), eye, look, kLookBackFieldRadians, m_frameDt);
        return true;
    }
    m_view.draw(m_app.renderer(), m_chase, m_frameDt);
    return true;
}

void LicenceRunScreen::draw(SpriteBatch& batch) {
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    const float w = m_app.canvasWidth();
    const float h = m_app.canvasHeight();
    AssetStore& assets = m_app.assets();
    auto sprite = [&](const std::string& path, float x, float y) -> const Texture* {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
        return t;
    };
    if (!m_worldLoaded) {
        batch.fill(0.f, 0.f, w, h, rgba(20, 20, 30, 255));
        ctx.font.drawCentered(batch, m_status, w * 0.5f, h * 0.5f - 10.f, 18.f, kWhite);
        ctx.font.drawCentered(batch, "Escape goes back to the licence menu", w * 0.5f, h * 0.5f + 30.f, 14.f, rgba(255, 220, 90, 255));
        return;
    }
    HudState s;
    s.kind = HudKind::Licence;
    const CarPose pose = localPose();
    s.speedKmh = pose.speedKmh;
    s.racers = 1;
    s.clock = m_time;
    s.minimap = &m_world.minimap;
    s.waiting = m_phase == Phase::Intro;
    // booster from full gauge shows in slot window like a race item
    s.heldItem = m_slotItem;
    if (m_slotAge >= 0.f && m_slotItem >= 0) {
        s.pickupAge = static_cast<double>(m_slotAge);
        s.pickupItem = m_slotItem;
    }
    if (m_phase == Phase::Countdown) {
        const float left = kCountdownSeconds - m_phaseTime;
        s.countdownStage = m_phaseTime < kCountdownPreroll ? 0 : left > 2.f ? 3 : left > 1.f ? 2 : 1;
        if (s.countdownStage > 0) s.countdownAge = static_cast<double>(static_cast<float>(s.countdownStage) - left);
    } else if (m_phase == Phase::Running && m_phaseTime < 1.f) {
        s.countdownStage = 4;
        s.countdownAge = m_phaseTime;
    }
    if (m_phase >= Phase::Countdown && m_phase <= Phase::Running) {
        const bool keys = m_phase == Phase::Running && !m_autoDrive;
        s.licenceKeys[0] = keys && m_app.raceKeyDown(RaceKey::Up);
        s.licenceKeys[1] = keys && m_app.raceKeyDown(RaceKey::Right);
        s.licenceKeys[2] = keys && m_app.raceKeyDown(RaceKey::Down);
        s.licenceKeys[3] = keys && m_app.raceKeyDown(RaceKey::Left);
        s.licenceKeys[4] = keys && m_app.raceKeyDown(RaceKey::Drift);
        s.licenceKeys[5] = keys && m_app.raceKeyDown(RaceKey::Item);
        if (m_phase == Phase::Running && m_phaseTime > kTimerDelay) s.licenceTimerMs = m_limitMs;
        if (m_phase == Phase::Running) {
            // LicenseGame strip picks beginnertext 01 02 or 03 by test key
            char strip[64];
            std::snprintf(strip, sizeof(strip), "LicenseGame/licence_beginnertext_%02u.png", std::min<uint32_t>(m_key % 10 + 1, 3));
            s.licenceLine = strip;
        }
    }
    if (m_viewHandle >= 0 && m_sim.hasLocal()) {
        HudCar car;
        car.x = pose.x;
        car.y = pose.y;
        car.local = true;
        const DriverRow* driver = m_app.session().catalog().driver(m_app.session().myDriverKey());
        car.driverAsset = driver && !driver->asset.empty() ? driver->asset : "Cosmo";
        s.cars.push_back(car);
    }
    s.status = m_status;
    if (m_phase >= Phase::Countdown) m_hud.draw(ctx, assets, s, w, h);
    if (m_phase >= Phase::Countdown && m_phase <= Phase::Running) drawGaugeFill(ctx, w, h);
    if (m_phase == Phase::Intro) {
        // FUN 0041F870 phase 1 mummy board text MSG LICENSE key and two rewards
        if (!sprite("LicenseGame/dialogue_char_mummy_licence.png", kBoardX, kBoardY))
            batch.fill(kBoardX + 250.f, kBoardY + 50.f, 620.f, 260.f, rgba(255, 200, 40, 240));
        char key[32];
        std::snprintf(key, sizeof(key), "MSG_LICENSE_%02u", m_key);
        const std::vector<std::string> lines = wrapText(ctx.font, m_app.tr(key), 13.f, kBoardTextW);
        float y = kBoardTextY;
        for (const std::string& line : lines) {
            ctx.font.draw(batch, line, kBoardTextX, y, 13.f, kInkDark);
            y += 16.f;
            if (y > kBoardTextY + 150.f) break;
        }
        // 0x00C5 def of key gives two board rewards gold at 242 exp at 264
        const LicenceTestDef* def = m_app.session().catalog().licenceTest(m_key);
        ctx.bold.draw(batch, std::to_string(def ? def->rewardGold : 0u), kBoardGoldX, kBoardGoldY, 15.f, kInkDark);
        ctx.bold.draw(batch, std::to_string(def ? def->rewardExp : 0u), kBoardGoldX, kBoardExpY, 15.f, kInkDark);
    }
    if (m_phase == Phase::Result) {
        // licence clear popup exp and gold drawn right aligned
        if (!sprite("Popup/LicenseStepClear/licence_finish.png", kClearX, kClearY))
            batch.fill(kClearX + 90.f, kClearY + 180.f, 510.f, 250.f, rgba(255, 200, 40, 240));
        // license result reward draw 0x461F40 prints def rewards of key
        const LicenceTestDef* def = m_app.session().catalog().licenceTest(m_key);
        const std::string exp = std::to_string(def ? def->rewardExp : 0u);
        const std::string gold = std::to_string(def ? def->rewardGold : 0u);
        drawAligned(ctx, ctx.bold, exp, kClearExpX, kClearNumberY, 17.f, kInkDark, Align::Right);
        drawAligned(ctx, ctx.bold, gold, kClearGoldX, kClearNumberY, 17.f, kInkDark, Align::Right);
        // 0x4621B8 def reward icon at 476 370 pendant type 7 icon base 00
        if (def && def->rewardItemKey != 0) {
            const std::string icon = missionRewardIcon(m_app.session(), def->rewardItemType, def->rewardItemKey);
            if (!icon.empty()) sprite(icon, 476.f, 370.f);
        }
        // FUN 00462400 keeps MSG TUTORIAL lines but license result reward draw never draws them
    }
    if (m_phase == Phase::Submit) ctx.font.drawCentered(batch, "waiting for the 0x00A3 answer", w * 0.5f, h * 0.5f, 18.f, kWhite);
    if (m_phase == Phase::FailBox) m_hud.drawInfoBox(ctx, assets, wrapText(ctx.bold, m_app.tr("MSG_TUTORIAL_FAIL"), 15.f, 320.f), true);
    if (m_phase == Phase::ExitBox) m_hud.drawInfoBox(ctx, assets, wrapText(ctx.bold, m_app.tr("MSG_CONFIRM_EXIT"), 15.f, 320.f), true);
}

// we paint the same sweep as the needle
void LicenceRunScreen::drawGaugeFill(DrawContext& ctx, float canvasW, float canvasH) const {
    if (!m_gaugeTest) return;
    const float part = m_gauge / kGaugeFull;
    if (part <= 0.f) return;
    const Texture* white = m_app.assets().white();
    if (!white || !white->valid()) return;
    const float px = canvasW - kGaugeRight + kGaugePivotX;
    const float py = canvasH - kGaugeBottom + kGaugePivotY;
    const int steps = static_cast<int>(static_cast<float>(kGaugeSteps) * std::min(part, 1.f) + 0.5f);
    for (int i = 0; i < steps; ++i) {
        const float f0 = static_cast<float>(i) / static_cast<float>(kGaugeSteps);
        const float f1 = static_cast<float>(i + 1) / static_cast<float>(kGaugeSteps);
        const float a0 = kGaugeStart + f0 * kGaugeSweep;
        const float a1 = kGaugeStart + f1 * kGaugeSweep;
        const float d0[2] = {std::sin(a0), -std::cos(a0)};
        const float d1[2] = {std::sin(a1), -std::cos(a1)};
        const float xy[8] = {px + d0[0] * kGaugeInner, py + d0[1] * kGaugeInner,
                             px + d0[0] * kGaugeOuter, py + d0[1] * kGaugeOuter,
                             px + d1[0] * kGaugeOuter, py + d1[1] * kGaugeOuter,
                             px + d1[0] * kGaugeInner, py + d1[1] * kGaugeInner};
        const float uv[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        // ramp of board redg02 dds yellow at start red at full gauge
        const uint8_t green = static_cast<uint8_t>(230.f - 190.f * f1);
        ctx.batch.drawQuad(white->handle, xy, uv, rgba(255, green, 50, 200));
    }
}

void LicenceRunScreen::onKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    const bool enter = key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER;
    if (!m_worldLoaded) { if (key == GLFW_KEY_ESCAPE) leaveToMenu(); return; }
    switch (m_phase) {
    case Phase::Intro:
        if (enter) beginCountdown();
        else if (key == GLFW_KEY_ESCAPE) { m_phase = Phase::ExitBox; m_phaseTime = 0.f; }
        break;
    case Phase::Countdown:
    case Phase::Running:
        if (key == GLFW_KEY_ESCAPE) { m_phase = Phase::ExitBox; m_phaseTime = 0.f; }
        break;
    case Phase::Result:
        if (enter || key == GLFW_KEY_ESCAPE || key == GLFW_KEY_SPACE) leaveToMenu();
        break;
    case Phase::FailBox:
        if (enter) restart();
        else if (key == GLFW_KEY_ESCAPE) leaveToMenu();
        break;
    case Phase::ExitBox:
        if (enter) leaveToMenu();
        else if (key == GLFW_KEY_ESCAPE) { m_phase = m_runClock > 0.f ? Phase::Running : Phase::Intro; }
        break;
    default:
        break;
    }
}

void LicenceRunScreen::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    if (m_phase == Phase::Intro && kSkipZone.contains(x, y)) { m_app.click(); beginCountdown(); return; }
    if (m_phase == Phase::Result && kClearEnter.contains(x, y)) { m_app.click(); leaveToMenu(); return; }
    if (m_phase == Phase::FailBox) {
        if (RaceHud::pauseRow(0).contains(x, y)) { m_app.click(); restart(); }
        else if (RaceHud::pauseRow(1).contains(x, y)) { m_app.click(); leaveToMenu(); }
        return;
    }
    if (m_phase == Phase::ExitBox) {
        if (RaceHud::pauseRow(0).contains(x, y)) { m_app.click(); leaveToMenu(); }
        else if (RaceHud::pauseRow(1).contains(x, y)) { m_app.click(); m_phase = m_runClock > 0.f ? Phase::Running : Phase::Intro; }
    }
}

}
