#include "GhostModeScreen.h"

#include "race/LookBack.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "engine/render/scene_renderer.h"
#include "net/Session.h"
#include "net/Utf.h"
#include "screens/LicenceScreen.h"
#include "screens/ShopCommon.h"
#include "ui/MenuFrame.h"

#include <GLFW/glfw3.h>
#include <bx/math.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace KnC::Client {

using KnC::Kart::Client::GhostSample;
using KnC::Kart::Client::InputFlags;

namespace {

// FUN 00439630 back 37 88 top 25 50 thumbs from 130 130 every 153 up to 742
constexpr float kBackX = 37.f;
constexpr float kBackY = 88.f;
constexpr float kTopX = 25.f;
constexpr float kTopY = 50.f;
constexpr float kThumbX = 130.f;
constexpr float kThumbY = 130.f;
constexpr float kThumbStep = 153.f;
constexpr float kThumbLast = 742.f;
constexpr float kThumbW = 135.f;
constexpr float kThumbH = 75.f;
// theme name centred 509 223 track picture at 192 301 track name centred 356 500
constexpr float kThemeNameX = 509.f;
constexpr float kThemeNameY = 223.f;
constexpr float kPictureX = 192.f;
constexpr float kPictureY = 301.f;
constexpr float kTrackNameX = 356.f;
constexpr float kTrackNameY = 500.f;
// info text 154 558 wide 410 record rows on 334 every 51 at 838 best on 525
constexpr float kInfoX = 154.f;
constexpr float kInfoY = 558.f;
constexpr float kInfoW = 410.f;
constexpr float kRecordX = 838.f;
constexpr float kRecordY = 334.f;
constexpr float kRecordStep = 51.f;
constexpr float kMyBestY = 525.f;
// FUN 00439350 five stage buttons
constexpr float kTopLeftX = 68.f, kTopLeftY = 120.f;
constexpr float kTopRightX = 910.f, kTopRightY = 120.f;
constexpr float kBottomLeftX = 143.f, kBottomLeftY = 289.f;
constexpr float kBottomRightX = 540.f, kBottomRightY = 289.f;
constexpr float kStartX = 808.f, kStartY = 619.f;
// FUN 00425350 wait before countdown then sub 4B2690 mode 2 pre roll and three digits
constexpr float kWaitSeconds = 1.5f;
constexpr float kCountdownPreroll = 0.24f;
constexpr float kCountdownSeconds = 3.24f;
// 2005 waits 1000 ms after finish 2015 holds 3000 ms then 2020 4000 ms before popup
constexpr float kUploadDelay = 1.f;
constexpr float kResultDelay = 7.f;
// session chunks upload at 136 samples clamps own count to 2399
constexpr int kUploadCap = 2399;
// FUN 00458930 result popup at 336 181 ok at plus 109 361
constexpr float kPopupX = 336.f;
constexpr float kPopupY = 181.f;
constexpr Rect kPopupOk = {kPopupX + 109.f, kPopupY + 361.f, 96.f, 29.f};
constexpr float kRaceCaptureSeconds = 4.f;

bool autoGhost() {
    const char* env = std::getenv("KNC_AUTO_GHOST");
    return env && env[0] != '\0' && env[0] != '0';
}

// KNC AUTO GHOST names track id above one any other value picks first track
uint32_t autoGhostTrack() {
    const char* env = std::getenv("KNC_AUTO_GHOST");
    if (!env) return 0;
    const int v = std::atoi(env);
    return v > 1 ? static_cast<uint32_t>(v) : 0u;
}

std::string clockText(int32_t ms) {
    if (ms <= 0) return "--:--:---";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%03d", std::min(ms / 60000, 60), (ms / 1000) % 60, ms % 1000);
    return buf;
}

// GhostFrame is wire layout GhostSample is physics port layout
GhostSample toSample(const GhostFrame& f) {
    GhostSample g;
    std::memcpy(g.pos, f.pos, sizeof(g.pos));
    g.yawByte = f.yawByte;
    g.flags = static_cast<uint16_t>(f.flags);
    g.nibbles = static_cast<uint8_t>(f.nibbles);
    g.inputMask = f.inputMask;
    return g;
}

GhostFrame toFrame(const GhostSample& g) {
    GhostFrame f;
    std::memcpy(f.pos, g.pos, sizeof(f.pos));
    f.yawByte = g.yawByte;
    f.flags = g.flags;
    f.nibbles = g.nibbles;
    f.inputMask = g.inputMask;
    return f;
}

std::string themeMusic(const std::string& themeFolder) {
    std::string t;
    for (char c : themeFolder) t.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (t == "cookie") return "thema_cookie_BGM_01";
    if (t == "desert") return "thema_desert_BGM_01";
    if (t == "forest") return "thema_forest_BGM_01";
    if (t == "swamp") return "thema_swamp_BGM";
    if (t == "toy") return "thema_toy_BGM_01";
    if (t == "snow") return "thema_ice_BGM";
    if (t == "devil") return "thema_lava_BGM";
    return "thema_city_BGM";
}

void fullViewport(App& app) {
    KnC::Render::ViewportRect full;
    full.x = 0;
    full.y = 0;
    full.width = app.width();
    full.height = app.height();
    app.renderer().set_viewport(full);
}

}

bool GhostModeScreen::open(App& app) {
    if (!LicenceScreen::stageGateOpen(app)) {
        std::printf("[ghost] the top bar button answers nothing under level 10 with no licence grade\n");
        return false;
    }
    app.pushScreen(std::make_unique<GhostModeScreen>(app));
    return true;
}

void GhostModeScreen::enter() {
    AssetStore& assets = m_app.assets();
    addMenuFrame(*this, assets, FrameMode::Full, "ghost");
    auto button = [&](const char* id, const char* action, const char* art, float x, float y) {
        auto b = std::make_unique<ButtonWidget>();
        b->type = ElementType::Button;
        b->id = id;
        b->action = action;
        b->normal = assets.texture(std::string(art) + "00.png");
        b->hover = assets.texture(std::string(art) + "01.png");
        b->pressed = assets.texture(std::string(art) + "02.png");
        const Texture* size = b->normal ? b->normal : b->hover;
        b->rect = {x, y, size ? static_cast<float>(size->width) : 30.f, size ? static_cast<float>(size->height) : 30.f};
        b->zIndex = 30;
        add(std::move(b));
    };
    button("btn_theme_left", "theme_left", "GhostMode/Common_Top_Left_", kTopLeftX, kTopLeftY);
    button("btn_theme_right", "theme_right", "GhostMode/Common_Top_Right_", kTopRightX, kTopRightY);
    button("btn_track_left", "track_left", "GhostMode/Common_Bottom_Left_", kBottomLeftX, kBottomLeftY);
    button("btn_track_right", "track_right", "GhostMode/Common_Bottom_Right_", kBottomRightX, kBottomRightY);
    button("btn_start", "start", "GhostMode/Common_Start_", kStartX, kStartY);
    rebuildThemes();
    m_time = 0.f;
    m_boardReady = false;
    m_startSent = false;
    m_captured = false;
    m_autoStarted = false;
    m_status.clear();
    // stock Ghost button sends empty 0x011D board comes before ack
    m_app.session().openGhostMenu();
    m_app.playMusic("multiplay_lobby_bgm");
    const uint32_t wanted = autoGhostTrack();
    if (wanted != 0) {
        for (size_t t = 0; t < m_tracks.size(); ++t)
            for (size_t i = 0; i < m_tracks[t].size(); ++i)
                if (m_tracks[t][i] == wanted) { m_theme = static_cast<int>(t); m_track = static_cast<int>(i); }
        while (m_theme >= m_first + 5) ++m_first;
    }
}

void GhostModeScreen::leave() {}

// themes with thumb in 0x00C4 order then visible 0x00C3 tracks of each
void GhostModeScreen::rebuildThemes() {
    const Catalog& cat = m_app.session().catalog();
    AssetStore& assets = m_app.assets();
    m_themes.clear();
    m_tracks.clear();
    for (const ThemeRow& theme : cat.themes()) {
        const Texture* thumb = assets.texture("Popup/SelectTrack/thema_" + theme.folder + ".png");
        if (!thumb || !thumb->valid()) continue;
        std::vector<uint32_t> ids;
        for (const TrackRow& track : cat.tracks())
            if (track.visible && track.themeId == theme.themeId) ids.push_back(track.trackId);
        if (ids.empty()) continue;
        m_themes.push_back(theme.themeId);
        m_tracks.push_back(std::move(ids));
    }
    if (m_theme >= static_cast<int>(m_themes.size())) m_theme = 0;
    m_track = 0;
}

void GhostModeScreen::update(float dt) {
    m_time += dt;
    if (m_app.captureMode() && !m_captured && m_boardReady && m_time > 1.0f) {
        m_captured = true;
        m_app.captureStage("ghostmode");
        if (!autoGhost()) m_app.finishRun();
    }
    if (autoGhost() && m_boardReady && !m_autoStarted && m_time > 1.6f) {
        m_autoStarted = true;
        start();
    }
    // auto run ends once race above returns to menu
    if (autoGhost() && m_autoStarted && m_raceDone && m_app.captureMode()) m_app.finishRun();
}

// FUN 00439160 picked track sent as 0x00AA if profile grade meets required licence
void GhostModeScreen::start() {
    if (m_startSent) return;
    if (m_theme < 0 || m_theme >= static_cast<int>(m_tracks.size())) return;
    const std::vector<uint32_t>& ids = m_tracks[static_cast<size_t>(m_theme)];
    if (m_track < 0 || m_track >= static_cast<int>(ids.size())) return;
    const TrackRow* row = m_app.session().catalog().track(ids[static_cast<size_t>(m_track)]);
    if (!row) return;
    if (static_cast<int>(row->requiredLicense) > static_cast<int>(m_app.session().profile().band)) {
        m_status = "the track wants licence grade " + std::to_string(row->requiredLicense);
        return;
    }
    m_startSent = true;
    m_app.session().enterGhost(row->trackId);
    m_status = "0x00AA sent for track " + std::to_string(row->trackId);
}

void GhostModeScreen::exitToLobby() {
    m_app.session().openLobby();
    m_status = "0x0012 sent";
}

void GhostModeScreen::stepTheme(int delta) {
    const int themes = static_cast<int>(m_themes.size());
    // FUN 00439110 window slides right while seven thumbs remain past first
    if (delta > 0 && m_first + 7 < themes) ++m_first;
    if (delta < 0 && m_first > 0) --m_first;
}

void GhostModeScreen::stepTrack(int delta) {
    if (m_theme < 0 || m_theme >= static_cast<int>(m_tracks.size())) return;
    const int n = static_cast<int>(m_tracks[static_cast<size_t>(m_theme)].size());
    if (delta > 0 && m_track + 1 < n) ++m_track;
    if (delta < 0 && m_track > 0) --m_track;
}

void GhostModeScreen::onKey(int key, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) { exitToLobby(); return; }
        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) { start(); return; }
        if (key == GLFW_KEY_LEFT) { stepTrack(-1); return; }
        if (key == GLFW_KEY_RIGHT) { stepTrack(1); return; }
    }
    WidgetScreen::onKey(key, action, mods);
}

// menu frame and ghost art sit under buttons overlay strip and box sit over them
void GhostModeScreen::draw(SpriteBatch& batch) {
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    AssetStore& assets = m_app.assets();
    auto sprite = [&](const char* path, float x, float y) {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
    };
    for (Widget* w : m_order) if (w->visible && w->zIndex < 0) w->draw(ctx);
    sprite("GhostMode/Ghost_Back.png", kBackX, kBackY);
    sprite("GhostMode/Ghost_Top.png", kTopX, kTopY);
    for (Widget* w : m_order) if (w->visible && w->zIndex >= 0) w->draw(ctx);
    drawOverlay(ctx);
}

// FUN 00439040 thumb press picks theme and resets track to first
void GhostModeScreen::onMouseButton(int button, int action, float x, float y) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && y > kThumbY && y < kThumbY + kThumbH) {
        float tx = kThumbX;
        for (int t = m_first; t < static_cast<int>(m_themes.size()); ++t) {
            if (x > tx && x < tx + kThumbW) {
                if (m_theme != t) { m_theme = t; m_track = 0; m_app.click(); }
                return;
            }
            tx += kThumbStep;
            if (tx > kThumbLast) break;
        }
    }
    WidgetScreen::onMouseButton(button, action, x, y);
}

void GhostModeScreen::onAction(const std::string& action, Widget& source) {
    if (action == "theme_left") { stepTheme(-1); return; }
    if (action == "theme_right") { stepTheme(1); return; }
    if (action == "track_left") { stepTrack(-1); return; }
    if (action == "track_right") { stepTrack(1); return; }
    if (action == "start") { start(); return; }
    if (action == "lobby" || action == "channel") { exitToLobby(); return; }
    if (action == "quit") { m_app.quit(); return; }
    if (action == "ghost") return;
    if (action == "missions") { m_app.session().openMissionMenu(); m_status = "missions 0x008F sent"; return; }
    if (action == "garage") { m_app.session().openGarage(); return; }
    if (action == "shop") { m_app.session().openShop(); return; }
    if (frameAction(m_app, action)) return;
    m_status = source.id + " is not part of this phase";
}

// lobby acks land while this screen is pushed so it pops itself
void GhostModeScreen::onSession(SessionEvent event) {
    if (event == SessionEvent::LobbyAck || event == SessionEvent::MissionMenu || event == SessionEvent::GarageAck ||
        event == SessionEvent::ShopAck || event == SessionEvent::LicenseAck)
        m_app.popScreen();
    if (event == SessionEvent::Disconnected) m_status = "disconnected";
    if (event == SessionEvent::GhostMenuAck) {
        if (m_boardReady) m_raceDone = true;
        m_boardReady = true;
        m_status = "board ready " + std::to_string(m_app.session().ghostBoard().size()) + " tracks";
    }
    // sub 47CBA0 landed session carries ghost frames race starts on them
    if (event == SessionEvent::GhostSession && m_startSent) {
        const GhostSession& g = m_app.session().ghostSession();
        GhostSessionInfo info;
        info.trackId = g.trackId;
        info.name = u16ToUtf8(g.name);
        info.carKind = g.carKind;
        info.recordTimeMs = g.recordTimeMs;
        info.driverKey = g.driverKey;
        info.kartKey = g.kartKey;
        info.samples.reserve(g.frames.size());
        for (const GhostFrame& f : g.frames) info.samples.push_back(toSample(f));
        m_startSent = false;
        m_app.pushScreen(std::make_unique<GhostRaceScreen>(m_app, std::move(info)));
    }
}

void GhostModeScreen::drawOverlay(DrawContext& ctx) {
    AssetStore& assets = m_app.assets();
    SpriteBatch& batch = ctx.batch;
    auto sprite = [&](const std::string& path, float x, float y) -> const Texture* {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
        return t;
    };
    const Catalog& cat = m_app.session().catalog();
    float tx = kThumbX;
    for (int t = m_first; t < static_cast<int>(m_themes.size()); ++t) {
        const ThemeRow* theme = cat.theme(m_themes[static_cast<size_t>(t)]);
        if (theme) {
            const Texture* thumb = assets.texture("Popup/SelectTrack/thema_" + theme->folder + ".png");
            if (thumb && thumb->valid()) batch.draw(thumb->handle, tx, kThumbY, static_cast<float>(thumb->width), static_cast<float>(thumb->height));
        }
        if (t == m_theme) sprite("GhostMode/theme_select.png", tx, kThumbY);
        tx += kThumbStep;
        if (tx > kThumbLast) break;
    }
    if (m_theme < 0 || m_theme >= static_cast<int>(m_themes.size())) return;
    const ThemeRow* theme = cat.theme(m_themes[static_cast<size_t>(m_theme)]);
    if (theme) drawAligned(ctx, ctx.bold, m_app.tr(theme->nameKey), kThemeNameX, kThemeNameY, 15.f, kInkDark, Align::Centre);
    const std::vector<uint32_t>& ids = m_tracks[static_cast<size_t>(m_theme)];
    if (m_track < 0 || m_track >= static_cast<int>(ids.size())) return;
    const TrackRow* row = cat.track(ids[static_cast<size_t>(m_track)]);
    if (!row) return;
    const Texture* picture = assets.texture("Popup/SelectTrack/track_" + row->folder + ".png");
    if (picture && picture->valid()) batch.draw(picture->handle, kPictureX, kPictureY, static_cast<float>(picture->width), static_cast<float>(picture->height));
    else batch.fill(kPictureX, kPictureY, 328.f, 186.f, rgba(90, 100, 120, 255));
    drawAligned(ctx, ctx.bold, m_app.tr(row->nameKey), kTrackNameX, kTrackNameY, 15.f, kInkDark, Align::Centre);
    const std::vector<std::string> lines = wrapText(ctx.font, m_app.tr(row->nameKey + "_INFO"), 13.f, kInfoW);
    float ty = kInfoY;
    for (const std::string& line : lines) {
        if (ty > kInfoY + 110.f) break;
        ctx.font.draw(batch, line, kInfoX, ty, 13.f, kInkDark);
        ty += 16.f;
    }
    if (static_cast<int>(row->requiredLicense) > static_cast<int>(m_app.session().profile().band)) sprite("GhostMode/locked.png", kPictureX, kPictureY);
    // record box three rows then own best dashes shown when row empty
    const GhostTrackBoard* tb = m_app.session().ghostBoardOf(row->trackId);
    for (int i = 0; i < 3; ++i) {
        const float y = kRecordY + kRecordStep * static_cast<float>(i);
        const GhostRecordRow* r = tb && i < static_cast<int>(tb->entries.size()) ? &tb->entries[static_cast<size_t>(i)] : nullptr;
        if (!r || r->timeMs <= 0) {
            drawAligned(ctx, ctx.bold, "--:--:---", kRecordX, y + 10.f, 13.f, kInkDark, Align::Centre);
            continue;
        }
        drawAligned(ctx, ctx.bold, u16ToUtf8(r->name), kRecordX, y, 13.f, kInkDark, Align::Centre);
        drawAligned(ctx, ctx.bold, clockText(r->timeMs), kRecordX, y + 20.f, 13.f, kInkDark, Align::Centre);
    }
    drawAligned(ctx, ctx.bold, clockText(tb ? tb->best.timeMs : 0), kRecordX, kMyBestY, 13.f, kInkDark, Align::Centre);
    if (!m_status.empty() && m_app.options().statusLine) ctx.font.draw(batch, m_status, 60.f, 706.f, 12.f, kInkGrey);
}

GhostRaceScreen::GhostRaceScreen(App& app, GhostSessionInfo info) : m_app(app), m_info(std::move(info)) {}

GhostRaceScreen::~GhostRaceScreen() = default;

void GhostRaceScreen::enter() {
    fullViewport(m_app);
    m_autoDrive = autoGhost();
    m_status = "loading track " + std::to_string(m_info.trackId);
    if (!loadWorld()) return;
    spawnCars();
    m_stage = Stage::Wait;
    m_stageTime = 0.f;
}

void GhostRaceScreen::leave() {
    m_app.sound().stopEngine();
}

bool GhostRaceScreen::loadWorld() {
    Session& session = m_app.session();
    std::string error;
    TrackFiles files;
    if (!resolveTrackFiles(session.catalog(), static_cast<int>(m_info.trackId), m_app.options().gameDir, files, error)) {
        m_status = "track " + std::to_string(m_info.trackId) + ": " + error;
        std::printf("[ghost] %s\n", m_status.c_str());
        return false;
    }
    if (!loadRaceWorld(files, m_world, error)) {
        m_status = "world load failed: " + error;
        std::printf("[ghost] %s\n", m_status.c_str());
        return false;
    }
    if (!m_sim.init(m_world, session.profile().playerId, error)) { m_status = "sim init failed: " + error; return false; }
    m_view.load(m_app.renderer(), m_world);
    m_auto.reset(m_sim.line());
    m_worldLoaded = true;
    m_app.playMusic(themeMusic(files.themeFolder));
    std::printf("[ghost] %s/%s loaded laps %u line %zu points\n", files.themeFolder.c_str(), files.trackFolder.c_str(), files.laps,
                m_sim.line().size());
    return true;
}

// local car spawns on first start row ghost car from first sample blob of 0x00AA
void GhostRaceScreen::spawnCars() {
    Session& session = m_app.session();
    const Catalog& cat = session.catalog();
    const auto& rows = m_world.scene.start_rows;
    float x = 0.f, y = 0.f, z = 1.f, yaw = 0.f;
    if (!rows.empty()) { x = rows[0].x; y = rows[0].y; z = rows[0].z; yaw = rows[0].heading; }
    const KartRow* kart = cat.kart(session.myKartKey());
    const DriverRow* driver = cat.driver(session.myDriverKey());
    const std::string model = kart && !kart->model.empty() ? kart->model : "Basic_1";
    const std::string asset = driver && !driver->asset.empty() ? driver->asset : "Cosmo";
    CarSetup setup;
    setup.playerId = session.profile().playerId;
    if (kart) { setup.stats = kart->stats; setup.vehicleKind = static_cast<int>(kart->vehicleKind); }
    setup.carFile = kartCarFile(m_app.options().gameDir, model);
    setup.x = x; setup.y = y; setup.z = z; setup.yawDeg = yaw;
    std::string error;
    if (!m_sim.spawnLocal(setup, error)) { m_status = "local spawn failed: " + error; std::printf("[ghost] %s\n", m_status.c_str()); return; }
    m_viewHandle = m_view.addCar(m_app.renderer(), m_app.options().gameDir, model, asset);
    m_auto.start(x, y, yaw);
    m_chase = m_sim.pose(m_sim.localIndex());
    m_lastFace = -1;
    if (m_info.samples.size() > 1) {
        const KartRow* gk = cat.kart(m_info.kartKey);
        const DriverRow* gd = cat.driver(m_info.driverKey);
        const std::string gmodel = gk && !gk->model.empty() ? gk->model : model;
        const std::string gasset = gd && !gd->asset.empty() ? gd->asset : asset;
        m_ghostCar = m_sim.spawnGhost(session.profile().playerId + 1000000u, m_info.samples);
        if (m_ghostCar >= 0) m_ghostView = m_view.addCar(m_app.renderer(), m_app.options().gameDir, gmodel, gasset);
        std::printf("[ghost] ghost car %d view %d on %zu samples\n", m_ghostCar, m_ghostView, m_info.samples.size());
    } else {
        std::printf("[ghost] no ghost frames the run is a plain time attack\n");
    }
}

// FUN 00425210 GO sends 0x00B1 starts lap clock recorder and ghost replay
void GhostRaceScreen::release() {
    m_stage = Stage::Racing;
    m_stageTime = 0.f;
    m_runClock = 0.0;
    m_lapStartClock = 0.0;
    m_app.session().ghostStageBegin();
    m_sim.setGreenLight(true);
    m_sim.setSessionRunning(true);
    m_sim.setGhostRecording(true);
    if (m_ghostCar >= 0) m_sim.setGhostReplay(m_ghostCar, true);
    m_app.sound().play("countdown_go_snd");
    m_status = "GO 0x00B1 sent";
}

// state 2000 final line sends 0x00B2 finish camera win clip then upload one second later
void GhostRaceScreen::finish() {
    m_stage = Stage::Finished;
    m_stageTime = 0.f;
    m_finishMs = static_cast<int32_t>(m_runClock * 1000.0);
    m_app.session().ghostFinalLap();
    m_sim.setGhostRecording(false);
    m_view.setDriverClip(m_viewHandle, KnC::Tools::kDriverSeqWin);
    m_app.sound().stopEngine();
    m_app.sound().play("goal_finish_snd");
    m_status = "finished in " + clockText(m_finishMs);
}

// states 2010 to 2012 count chunks of 136 then submit track time and kind
void GhostRaceScreen::upload() {
    m_stage = Stage::Upload;
    m_stageTime = 0.f;
    const std::vector<GhostSample> samples = m_sim.localGhostSamples();
    std::vector<GhostFrame> frames;
    frames.reserve(samples.size());
    for (const GhostSample& g : samples) frames.push_back(toFrame(g));
    m_app.session().uploadGhost(frames, m_info.trackId, static_cast<uint32_t>(m_finishMs), m_info.carKind);
    m_stage = Stage::Waiting;
    m_status = "uploaded " + std::to_string(std::min(frames.size(), static_cast<size_t>(kUploadCap))) + " samples 0x00B0 sent";
    std::printf("[ghost] %s\n", m_status.c_str());
}

// sub 47CC20 result lands popup opens seven seconds later 0x011D ack pops run
void GhostRaceScreen::onSession(SessionEvent event) {
    if (event == SessionEvent::GhostMenuAck) { m_app.popScreen(); return; }
    if (event != SessionEvent::GhostResult) return;
    m_result = m_app.session().ghostResult();
    if (m_stage == Stage::Waiting) { m_stage = Stage::Result; m_stageTime = 0.f; }
}

std::string GhostRaceScreen::resultKey() const {
    const int32_t run = m_finishMs;
    const bool beatMine = run <= m_result.mine.timeMs;
    for (size_t i = 0; i < m_result.top.size(); ++i) {
        if (run <= m_result.top[i].timeMs) return beatMine ? "MSG_GHOST_MODE_SUCC_" + std::to_string(i + 1) : "MSG_GHOST_MODE_SUCC_4";
    }
    return beatMine ? "MSG_GHOST_MODE_SUCC" : "MSG_GHOST_MODE_SUCC_4";
}

// popup OK or exit box send 0x011D menu underneath pops this run on ack
void GhostRaceScreen::leaveToMenu() {
    if (m_leaveSent) return;
    m_leaveSent = true;
    m_app.sound().stopEngine();
    m_app.session().openGhostMenu();
    m_status = "0x011D sent";
}

CarPose GhostRaceScreen::localPose() const {
    if (!m_sim.hasLocal()) return m_chase;
    return m_sim.pose(m_sim.localIndex());
}

// sub 4A3D00 CarMissionRallyUpdate counts a lap when face passes start
void GhostRaceScreen::watchCheckpoints() {
    const int count = m_world.checkpointCount;
    if (count <= 0) return;
    const int face = m_sim.localCheckpointFace();
    if (face < 0 || face == m_lastFace) { if (face >= 0) m_lastFace = face; return; }
    m_lastFace = face;
    const int expected = (m_checkpoint + 1) % count;
    if (face != expected) return;
    m_checkpoint = face;
    if (face != 0) return;
    ++m_laps;
    const double lapTime = m_runClock - m_lapStartClock;
    m_lapStartClock = m_runClock;
    if (m_bestLap < 0.0 || lapTime < m_bestLap) m_bestLap = lapTime;
    const int total = static_cast<int>(m_world.files.laps);
    std::printf("[ghost] lap %d of %d closed at %.3f s\n", m_laps, total, m_runClock);
    if (m_laps >= total) { finish(); return; }
    const bool finalLap = m_laps + 1 == total;
    m_app.sound().play(finalLap ? "lap_check_finallap_snd" : "lap_check_snd");
    m_lapFlash = finalLap ? 2 : 3;
    m_lapFlashAt = static_cast<double>(m_time);
}

void GhostRaceScreen::update(float dt) {
    m_time += dt;
    m_frameDt = dt;
    m_stageTime += dt;
    if (!m_worldLoaded) return;
    if (m_stage == Stage::Wait && m_stageTime >= kWaitSeconds) { m_stage = Stage::Countdown; m_stageTime = 0.f; m_cueStage = -1; }
    if (m_stage == Stage::Countdown) {
        const float left = kCountdownSeconds - m_stageTime;
        const int digit = m_stageTime < kCountdownPreroll ? 0 : left > 2.f ? 3 : left > 1.f ? 2 : left > 0.f ? 1 : 0;
        if (digit > 0 && digit != m_cueStage) { m_cueStage = digit; m_app.sound().play("countdown_ready_snd"); }
        if (m_stageTime >= kCountdownSeconds) release();
    }
    const bool driving = m_stage == Stage::Racing;
    if (driving) {
        m_runClock += dt;
        if (m_app.captureMode() && !m_capturedRace && m_stageTime >= kRaceCaptureSeconds) {
            m_capturedRace = true;
            m_app.captureStage("ghostrace");
        }
    }
    if (m_stage == Stage::Finished && m_stageTime >= kUploadDelay) upload();
    if (m_stage == Stage::Result) {
        if (m_app.captureMode() && !m_capturedResult && m_stageTime >= kResultDelay + 0.8f) {
            m_capturedResult = true;
            m_app.captureStage("ghostresult");
        }
        if (m_autoDrive && m_stageTime >= kResultDelay + 2.5f && !m_leaveSent) leaveToMenu();
    }

    InputFlags flags;
    bool drift = false;
    if (m_sim.hasLocal()) {
        if (driving && !m_exitBox) {
            if (m_autoDrive) m_auto.drive(localPose(), flags, drift);
            else {
                flags.accel = m_app.raceKeyDown(RaceKey::Up) ? 1 : 0;
                flags.brake = m_app.raceKeyDown(RaceKey::Down) ? 1 : 0;
                flags.steerLeft = m_app.raceKeyDown(RaceKey::Left) ? 1 : 0;
                flags.steerRight = m_app.raceKeyDown(RaceKey::Right) ? 1 : 0;
                drift = m_app.raceKeyDown(RaceKey::Drift);
            }
        } else if (m_stage >= Stage::Finished) {
            flags.brake = 1;
        }
        const bool accelPressed = flags.accel != 0 && !m_accelWas;
        m_accelWas = flags.accel != 0;
        m_sim.setLocalInput(flags, drift, accelPressed);
    }
    const int ticks = m_sim.advance(dt);
    if (ticks > 0 && m_sim.hasLocal() && driving) watchCheckpoints();
    const float clock = m_app.renderer().animation().seconds();
    if (m_viewHandle >= 0 && m_sim.hasLocal()) m_view.setPose(m_viewHandle, m_sim.pose(m_sim.localIndex()), dt, clock);
    if (m_ghostCar >= 0 && m_ghostView >= 0) m_view.setPose(m_ghostView, m_sim.pose(m_ghostCar), dt, clock);
    if (m_sim.hasLocal()) m_chase = localPose();
    if (driving || m_stage == Stage::Countdown) {
        const CarPose pose = localPose();
        m_app.sound().engine("accel_my_snd_03", 0.6f + std::min(pose.speedKmh, 200.f) / 200.f * 1.2f, 0.5f);
    } else {
        m_app.sound().stopEngine();
    }
}

bool GhostRaceScreen::drawScene() {
    if (!m_worldLoaded || !m_view.loaded()) return false;
    if (m_stage >= Stage::Finished) {
        m_view.drawFinish(m_app.renderer(), m_chase, m_frameDt);
    } else if (m_stage == Stage::Racing && m_app.raceKeyDown(RaceKey::Back)) {
        float eye[3], look[3];
        lookBackCamera(localPose(), eye, look);
        m_view.drawFixed(m_app.renderer(), eye, look, kLookBackFieldRadians, m_frameDt);
    } else {
        m_view.draw(m_app.renderer(), m_chase, m_frameDt);
    }
    return true;
}

void GhostRaceScreen::fillHud(HudState& s) const {
    const CarPose pose = localPose();
    s.kind = HudKind::Ghost;
    s.speedKmh = pose.speedKmh;
    s.racers = 1;
    s.totalLaps = static_cast<int>(m_world.files.laps);
    s.lap = std::min(m_laps + 1, s.totalLaps);
    s.raceSeconds = m_runClock;
    s.bestLapSeconds = m_bestLap;
    s.clock = static_cast<double>(m_time);
    s.minimap = &m_world.minimap;
    s.waiting = m_stage == Stage::Loading || m_stage == Stage::Wait;
    if (m_stage == Stage::Countdown) {
        const float left = kCountdownSeconds - m_stageTime;
        s.countdownStage = m_stageTime < kCountdownPreroll ? 0 : left > 2.f ? 3 : left > 1.f ? 2 : 1;
        if (s.countdownStage > 0) s.countdownAge = static_cast<double>(static_cast<float>(s.countdownStage) - left);
        s.waiting = s.countdownStage == 0;
    } else if (m_stage == Stage::Racing && m_stageTime < 1.f) {
        s.countdownStage = 4;
        s.countdownAge = m_stageTime;
    }
    s.reverse = pose.reversing && m_stage == Stage::Racing;
    s.finished = m_stage >= Stage::Finished;
    if (m_lapFlash != 0) {
        s.lapFlash = m_lapFlash;
        s.lapFlashAge = static_cast<double>(m_time) - m_lapFlashAt;
        if (s.lapFlashAge > 4.0) s.lapFlash = 0;
    }
    const Catalog& cat = m_app.session().catalog();
    const DriverRow* mine = cat.driver(m_app.session().myDriverKey());
    if (m_sim.hasLocal()) {
        HudCar car;
        car.x = pose.x; car.y = pose.y; car.local = true;
        car.driverAsset = mine && !mine->asset.empty() ? mine->asset : "Cosmo";
        s.cars.push_back(car);
    }
    if (m_ghostCar >= 0) {
        const CarPose g = m_sim.pose(m_ghostCar);
        HudCar car;
        car.x = g.x; car.y = g.y; car.local = false;
        const DriverRow* gd = cat.driver(m_info.driverKey);
        car.driverAsset = gd && !gd->asset.empty() ? gd->asset : car.driverAsset;
        s.cars.push_back(car);
        // FUN 00499180 ghost name floats over kart through last view and lens
        if (m_view.camera().valid) {
            float view[16], eye[3], proj[16];
            m_view.lastView(view, eye);
            m_app.renderer().projection(proj);
            // canvas in pixels its origin and two scales letterboxed or stretched
            float px[4];
            m_app.canvasToPixels(0.f, 0.f, m_app.canvasWidth(), m_app.canvasHeight(), px);
            const float offX = px[0];
            const float offY = px[1];
            const float scaleX = px[2] / m_app.canvasWidth();
            const float scaleY = px[3] / m_app.canvasHeight();
            const float world[4] = {g.x, g.y, g.z + 2.2f, 1.f};
            float e[4], c[4];
            bx::vec4MulMtx(e, world, view);
            bx::vec4MulMtx(c, e, proj);
            if (c[3] > 0.001f) {
                const float nx = c[0] / c[3];
                const float ny = c[1] / c[3];
                if (nx >= -1.f && nx <= 1.f && ny >= -1.f && ny <= 1.f) {
                    HudNameTag tag;
                    tag.x = ((nx * 0.5f + 0.5f) * static_cast<float>(m_app.width()) - offX) / scaleX;
                    tag.y = ((0.5f - ny * 0.5f) * static_cast<float>(m_app.height()) - offY) / scaleY;
                    tag.name = m_info.name;
                    s.tags.push_back(tag);
                }
            }
        }
    }
    s.status = m_status;
}

void GhostRaceScreen::draw(SpriteBatch& batch) {
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    const float w = m_app.canvasWidth();
    const float h = m_app.canvasHeight();
    AssetStore& assets = m_app.assets();
    if (!m_worldLoaded) {
        batch.fill(0.f, 0.f, w, h, rgba(20, 20, 30, 255));
        ctx.font.drawCentered(batch, m_status, w * 0.5f, h * 0.5f - 10.f, 18.f, kWhite);
        ctx.font.drawCentered(batch, "Escape goes back to the ghost menu", w * 0.5f, h * 0.5f + 30.f, 14.f, rgba(255, 220, 90, 255));
        return;
    }
    HudState s;
    fillHud(s);
    if (m_stage < Stage::Finished) {
        m_hud.draw(ctx, assets, s, w, h);
    } else {
        // finish camera keeps ghost name plate hud goes away as in race
        for (const HudNameTag& tag : s.tags) {
            const float width = ctx.bold.measure(tag.name, 11.f) + 8.f;
            const float x = std::floor(tag.x - width * 0.5f);
            const float y = std::floor(tag.y - 8.f);
            batch.fill(x, y, width, 16.f, rgba(30, 30, 30, 150));
            ctx.bold.draw(batch, tag.name, x + 4.f, y + 2.f, 11.f, kWhite);
        }
    }
    if (m_stage == Stage::Result && m_stageTime >= kResultDelay) {
        // FUN 00459100 dims background lines centred x plus170 track x226 y114 time x226 y142
        batch.fill(0.f, 0.f, w, h, rgba(0, 0, 0, 0x60));
        const Texture* sheet = assets.texture("Popup/GhostModeResult/Ghost_Result.png");
        if (sheet && sheet->valid()) batch.draw(sheet->handle, kPopupX, kPopupY, static_cast<float>(sheet->width), static_cast<float>(sheet->height));
        else batch.fill(kPopupX, kPopupY, 341.f, 396.f, rgba(230, 230, 230, 240));
        const std::vector<std::string> lines = wrapText(ctx.bold, m_app.tr(resultKey()), 13.f, 239.f);
        const int n = static_cast<int>(std::min<size_t>(lines.size(), 4));
        float y = kPopupY + static_cast<float>((110 - n * 20) / 2) + 16.f;
        for (int i = 0; i < n; ++i) { ctx.bold.drawCentered(batch, lines[static_cast<size_t>(i)], kPopupX + 170.f, y, 13.f, kInkDark); y += 20.f; }
        const TrackRow* row = m_app.session().catalog().track(m_info.trackId);
        drawAligned(ctx, ctx.bold, row ? m_app.tr(row->nameKey) : std::to_string(m_info.trackId), kPopupX + 226.f, kPopupY + 114.f, 13.f, kInkDark, Align::Centre);
        drawAligned(ctx, ctx.bold, clockText(m_finishMs), kPopupX + 226.f, kPopupY + 142.f, 13.f, kInkDark, Align::Centre);
        for (size_t i = 0; i < 3; ++i) {
            const float ry = kPopupY + 204.f + 28.f * static_cast<float>(i);
            if (i >= m_result.top.size() || m_result.top[i].timeMs <= 0) continue;
            drawAligned(ctx, ctx.bold, u16ToUtf8(m_result.top[i].name), kPopupX + 160.f, ry, 13.f, kInkDark, Align::Centre);
            drawAligned(ctx, ctx.bold, clockText(m_result.top[i].timeMs), kPopupX + 286.f, ry, 13.f, kInkDark, Align::Centre);
        }
        drawAligned(ctx, ctx.bold, clockText(m_result.mine.timeMs), kPopupX + 170.f, kPopupY + 326.f, 13.f, kInkDark, Align::Centre);
        const Texture* ok = assets.texture("Buttons/ui_ok_00.png");
        if (ok && ok->valid()) batch.draw(ok->handle, kPopupOk.x, kPopupOk.y, static_cast<float>(ok->width), static_cast<float>(ok->height));
        else ctx.bold.drawCentered(batch, "OK", kPopupOk.x + 48.f, kPopupOk.y + 6.f, 15.f, kInkDark);
    } else if (m_stage == Stage::Upload || m_stage == Stage::Waiting) {
        ctx.font.drawCentered(batch, "waiting for the 0x00B0 answer", w * 0.5f, h * 0.8f, 16.f, kWhite);
    }
    if (m_exitBox) m_hud.drawInfoBox(ctx, assets, wrapText(ctx.bold, m_app.tr("MSG_CONFIRM_EXIT"), 15.f, 320.f), true);
}

void GhostRaceScreen::onKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    const bool enter = key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER;
    if (!m_worldLoaded) { if (key == GLFW_KEY_ESCAPE) leaveToMenu(); return; }
    if (m_exitBox) {
        if (enter) leaveToMenu();
        else if (key == GLFW_KEY_ESCAPE) m_exitBox = false;
        return;
    }
    if (m_stage == Stage::Result && m_stageTime >= kResultDelay) {
        if (enter) leaveToMenu();
        return;
    }
    if (key == GLFW_KEY_ESCAPE && m_stage < Stage::Finished) m_exitBox = true;
}

void GhostRaceScreen::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    if (m_exitBox) {
        if (RaceHud::pauseRow(0).contains(x, y)) { m_app.click(); leaveToMenu(); }
        else if (RaceHud::pauseRow(1).contains(x, y)) { m_app.click(); m_exitBox = false; }
        return;
    }
    if (m_stage == Stage::Result && m_stageTime >= kResultDelay && kPopupOk.contains(x, y)) { m_app.click(); leaveToMenu(); }
}

}
