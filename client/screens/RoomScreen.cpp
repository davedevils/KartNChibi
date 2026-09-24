#include "RoomScreen.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "assets/NifEmbeddedTextures.h"
#include "engine/render/map_scene.h"
#include "engine/render/nif_prop_model.h"
#include "engine/render/scene_renderer.h"
#include "net/Utf.h"
#include "race/RaceEffects.h"
#include "screens/HelpPopup.h"
#include "screens/MenuPopup.h"
#include "screens/ShopCommon.h"
#include "tools/track_scene/ghost_car.h"
#include "ui/CharPanel.h"
#include "ui/MenuFrame.h"

#include "games/kart/physics/client/motion_packet.h"

#include <GLFW/glfw3.h>
#include <bx/math.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace KnC::Client {

namespace {

// stock room class sub 410CB0 title at 330 10 mode plate at 761 6 lock at 286 0
constexpr float kTitleX = 330.f;
constexpr float kTitleY = 10.f;
constexpr float kModeX = 761.f;
constexpr float kModeY = 6.f;
constexpr float kLockX = 286.f;
constexpr float kLockY = 0.f;
// the player list plate then one row of 25 px per seat under it
constexpr float kRowStep = 25.f;
constexpr float kRowsBelowList = 28.f;
// the track picture at 10 576 its name on 578 the weather at 10 693 the bonus at 262 688
constexpr float kTrackX = 10.f;
constexpr float kTrackY = 576.f;
// the chat box sub 40FD80 at 372 573 of 335 by 150 eight lines the inputs on 735
constexpr Rect kChat = {372.f, 573.f, 335.f, 150.f};
constexpr Rect kChatInput = {542.f, 735.f, 286.f, 20.f};
constexpr Rect kWhisperInput = {380.f, 735.f, 120.f, 20.f};
// the room world of the stock waiting room the karts on its start rows the camera in front
constexpr float kCarGap = 4.4f;
// the stock capture puts the hedge a quarter down and the kart two thirds down so the eye sits low
constexpr float kCameraDistance = 10.f;
constexpr float kCameraHeight = 4.5f;
constexpr float kCameraLookZ = 2.f;
// the field that puts the Floor01 fence band on rows 194 to 234 like the stock capture
constexpr float kRoomFieldRadians = 1.028f;
// the stock eye and look of mode 14 sit this much over our Floor01 rows
constexpr float kRoomFloorRise = 4.11f;
// camera update 0x43F040 mode 13 eye 14 behind the car on minus x 8 4 up look 3 4 up
constexpr float kFollowBack = 14.f;
constexpr float kFollowEyeUp = 8.4f;
constexpr float kFollowLookUp = 3.4f;
// the eye stops at x minus 10 and climbs a fifth of the way the car goes on past it
constexpr float kFollowEyeMaxX = -10.f;
constexpr float kFollowClimb = 0.2f;
// the eye height eases half the gap a 60 Hz frame the other axes snap
constexpr float kFollowHeightEase = 0.5f;
constexpr float kStockFrameSeconds = 1.f / 60.f;
constexpr float kDegToRad = 3.14159265f / 180.f;
// the stock auto start runs thirty seconds from the second seat the digits sit at 5 and 35 on 300
constexpr float kAutoStartSeconds = 30.f;
// the inviting box of the random invite on the stock capture and how long ours keeps it
constexpr float kInviteBoxX = 395.f;
constexpr float kInviteBoxY = 195.f;
constexpr float kInviteBoxSeconds = 3.f;
// the auto walk waits this long after the track ack before the start press
constexpr float kAutoStartDelay = 1.0f;
// the room drive the stock runs cars frame update in stage 9 the report period of 0x0040
constexpr double kRoomMotionPeriod = 0.1;
// FUN 0040e430 the team rows sit 32 px under the team plate the single rows 28 under the list plate
constexpr float kTeamRowsBelowPlate = 32.f;
// FUN 0040c120 the blue column starts at 721 and the red one at 872 in both team modes
constexpr float kTeamBlueX = 721.f;
constexpr float kTeamRedX = 872.f;
constexpr uint32_t kNameSelf = rgba(0, 0, 255, 255);
constexpr uint32_t kNameOther = rgba(0, 0, 255, 255);

const char* modePlate(uint32_t mode) {
    switch (mode) {
    case 0: return "Room/WaitingRoom_Top_ItemSingle.png";
    case 1: return "Room/WaitingRoom_Top_ItemTeam.png";
    case 2: return "Room/WaitingRoom_Top_SpeedSigle.png";
    case 3: return "Room/WaitingRoom_Top_SpeedTeam.png";
    default: return "Room/WaitingRoom_Top_Battle.png";
    }
}

// the player list plate spot by mode as the stock switch places it
void listOrigin(uint32_t mode, float& x, float& y) {
    x = 868.f;
    y = 538.f;
    if (mode == 1) { x = 793.f; y = 609.f; }
    else if (mode == 3) { x = 793.f; y = 509.f; }
    else if (mode == 2) { y = 338.f; }
}

}

void RoomScreen::enter() {
    loadLayout("ui_state_08_room.json");
    AssetStore& assets = m_app.assets();
    // the json lists the per seat sprites at the origin the list draws them by hand
    for (int i = 0; i <= 11; ++i) {
        if (Widget* w = find("image_" + std::to_string(i))) w->visible = false;
    }
    auto back = std::make_unique<ImageWidget>();
    back->type = ElementType::Image;
    back->id = "room_back";
    back->texture = assets.texture("Room/WaitingRoom_Back.png");
    back->rect = {0.f, 0.f, m_app.canvasWidth(), m_app.canvasHeight()};
    back->zIndex = -10;
    add(std::move(back));
    addMenuFrame(*this, assets, FrameMode::Room, "");

    if (Widget* w = find("button_12")) w->action = "pick_track";
    if (Widget* w = find("button_15")) { w->action = "ready"; m_readyButton = dynamic_cast<ButtonWidget*>(w); }
    if (Widget* w = find("button_16")) { w->action = "start"; m_startButton = dynamic_cast<ButtonWidget*>(w); }
    // the JSON lists two blue and two red team buttons the wire team is 0 red 1 blue
    for (const char* id : {"button_13", "button_17"}) if (Widget* w = find(id)) w->action = "team_blue";
    for (const char* id : {"button_14", "button_18"}) if (Widget* w = find(id)) w->action = "team_red";

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
    button("btn_quick_garage", "quick_garage", "Room/WaitingRoom_Top_QuickGarage_", 139.f, 4.f);
    button("btn_invite", "invite", "Room/Random_Invite_", 174.f, 483.f);
    button("btn_chat_mode", "chat_mode", "Room/Lobby_Chat_All", 347.f, 738.f);
    if (ButtonWidget* b = findAs<ButtonWidget>("btn_chat_mode")) {
        b->normal = assets.texture("Room/Lobby_Chat_All.png");
        b->hover = b->normal;
        b->pressed = assets.texture("Room/Lobby_Chat_Whisper.png");
        b->rect = {347.f, 738.f, 23.f, 21.f};
    }
    button("btn_chat_up", "chat_up", "Room/WaitingRoom_Chat_Up_", 688.f, 680.f);
    button("btn_chat_down", "chat_down", "Room/WaitingRoom_Chat_Down_", 688.f, 707.f);

    auto input = std::make_unique<InputWidget>();
    input->type = ElementType::Input;
    input->id = "chat_input";
    input->maxLength = 200;
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
    m_whisperInput = static_cast<InputWidget*>(add(std::move(whisper)));

    m_status.clear();
    m_time = 0.f;
    m_autoTrackSent = false;
    m_autoReadySent = false;
    m_autoStartSent = false;
    m_captured = false;
    m_capturedMore = false;
    m_capturedMembers = 0;
    m_trackAckAt = -1.f;
    m_ready = false;
    m_autoTeamSent = false;
    m_readySoundPlayed = false;
    m_autoStartLeft = -1.f;
    m_simReady = false;
    m_localSpawned = false;
    m_followValid = false;
    m_driveClock = 0.0;
    m_motionAt = 0.0;
    m_teams.clear();
    // the room reads its own 0x0021 0x0064 and 0x0040 so the team space and the other karts stay right
    m_app.setFrameTap([this](uint16_t op, Packet& pkt) { onRoomFrame(op, pkt); });
    // the room world is the decor of 0x0013 as sub 488300 composes it the empty scene stays the fallback
    m_worldLoaded = false;
    m_inviteBoxLeft = -1.f;
    if (!m_app.options().gameDir.empty()) {
        std::string error;
        m_worldLoaded = loadRoomWorld(error);
        if (!m_worldLoaded) std::printf("[room] world failed %s\n", error.c_str());
    }
    m_sceneReady = m_worldLoaded || m_view.loadEmpty(m_app.renderer());
    startRoomDrive();
    refreshButtons();
    refreshCars();
}

namespace {

// the first nif of a folder the object folders name their nif after themselves with a few odd ones
std::string firstNifIn(const std::string& dir) {
    std::error_code ignored;
    if (!std::filesystem::is_directory(dir, ignored)) return std::string();
    for (const auto& entry : std::filesystem::directory_iterator(dir, ignored)) {
        std::string ext = entry.path().extension().string();
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext == ".nif") return entry.path().string();
    }
    return std::string();
}

}

// the old room nifs embed their textures the dds copies land in a cache under the temp folder
std::string embeddedTextureCache(const std::string& nifPath) {
    std::error_code ignored;
    const std::filesystem::path nif(nifPath);
    const std::filesystem::path dir = std::filesystem::temp_directory_path(ignored) / "knc_client" / "nif_textures" / nif.parent_path().filename() / nif.stem();
    return dir.string();
}

// sub 488300 walks the rows kind 0 Sky 1 Floor 2 BgObj 3 Object 4 Effect floor brings the COL
bool RoomScreen::loadRoomWorld(std::string& error) {
    const Session& session = m_app.session();
    const Catalog& cat = session.catalog();
    const std::string root = m_app.options().gameDir + "/Data/Public/World/Room/";
    std::string floor;
    for (const RoomDecor& d : session.room().decor) {
        if (d.category != 1 || d.placed != 1) continue;
        if (const RoomObjectRow* row = cat.roomObject(d.catalogKey)) floor = row->folder;
    }
    // the server always sends a floor row Floor01 is its default for a master who placed none
    if (floor.empty()) floor = "Floor01";
    TrackFiles files;
    files.trackDir = root + "Floor/" + floor;
    files.mapDir = files.trackDir;
    files.themeFolder = "Room";
    files.trackFolder = floor;
    m_world = RaceWorld();
    if (!loadRaceWorld(files, m_world, error)) {
        // the old fallback of this client the Forest folder of the room world
        files.trackDir = root + "Forest";
        files.mapDir = files.trackDir;
        files.trackFolder = "Forest";
        if (!loadRaceWorld(files, m_world, error)) return false;
    }
    KnC::Render::MapScene& scene = m_world.scene.scene;
    // Floor01 and its kin name no dds file the textures sit inside the nif as NiPixelData
    const std::string floorNif = findEntryCi(files.trackDir, "track.nif");
    if (!floorNif.empty()) applyEmbeddedTextures(extractEmbeddedTextures(floorNif, embeddedTextureCache(floorNif)), scene);
    int loaded = 0;
    for (const RoomDecor& d : session.room().decor) {
        if (d.category == 1 || d.placed != 1) continue;
        const RoomObjectRow* row = cat.roomObject(d.catalogKey);
        if (!row) { std::printf("[room] decor key %u has no 0x010C row\n", d.catalogKey); continue; }
        std::string dir;
        std::string nif;
        switch (d.category) {
        case 0: dir = root + "Sky/" + row->folder; nif = findEntryCi(dir, "sky.nif"); break;
        case 2: dir = root + "BgObj/" + row->folder; nif = findEntryCi(dir, "backGround.nif"); break;
        case 3: dir = root + "Object/" + row->folder; nif = findEntryCi(dir, row->folder + ".nif"); if (nif.empty()) nif = firstNifIn(dir); break;
        case 4: dir = root + "Effect/" + row->folder; nif = findEntryCi(dir, "effect.nif"); break;
        default: break;
        }
        if (nif.empty()) { std::printf("[room] decor %s of kind %u has no nif under %s\n", row->folder.c_str(), d.category, dir.c_str()); continue; }
        KnC::Render::NifModelRequest request;
        request.nif_path = nif;
        request.texture_dir = dir;
        KnC::Render::PropModel model;
        std::string modelError;
        if (!KnC::Render::load_prop_model(request, model, modelError)) { std::printf("[room] %s failed %s\n", nif.c_str(), modelError.c_str()); continue; }
        KnC::Tools::resolve_textures(dir, model);
        applyEmbeddedTextures(extractEmbeddedTextures(nif, embeddedTextureCache(nif)), model);
        KnC::Render::PropInstance instance;
        instance.model_index = scene.prop_models.size();
        if (d.category == 3) {
            bx::mtxRotateZ(instance.world, d.yawDeg * kDegToRad);
            instance.world[12] = d.x;
            instance.world[13] = d.y;
            instance.world[14] = d.z;
        }
        if (model.bound.radius > 0.f) {
            const float lo[3] = {instance.world[12] + model.bound.center[0] - model.bound.radius, instance.world[13] + model.bound.center[1] - model.bound.radius, instance.world[14] + model.bound.center[2] - model.bound.radius};
            const float hi[3] = {instance.world[12] + model.bound.center[0] + model.bound.radius, instance.world[13] + model.bound.center[1] + model.bound.radius, instance.world[14] + model.bound.center[2] + model.bound.radius};
            KnC::Render::expand_scene_bounds(lo, scene);
            KnC::Render::expand_scene_bounds(hi, scene);
        }
        std::printf("[room] decor %s kind %u bound %.1f %.1f %.1f radius %.1f\n", row->folder.c_str(), d.category, model.bound.center[0], model.bound.center[1], model.bound.center[2], model.bound.radius);
        scene.prop_models.push_back(std::move(model));
        scene.prop_instances.push_back(instance);
        ++loaded;
    }
    // the room nifs name textures of the shared pool under World Room the walk below the root finds them
    for (KnC::Render::PropModel& model : scene.prop_models) KnC::Tools::resolve_textures(root, model);
    std::printf("[room] world floor %s with %d decor nifs of %zu rows\n", floor.c_str(), loaded, session.room().decor.size());
    return m_view.load(m_app.renderer(), m_world);
}

void RoomScreen::leave() {
    m_app.setFrameTap(nullptr);
    KnC::Render::ViewportRect full;
    full.x = 0;
    full.y = 0;
    full.width = m_app.width();
    full.height = m_app.height();
    m_app.renderer().set_viewport(full);
}

void RoomScreen::refreshButtons() {
    const Session& session = m_app.session();
    const bool master = session.isRoomMaster();
    if (m_readyButton) m_readyButton->visible = !master;
    if (m_startButton) m_startButton->visible = master;
    if (Widget* w = find("button_12")) w->enabled = master;
    if (Widget* w = find("btn_invite")) w->visible = master;
    // FUN 0040f7e0 registers one team pair per mode 13 and 14 on 509 speed 17 and 18 on 609 item
    const uint32_t mode = session.room().gameMode;
    for (const char* id : {"button_13", "button_14"}) {
        if (Widget* w = find(id)) w->visible = mode == 3;
    }
    for (const char* id : {"button_17", "button_18"}) {
        if (Widget* w = find(id)) w->visible = mode == 1;
    }
}

// the room physics the floor col of the decor carries the local car as the stock stage 9 does
void RoomScreen::startRoomDrive() {
    m_simReady = false;
    m_localSpawned = false;
    m_followValid = false;
    if (!m_worldLoaded) return;
    std::string error;
    if (!m_sim.init(m_world, m_app.session().profile().playerId, error)) {
        std::printf("[room] sim init failed %s\n", error.c_str());
        return;
    }
    // the stock never gates the room car on the start light so the engine force runs from the first frame
    m_sim.setSessionRunning(true);
    m_sim.setGreenLight(true);
    m_simReady = true;
}

// 0x0021 and 0x0064 both speak the wire team 0 red 1 blue the room keeps that space for its draw
void RoomScreen::onRoomFrame(uint16_t op, Packet& pkt) {
    if (op == 0x0021) {
        if (pkt.remaining() < 12) return;
        pkt.readUInt32();
        const uint32_t team = pkt.readUInt32();
        const uint32_t id = pkt.readUInt32();
        m_teams[id] = static_cast<int>(team);
        return;
    }
    if (op == 0x0064) {
        if (pkt.remaining() < 8) return;
        const uint32_t id = pkt.readUInt32();
        const uint32_t team = pkt.readUInt32();
        m_teams[id] = static_cast<int>(team);
        return;
    }
    if (op == 0x0022) {
        if (pkt.remaining() >= 4) m_teams.erase(pkt.readUInt32());
        return;
    }
    if (op == 0x0040 && m_simReady) {
        const std::vector<uint8_t>& p = pkt.payload();
        for (const KnC::Kart::Client::MotionRecvEntry& e : KnC::Kart::Client::net_motion_recv_0x40(p.data(), p.size()))
            m_sim.applyMotion(e);
    }
}

int RoomScreen::wireTeam(uint32_t playerId) const {
    const auto it = m_teams.find(playerId);
    return it == m_teams.end() ? -1 : it->second;
}

// the six flags of the room keys the fixed ticks then the report of 0x0040 every tenth
void RoomScreen::driveRoom(float dt) {
    if (!m_simReady || !m_localSpawned) return;
    KnC::Kart::Client::InputFlags flags;
    bool drift = false;
    const bool typing = (m_chatInput && m_chatInput->focused) || (m_whisperInput && m_whisperInput->focused);
    if (!typing) {
        flags.accel = m_app.raceKeyDown(RaceKey::Up) ? 1 : 0;
        flags.brake = m_app.raceKeyDown(RaceKey::Down) ? 1 : 0;
        flags.steerLeft = m_app.raceKeyDown(RaceKey::Left) ? 1 : 0;
        flags.steerRight = m_app.raceKeyDown(RaceKey::Right) ? 1 : 0;
        drift = m_app.raceKeyDown(RaceKey::Drift);
    }
    // KNC ROOM DRIVE holds the gas and a slow right turn so a script can prove the room car moves
    static const bool autoDrive = std::getenv("KNC_ROOM_DRIVE") != nullptr;
    if (autoDrive) {
        flags.accel = 1;
        flags.steerRight = m_driveClock > 3.0 ? 1 : 0;
    }
    m_sim.setLocalInput(flags, drift, false);
    const int ticks = m_sim.advance(dt);
    (void)ticks;
    followCamera(dt);
    const float clock = m_app.renderer().animation().seconds();
    for (Car& c : m_cars) {
        if (c.handle < 0 || c.carIndex < 0) continue;
        const CarPose pose = m_sim.pose(c.carIndex);
        c.x = pose.x; c.y = pose.y; c.z = pose.z;
        m_view.setPose(c.handle, pose, dt, clock);
    }
    if (autoDrive) {
        static double lastLog = -1.0;
        if (m_driveClock - lastLog >= 1.0) {
            lastLog = m_driveClock;
            const CarPose own = m_sim.pose(m_sim.localIndex());
            std::printf("[room] drive %.1f s at %.2f %.2f %.2f yaw %.1f kmh %.1f\n", m_driveClock, own.x, own.y, own.z,
                        own.yawDeg, own.speedKmh);
        }
    }
    // motion send 0x49BEB0 reads the real clock at the send the frame clock bunched two reports after a slow frame
    const double now = App::uptimeMs() / 1000.0;
    if (now - m_motionAt >= kRoomMotionPeriod) {
        m_motionAt = now;
        const std::vector<uint8_t> body = KnC::Kart::Client::net_motion_pack_0x40(m_sim.localMotion());
        Packet p = Packet::fromCmdFull(0x0040);
        p.writeBytes(body.data(), body.size());
        m_app.session().send(p);
    }
}

// one kart per seat in slot order the models come from the catalogue rows of the member keys
void RoomScreen::refreshCars() {
    if (!m_sceneReady) return;
    const Session& session = m_app.session();
    const RoomState& room = session.room();
    std::vector<RoomMember> members = room.members;
    std::stable_sort(members.begin(), members.end(), [](const RoomMember& a, const RoomMember& b) { return a.slot < b.slot; });
    const auto lookOf = [&](const RoomMember& m) {
        std::string token = kartViewModel(m_app, m.kartKey, m.customCar) + "|" + std::to_string(m.driverKey) + " p" +
                            std::to_string(m.petKey);
        for (uint32_t key : m.kartParts) token += " " + std::to_string(key);
        for (uint32_t key : m.accessory) token += " " + std::to_string(key);
        return token;
    };
    bool same = members.size() == m_cars.size();
    for (size_t i = 0; same && i < members.size(); ++i)
        same = members[i].playerId == m_cars[i].playerId && lookOf(members[i]) == m_cars[i].look;
    if (same) return;
    for (Car& c : m_cars) if (c.handle >= 0) m_view.removeCar(c.handle);
    m_cars.clear();
    for (const RoomMember& m : members) {
        Car c;
        c.playerId = m.playerId;
        c.look = lookOf(m);
        const KartRow* kart = session.catalog().kart(m.kartKey);
        const DriverRow* driver = session.catalog().driver(m.driverKey);
        c.model = kart && !kart->model.empty() ? kart->model : std::string("Basic_1");
        c.driver = driver && !driver->asset.empty() ? driver->asset : std::string("Cosmo");
        // the member blobs dress the driver and paint the kart with plate and antenna like the race grid
        const KartLook look = kartLook(m_app, m.kartKey, m.kartParts);
        c.handle = m_view.addCar(m_app.renderer(), m_app.options().gameDir, kartViewModel(m_app, m.kartKey, m.customCar),
                                 c.driver, look.paint, driverParts(m_app, c.driver, m.accessory));
        raceEffects().setLook(c.handle, look.plate, look.antenna);
        // sub 40CC90 loads the worn pet of the 0x0021 row beside the member on the room stand
        if (const PetRow* pet = m.petKey != 0 ? session.catalog().pet(m.petKey) : nullptr) {
            const PetFiles files = petFiles(m_app.options().gameDir, pet->model);
            m_view.setCarPet(m_app.renderer(), c.handle, files.nif, files.facialDir);
        }
        m_cars.push_back(c);
    }
    // the start rows of the room world seat the members the empty scene keeps a row of its own
    const auto& rows = m_world.scene.start_rows;
    const float span = kCarGap * static_cast<float>(m_cars.size() > 0 ? m_cars.size() - 1 : 0);
    const uint32_t self = session.profile().playerId;
    for (size_t i = 0; i < m_cars.size(); ++i) {
        CarPose pose;
        if (m_worldLoaded && i < rows.size()) {
            // FUN 004A05C0 seats the car on the row heading 0 drives toward minus x so the nose faces the eye
            pose.x = rows[i].x; pose.y = rows[i].y; pose.z = rows[i].z; pose.yawDeg = rows[i].heading;
        } else {
            pose.y = -span * 0.5f + kCarGap * static_cast<float>(i);
            pose.yawDeg = 0.f;
        }
        Car& c = m_cars[i];
        c.x = pose.x; c.y = pose.y; c.z = pose.z;
        // the physics keeps its car across a seat change a fresh seat takes a body on its row
        c.carIndex = m_simReady ? m_sim.carIndex(c.playerId) : -1;
        if (m_simReady && c.carIndex < 0) {
            if (c.playerId == self) {
                CarSetup setup;
                setup.playerId = c.playerId;
                for (const RoomMember& m : room.members) {
                    if (m.playerId != c.playerId) continue;
                    if (const KartRow* kart = session.catalog().kart(m.kartKey)) {
                        setup.stats = kart->stats;
                        setup.vehicleKind = static_cast<int>(kart->vehicleKind);
                    }
                }
                setup.carFile = kartCarFile(m_app.options().gameDir, c.model);
                setup.x = pose.x; setup.y = pose.y; setup.z = pose.z; setup.yawDeg = pose.yawDeg;
                std::string error;
                if (m_sim.spawnLocal(setup, error)) {
                    c.carIndex = m_sim.localIndex();
                    m_localSpawned = true;
                } else {
                    std::printf("[room] local car failed %s\n", error.c_str());
                }
            } else {
                c.carIndex = m_sim.spawnRemote(c.playerId, pose.x, pose.y, pose.z, pose.yawDeg);
            }
        }
        if (c.handle >= 0 && c.carIndex < 0) m_view.setPose(c.handle, pose, 0.f, 0.f);
    }
}

// camera mode 14 of FUN 0043ED70 a fixed eye and look the stock floor sits 6 5 above our rows
void RoomScreen::roomCamera(float eye[3], float look[3]) const {
    if (m_followValid) {
        for (int i = 0; i < 3; ++i) { eye[i] = m_followEye[i]; look[i] = m_followLook[i]; }
        return;
    }
    eye[0] = -31.5f; eye[1] = 10.3f; eye[2] = 13.3f - kRoomFloorRise;
    look[0] = -17.5f; look[1] = 10.3f; look[2] = 8.3f - kRoomFloorRise;
}

// mode 13 at the start row gives the mode 14 numbers so the first frame does not jump
void RoomScreen::followCamera(float dt) {
    if (!m_simReady || !m_localSpawned) {
        m_followValid = false;
        return;
    }
    const CarPose own = m_sim.pose(m_sim.localIndex());
    float x = own.x - kFollowBack;
    float climb = 0.f;
    if (x > kFollowEyeMaxX) {
        climb = (x - kFollowEyeMaxX) * kFollowClimb;
        x = kFollowEyeMaxX;
    }
    const float eyeZ = own.z + kFollowEyeUp + climb;
    if (!m_followValid) {
        m_followEye[2] = eyeZ;
    } else {
        const float blend = 1.f - std::pow(1.f - kFollowHeightEase, dt / kStockFrameSeconds);
        m_followEye[2] += (eyeZ - m_followEye[2]) * blend;
    }
    m_followEye[0] = x;
    m_followEye[1] = own.y;
    m_followLook[0] = own.x;
    m_followLook[1] = own.y;
    m_followLook[2] = own.z + kFollowLookUp;
    m_followValid = true;
}

bool RoomScreen::project(const float world[3], float& sx, float& sy) const {
    // the view of the last scene draw and the lens of the frame as the race hud projects its tags
    float eye[3], view[16], proj[16];
    m_view.lastView(view, eye);
    m_app.renderer().projection(proj);
    float v[4] = {world[0], world[1], world[2], 1.f};
    float e[4], c[4];
    bx::vec4MulMtx(e, v, view);
    bx::vec4MulMtx(c, e, proj);
    if (c[3] <= 0.001f) return false;
    const float nx = c[0] / c[3];
    const float ny = c[1] / c[3];
    // the sprite canvas is letterboxed in the window the same way the batch maps it
    const float scale = std::min(static_cast<float>(m_app.width()) / m_app.canvasWidth(), static_cast<float>(m_app.height()) / m_app.canvasHeight());
    const float offX = (static_cast<float>(m_app.width()) - m_app.canvasWidth() * scale) * 0.5f;
    const float offY = (static_cast<float>(m_app.height()) - m_app.canvasHeight() * scale) * 0.5f;
    const float px = (nx * 0.5f + 0.5f) * static_cast<float>(m_app.width());
    const float py = (0.5f - ny * 0.5f) * static_cast<float>(m_app.height());
    sx = (px - offX) / scale;
    sy = (py - offY) / scale;
    return true;
}

bool RoomScreen::drawScene() {
    if (!m_sceneReady || m_cars.empty()) return false;
    KnC::Render::ViewportRect full;
    full.x = 0;
    full.y = 0;
    full.width = m_app.width();
    full.height = m_app.height();
    const KnC::Render::ViewportRect& now = m_app.renderer().viewport();
    if (now.x != full.x || now.y != full.y || now.width != full.width || now.height != full.height)
        m_app.renderer().set_viewport(full);
    float eye[3], look[3];
    roomCamera(eye, look);
    m_view.drawFixed(m_app.renderer(), eye, look, kRoomFieldRadians, 1.f / 25.f);
    return true;
}

// the stock floats a name tag over every kart a dark plate the level flag and the name in white
void RoomScreen::drawNamePlates(DrawContext& ctx) {
    const RoomState& room = m_app.session().room();
    AssetStore& assets = m_app.assets();
    for (const Car& c : m_cars) {
        const RoomMember* member = nullptr;
        for (const RoomMember& m : room.members) if (m.playerId == c.playerId) member = &m;
        if (!member) continue;
        const float head[3] = {c.x, c.y, c.z + 2.4f};
        float sx = 0.f, sy = 0.f;
        if (!project(head, sx, sy)) continue;
        const std::string name = u16ToUtf8(member->name);
        const float w = ctx.bold.measure(name, 14.f) + 12.f;
        const float x = std::floor(sx - w * 0.5f + 12.f);
        const float y = std::floor(sy - 11.f);
        ctx.batch.fill(x, y, w, 22.f, rgba(40, 40, 40, 170));
        char badge[48];
        std::snprintf(badge, sizeof(badge), "Icon/lv_icon_s_%03d.png", std::min(std::max(static_cast<int>(member->level) + 1, 1), 50));
        const Texture* level = assets.texture(badge);
        if (level && level->valid()) ctx.batch.draw(level->handle, x - 24.f, y - 6.f, static_cast<float>(level->width), static_cast<float>(level->height));
        // sub 499180 the worn pendant of the 0x0021 row Icon base s left of the level flag
        if (const PendantDef* p = m_app.session().pendantDef(member->pendantKey)) {
            const Texture* icon = assets.texture("Icon/" + p->iconBase + "_s.png");
            if (icon && icon->valid()) ctx.batch.draw(icon->handle, x - 50.f, y - 5.f, static_cast<float>(icon->width), static_cast<float>(icon->height));
        }
        ctx.bold.draw(ctx.batch, name, x + 6.f, y + 3.f, 14.f, kInkWhite);
    }
}

std::string RoomScreen::trackName(int trackId) const {
    const Catalog& cat = m_app.session().catalog();
    const TrackRow* row = cat.track(static_cast<uint32_t>(trackId));
    if (!row) return "track " + std::to_string(trackId);
    const std::string& name = m_app.tr(row->nameKey);
    return name.empty() || name == row->nameKey ? row->folder : name;
}

void RoomScreen::update(float dt) {
    if (m_inviteBoxLeft > 0.f) m_inviteBoxLeft -= dt;
    m_time += dt;
    m_driveClock += static_cast<double>(dt);
    driveRoom(dt);
    Session& session = m_app.session();
    const Options& o = m_app.options();
    const RoomState& room = session.room();
    if (!room.inRoom) return;
    const int autoTrack = o.autoRaceTrack > 0 ? o.autoRaceTrack : o.autoCreateTrack;

    // the room capture waits a moment so the member rows and the art are in
    if (!m_captured && m_app.captureMode() && m_time > 0.8f && !room.members.empty()) {
        m_captured = true;
        m_capturedMembers = room.members.size();
        m_app.captureStage("room");
        if (o.stopAt == "room") m_app.finishRun();
        // the two popup states open their box on the room and take its picture
        if (o.stopAt == "trackpick") { openTrackPick(); m_app.captureStage("trackpick"); m_app.finishRun(); }
        if (o.stopAt == "inviting") { m_inviteBoxLeft = 10.f; m_app.captureStage("inviting"); m_app.finishRun(); }
    }
    // a second seat filled after the first capture is worth one more picture
    if (m_captured && !m_capturedMore && m_app.captureMode() && room.members.size() > m_capturedMembers && m_time > 1.f) {
        m_capturedMore = true;
        m_app.captureStage("room2");
    }

    if (o.team >= 0 && !m_autoTeamSent && m_time > 0.6f) {
        m_autoTeamSent = true;
        session.selectTeam(static_cast<uint32_t>(o.team));
        m_status = "auto team 0x0064 " + std::string(o.team == 0 ? "red" : "blue");
    }
    if (room.allReady && !m_readySoundPlayed) {
        m_readySoundPlayed = true;
        m_app.sound().play("button_ready_snd");
    }
    // the stock counts thirty seconds once a second seat fills the master sees the digits
    if (room.members.size() >= 2 && session.isRoomMaster()) {
        if (m_autoStartLeft < 0.f) m_autoStartLeft = kAutoStartSeconds;
        else m_autoStartLeft = std::max(0.f, m_autoStartLeft - dt);
    } else {
        m_autoStartLeft = -1.f;
    }
    if (session.isRoomMaster() && autoTrack > 0 && !m_autoTrackSent && m_time > 0.3f) {
        m_autoTrackSent = true;
        session.selectTrack(static_cast<uint32_t>(autoTrack), 0);
        m_status = "auto picked track " + std::to_string(autoTrack);
    }
    if (o.autoJoin && !session.isRoomMaster() && !m_autoReadySent && m_time > 0.5f) {
        m_autoReadySent = true;
        m_ready = true;
        session.sendReadyToggle(true);
        m_status = "auto ready";
    }
    if (o.autoRaceTrack > 0 && session.isRoomMaster() && !m_autoStartSent && m_trackAckAt >= 0.f &&
        room.trackId == o.autoRaceTrack && m_time - m_trackAckAt > kAutoStartDelay + static_cast<float>(o.waitSeconds)) {
        m_autoStartSent = true;
        session.sendReadyToggle(true);
        m_status = "auto start pressed 0x0033";
    }
}

// sub 411470 Escape opens the Menu box in the room too the Exit button leaves it
void RoomScreen::onKey(int key, int action, int mods) {
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        if (m_chatInput && m_chatInput->focused) focus(nullptr);
        m_app.pushScreen(std::make_unique<MenuPopup>(m_app));
        return;
    }
    if (action == GLFW_PRESS && key == GLFW_KEY_F1) { m_app.pushScreen(std::make_unique<HelpPopup>(m_app)); return; }
    if (action == GLFW_PRESS && (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) && m_chatInput && !m_chatInput->focused) {
        focus(m_chatInput);
        return;
    }
    WidgetScreen::onKey(key, action, mods);
}

void RoomScreen::onAction(const std::string& action, Widget& source) {
    Session& session = m_app.session();
    if (action == "pick_track") {
        if (!session.isRoomMaster()) { m_status = "only the master picks the track"; return; }
        openTrackPick();
    } else if (action == "ready") {
        m_ready = !m_ready;
        session.sendReadyToggle(m_ready);
        m_status = m_ready ? "ready sent" : "ready cleared";
    } else if (action == "start") {
        session.sendReadyToggle(true);
        m_status = "start pressed 0x0033";
    } else if (action == "exit" || action == "lobby") {
        exitRoom();
    } else if (action == "team_red" || action == "team_blue") {
        const uint32_t mode = session.room().gameMode;
        if (mode != 1 && mode != 3) { m_status = "teams exist in the item team and speed team modes only"; return; }
        const uint32_t team = action == "team_red" ? 0u : 1u;
        session.selectTeam(team);
        m_status = std::string("team 0x0064 ") + (team == 0 ? "red" : "blue") + " sent";
    } else if (action == "chat_mode") {
        if (m_whisperInput) focus(m_whisperInput);
    } else if (action == "chat_up" || action == "chat_down") {
        return;
    } else if (action == "invite") {
        // 0x012F the random invite of the master the stock shows its inviting box for a while after
        if (!session.isRoomMaster()) { m_status = "only the master invites"; return; }
        session.requestRandomInvite();
        m_inviteBoxLeft = kInviteBoxSeconds;
        m_status = "random invite 0x012F sent";
    } else if (action == "quick_garage") {
        // the stock top bar of the room opens the garage with the same C2S 0x000F the frame button sends
        session.openGarage();
        m_status = "quick garage 0x000F sent";
    } else if (action == "messenger") {
        m_status = source.id + " is not part of this phase";
    } else if (frameAction(m_app, action)) {
        return;
    } else {
        m_status = source.id + " is not part of this phase";
    }
}

void RoomScreen::exitRoom() {
    m_status = "leaving 0x0012";
    m_app.session().openLobby();
}

void RoomScreen::openTrackPick() {
    m_app.pushScreen(std::make_unique<TrackPickPopup>(m_app));
}

void RoomScreen::sendChat() {
    if (!m_chatInput || m_chatInput->text.empty()) return;
    std::u16string line = utf8ToU16(m_chatInput->text);
    if (m_whisperInput && !m_whisperInput->text.empty()) line = u"/w " + utf8ToU16(m_whisperInput->text) + u" " + line;
    m_app.session().sendChat(line);
    m_chatInput->text.clear();
}

void RoomScreen::onSession(SessionEvent event) {
    if (event == SessionEvent::RoomEnter || event == SessionEvent::RoomChanged) {
        refreshButtons();
        refreshCars();
    }
    if (event == SessionEvent::RoomTrack) {
        const int autoTrack = m_app.options().autoRaceTrack > 0 ? m_app.options().autoRaceTrack : m_app.options().autoCreateTrack;
        if (autoTrack <= 0 || m_app.session().room().trackId == autoTrack) m_trackAckAt = m_time;
    }
}

void RoomScreen::drawOverlay(DrawContext& ctx) {
    Session& session = m_app.session();
    const RoomState& room = session.room();
    AssetStore& assets = m_app.assets();
    auto sprite = [&](const std::string& path, float x, float y) {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) ctx.batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
    };

    drawNamePlates(ctx);
    // the title line font 7 the mode plate the lock state the room number prints plus one
    char title[128];
    std::snprintf(title, sizeof(title), "[%03u] %s  (%zu/%u)", room.roomId + 1, u16ToUtf8(room.name).c_str(), room.members.size(), room.maxPlayers);
    ctx.bold.draw(ctx.batch, title, kTitleX, kTitleY, 17.f, rgba(0, 0, 0, 240));
    sprite(modePlate(room.gameMode), kModeX, kModeY);
    sprite(room.hasPassword ? "Room/lock_00.png" : "Room/unlock_00.png", kLockX, kLockY);

    // the player list plate and its rows the team modes split the seats on two columns
    float lx = 0.f, ly = 0.f;
    listOrigin(room.gameMode, lx, ly);
    const bool teamMode = room.gameMode == 1 || room.gameMode == 3;
    sprite("Room/playerList.png", lx, ly);
    std::vector<RoomMember> members = room.members;
    std::stable_sort(members.begin(), members.end(), [](const RoomMember& a, const RoomMember& b) { return a.slot < b.slot; });
    // FUN 0040e430 the first team row sits 32 px under the team plate so the rows clear it
    float yRed = ly + (teamMode ? kTeamRowsBelowPlate : kRowsBelowList);
    float yBlue = yRed;
    float ySingle = ly + kRowsBelowList;
    for (const RoomMember& m : members) {
        float x = 872.f;
        float y = ySingle;
        // 0x0021 and 0x0064 both carry the wire team 0 red 1 blue the room map keeps that space
        const int team = wireTeam(m.playerId);
        const bool blue = team == 1;
        if (teamMode) {
            x = blue ? kTeamBlueX : kTeamRedX;
            y = blue ? yBlue : yRed;
            if (blue) yBlue += kRowStep; else yRed += kRowStep;
        } else {
            ySingle += kRowStep;
        }
        // row plate player back 00 the info icon at plus 109 the flag or the kick X at plus 128
        sprite(teamMode ? (blue ? "Room/player_back_02.png" : "Room/player_back_01.png") : "Room/player_back_00.png", x, y - 3.f);
        const int ping = 0;
        sprite(ping < 150 ? "Icon/ping_good.png" : ping < 300 ? "Icon/ping_normal.png" : "Icon/ping_bad.png", x + 3.f, y + 1.f);
        const bool self = m.playerId == session.profile().playerId;
        ctx.font.draw(ctx.batch, u16ToUtf8(m.name), x + 24.f, y + 2.f, 14.f, self ? kNameSelf : kNameOther);
        if (m.playerId != room.masterPlayerId && m.ready != 0) sprite("Room/UI_Waitingroom_player_ready.png", x + 25.f, y - 1.f);
        sprite("Room/UI_Waitingroom_player_info_00.png", x + 109.f, y - 3.f);
        if (m.playerId == room.masterPlayerId) sprite("Room/UI_Waitingroom_player_master.png", x + 128.f, y - 4.f);
        else if (session.isRoomMaster()) sprite("Room/UI_Waitingroom_player_out_00.png", x + 128.f, y + 4.f);
    }
    if (m_autoStartLeft >= 0.f) {
        const int left = static_cast<int>(std::ceil(m_autoStartLeft));
        char digit[48];
        std::snprintf(digit, sizeof(digit), "Room/Num_%02d.png", std::min(left / 10, 9));
        sprite(digit, 5.f, 300.f);
        std::snprintf(digit, sizeof(digit), "Room/Num_%02d.png", left % 10);
        sprite(digit, 35.f, 300.f);
    }

    // the track picture with its name the weather and the bonus icon
    const TrackRow* track = session.catalog().track(static_cast<uint32_t>(room.trackId));
    if (track) {
        sprite("Popup/SelectTrack/track_" + track->folder + ".png", kTrackX, kTrackY);
        const std::string name = trackName(room.trackId);
        ctx.bold.draw(ctx.batch, name, kTrackX + 2.f, kTrackY + 2.f, 17.f, rgba(16, 16, 16, 255));
        ctx.bold.draw(ctx.batch, name, kTrackX + 3.f, kTrackY + 3.f, 17.f, rgba(250, 240, 72, 224));
    } else {
        ctx.font.draw(ctx.batch, "no track picked", kTrackX + 20.f, kTrackY + 80.f, 14.f, kInkGrey);
    }
    sprite(room.trackWeather == 1 ? "Popup/SelectTrack/Track_Weather_Night_00.PNG"
           : room.trackWeather == 2 ? "Popup/SelectTrack/Track_Weather_Rain_00.png" : "Popup/SelectTrack/Track_Weather_Sun_00.png", 10.f, 693.f);
    // the stock Track button sits over the picture so it draws again after it
    if (Widget* w = find("button_12")) if (w->visible) w->draw(ctx);

    // chat lines bottom up in the dark box
    ctx.batch.setClip(kChat.x, kChat.y, kChat.w, kChat.h);
    const float lineH = 17.f;
    float y = kChat.y + kChat.h - lineH - 4.f;
    const std::vector<ChatLine>& chat = session.chat();
    for (size_t i = chat.size(); i > 0 && y > kChat.y - lineH; --i) {
        const ChatLine& line = chat[i - 1];
        std::string text;
        uint32_t colour = kWhite;
        if (line.type == 0) text = u16ToUtf8(line.sender) + " : " + u16ToUtf8(line.text);
        else if (line.type == 3) { text = u16ToUtf8(line.text); colour = rgba(255, 64, 64, 255); }
        // sub 47EC00 prints the resolved key alone the name rides the wire unread
        else if (line.type == 5) { text = m_app.tr(u16ToUtf8(line.text)); colour = rgba(64, 255, 64, 255); }
        else if (line.type == 6) { text = (line.outgoing ? "To " : "From ") + u16ToUtf8(line.sender) + " : " + u16ToUtf8(line.text); colour = rgba(255, 150, 240, 255); }
        else text = u16ToUtf8(line.text);
        ctx.font.drawClipped(ctx.batch, text, kChat.x + 8.f, y, kChat.w - 16.f, 15.f, colour);
        y -= lineH;
    }
    ctx.batch.clearClip();
    // the inviting box of the stock capture at 395 195 the magnifier over its bottom at 540 318
    if (m_inviteBoxLeft > 0.f) {
        const Texture* box = assets.texture("Room/Random_Invitewaiting_00.png");
        if (box && box->valid()) sprite("Room/Random_Invitewaiting_00.png", kInviteBoxX, kInviteBoxY);
        else {
            ctx.batch.fill(kInviteBoxX, kInviteBoxY, 300.f, 180.f, rgba(70, 90, 170, 235));
            drawAligned(ctx, ctx.bold, "Inviting ...", kInviteBoxX + 150.f, kInviteBoxY + 18.f, 20.f, kInkWhite, Align::Centre);
            drawAligned(ctx, ctx.bold, "Invitation has been sent.", kInviteBoxX + 150.f, kInviteBoxY + 84.f, 13.f, kInkWhite, Align::Centre);
            drawAligned(ctx, ctx.bold, "Please wait.", kInviteBoxX + 150.f, kInviteBoxY + 100.f, 13.f, kInkWhite, Align::Centre);
        }
        sprite("Room/Random_Invitewaiting_01.png", kInviteBoxX + 145.f, kInviteBoxY + 123.f);
    }
    if (!m_status.empty() && m_app.options().statusLine) ctx.font.draw(ctx.batch, m_status, 372.f, 556.f, 12.f, kInkGrey);
}

namespace {

// sub 474A30 back at 124 120 thumbs at plus 77 every 154 on 67 picture at plus 56 227
constexpr float kPickX = 124.f;
constexpr float kPickY = 120.f;
constexpr float kPickW = 754.f;
constexpr float kPickH = 515.f;
constexpr float kThemeX = 77.f;
constexpr float kThemeStep = 154.f;
constexpr float kThemeY = 67.f;
constexpr float kThemeW = 138.f;
constexpr float kThemeH = 83.f;
constexpr Rect kThemeLeft = {kPickX + 18.f, kPickY + 62.f, 30.f, 88.f};
constexpr Rect kThemeRight = {kPickX + 700.f, kPickY + 62.f, 30.f, 88.f};
constexpr Rect kTrackLeft = {kPickX + 14.f, kPickY + 219.f, 30.f, 241.f};
constexpr Rect kTrackRight = {kPickX + 401.f, kPickY + 219.f, 30.f, 241.f};
constexpr Rect kOk = {kPickX + 282.f, kPickY + 478.f, 94.f, 28.f};
constexpr Rect kCancel = {kPickX + 377.f, kPickY + 478.f, 94.f, 28.f};
constexpr float kBigX = 56.f;
constexpr float kBigY = 227.f;
constexpr float kBigW = 328.f;
constexpr float kBigH = 186.f;
// the weather radio at plus 475 557 639 on 381 the pick rides the 0x0035 tail
constexpr float kWeatherX[3] = {475.f, 557.f, 639.f};
constexpr float kWeatherY = 381.f;
constexpr float kWeatherW = 64.f;
constexpr float kWeatherH = 64.f;
const char* const kWeatherArt[3] = {"Popup/SelectTrack/Track_Weather_Sun_", "Popup/SelectTrack/Track_Weather_Night_",
                                    "Popup/SelectTrack/Track_Weather_Rain_"};

}

// the themes of 0x00C4 that own a pickable track the picked one opens on the current room track
TrackPickPopup::TrackPickPopup(App& app) : m_app(app) {
    const std::vector<const TrackRow*> rows = m_app.session().catalog().pickableTracks();
    const int current = m_app.session().room().trackId;
    for (const TrackRow* row : rows) {
        bool known = false;
        for (uint32_t t : m_themes) known = known || t == row->themeId;
        if (!known) m_themes.push_back(row->themeId);
    }
    for (size_t i = 0; i < rows.size(); ++i) {
        if (static_cast<int>(rows[i]->trackId) != current) continue;
        for (size_t t = 0; t < m_themes.size(); ++t) if (m_themes[t] == rows[i]->themeId) m_theme = static_cast<int>(t);
        m_index = 0;
        for (const TrackRow* row : rows) {
            if (row->themeId != rows[i]->themeId) continue;
            if (row->trackId == rows[i]->trackId) break;
            ++m_index;
        }
    }
    if (m_theme >= 4) m_first = m_theme - 3;
    const uint32_t weather = m_app.session().room().trackWeather;
    if (weather <= 2) m_weather = static_cast<int>(weather);
}

std::vector<const TrackRow*> TrackPickPopup::themeTracks() const {
    std::vector<const TrackRow*> out;
    if (m_theme < 0 || m_theme >= static_cast<int>(m_themes.size())) return out;
    for (const TrackRow* row : m_app.session().catalog().pickableTracks())
        if (row->themeId == m_themes[static_cast<size_t>(m_theme)]) out.push_back(row);
    return out;
}

void TrackPickPopup::draw(SpriteBatch& batch) {
    const FontAtlas& font = m_app.font();
    AssetStore& assets = m_app.assets();
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    auto sprite = [&](const std::string& path, float x, float y) -> const Texture* {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
        return t;
    };
    if (!sprite("Popup/SelectTrack/Track_Back.png", kPickX, kPickY)) batch.fill(kPickX, kPickY, kPickW, kPickH, rgba(60, 60, 80, 240));
    const Catalog& cat = m_app.session().catalog();
    if (m_themes.empty()) {
        font.drawCentered(batch, "no 0x00C3 track rows", kPickX + 377.f, kPickY + 240.f, 18.f, kWhite);
        return;
    }
    // the theme strip four thumbs the picked one wears the select frame
    for (int k = 0; k < 4; ++k) {
        const int t = m_first + k;
        if (t < 0 || t >= static_cast<int>(m_themes.size())) continue;
        const ThemeRow* theme = cat.theme(m_themes[static_cast<size_t>(t)]);
        const float tx = kPickX + kThemeX + kThemeStep * static_cast<float>(k);
        const Texture* thumb = theme ? assets.texture("Popup/SelectTrack/thema_" + theme->folder + ".png") : nullptr;
        if (thumb && thumb->valid()) batch.draw(thumb->handle, tx, kPickY + kThemeY, kThemeW, kThemeH);
        else batch.fill(tx, kPickY + kThemeY, kThemeW, kThemeH, rgba(90, 120, 90, 255));
        if (t == m_theme) sprite("Popup/SelectTrack/theme_select.png", tx - 4.f, kPickY + kThemeY - 4.f);
    }
    sprite("Popup/SelectTrack/Common_Top_Left_00.png", kThemeLeft.x, kThemeLeft.y);
    sprite("Popup/SelectTrack/Common_Top_Right_00.png", kThemeRight.x, kThemeRight.y);
    const ThemeRow* theme = cat.theme(m_themes[static_cast<size_t>(m_theme)]);
    drawAligned(ctx, ctx.bold, theme ? m_app.tr(theme->nameKey) : std::string("theme"), kPickX + 377.f, kPickY + 167.f, 14.f, kInkBlack, Align::Centre);
    // the track of the theme its big picture its star row its name and the info text
    const std::vector<const TrackRow*> rows = themeTracks();
    if (rows.empty()) return;
    if (m_index < 0) m_index = 0;
    if (m_index >= static_cast<int>(rows.size())) m_index = static_cast<int>(rows.size()) - 1;
    const TrackRow* row = rows[static_cast<size_t>(m_index)];
    const Texture* preview = assets.texture("Popup/SelectTrack/track_" + row->folder + ".png");
    if (preview && preview->valid()) batch.draw(preview->handle, kPickX + kBigX, kPickY + kBigY, kBigW, kBigH);
    else batch.fill(kPickX + kBigX, kPickY + kBigY, kBigW, kBigH, rgba(90, 100, 120, 255));
    for (int i = 0; i < 5; ++i)
        sprite(i < static_cast<int>(row->difficulty) ? "Popup/SelectTrack/track_level_star_01.png" : "Popup/SelectTrack/track_level_star_00.png",
               kPickX + 61.f + 27.f * static_cast<float>(i), kPickY + 387.f);
    sprite("Popup/SelectTrack/Common_Bottom_Left_00.png", kTrackLeft.x, kTrackLeft.y);
    sprite("Popup/SelectTrack/Common_Bottom_Right_00.png", kTrackRight.x, kTrackRight.y);
    drawAligned(ctx, ctx.bold, m_app.tr(row->nameKey), kPickX + 221.f, kPickY + 430.f, 14.f, kInkBlack, Align::Centre);
    const std::vector<std::string> lines = wrapText(font, m_app.tr(row->nameKey + "_INFO"), 13.f, 290.f);
    float ty = kPickY + 238.f;
    for (const std::string& line : lines) {
        if (ty > kPickY + 330.f) break;
        font.draw(batch, line, kPickX + 442.f, ty, 13.f, kInkDark);
        ty += 16.f;
    }
    // the picked icon wears the 01 frame the others the 00 one the night file is spelled PNG
    for (int i = 0; i < 3; ++i) {
        const std::string tail = i == m_weather ? "01" : "00";
        if (!sprite(std::string(kWeatherArt[i]) + tail + ".png", kPickX + kWeatherX[i], kPickY + kWeatherY))
            sprite(std::string(kWeatherArt[i]) + tail + ".PNG", kPickX + kWeatherX[i], kPickY + kWeatherY);
    }
    sprite("Buttons/Common_OK_half_00.png", kOk.x, kOk.y);
    sprite("Buttons/Common_Cancel_half_00.png", kCancel.x, kCancel.y);
}

// the top arrows scroll the theme window the bottom ones step the track of the theme
void TrackPickPopup::stepTheme(int delta) {
    if (m_themes.empty()) return;
    const int n = static_cast<int>(m_themes.size());
    m_theme = std::min(std::max(m_theme + delta, 0), n - 1);
    if (m_theme < m_first) m_first = m_theme;
    if (m_theme > m_first + 3) m_first = m_theme - 3;
    m_index = 0;
}

void TrackPickPopup::step(int delta) {
    const size_t n = themeTracks().size();
    if (n == 0) return;
    m_index = (m_index + delta + static_cast<int>(n)) % static_cast<int>(n);
}

void TrackPickPopup::confirm() {
    const std::vector<const TrackRow*> rows = themeTracks();
    if (!rows.empty() && m_index >= 0 && m_index < static_cast<int>(rows.size())) {
        // 0x0035 carries the track then the weather word 0 sun 1 night 2 rain
        m_app.session().selectTrack(rows[static_cast<size_t>(m_index)]->trackId, static_cast<uint32_t>(m_weather));
    }
    close();
}

void TrackPickPopup::close() {
    if (m_closed) return;
    m_closed = true;
    m_app.popScreen();
}

void TrackPickPopup::onKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_LEFT) step(-1);
    else if (key == GLFW_KEY_RIGHT) step(1);
    else if (key == GLFW_KEY_UP) stepTheme(-1);
    else if (key == GLFW_KEY_DOWN) stepTheme(1);
    else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) confirm();
    else if (key == GLFW_KEY_ESCAPE) close();
}

void TrackPickPopup::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    if (kThemeLeft.contains(x, y)) { m_app.click(); if (m_first > 0) --m_first; return; }
    if (kThemeRight.contains(x, y)) { m_app.click(); if (m_first + 4 < static_cast<int>(m_themes.size())) ++m_first; return; }
    for (int k = 0; k < 4; ++k) {
        const Rect thumb = {kPickX + kThemeX + kThemeStep * static_cast<float>(k), kPickY + kThemeY, kThemeW, kThemeH};
        if (!thumb.contains(x, y)) continue;
        const int t = m_first + k;
        if (t >= 0 && t < static_cast<int>(m_themes.size())) { m_app.click(); m_theme = t; m_index = 0; }
        return;
    }
    if (kTrackLeft.contains(x, y)) { m_app.click(); step(-1); return; }
    if (kTrackRight.contains(x, y)) { m_app.click(); step(1); return; }
    for (int i = 0; i < 3; ++i) {
        const Rect icon = {kPickX + kWeatherX[i], kPickY + kWeatherY, kWeatherW, kWeatherH};
        if (!icon.contains(x, y)) continue;
        m_app.click();
        m_weather = i;
        return;
    }
    if (kOk.contains(x, y)) { m_app.click(); confirm(); return; }
    if (kCancel.contains(x, y)) { m_app.click(); close(); return; }
    if (x < kPickX || x > kPickX + kPickW || y < kPickY || y > kPickY + kPickH) close();
}

}
