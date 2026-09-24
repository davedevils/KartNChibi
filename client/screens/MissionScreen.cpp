#include "MissionScreen.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "engine/render/nif_prop_model.h"
#include "engine/render/scene_renderer.h"
#include "net/Utf.h"
#include "screens/GhostModeScreen.h"
#include "screens/HelpPopup.h"
#include "screens/MenuPopup.h"
#include "screens/ShopCommon.h"
#include "tools/track_scene/ghost_car.h"
#include "ui/MenuFrame.h"
#include "ui/MessagePopup.h"

#include <GLFW/glfw3.h>
#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>

namespace KnC::Client {

using KnC::Kart::Client::InputFlags;

namespace {

// the stock mission menu sub 43BAD0 the back at 31 85 the top at 25 50 over the menu frame
constexpr float kBackX = 31.f;
constexpr float kBackY = 85.f;
constexpr float kTopX = 25.f;
constexpr float kTopY = 50.f;
// five slots 94 px apart from 207 the lock at 55 the plate at 61 the cover at 51
constexpr float kRowY = 207.f;
constexpr float kRowStep = 94.f;
constexpr int kRowsShown = 5;
constexpr Rect kUpButton = {51.f, 167.f, 85.f, 32.f};
constexpr Rect kDownButton = {51.f, 666.f, 85.f, 32.f};
constexpr Rect kStartButton = {457.f, 587.f, 65.f, 60.f};
// picture at 203 211 title at 615 279 the sub line on 310 the tip plate at 615 548
constexpr float kPictureX = 203.f;
constexpr float kPictureY = 211.f;
constexpr float kTitleX = 615.f;
constexpr float kTitleY = 279.f;
constexpr float kTitleW = 298.f;
// sub 43C060 gold 308 561 exp 308 582 fee 269 623 the reward icon at 401 551
constexpr float kRewardIconX = 401.f;
constexpr float kRewardIconY = 551.f;

}

// the reward icon of the 0x0087 type 0 driver 1 kart 2 item 3 part 4 pet 7 pendant
std::string missionRewardIcon(const Session& session, uint32_t type, uint32_t key) {
    const Catalog& cat = session.catalog();
    switch (type) {
    case 0: if (const DriverRow* r = cat.driver(key)) return driverIcon(*r); break;
    case 1: if (const KartRow* r = cat.kart(key)) return kartIcon(*r); break;
    case 2: if (const ItemRow* r = cat.item(key)) return itemIcon(*r); break;
    case 3: if (const PartRow* r = cat.part(key)) return partIcon(*r); break;
    case 4: if (const PetRow* r = cat.pet(key)) return petIcon(*r); break;
    case 7: if (const PendantDef* p = session.pendantDef(key)) return "Icon/" + p->iconBase + "_00.png"; break;
    default: break;
    }
    return std::string();
}

bool missionRowPlayable(const std::vector<MissionProgress>& rows, int index) {
    if (index < 0 || index >= static_cast<int>(rows.size())) return false;
    if (rows[static_cast<size_t>(index)].cleared == 1 || index == 0) return true;
    return rows[static_cast<size_t>(index - 1)].cleared == 1 && rows[static_cast<size_t>(index)].cleared == 0;
}

void MissionMenuScreen::enter() {
    AssetStore& assets = m_app.assets();
    addMenuFrame(*this, assets, FrameMode::Full, "missions");
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
    image("mission_back", "MissionMenu/Mission_Back.png", kBackX, kBackY, -10);
    image("mission_top", "MissionMenu/Mission_Top.png", kTopX, kTopY, -9);
    auto button = [&](const char* id, const char* action, const char* art, const Rect& rect) {
        auto b = std::make_unique<ButtonWidget>();
        b->type = ElementType::Button;
        b->id = id;
        b->action = action;
        b->normal = assets.texture(std::string(art) + "00.png");
        b->hover = assets.texture(std::string(art) + "01.png");
        b->pressed = assets.texture(std::string(art) + "02.png");
        b->rect = rect;
        b->zIndex = 30;
        add(std::move(b));
    };
    button("btn_up", "up", "MissionMenu/Mission_up_", kUpButton);
    button("btn_down", "down", "MissionMenu/Mission_down_", kDownButton);
    button("btn_start", "start", "MissionMenu/Mission_start_", kStartButton);
    m_time = 0.f;
    m_captured = false;
    m_autoSent = false;
    m_picked = false;
    m_status.clear();
    pickEntryRow();
    // the init tail of stage 24 as the lobby sends it
    m_app.session().sendStageTail();
}

void MissionMenuScreen::pickEntryRow() {
    const std::vector<MissionProgress>& rows = m_app.session().missionProgress();
    m_selected = rows.empty() ? -1 : static_cast<int>(rows.size()) - 1;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].cleared == 0) { m_selected = static_cast<int>(i); break; }
    }
    const int wanted = m_app.options().autoMission;
    for (size_t i = 0; i < rows.size(); ++i)
        if (wanted >= 0 && static_cast<int>(rows[i].missionId) == wanted) m_selected = static_cast<int>(i);
    m_top = m_selected < 0 ? 0 : m_selected - m_selected % kRowsShown;
    m_picked = !rows.empty();
}

void MissionMenuScreen::update(float dt) {
    m_time += dt;
    // a 0x0088 that lands after the push picks its row then
    if (!m_picked && !m_app.session().missionProgress().empty()) pickEntryRow();
    if (Widget* start = find("btn_start")) start->visible = missionRowPlayable(m_app.session().missionProgress(), m_selected);
    const Options& o = m_app.options();
    if (m_app.captureMode() && !m_captured && m_time > 0.8f) {
        m_captured = true;
        m_app.captureStage("missions");
        if (o.stopAt == "missions" && !m_app.scripted()) m_app.finishRun();
    }
    if (o.autoMission >= 0 && !m_autoSent && m_time > 1.5f) {
        m_autoSent = true;
        startSelected();
    }
}

void MissionMenuScreen::page(int delta) {
    const int n = static_cast<int>(m_app.session().missionProgress().size());
    if (delta < 0 && m_top >= kRowsShown) m_top -= kRowsShown;
    if (delta > 0 && m_top + kRowsShown < n) m_top += kRowsShown;
}

void MissionMenuScreen::askStart() {
    const std::vector<MissionProgress>& rows = m_app.session().missionProgress();
    if (!missionRowPlayable(rows, m_selected)) return;
    // the shipped table is two lines off around MISSION ENTER the fee question sits on CAR THUNDER INFO
    std::string text = m_app.tr("MISSION_ENTER");
    if (text.rfind("CAR_", 0) == 0 || text == "MISSION_ENTER") text = m_app.tr("CAR_THUNDER_INFO");
    for (size_t at = text.find("\\n"); at != std::string::npos; at = text.find("\\n", at)) text.replace(at, 2, " ");
    m_app.pushScreen(std::make_unique<MessagePopup>(m_app, text, [this]() { startSelected(); }, nullptr, true));
}

void MissionMenuScreen::startSelected() {
    const std::vector<MissionProgress>& rows = m_app.session().missionProgress();
    if (m_selected < 0 || m_selected >= static_cast<int>(rows.size())) {
        m_status = "no 0x0088 mission row to start";
        return;
    }
    if (!missionRowPlayable(rows, m_selected)) {
        m_status = "the row is locked";
        std::printf("[missions] row %d is locked no 0x0090\n", m_selected);
        return;
    }
    const uint32_t id = rows[static_cast<size_t>(m_selected)].missionId;
    m_app.session().startMission(id);
    m_status = "start 0x0090 sent for mission " + std::to_string(id);
    std::printf("[missions] %s\n", m_status.c_str());
}

// sub 43C550 Escape opens the Menu box F1 the help the menu has no arrow or Enter keys
void MissionMenuScreen::onKey(int key, int action, int mods) {
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) { m_app.pushScreen(std::make_unique<MenuPopup>(m_app)); return; }
    if (action == GLFW_PRESS && key == GLFW_KEY_F1) { m_app.pushScreen(std::make_unique<HelpPopup>(m_app)); return; }
    WidgetScreen::onKey(key, action, mods);
}

// sub 43BED0 a press on a medal picks the row even a locked one
void MissionMenuScreen::onMouseButton(int button, int action, float x, float y) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        for (int i = 0; i < kRowsShown; ++i) {
            const Rect medal = {51.f, 203.f + kRowStep * static_cast<float>(i), 85.f, 84.f};
            if (!medal.contains(x, y)) continue;
            const int row = m_top + i;
            if (row < static_cast<int>(m_app.session().missionProgress().size())) {
                m_selected = row;
                m_app.click();
                std::printf("[missions] row %d picked mission %u\n", row, m_app.session().missionProgress()[static_cast<size_t>(row)].missionId);
            }
            return;
        }
    }
    WidgetScreen::onMouseButton(button, action, x, y);
}

void MissionMenuScreen::onAction(const std::string& action, Widget&) {
    if (action == "up") page(-1);
    else if (action == "down") page(1);
    else if (action == "start") askStart();
    else if (action == "back" || action == "lobby") m_app.session().openLobby();
    else if (action == "shop") m_app.session().openShop();
    else if (action == "garage") m_app.session().openGarage();
    else if (action == "ghost") GhostModeScreen::open(m_app);
    else if (action == "missions") return;
    else frameAction(m_app, action);
}

void MissionMenuScreen::onSession(SessionEvent event) {
    if (event == SessionEvent::MissionStart) m_app.go("mission");
    if (event == SessionEvent::MissionMenu) pickEntryRow();
}

// the five slots on the left the picture the texts the reward and the tip plate on the right
void MissionMenuScreen::drawOverlay(DrawContext& ctx) {
    const Session& session = m_app.session();
    AssetStore& assets = m_app.assets();
    auto sprite = [&](const std::string& path, float x, float y, float scale = 1.f) {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) ctx.batch.draw(t->handle, x, y, static_cast<float>(t->width) * scale, static_cast<float>(t->height) * scale);
    };
    const std::vector<MissionProgress>& rows = session.missionProgress();
    // the NEW plate rides the first open row that is not cleared
    int newRow = -1;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        if (missionRowPlayable(rows, i) && rows[static_cast<size_t>(i)].cleared == 0) { newRow = i; break; }
    }
    for (int i = 0; i < kRowsShown; ++i) {
        const int row = m_top + i;
        const float y = kRowY + kRowStep * static_cast<float>(i);
        // a slot past the end of the list is a locked slot too
        const bool locked = !missionRowPlayable(rows, row);
        if (locked) sprite("MissionMenu/Mission_lock.PNG", 55.f, y);
        char plate[64];
        std::snprintf(plate, sizeof(plate), "MissionMenu/Mission_lv%d_hide.PNG", i + 1);
        sprite(plate, 61.f, y + 31.f);
        sprite("MissionMenu/Mission_cover.png", 51.f, y - 4.f);
        if (row == newRow) sprite("MissionMenu/Mission_new.png", 48.f, y + 21.f);
        if (row < static_cast<int>(rows.size()) && rows[static_cast<size_t>(row)].cleared == 1)
            sprite("MissionMenu/Mission_clear.png", 51.f, y - 4.f);
    }
    if (m_selected >= 0 && m_selected < static_cast<int>(rows.size())) {
        const MissionProgress& row = rows[static_cast<size_t>(m_selected)];
        const MissionDef* d = session.missionDef(row.missionId);
        // the picture and the tip plate go by the mission id the chapter and the level by the row
        char name[64];
        std::snprintf(name, sizeof(name), "MissionMenu/select_track_Mission_%02u.png", row.missionId + 1);
        sprite(name, kPictureX, kPictureY);
        std::snprintf(name, sizeof(name), "MissionMenu/Mission_chapter_%d.PNG", m_selected / kRowsShown + 1);
        sprite(name, 300.f, 400.f);
        std::snprintf(name, sizeof(name), "MissionMenu/Mission_lv_%d.png", m_selected % kRowsShown + 1);
        sprite(name, 445.f, 409.f);
        std::snprintf(name, sizeof(name), "MissionMenu/MISSION_%02u_TIP.PNG", row.missionId + 1);
        sprite(name, 615.f, 548.f);
        if (d) {
            // the title in blue bold the sub line in white the story waits for the run board
            const std::string title = m_app.tr(d->titleKey);
            ctx.bold.drawClipped(ctx.batch, title.empty() ? d->worldName : title, kTitleX, kTitleY, kTitleW, 17.f, rgba(74, 148, 255, 255));
            const std::vector<std::string> sub = wrapText(ctx.font, m_app.tr(d->subKey), 15.f, kTitleW);
            float y = kTitleY + 31.f;
            for (const std::string& line : sub) { ctx.font.draw(ctx.batch, line, kTitleX, y, 15.f, kInkWhite); y += 18.f; if (y > 530.f) break; }
            // a cleared row shows 0 0 0 and no icon as sub 43C060 does
            const bool cleared = row.cleared == 1;
            ctx.bold.draw(ctx.batch, std::to_string(cleared ? 0u : d->rewardMileage), 308.f, 561.f, 15.f, kInkDark);
            ctx.bold.draw(ctx.batch, std::to_string(cleared ? 0u : d->rewardExp), 308.f, 582.f, 15.f, kInkDark);
            ctx.bold.draw(ctx.batch, std::to_string(cleared ? 0u : d->rewardExtra), 269.f, 623.f, 15.f, kInkDark);
            if (!cleared && d->rewardItemKey != 0) {
                const std::string icon = missionRewardIcon(session, d->rewardItemType, d->rewardItemKey);
                if (!icon.empty()) {
                    if (d->rewardItemType == 5) sprite(icon, 369.f, 519.f, 0.5f);
                    else sprite(icon, kRewardIconX, kRewardIconY);
                }
            }
        }
    }
    if (!m_status.empty() && m_app.options().statusLine) ctx.font.draw(ctx.batch, m_status, 203.f, 700.f, 12.f, kInkGrey);
}

namespace {

// FUN 004B4240 mode 0 mummy board 30 150 text plus 380 115 wide 355 numbers plus 810
constexpr float kBoardX = 30.f;
constexpr float kBoardY = 150.f;
constexpr float kBoardTextDx = 380.f;
constexpr float kBoardTextDy = 115.f;
constexpr float kBoardTextW = 355.f;
constexpr float kBoardNumDx = 810.f;
constexpr float kBoardFeeDy = 104.f;
constexpr float kBoardGoldDy = 138.f;
constexpr float kBoardExpDy = 160.f;
constexpr float kBoardIconDx = 747.f;
constexpr float kBoardIconDy = 202.f;
// the skip key art of the board at plus 830 240
constexpr Rect kBoardSkip = {kBoardX + 830.f, kBoardY + 240.f, 70.f, 60.f};
// FUN 0043AB00 1010 holds 1500 ms then sub 4B2690 mode 2 the pre roll and the three digits
constexpr float kPreSeconds = 1.5f;
constexpr float kCountdownPreroll = 0.24f;
constexpr float kCountdownSeconds = 3.24f;
// 3010 and 4010 hold 3000 ms on the clip before the popup
constexpr float kClipSeconds = 3.f;
// FUN 0046B750 finish board 163 15 exp 485 338 gold 615 338 icon 476 370
constexpr float kFinishX = 163.f;
constexpr float kFinishY = 15.f;
constexpr float kFinishExpX = 485.f;
constexpr float kFinishGoldX = 615.f;
constexpr float kFinishNumY = 338.f;
constexpr Rect kFinishEnter = {kFinishX + 510.f, kFinishY + 360.f, 70.f, 60.f};
// the fail board slides in from x minus 550 to 254 on y 51 by 50 per frame
constexpr float kFailY = 51.f;
constexpr float kFailFrom = -550.f;
constexpr float kFailTo = 254.f;
constexpr Rect kFailEnter = {kFailTo + 420.f, kFailY + 220.f, 70.f, 60.f};
// the popup closes by itself after 30 s FUN 0046B240
constexpr float kPopupSeconds = 30.f;
// FUN 00439DB0 the count tens at 10 units at 40 the slash at 65 the goal at 90 and 120
constexpr float kCounterY = 300.f;
// sub 4D9650 a time item within 9 sub 4DAA20 a box within 4 back after 50 s
constexpr float kTimeItemReach = 9.f;
constexpr float kBoxReach = 4.f;
constexpr float kBoxRespawn = 50.f;
// sub 4D8590 time items 3 up at scale 4 sub 4D9ED0 boxes 2 up at scale 2
constexpr float kTimeItemLift = 3.f;
constexpr float kTimeItemScale = 4.f;
constexpr float kBoxLift = 2.f;
constexpr float kBoxScale = 2.f;
// the time item types 0 and 1 add 2 and 5 s the types 2 and 3 take them
constexpr double kTimeItemMs[4] = {2000.0, 5000.0, -2000.0, -5000.0};
// the gimmick nifs of the four types the obfuscated names of 0x4D8590 decoded
const char* const kTimeItemNif[4] = {"mi01_ef_b2", "mi01_ef_b5", "mi01_ef_r2", "mi01_ef_r5"};
// the travel a lap needs before the START face closes it the car spawns on that face
constexpr float kLapTravel = 200.f;

std::string gimmickFile(const std::string& trackDir, const std::string& name) {
    const std::string dir = findEntryCi(trackDir, "Gimmick");
    if (dir.empty()) return std::string();
    return findEntryCi(dir, name);
}

}

MissionRunScreen::MissionRunScreen(App& app) : m_app(app) {}

MissionRunScreen::~MissionRunScreen() = default;

void MissionRunScreen::enter() {
    Session& session = m_app.session();
    const MissionRun& run = session.missionRun();
    const MissionDef* def = session.missionDef(run.missionId);
    const MissionProgress* state = session.missionState(run.missionId);
    // 0x43AB00 keeps the cleared flag of the row the finish board shows 0 on a replay
    m_firstClear = !(state && state->cleared == 1);
    m_autoDrive = m_app.options().autoMission >= 0;
    m_status = def ? "loading World/Mission/" + def->worldName : "mission " + std::to_string(run.missionId) + " has no 0x0087 row";
    m_app.playMusic("mission_bmg_01");
    if (!def) return;
    if (!loadWorld(def->worldName)) return;
    loadGimmicks(def->kind);
    spawnLocal();
    m_phase = Phase::Board;
    m_phaseTime = 0.f;
}

bool MissionRunScreen::loadWorld(const std::string& worldName) {
    TrackFiles files;
    files.themeFolder = "Mission";
    files.trackFolder = worldName;
    files.laps = 1;
    const std::string world = m_app.options().gameDir + "/Data/Public/World";
    files.mapDir = findEntryCi(world, "Mission");
    if (files.mapDir.empty()) { m_status = "no World/Mission folder"; std::printf("[mission] %s\n", m_status.c_str()); return false; }
    files.trackDir = findEntryCi(files.mapDir, worldName);
    if (files.trackDir.empty()) { m_status = "no World/Mission/" + worldName; std::printf("[mission] %s\n", m_status.c_str()); return false; }
    for (char& c : files.mapDir) if (c == '\\') c = '/';
    for (char& c : files.trackDir) if (c == '\\') c = '/';
    std::string error;
    if (!loadRaceWorld(files, m_world, error)) { m_status = "world load failed: " + error; std::printf("[mission] %s\n", m_status.c_str()); return false; }
    if (!m_sim.init(m_world, m_app.session().profile().playerId, error)) { m_status = "sim init failed: " + error; return false; }
    if (const OwnedPet* worn = m_app.session().catalog().equippedPet()) m_sim.setEquippedPet(worn->petKey);
    m_view.load(m_app.renderer(), m_world);
    m_auto.reset(m_sim.line());
    m_worldLoaded = true;
    m_status = "Mission/" + worldName + " loaded";
    std::printf("[mission] %s racing line %zu points checkpoints %d\n", m_status.c_str(), m_sim.line().size(),
                m_world.checkpointCount);
    return true;
}

// kind 0 reads gimmick 01 seven in ten else 02 kind 1 either file half and half
void MissionRunScreen::loadGimmicks(uint32_t kind) {
    m_gimmicks.clear();
    if (kind > 1) {
        std::printf("[mission] kind %u loads no gimmick\n", kind);
        return;
    }
    const bool first = kind == 0 ? std::rand() % 10 < 7 : std::rand() % 10 < 5;
    const std::string trackDir = m_world.files.trackDir;
    m_gimmickFile = gimmickFile(trackDir, first ? "gimmick_01.ini" : "gimmick_02.ini");
    // the stock load fails on a world with no such file the run then has no gimmick at all
    if (m_gimmickFile.empty()) {
        std::printf("[mission] kind %u world %s has no gimmick 01 or 02 ini\n", kind, trackDir.c_str());
        return;
    }
    std::ifstream in(m_gimmickFile);
    std::string line;
    while (std::getline(in, line) && m_gimmicks.size() < 80) {
        for (char& c : line) if (c == ',') c = ' ';
        std::istringstream row(line);
        Gimmick g;
        if (kind == 0) {
            if (!(row >> g.type >> g.x >> g.y >> g.z)) continue;
            if (g.type < 0 || g.type > 3) continue;
            g.z += kTimeItemLift;
        } else {
            if (!(row >> g.x >> g.y >> g.z)) continue;
            g.z += kBoxLift;
        }
        m_gimmicks.push_back(g);
    }
    // one prop per row the nif of its type or the box nif of the kind 1 worlds
    namespace fs = std::filesystem;
    std::map<std::string, KnC::Render::PropModel> models;
    for (Gimmick& g : m_gimmicks) {
        const std::string stem = kind == 0 ? kTimeItemNif[g.type] : "gimmick";
        auto it = models.find(stem);
        if (it == models.end()) {
            KnC::Render::NifModelRequest request;
            request.nif_path = gimmickFile(trackDir, stem + ".nif");
            request.texture_dir = fs::path(request.nif_path).parent_path().string();
            request.play_stopped_controllers = true;
            KnC::Render::PropModel model;
            std::string error;
            if (request.nif_path.empty() || !KnC::Render::load_prop_model(request, model, error)) {
                std::printf("[mission] gimmick nif %s did not load %s\n", stem.c_str(), error.c_str());
                models[stem] = KnC::Render::PropModel();
                continue;
            }
            KnC::Tools::resolve_textures(request.texture_dir, model);
            it = models.emplace(stem, std::move(model)).first;
        }
        if (it->second.parts.empty()) continue;
        g.prop = m_view.addProp(m_app.renderer(), it->second);
    }
    placeGimmicks();
    std::printf("[mission] kind %u gimmicks %zu from %s\n", kind, m_gimmicks.size(), m_gimmickFile.c_str());
}

void MissionRunScreen::placeGimmicks() {
    const MissionDef* def = m_app.session().missionDef(m_app.session().missionRun().missionId);
    const float scale = def && def->kind == 0 ? kTimeItemScale : kBoxScale;
    for (const Gimmick& g : m_gimmicks) {
        if (g.prop < 0) continue;
        float s[16];
        float t[16];
        float world[16];
        bx::mtxScale(s, scale);
        bx::mtxTranslate(t, g.x, g.y, g.z);
        bx::mtxMul(world, s, t);
        m_view.placeProp(g.prop, world, !g.taken);
    }
}

void MissionRunScreen::spawnLocal() {
    Session& session = m_app.session();
    const auto& rows = m_world.scene.start_rows;
    float x = 0.f, y = 0.f, z = 1.f, yaw = 0.f;
    if (!rows.empty()) { x = rows[0].x; y = rows[0].y; z = rows[0].z; yaw = rows[0].heading; }
    const uint32_t kartKey = session.myKartKey();
    const uint32_t driverKey = session.myDriverKey();
    const KartRow* kart = session.catalog().kart(kartKey);
    const DriverRow* driver = session.catalog().driver(driverKey);
    const std::string model = kart && !kart->model.empty() ? kart->model : "Basic_1";
    const std::string asset = driver && !driver->asset.empty() ? driver->asset : "Cosmo";
    CarSetup setup;
    setup.playerId = session.profile().playerId;
    if (kart) { setup.stats = kart->stats; setup.vehicleKind = static_cast<int>(kart->vehicleKind); }
    setup.carFile = kartCarFile(m_app.options().gameDir, model);
    setup.x = x; setup.y = y; setup.z = z; setup.yawDeg = yaw;
    std::string error;
    if (!m_sim.spawnLocal(setup, error)) { m_status = "local spawn failed: " + error; std::printf("[mission] %s\n", m_status.c_str()); return; }
    m_viewHandle = m_view.addCar(m_app.renderer(), m_app.options().gameDir, model, asset);
    m_auto.start(x, y, yaw);
    m_chase = m_sim.pose(m_sim.localIndex());
    m_lastX = x;
    m_lastY = y;
    m_lastFace = m_sim.localCheckpointFace();
}

CarPose MissionRunScreen::localPose() const {
    if (!m_sim.hasLocal()) return m_chase;
    return m_sim.pose(m_sim.localIndex());
}

double MissionRunScreen::timeLeftMs() const {
    const MissionDef* def = m_app.session().missionDef(m_app.session().missionRun().missionId);
    if (!def || def->timeLimitMs <= 0) return 0.0;
    return static_cast<double>(def->timeLimitMs) + m_bonusMs - static_cast<double>(m_runClock) * 1000.0;
}

void MissionRunScreen::onSession(SessionEvent event) {
    if (event == SessionEvent::MissionComplete) m_status = "mission complete 0x008C landed";
    // the menu ack or the lobby ack lands under the run when the exit box sent its request
    if (event == SessionEvent::LobbyAck || event == SessionEvent::MissionMenu) m_app.sound().stopEngine();
}

void MissionRunScreen::update(float dt) {
    m_time += dt;
    m_frameDt = dt;
    m_phaseTime += dt;
    if (!m_worldLoaded) return;
    Session& session = m_app.session();
    const MissionRun& run = session.missionRun();
    if (m_phase == Phase::Board) {
        if (m_app.captureMode() && !m_capturedBoard && m_phaseTime > 0.8f) {
            m_capturedBoard = true;
            m_app.captureStage("mission_board");
        }
        // FUN 0043AA70 the enter key hides the board the auto run presses it after a moment
        if (m_autoDrive && m_phaseTime > 1.5f) { m_phase = Phase::Pre; m_phaseTime = 0.f; }
    }
    if (m_phase == Phase::Pre && m_phaseTime >= kPreSeconds) { m_phase = Phase::Countdown; m_phaseTime = 0.f; m_cueStage = -1; }
    if (m_phase == Phase::Countdown) {
        const float left = kCountdownSeconds - m_phaseTime;
        const int digit = m_phaseTime < kCountdownPreroll ? 0 : left > 2.f ? 3 : left > 1.f ? 2 : left > 0.f ? 1 : 0;
        if (digit > 0 && digit != m_cueStage) { m_cueStage = digit; m_app.sound().play("countdown_ready_snd"); }
        if (m_phaseTime >= kCountdownSeconds) {
            // sub 4B1DC0 the GO arms the deadline of sub 43A150 now plus the def limit
            m_phase = Phase::Running;
            m_phaseTime = 0.f;
            m_runClock = 0.f;
            m_app.sound().play("countdown_go_snd");
            m_status = "the run started";
        }
    }
    const bool driving = m_phase == Phase::Running;
    if (driving) {
        m_runClock += dt;
        if (m_app.captureMode() && !m_captured && m_runClock > 12.f) {
            m_captured = true;
            m_app.captureStage("mission");
        }
        // state 4000 when the deadline passes the time items move it
        const MissionDef* def = session.missionDef(run.missionId);
        if (def && def->timeLimitMs > 0 && timeLeftMs() <= 0.0) {
            std::printf("[mission] time up at %.1f s\n", m_runClock);
            failed();
        }
    }
    if ((m_phase == Phase::Success || m_phase == Phase::Fail) && m_phaseTime >= kClipSeconds) {
        // 3010 waits for the 0x008C answer the fail board comes at once
        if (m_phase == Phase::Fail || run.complete) { m_phase = Phase::Popup; m_phaseTime = 0.f; }
    }
    if (m_phase == Phase::Popup) {
        if (m_app.captureMode() && !m_capturedEnd && m_phaseTime > 1.2f) {
            m_capturedEnd = true;
            m_app.captureStage(m_passed ? "mission_done" : "mission_fail");
            if (!m_app.scripted()) m_app.finishRun();
        }
        if (m_phaseTime >= kPopupSeconds) backToMenu();
    }
    // the flash of a time item fades ten a 60 Hz frame from 250
    if (m_flashAlpha > 0.f) m_flashAlpha = std::max(0.f, m_flashAlpha - 600.f * dt);
    if (m_flashType >= 0) m_flashAge += dt;

    InputFlags flags;
    bool drift = false;
    if (m_sim.hasLocal()) {
        if (m_phase >= Phase::Running) {
            m_sim.setGreenLight(true);
            m_sim.setSessionRunning(true);
        }
        if (driving) {
            if (m_autoDrive) {
                m_auto.drive(localPose(), flags, drift);
                autoSteerToBox(flags);
            } else {
                flags.accel = m_app.raceKeyDown(RaceKey::Up) ? 1 : 0;
                flags.brake = m_app.raceKeyDown(RaceKey::Down) ? 1 : 0;
                flags.steerLeft = m_app.raceKeyDown(RaceKey::Left) ? 1 : 0;
                flags.steerRight = m_app.raceKeyDown(RaceKey::Right) ? 1 : 0;
                drift = m_app.raceKeyDown(RaceKey::Drift);
            }
        } else if (m_phase >= Phase::Success) {
            flags.brake = 1;
        }
        const bool accelPressed = flags.accel != 0 && !m_accelWas;
        m_accelWas = flags.accel != 0;
        m_sim.setLocalInput(flags, drift, accelPressed);
    }
    const int ticks = m_sim.advance(dt);
    if (ticks > 0 && m_sim.hasLocal() && driving) {
        const CarPose now = localPose();
        m_travelled += std::sqrt((now.x - m_lastX) * (now.x - m_lastX) + (now.y - m_lastY) * (now.y - m_lastY));
        m_lastX = now.x;
        m_lastY = now.y;
        watchGimmicks();
        watchEnd();
    }
    const float clock = m_app.renderer().animation().seconds();
    if (m_viewHandle >= 0 && m_sim.hasLocal()) m_view.setPose(m_viewHandle, m_sim.pose(m_sim.localIndex()), dt, clock);
    if (m_sim.hasLocal()) m_chase = localPose();
    const CarPose pose = localPose();
    if (driving || m_phase == Phase::Countdown) m_app.sound().engine("accel_my_snd_03", 0.6f + std::min(pose.speedKmh, 200.f) / 200.f * 1.2f, 0.5f);
    else m_app.sound().stopEngine();
}

// sub 4D9650 a time item within 9 moves the deadline sub 4DAA20 a box within 4 counts one
void MissionRunScreen::watchGimmicks() {
    const MissionDef* def = m_app.session().missionDef(m_app.session().missionRun().missionId);
    if (!def || m_gimmicks.empty()) return;
    const CarPose pose = localPose();
    bool moved = false;
    for (Gimmick& g : m_gimmicks) {
        if (g.taken) {
            // a box comes back after 50 s a time item stays taken
            if (def->kind == 1 && m_runClock >= g.respawnAt) { g.taken = false; moved = true; }
            continue;
        }
        const float dx = g.x - pose.x, dy = g.y - pose.y, dz = g.z - pose.z;
        const float reach = def->kind == 0 ? kTimeItemReach : kBoxReach;
        const float dist2 = def->kind == 0 ? dx * dx + dy * dy + dz * dz : dx * dx + dy * dy;
        if (dist2 >= reach * reach) continue;
        g.taken = true;
        moved = true;
        if (def->kind == 0) {
            m_bonusMs += kTimeItemMs[g.type];
            m_flashColour = g.type < 2 ? 0 : 1;
            m_flashAlpha = 250.f;
            m_flashType = g.type;
            m_flashAge = 0.f;
            m_app.sound().play("itembox_get_snd");
            std::printf("[mission] time item type %d %+0.0f ms left %.0f ms\n", g.type, kTimeItemMs[g.type], timeLeftMs());
        } else {
            g.respawnAt = m_runClock + kBoxRespawn;
            ++m_boxes;
            m_app.sound().play("itembox_get_snd");
            std::printf("[mission] box %d of %d\n", m_boxes, def->goalCount);
        }
    }
    if (moved) placeGimmicks();
}

// the auto run leans toward a box ahead of the line a test aid a hand drives through them
void MissionRunScreen::autoSteerToBox(InputFlags& flags) {
    const MissionDef* def = m_app.session().missionDef(m_app.session().missionRun().missionId);
    if (!def || def->kind != 1 || flags.brake) return;
    const CarPose pose = localPose();
    float best = 1e30f;
    float bestError = 0.f;
    for (const Gimmick& g : m_gimmicks) {
        if (g.taken) continue;
        const float dx = g.x - pose.x, dy = g.y - pose.y;
        const float d2 = dx * dx + dy * dy;
        if (d2 > 40.f * 40.f || d2 >= best) continue;
        // the heading of the line follower forward is minus cos A and sin A
        float error = std::atan2(dy, -dx) * 57.29578f - pose.yawDeg;
        while (error > 180.f) error -= 360.f;
        while (error < -180.f) error += 360.f;
        if (std::fabs(error) > 35.f) continue;
        best = d2;
        bestError = error;
    }
    if (best > 1e29f) return;
    flags.steerLeft = bestError < -3.f ? 1 : 0;
    flags.steerRight = bestError > 3.f ? 1 : 0;
}

// FUN 0043AB00 1100 mission 0 ends on a FINISH face the others on START after the lap flag
void MissionRunScreen::watchEnd() {
    const MissionRun& run = m_app.session().missionRun();
    const MissionDef* def = m_app.session().missionDef(run.missionId);
    if (!def || def->kind > 2) return;
    if (run.missionId == 0) {
        if (m_travelled > kLapTravel && m_sim.localOnFace("FINISH")) {
            std::printf("[mission] FINISH face at %.1f s\n", m_runClock);
            goalReached();
        }
        return;
    }
    const int count = m_world.checkpointCount;
    if (count <= 0) return;
    const int face = m_sim.localCheckpointFace();
    if (face == m_lastFace) return;
    m_lastFace = face;
    if (face < 0) return;
    if (std::getenv("KNC_MISSION_FACES")) std::printf("[mission] face %d cursor %d at %.1f s\n", face, m_checkpoint, m_runClock);
    // a narrow CHECK face a wheel never touched is skipped as the port cursor does up to four ahead
    const bool forward = face > m_checkpoint && face <= m_checkpoint + 4;
    const bool closing = face == 0 && m_checkpoint >= count - 4 && m_travelled >= kLapTravel;
    if (!forward && !closing) return;
    m_checkpoint = face;
    if (face != 0) return;
    // car mission rally update raises the lap flag when the cursor passes the START face
    m_lapFlag = true;
    m_app.sound().play("lap_check_snd");
    std::printf("[mission] START face with the lap flag at %.1f s boxes %d\n", m_runClock, m_boxes);
    if (def->kind == 1 && m_boxes < def->goalCount) { failed(); return; }
    goalReached();
}

// state 3000 the 0x008C goes out the win clip plays three seconds before the board
void MissionRunScreen::goalReached() {
    if (m_goalSent || m_phase != Phase::Running) return;
    m_goalSent = true;
    m_passed = true;
    const uint32_t id = m_app.session().missionRun().missionId;
    m_app.session().sendMissionGoal(id);
    m_phase = Phase::Success;
    m_phaseTime = 0.f;
    m_sim.setFinished(m_app.session().profile().playerId);
    m_view.setDriverClip(m_viewHandle, KnC::Tools::kDriverSeqWin);
    m_app.sound().stopEngine();
    m_app.sound().play("mission_success");
    m_status = "goal reached, 0x008C sent for mission " + std::to_string(id);
    std::printf("[mission] %s\n", m_status.c_str());
}

// state 4000 the lose clip then the fail board nothing goes out
void MissionRunScreen::failed() {
    if (m_phase != Phase::Running) return;
    m_passed = false;
    m_phase = Phase::Fail;
    m_phaseTime = 0.f;
    m_sim.setFinished(m_app.session().profile().playerId);
    m_view.setDriverClip(m_viewHandle, KnC::Tools::kDriverSeqLose);
    m_app.sound().stopEngine();
    m_status = "failed";
    std::printf("[mission] failed at %.1f s\n", m_runClock);
}

// MSG LOBBY EXIT OK posts 0x7E8 stage 25 answers it with MSG WAIT and C2S 0x008F back to the menu
void MissionRunScreen::leaveRun() {
    m_app.sound().stopEngine();
    m_app.session().openMissionMenu();
    std::printf("[mission] exit box OK sent 0x008F\n");
}

// FUN 0046B240 the popup close goes back to stage 24 with no wire the defs are still on the session
void MissionRunScreen::backToMenu() {
    m_app.sound().stopEngine();
    m_app.go("missions");
}

bool MissionRunScreen::drawScene() {
    if (!m_worldLoaded || !m_view.loaded()) return false;
    if (m_phase >= Phase::Success && m_phase != Phase::ExitBox) m_view.drawFinish(m_app.renderer(), m_chase, m_frameDt);
    else m_view.draw(m_app.renderer(), m_chase, m_frameDt);
    return true;
}

void MissionRunScreen::drawCounter(DrawContext& ctx, int count, int goal) {
    AssetStore& assets = m_app.assets();
    auto glyph = [&](int digit, float x) {
        char name[48];
        std::snprintf(name, sizeof(name), "Mission/Num_%02d.png", digit);
        const Texture* t = assets.texture(name);
        if (t && t->valid()) ctx.batch.draw(t->handle, x, kCounterY, static_cast<float>(t->width), static_cast<float>(t->height));
    };
    count = std::min(std::max(count, 0), 999);
    float x = 0.f;
    if (count / 100 == 0) {
        glyph(count / 10, 10.f);
        glyph(count % 10, 40.f);
        x = 40.f;
    } else {
        glyph(count / 100, 5.f);
        glyph(count % 100 / 10, 35.f);
        glyph(count % 10, 65.f);
        x = 65.f;
    }
    glyph(10, x + 25.f);
    float gx = x + 50.f;
    if (goal / 100 != 0) { glyph(goal / 100, gx); gx = x + 80.f; }
    glyph(goal % 100 / 10, gx);
    glyph(goal % 10, gx + 30.f);
}

void MissionRunScreen::draw(SpriteBatch& batch) {
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    const float w = m_app.canvasWidth();
    const float h = m_app.canvasHeight();
    AssetStore& assets = m_app.assets();
    Session& session = m_app.session();
    const MissionRun& run = session.missionRun();
    const MissionDef* def = session.missionDef(run.missionId);
    auto sprite = [&](const std::string& path, float x, float y) -> const Texture* {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
        return t;
    };
    if (!m_worldLoaded) {
        batch.fill(0.f, 0.f, w, h, rgba(20, 20, 30, 255));
        ctx.font.drawCentered(batch, m_status, w * 0.5f, h * 0.5f - 10.f, 18.f, kWhite);
        ctx.font.drawCentered(batch, "Escape leaves with 0x008F", w * 0.5f, h * 0.5f + 30.f, 14.f, rgba(255, 220, 90, 255));
        return;
    }
    // the time item flash covers the frame under the hud blue for a plus red for a minus
    if (m_flashAlpha > 0.f && m_flashColour >= 0) {
        const Texture* t = assets.texture(m_flashColour == 0 ? "Mission/Mission_1024_768_blue.PNG" : "Mission/Mission_1024_768_red.PNG");
        const uint32_t tint = rgba(255, 255, 255, static_cast<uint8_t>(std::min(m_flashAlpha, 255.f)));
        if (t && t->valid()) batch.draw(t->handle, 0.f, 0.f, w, h, tint);
    }
    HudState s;
    s.kind = HudKind::Mission;
    const CarPose pose = localPose();
    s.speedKmh = pose.speedKmh;
    s.racers = 1;
    s.clock = m_time;
    s.minimap = &m_world.minimap;
    s.missionMap = true;
    s.waiting = m_phase == Phase::Board || m_phase == Phase::Pre;
    if (m_phase == Phase::Countdown) {
        const float left = kCountdownSeconds - m_phaseTime;
        s.countdownStage = m_phaseTime < kCountdownPreroll ? 0 : left > 2.f ? 3 : left > 1.f ? 2 : 1;
        if (s.countdownStage > 0) s.countdownAge = static_cast<double>(static_cast<float>(s.countdownStage) - left);
        s.waiting = s.countdownStage == 0;
    } else if (m_phase == Phase::Running && m_phaseTime < 1.f) {
        s.countdownStage = 4;
        s.countdownAge = m_phaseTime;
    }
    // FUN 00439DB0 the time left of the def and the time items on the nine LapTime glyphs at 400 50
    if (m_phase == Phase::Running && def && def->timeLimitMs > 0) s.licenceTimerMs = std::max(0.0, timeLeftMs());
    if (m_viewHandle >= 0 && m_sim.hasLocal()) {
        HudCar car;
        car.x = pose.x;
        car.y = pose.y;
        car.local = true;
        const DriverRow* driver = session.catalog().driver(session.myDriverKey());
        car.driverAsset = driver && !driver->asset.empty() ? driver->asset : "Cosmo";
        s.cars.push_back(car);
    }
    s.status = m_status;
    if (m_phase >= Phase::Countdown && m_phase < Phase::Success) {
        m_hud.draw(ctx, assets, s, w, h);
        if (m_phase == Phase::Running && def && def->kind == 1) drawCounter(ctx, m_boxes, def->goalCount);
    }
    // the number of a time item zooms in at 482 300 from ten times its size
    if (m_flashType >= 0 && m_flashAge < 1.f) {
        static const char* const kNum[4] = {"Mission/2_blue.PNG", "Mission/5_blue.PNG", "Mission/2_red.PNG", "Mission/5_red.PNG"};
        const Texture* t = assets.texture(kNum[m_flashType]);
        if (t && t->valid()) {
            const float scale = std::max(1.f, 10.f - m_flashAge * 36.f);
            const float tw = static_cast<float>(t->width) * scale;
            const float th = static_cast<float>(t->height) * scale;
            batch.draw(t->handle, 482.f - tw * 0.5f + static_cast<float>(t->width) * 0.5f, 300.f - th * 0.5f + static_cast<float>(t->height) * 0.5f, tw, th);
        }
    }
    if (m_phase == Phase::Board) {
        // mission hud draw 0x4B4350 the board the description the three numbers and the reward icon
        if (!sprite("MissionMenu/dialogue_char_mummy.png", kBoardX, kBoardY))
            batch.fill(kBoardX + 280.f, kBoardY + 50.f, 620.f, 260.f, rgba(255, 200, 40, 240));
        if (def) {
            // the story carries a percent s for the nickname of the player
            std::string story = m_app.tr(def->descKey);
            const size_t at = story.find("%s");
            if (at != std::string::npos) story.replace(at, 2, u16ToUtf8(session.profile().nickname));
            const std::vector<std::string> lines = wrapText(ctx.font, story, 13.f, kBoardTextW);
            float y = kBoardY + kBoardTextDy;
            for (const std::string& line : lines) {
                ctx.font.draw(batch, line, kBoardX + kBoardTextDx, y, 13.f, kInkDark);
                y += 16.f;
                if (y > kBoardY + kBoardTextDy + 140.f) break;
            }
            ctx.bold.draw(batch, std::to_string(m_firstClear ? def->rewardExtra : 0u), kBoardX + kBoardNumDx, kBoardY + kBoardFeeDy, 15.f, kInkDark);
            ctx.bold.draw(batch, std::to_string(def->rewardMileage), kBoardX + kBoardNumDx, kBoardY + kBoardGoldDy, 15.f, kInkDark);
            ctx.bold.draw(batch, std::to_string(def->rewardExp), kBoardX + kBoardNumDx, kBoardY + kBoardExpDy, 15.f, kInkDark);
            const std::string icon = missionRewardIcon(session, def->rewardItemType, def->rewardItemKey);
            if (!icon.empty()) sprite(icon, kBoardX + kBoardIconDx, kBoardY + kBoardIconDy);
        }
    }
    if (m_phase == Phase::Popup && m_passed) {
        // FUN 0046B2B0 the finish board a first clear shows exp gold and icon a replay 0 0
        if (!sprite("MissionMenu/Mission_finish.png", kFinishX, kFinishY))
            batch.fill(kFinishX + 90.f, kFinishY + 180.f, 510.f, 250.f, rgba(255, 200, 40, 240));
        if (def) {
            drawAligned(ctx, ctx.bold, std::to_string(m_firstClear ? def->rewardExp : 0u), kFinishExpX, kFinishNumY, 17.f, kInkDark, Align::Right);
            drawAligned(ctx, ctx.bold, std::to_string(m_firstClear ? def->rewardMileage : 0u), kFinishGoldX, kFinishNumY, 17.f, kInkDark, Align::Right);
            const std::string icon = m_firstClear ? missionRewardIcon(session, def->rewardItemType, def->rewardItemKey) : std::string();
            if (!icon.empty()) sprite(icon, 476.f, 370.f);
        }
    } else if (m_phase == Phase::Popup) {
        // the fail board slides in from the left by 50 px per 60 Hz frame
        const float x = std::min(kFailTo, kFailFrom + m_phaseTime * 3000.f);
        if (!sprite("MissionMenu/Mission_fail.png", x, kFailY)) batch.fill(x, kFailY, 550.f, 507.f, rgba(255, 200, 40, 240));
    }
    if (m_phase == Phase::ExitBox) m_hud.drawInfoBox(ctx, assets, wrapText(ctx.bold, m_app.tr("MSG_LOBBY_EXIT"), 15.f, 320.f), true);
}

void MissionRunScreen::onKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    const bool enter = key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER;
    if (!m_worldLoaded) { if (key == GLFW_KEY_ESCAPE) leaveRun(); return; }
    switch (m_phase) {
    case Phase::Board:
        if (enter) { m_phase = Phase::Pre; m_phaseTime = 0.f; }
        else if (key == GLFW_KEY_ESCAPE) { m_phase = Phase::ExitBox; m_phaseTime = 0.f; }
        break;
    case Phase::Pre:
    case Phase::Countdown:
    case Phase::Running:
        if (key == GLFW_KEY_ESCAPE) { m_phase = Phase::ExitBox; m_phaseTime = 0.f; }
        break;
    case Phase::Popup:
        // FUN 0046B6E0 Enter Escape or Space close the board
        if (enter || key == GLFW_KEY_ESCAPE || key == GLFW_KEY_SPACE) backToMenu();
        break;
    case Phase::ExitBox:
        if (enter) leaveRun();
        else if (key == GLFW_KEY_ESCAPE) m_phase = m_runClock > 0.f ? Phase::Running : Phase::Board;
        break;
    default:
        break;
    }
}

void MissionRunScreen::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    if (m_phase == Phase::Board && kBoardSkip.contains(x, y)) { m_app.click(); m_phase = Phase::Pre; m_phaseTime = 0.f; return; }
    if (m_phase == Phase::Popup && (kFinishEnter.contains(x, y) || kFailEnter.contains(x, y))) { m_app.click(); backToMenu(); return; }
    if (m_phase == Phase::ExitBox) {
        if (RaceHud::pauseRow(0).contains(x, y)) { m_app.click(); leaveRun(); }
        else if (RaceHud::pauseRow(1).contains(x, y)) { m_app.click(); m_phase = m_runClock > 0.f ? Phase::Running : Phase::Board; }
    }
}

}
