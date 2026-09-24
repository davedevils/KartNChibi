#include "RaceScreen.h"

#include "race/LookBack.h"

#include "app/App.h"
#include "engine/render/nif_prop_model.h"
#include "engine/render/scene_renderer.h"
#include "engine/render/texture_cache.h"
#include "games/kart/physics/client/world_collision.h"
#include "race/RaceEffects.h"
#include "net/Utf.h"

#include <GLFW/glfw3.h>
#include <bx/math.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <thread>

namespace KnC::Client {

using KnC::Kart::Client::InputFlags;
using KnC::Kart::Client::MotionRecvEntry;

namespace {

// a real kart first moves 9 5 to 11 5 seconds after the GO the green light comes at ten
constexpr float kCountdownSeconds = 10.f;
// FUN 0043ed70 case 0xf the eye stands 9 units to the side of the first row and 5 above it
constexpr float kIntroSide = 9.f;
constexpr float kIntroLift = 5.f;
// 0x43fad2 the eye and the look point walk the grid line by 0 13 units a frame
constexpr float kIntroRatePerFrame = 0.13f;
// the fly over ends when the look point comes back inside 3 units of the first row 0x5a32b8
constexpr float kIntroReturnReach = 3.f;
// FUN 004024f0 states 3 and 4 hold the front view 1500 then 500 ms before the chase camera
constexpr float kIntroFrontSeconds = 2.f;
// the intro must hand over before the three two one so the pan gets the rest of the ten seconds
constexpr float kIntroHandoverSeconds = 7.f;
// camera fov update 0x43e958 leaves the modes 15 and 16 on the speed formula so at rest the base field
constexpr float kIntroFieldRadians = 1.05f;
// 0x5a3230 mode 16 stands in front of the car the same distance the mode 9 finish camera keeps
constexpr float kIntroFrontDistance = 9.f;
constexpr float kIntroFrontHeight = 3.5f;
// the 0x0040 report period
constexpr double kMotionPeriod = 0.1;
// an item box is taken inside this reach and comes back after the cooldown the server bots use 6 0
constexpr float kBoxReach = 6.f;
constexpr float kBoxCooldown = 6.f;
// the kinds the auto driver rolls the icons exist for them
constexpr int32_t kItemKinds[] = {3, 6, 10, 14};
// the effect codes the client keeps every other code is dropped
constexpr int kEffectCodes[] = {100, 200, 300, 700, 1000};
// the mid race capture comes this long after the green light
constexpr float kRaceCaptureSeconds = 20.f;
// FUN 00401D90 state 2010 holds the finish camera 6000 ms after the board then FUN 0043D7E0 fades 300 ms
constexpr float kFinishHoldSeconds = 6.f;
constexpr float kFadeSeconds = 0.3f;
// the finish camera capture one second in the podium capture after the third kart landed
constexpr float kFinishCaptureSeconds = 1.f;
constexpr float kPodiumCaptureSeconds = 5.f;
// the stock waits for the room screen of the server ours leaves after this rest when none came
constexpr float kPodiumFallbackSeconds = 20.f;
// the system line of a game message stays this long at the top right
constexpr double kSystemLineSeconds = 5.0;
// camera shake kick values of the podium thumps 0x45FA0000 and 0x459C4000
constexpr float kPodiumShake = 8000.f;
constexpr float kLandingShake = 5000.f;
// the raw key codes of the debug boost keys at the green light
constexpr int kVkOne = 0x31;
// sub 4B2220 the countdown nif at scale 0x3D4CCCCD sub 4B23C0 5 units ahead of the car and 4 5 up
const char* const kCountdownNif = "Effect/startcount.nif";
constexpr float kCountdownScale = 0.05f;
constexpr float kCountdownAhead = 5.f;
constexpr float kCountdownLift = 4.5f;
// the rain loop clip weather stream wav is 3 13 s long
constexpr float kRainLoopSeconds = 3.1f;
constexpr float kDegToRadRace = 3.14159265f / 180.f;
// the countdown capture waits for the two so the chase camera has eased in behind the kart
constexpr float kCountdownCaptureSeconds = 8.6f;

// one line per race load step the span of the step and the texture files read inside it
struct LoadClock {
    double last = App::uptimeMs();
    void mark(const std::string& step) {
        const double now = App::uptimeMs();
        const KnC::Render::TextureLoadStats t = KnC::Render::take_texture_load_stats();
        if (t.count == 0)
            std::printf("[time] race %s %.0f ms\n", step.c_str(), now - last);
        else
            std::printf("[time] race %s %.0f ms, %zu textures read %.0f parse %.0f inspect %.0f create %.0f\n",
                        step.c_str(), now - last, t.count, t.read_ms, t.parse_ms, t.inspect_ms, t.create_ms);
        last = now;
    }
};

// every texture file a prop model names the diffuse the detail the flip frames and the particles
void collectTextures(const KnC::Render::PropModel& model, std::vector<std::string>& out) {
    for (const KnC::Render::PropPart& part : model.parts) {
        out.push_back(part.texture_path);
        out.push_back(part.detail_texture_path);
    }
    for (const auto& channel : model.animation.flip_channels)
        for (const std::string& path : channel.textures) out.push_back(path);
    for (const KnC::Render::ParticleSystemDefinition& system : model.particle_systems)
        out.push_back(system.texture_path);
}

// the use cue of the item kinds the pages name the rest play the generic use cue
const char* itemUseSound(int32_t kind) {
    switch (kind) {
    case 2: return "item_spike_on_snd";
    case 10: return "item_rocket_shot_snd";
    case 11: return "item_hive_use_snd";
    case 14: return "item_ice_throw_snd";
    case 17: return "item_hammer_throw";
    case 18: return "item_bomb_throw";
    case 19: return "item_dung_throw";
    default: return "item_shield_use_snd";
    }
}

// the theme loop by the World folder name the pak names five themes the others take the city loop
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

}

// the files of the track read on a thread the device upload stays on the main thread
struct RaceScreen::LoadJob {
    std::thread thread;
    std::atomic<bool> done{false};
    // the car effects parse on the same thread once the race stands it takes them later
    std::atomic<bool> effectsDone{false};
    // raised when the race screen goes the thread ends after the file in hand
    std::atomic<bool> stop{false};
    bool effectsAdopted = false;
    bool ok = false;
    std::string error;
    TrackFiles files;
    std::string gameDir;
    std::map<std::string, KnC::Render::PropModel> effects;
    double worldMs = 0.0;
    double effectsMs = 0.0;
    double texturesMs = 0.0;
    size_t textures = 0;
};

RaceScreen::RaceScreen(App& app) : m_app(app), m_wire(app.session()) {}

RaceScreen::~RaceScreen() {
    if (m_load) m_load->stop.store(true);
    joinLoad();
}

void RaceScreen::joinLoad() {
    if (m_load && m_load->thread.joinable()) m_load->thread.join();
}

// the parsed effects join the race once their thread is done their textures are decoded already
void RaceScreen::adoptEffects() {
    if (!m_load || m_load->effectsAdopted || !m_load->effectsDone.load() || !m_worldLoaded) return;
    if (m_load->thread.joinable()) m_load->thread.join();
    m_load->effectsAdopted = true;
    raceEffects().adopt(std::move(m_load->effects));
    std::printf("[time] race effects %.0f ms on their thread in at %.0f ms after enter\n", m_load->effectsMs,
                App::uptimeMs() - m_enterMs);
}

std::string RaceScreen::kartModel(uint32_t kartKey) const {
    const KartRow* row = m_app.session().catalog().kart(kartKey);
    return row && !row->model.empty() ? row->model : "Basic_1";
}

std::string RaceScreen::driverAsset(uint32_t driverKey) const {
    const DriverRow* row = m_app.session().catalog().driver(driverKey);
    return row && !row->asset.empty() ? row->asset : "Cosmo";
}

// KNC RACE MODE forces the game mode of the launch a capture aid for the speed and team panels
uint32_t RaceScreen::raceGameMode() const {
    if (const char* forced = std::getenv("KNC_RACE_MODE")) return static_cast<uint32_t>(std::atoi(forced));
    return m_app.session().raceLaunch().gameMode;
}

RaceScreen::Slot* RaceScreen::slot(uint32_t playerId) {
    for (Slot& s : m_slots) if (s.playerId == playerId) return &s;
    return nullptr;
}

void RaceScreen::enter() {
    Session& session = m_app.session();
    const RaceLaunch& launch = session.raceLaunch();
    m_enterMs = App::uptimeMs();
    m_wire.begin(launch, session.profile().playerId);
    // the stock reads no frame while its stage loads so they wait here and replay in order once it stands
    m_app.setFrameTap([this](uint16_t op, Packet& pkt) {
        if (m_loading) {
            m_heldFrames.emplace_back(op, pkt);
            return;
        }
        m_wire.onFrame(op, pkt);
    });
    // the auto race and the auto join drive the line KNC SAMPLE DRIVE lets the sample race drive too
    const bool sampleDrive = std::getenv("KNC_SAMPLE_DRIVE") != nullptr;
    m_autoDrive = (m_app.options().autoRaceTrack > 0 || m_app.options().autoJoin) && (!m_app.sampleMode() || sampleDrive);
    m_stage = Stage::Loading;
    m_stageTime = 0.f;
    m_status = "loading track " + std::to_string(launch.trackId);

    m_wire.onGridSpawn = [this](const Racer& r) { spawnRacer(r); };
    // FUN 0047ADE0 raises the flag FUN 00402210 answers it on the next frame once the stage loaded
    m_wire.onRankBoard = [this]() {
        m_rankBoardSeen = true;
        if (!m_worldLoaded) return;
        m_wire.sendSceneLoaded();
        if (m_stage == Stage::Loading) m_stage = Stage::Grid;
    };
    m_wire.onGo = [this]() {
        m_stage = Stage::Countdown;
        m_stageTime = 0.f;
        m_raceClock = 0.0;
        armIntroPan();
        m_status = "GO received, the countdown runs";
        // the slot mirror of empty hands the first thing a client says once the GO lands
        Packet slots = Packet::fromCmdFull(0x00CF);
        slots.writeInt32(-1); slots.writeInt32(-1); slots.writeInt32(-1);
        m_app.session().send(slots);
    };
    m_wire.onMotion = [this](const std::vector<MotionRecvEntry>& entries) {
        for (const MotionRecvEntry& e : entries) m_sim.applyMotion(e);
    };
    m_wire.onTeleport = [this](uint32_t id, float x, float y, float z, float yaw) { m_sim.teleport(id, x, y, z, yaw); };
    m_wire.onEffect = [this](uint32_t id, int code) {
        if (id == m_wire.localPlayerId()) m_hitAt = m_time;
        // the impact sprite of the car the code 1000 alone plays it 0x496066
        if (Slot* hitSlot = slot(id)) raceEffects().hit(hitSlot->viewHandle, code, m_app.renderer().animation().seconds());
        for (int known : kEffectCodes) if (known == code) { m_sim.applyEffect(id, code); return; }
    };
    m_wire.onItemGrant = [this](uint32_t id, int32_t item, int32_t slotIndex) {
        (void)slotIndex;
        if (id == m_wire.localPlayerId()) return;
        std::printf("[race] %u holds item %d\n", id, item);
    };
    m_wire.onItemSpawn = [this](const ItemSpawn& s) {
        std::printf("[race] item %d spawned by %u at %.1f %.1f\n", s.kind, s.playerId, s.x, s.y);
        // the echo of our own use plays nothing twice the others play at a lower volume
        if (s.playerId != m_wire.localPlayerId()) m_app.sound().play(itemUseSound(s.kind), 0.5f);
    };
    m_wire.onLocalFinish = [this]() {
        if (m_stage < Stage::Finished) { m_stage = Stage::Finished; m_stageTime = 0.f; }
        m_sim.setFinished(m_wire.localPlayerId());
        m_status = "finished, the finish camera";
        m_app.sound().stopEngine();
        // sub 47A5C0 rank 0 plays the winner cue every other rank the finish cue
        m_app.sound().play(m_wire.reward().finishRank == 0 ? "goal_winner_snd" : "goal_finish_snd");
    };
    m_wire.onResultBoard = [this]() {
        // sub 47A760 the race end at 2000 starts for everyone here finished or not
        if (m_stage < Stage::Finished) {
            m_stage = Stage::Finished;
            m_stageTime = 0.f;
            m_sim.setFinished(m_wire.localPlayerId());
            m_app.sound().stopEngine();
        }
        m_boardAt = m_time;
        m_resultTime = 0.f;
        // sub 4B67E0 raises the board flag the state 2020 of FUN 00401D90 clears it before the podium
        m_boardOpen = true;
        m_app.sound().play("goal_result_snd");
    };
    m_wire.onGameMessage = [this](const std::string& key, const std::u16string& name, int32_t) {
        // the def trans line of the key its s spot takes the name as the stock message area does
        std::string line = m_app.tr(key);
        const size_t at = line.find("%s");
        if (at != std::string::npos) line.replace(at, 2, u16ToUtf8(name));
        m_systemLine = line;
        m_systemLineAt = m_time;
    };
    m_wire.onRacerLeft = [this](uint32_t id) {
        if (Slot* s = slot(id)) {
            m_sim.removeCar(id);
            m_view.removeCar(s->viewHandle);
            s->viewHandle = -1;
            s->carIndex = -1;
        }
    };

    startLoad(static_cast<int>(launch.trackId));
}

void RaceScreen::leave() {
    m_app.setFrameTap(nullptr);
    m_app.sound().stopEngine();
    if (m_load) m_load->stop.store(true);
    joinLoad();
    KnC::Render::drop_prefetched_textures();
}

// the track the effects and the textures read on a thread the window keeps drawing meanwhile
void RaceScreen::startLoad(int trackId) {
    std::string error;
    auto job = std::make_unique<LoadJob>();
    job->gameDir = m_app.options().gameDir;
    if (!resolveTrackFiles(m_app.session().catalog(), trackId, job->gameDir, job->files, error)) {
        m_status = "track " + std::to_string(trackId) + ": " + error;
        std::printf("[race] %s\n", m_status.c_str());
        return;
    }
    m_loadTrackId = trackId;
    m_loading = true;
    KnC::Render::take_texture_load_stats();
    LoadJob* raw = job.get();
    RaceWorld* world = &m_world;
    job->thread = std::thread([raw, world]() {
        // KNC RACE LOAD DELAY holds the thread that many ms a slow machine for the hold and wait proofs
        if (const char* delay = std::getenv("KNC_RACE_LOAD_DELAY"))
            std::this_thread::sleep_for(std::chrono::milliseconds(std::atoi(delay)));
        double at = App::uptimeMs();
        raw->ok = loadRaceWorld(raw->files, *world, raw->error);
        raw->worldMs = App::uptimeMs() - at;
        if (raw->ok) {
            at = App::uptimeMs();
            // the textures of the scene the sky and the box decode on every spare core
            std::vector<std::string> paths;
            const KnC::Render::MapScene& scene = world->scene.scene;
            for (const KnC::Render::PropModel& model : scene.prop_models) collectTextures(model, paths);
            for (const KnC::Render::SkyPhase& phase : scene.sky_phases) collectTextures(phase.model, paths);
            if (world->scene.has_item_box) collectTextures(world->scene.item_box_model, paths);
            const unsigned cores = std::max(2u, std::thread::hardware_concurrency());
            raw->textures = KnC::Render::prefetch_textures(paths, std::min(cores - 1, 6u));
            raw->texturesMs = App::uptimeMs() - at;
        }
        raw->done.store(true);
        if (!raw->ok) return;
        // car effects show ten seconds after GO at the soonest this thread yields to the frame loop
        KnC::Render::lower_current_thread_priority();
        const double from = App::uptimeMs();
        raw->effects = RaceEffects::parseAll(raw->gameDir, &raw->stop);
        std::vector<std::string> paths;
        for (const auto& entry : raw->effects) collectTextures(entry.second, paths);
        KnC::Render::prefetch_textures(paths, 2, &raw->stop, true);
        raw->effectsMs = App::uptimeMs() - from;
        raw->effectsDone.store(true);
    });
    m_load = std::move(job);
    std::printf("[time] race load thread started %.0f ms after enter\n", App::uptimeMs() - m_enterMs);
}

// the main thread side once the thread is done the device upload the sim and the held frames
void RaceScreen::finishLoad() {
    // the thread goes on with the effects it is joined once they are in
    m_loading = false;
    LoadJob& job = *m_load;
    const KnC::Render::TextureLoadStats read = KnC::Render::take_texture_load_stats();
    std::printf("[time] race load thread world %.0f ms textures %.0f ms, %zu of %zu files read %.0f parse %.0f inspect %.0f\n",
                job.worldMs, job.texturesMs, job.textures, read.count, read.read_ms, read.parse_ms, read.inspect_ms);
    if (!job.ok) {
        m_status = "world load failed: " + job.error;
        std::printf("[race] %s\n", m_status.c_str());
        m_heldFrames.clear();
        return;
    }
    if (finishWorld(job.files)) {
        std::printf("[time] race ready %.0f ms after enter (at %.0f ms)\n", App::uptimeMs() - m_enterMs,
                    App::uptimeMs());
    }
    // the frames of the load play now in order the grid the board and the GO among them
    std::vector<std::pair<uint16_t, Packet>> held;
    held.swap(m_heldFrames);
    if (!held.empty()) std::printf("[race] %zu frames held through the load replayed\n", held.size());
    for (auto& frame : held) m_wire.onFrame(frame.first, frame.second);
}

bool RaceScreen::finishWorld(const TrackFiles& files) {
    Session& session = m_app.session();
    const int trackId = m_loadTrackId;
    LoadClock clock;
    std::string error;
    if (!m_sim.init(m_world, session.profile().playerId, error)) {
        m_status = "sim init failed: " + error;
        return false;
    }
    // sub 4ADB90 stocks the blue band only in the mode 3 of dword 0xB23178 the speed team race
    m_sim.setGaugeTeamMode(raceGameMode() == 3);
    clock.mark("sim");
    m_view.load(m_app.renderer(), m_world);
    clock.mark("upload");
    // the weather of the track pick 0 sun 1 night 2 rain a night takes the sky night nif
    uint32_t weather = session.room().trackWeather != 0 ? session.room().trackWeather
                                                        : m_app.session().raceLaunch().flag;
    // KNC WEATHER forces the weather word of the launch so a capture can show the rain or the snow
    if (const char* forced = std::getenv("KNC_WEATHER")) weather = static_cast<uint32_t>(std::atoi(forced));
    m_view.setNightSky(m_app.renderer(), weather == 1);
    // sub 4D1C70 0 clear 1 night 2 rain 3 snow and the track of record 60 keeps the rain away
    RaceWeather mode = RaceWeather::Clear;
    if (weather <= 3) mode = static_cast<RaceWeather>(weather);
    if (mode == RaceWeather::Rain && trackId == 60) mode = RaceWeather::Clear;
    m_view.setWeather(m_app.renderer(), m_app.options().gameDir, mode);
    std::printf("[race] weather %u night %d\n", weather, weather == 1 ? 1 : 0);
    clock.mark("weather");
    // sub 4AE590 the gauge nifs load with the stage the blue band only in the speed team mode
    m_gauge.load(m_app.renderer(), m_app.options().gameDir, raceGameMode() == 3);
    clock.mark("gauge");
    loadCountdownProp();
    clock.mark("countdown");
    loadItemBoxes();
    m_drumBroken.assign(m_world.itemDrums.size(), 0);
    // world gimmick load by track 0x4D4180 the placed gimmick nifs of the track become hit rows
    m_gimmicks = KnC::Kart::Client::GimmickWorld();
    for (const KnC::Tools::TrackGimmick& g : m_world.scene.gimmicks) {
        if (g.gimmick_class < 0) continue;
        KnC::Kart::Client::GimmickInstance row;
        row.x = g.position[0];
        row.y = g.position[1];
        row.z = g.position[2];
        row.gimmickClass = g.gimmick_class;
        m_gimmicks.rows.push_back(row);
    }
    m_gimmicks.loaded = m_gimmicks.rows.empty() ? 0 : 1;
    std::printf("[race] %zu themed gimmick rows on the track\n", m_gimmicks.rows.size());
    m_auto.reset(m_sim.line());
    std::printf("[race] racing line %zu points\n", m_sim.line().size());
    clock.mark("gimmick rows");
    m_worldLoaded = true;
    m_stageTime = 0.f;
    m_app.playMusic(themeMusic(files.themeFolder));
    m_status = files.themeFolder + "/" + files.trackFolder + " loaded, waiting for the grid";
    if (!m_world.scene.start_rows.empty()) {
        const auto& row = m_world.scene.start_rows.front();
        m_chase.x = row.x; m_chase.y = row.y; m_chase.z = row.z; m_chase.yawDeg = row.heading;
    }
    return true;
}

// sub 4B2220 the countdown is one nif beside the cars not a sprite on the canvas
void RaceScreen::loadCountdownProp() {
    m_countdownProp = -1;
    m_countdownBeat = -1;
    namespace fs = std::filesystem;
    const fs::path nif = fs::path(m_app.options().gameDir) / "Data" / "Public" / kCountdownNif;
    KnC::Render::NifModelRequest request;
    request.nif_path = nif.string();
    request.texture_dir = nif.parent_path().string();
    request.play_stopped_controllers = true;
    KnC::Render::PropModel model;
    std::string error;
    if (!KnC::Render::load_prop_model(request, model, error)) {
        std::printf("[race] %s failed %s\n", request.nif_path.c_str(), error.c_str());
        return;
    }
    KnC::Tools::resolve_textures(request.texture_dir, model);
    m_countdownProp = m_view.addProp(m_app.renderer(), model);
    std::printf("[race] countdown nif %zu parts prop %d\n", model.parts.size(), m_countdownProp);
}

// sub 4B23C0 mode 2 the nif stands 5 ahead of the car 4 5 up turned by yaw plus 90
void RaceScreen::updateCountdownProp(int stage) {
    if (m_countdownProp < 0) return;
    if (stage < 1 || stage > 4) {
        float hide[16];
        bx::mtxIdentity(hide);
        m_view.placeProp(m_countdownProp, hide, false);
        m_countdownBeat = -1;
        return;
    }
    const CarPose own = localPose();
    const float yawRad = own.yawDeg * kDegToRadRace;
    // math dir from heading 0x44DF00 the heading yaw minus 90 gives the car forward
    const float aheadRad = (own.yawDeg - 90.f) * kDegToRadRace;
    const float at[3] = {own.x + std::sin(aheadRad) * kCountdownAhead, own.y + std::cos(aheadRad) * kCountdownAhead,
                         own.z + kCountdownLift};
    float scale[16], turn[16], world[16];
    bx::mtxScale(scale, kCountdownScale);
    bx::mtxRotateZ(turn, -(own.yawDeg + 90.f) * kDegToRadRace);
    bx::mtxMul(world, scale, turn);
    world[12] = at[0];
    world[13] = at[1];
    world[14] = at[2];
    (void)yawRad;
    m_view.placeProp(m_countdownProp, world, true);
    // sub 4B23C0 case 2 resets the clip once at the arm sub 4B2690 only plays a cue on each beat
    if (m_countdownBeat < 1) {
        m_countdownBeat = stage;
        m_view.restartProp(m_app.renderer(), m_countdownProp);
        std::printf("[race] countdown %d car %.1f %.1f %.1f yaw %.1f plate %.1f %.1f %.1f eye %.1f %.1f %.1f\n", stage,
                    own.x, own.y, own.z, own.yawDeg, at[0], at[1], at[2], m_view.camera().eye[0],
                    m_view.camera().eye[1], m_view.camera().eye[2]);
    }
}

void RaceScreen::spawnRacer(const Racer& racer) {
    if (!m_worldLoaded) return;
    LoadClock clock;
    KnC::Render::take_texture_load_stats();
    const auto& rows = m_world.scene.start_rows;
    float x = 0.f, y = 0.f, z = 1.f, yaw = 0.f;
    if (!rows.empty()) {
        const auto& row = rows[racer.gridIndex < rows.size() ? racer.gridIndex : 0];
        x = row.x; y = row.y; z = row.z; yaw = row.heading;
    } else {
        std::printf("[race] no start rows the car lands at the origin\n");
    }
    Slot* existing = slot(racer.playerId);
    if (existing && existing->carIndex >= 0) return;
    Slot s;
    s.playerId = racer.playerId;
    const std::string model = kartModel(racer.kartKey);
    const std::string driver = driverAsset(racer.driverKey);
    if (racer.local) {
        CarSetup setup;
        setup.playerId = racer.playerId;
        const KartRow* kart = m_app.session().catalog().kart(racer.kartKey);
        if (kart) {
            setup.stats = kart->stats;
            setup.vehicleKind = static_cast<int>(kart->vehicleKind);
        } else {
            std::printf("[race] kart %u has no 0x00C0 row the stats stay zero\n", racer.kartKey);
        }
        setup.carFile = kartCarFile(m_app.options().gameDir, model);
        setup.x = x; setup.y = y; setup.z = z; setup.yawDeg = yaw;
        std::string error;
        if (!m_sim.spawnLocal(setup, error)) {
            m_status = "local spawn failed: " + error;
            std::printf("[race] %s\n", m_status.c_str());
            return;
        }
        s.carIndex = m_sim.localIndex();
        m_auto.start(x, y, yaw);
        m_checkpoint = 0;
        m_laps = 0;
        m_lastFace = -1;
    } else {
        s.carIndex = m_sim.spawnRemote(racer.playerId, x, y, z, yaw);
    }
    // the paint the plate and the antenna keys of the 0x00C0 row the local owned kart wins
    std::array<uint32_t, 3> look{};
    if (const KartRow* row = m_app.session().catalog().kart(racer.kartKey))
        for (size_t i = 0; i < 3; ++i) look[i] = row->skins[i];
    if (racer.local && m_app.session().profile().kartInstance >= 0)
        if (const OwnedKart* owned = m_app.session().catalog().ownedKart(static_cast<uint32_t>(m_app.session().profile().kartInstance)))
            for (size_t i = 0; i < 3; ++i) if (owned->part[i] != 0) look[i] = owned->part[i];
    auto partModel = [this](uint32_t key) {
        const PartRow* row = key != 0 ? m_app.session().catalog().part(key) : nullptr;
        return row ? row->model : std::string();
    };
    clock.mark("racer sim " + std::to_string(racer.playerId));
    s.viewHandle = m_view.addCar(m_app.renderer(), m_app.options().gameDir, model, driver, partModel(look[0]));
    clock.mark("racer kart and driver " + model + " " + driver);
    raceEffects().setLook(s.viewHandle, partModel(look[1]), partModel(look[2]));
    // driver manager load driver 0x48CE97 the pet of the 0x003E key the local one falls back on the garage
    uint32_t petKey = racer.petKey;
    if (racer.local && petKey == 0)
        if (const OwnedPet* worn = m_app.session().catalog().equippedPet()) petKey = worn->petKey;
    // KNC PET TEST seats that pet key on every racer a capture aid for the hover
    if (const char* forced = std::getenv("KNC_PET_TEST")) petKey = static_cast<uint32_t>(std::atoi(forced));
    if (const PetRow* pet = petKey != 0 ? m_app.session().catalog().pet(petKey) : nullptr) {
        const std::string root = m_app.options().gameDir + "/Data/Public/Pet/";
        const std::string folder = findEntryCi(root + "Body", pet->model);
        const std::string nif = folder.empty() ? std::string() : findEntryCi(folder, "body.nif");
        m_view.setCarPet(m_app.renderer(), s.viewHandle, nif, findEntryCi(root + "Facial", pet->model));
        if (nif.empty()) std::printf("[race] pet %u model %s has no body nif\n", petKey, pet->model.c_str());
    }
    clock.mark("racer pet and look");
    if (existing) *existing = s; else m_slots.push_back(s);
    std::printf("[race] racer %u on row %u kart %s driver %s car %d view %d\n", racer.playerId, racer.gridIndex,
                model.c_str(), driver.c_str(), s.carIndex, s.viewHandle);
}

// FUN 0043ed70 case 0xf the grid line ends the eye 9 to the side of row 0 and 5 up
void RaceScreen::armIntroPan() {
    m_panReady = false;
    m_panPos = 0.f;
    m_panDir = 1;
    const auto& rows = m_world.scene.start_rows;
    if (rows.empty()) return;
    // 0x4033a2 the last row of the fly over is the live car count capped at eight
    int cars = 0;
    for (const Slot& s : m_slots) if (s.carIndex >= 0) ++cars;
    if (cars < 2) cars = 2;
    if (cars > 8) cars = 8;
    size_t last = static_cast<size_t>(cars - 1);
    if (last >= rows.size()) last = rows.size() - 1;
    m_panFrom[0] = rows[0].x; m_panFrom[1] = rows[0].y; m_panFrom[2] = rows[0].z;
    m_panTo[0] = rows[last].x; m_panTo[1] = rows[last].y; m_panTo[2] = rows[last].z;
    m_panHeadingDeg = rows[0].heading;
    const float dx = m_panTo[0] - m_panFrom[0];
    const float dy = m_panTo[1] - m_panFrom[1];
    m_panSpan = std::sqrt(dx * dx + dy * dy);
    if (m_panSpan < 1.f) m_panSpan = 1.f;
    // the stock rate ends early on a short grid so the trip is scaled to fill the window
    const float window = kIntroHandoverSeconds - kIntroFrontSeconds;
    m_panRate = kIntroRatePerFrame * 60.f;
    m_panSeconds = 2.f * m_panSpan / m_panRate;
    if (m_panSeconds > window || m_panSeconds < window * 0.5f) {
        m_panSeconds = window;
        m_panRate = 2.f * m_panSpan / m_panSeconds;
    }
    m_panReady = true;
    std::printf("[race] intro pan over %d rows span %.1f for %.1f s\n", cars, m_panSpan, m_panSeconds);
}

// 0x43fa0a the look point walks the line and turns round at the far row the eye keeps its offset
bool RaceScreen::introCamera(float eye[3], float look[3]) const {
    if (!m_panReady || m_stage != Stage::Countdown || m_stageTime >= kIntroHandoverSeconds) return false;
    const float h = (m_panHeadingDeg - 90.f) * 3.14159265f / 180.f;
    if (m_stageTime < m_panSeconds) {
        const float t = m_panPos / m_panSpan;
        for (int i = 0; i < 3; ++i) look[i] = m_panFrom[i] + (m_panTo[i] - m_panFrom[i]) * t;
        // math point along heading 0x44dec0 nine units to the side of the row then five up
        eye[0] = look[0] + std::cos(-h) * kIntroSide;
        eye[1] = look[1] + std::sin(-h) * kIntroSide;
        eye[2] = look[2] + kIntroLift;
        return true;
    }
    // mode 16 the chase camera mirrored the eye stands in front of the own kart looking back
    const CarPose own = localPose();
    const float yaw = (own.yawDeg + 90.f) * 3.14159265f / 180.f;
    eye[0] = own.x - std::sin(yaw) * kIntroFrontDistance;
    eye[1] = own.y - std::cos(yaw) * kIntroFrontDistance;
    eye[2] = own.z + kIntroFrontHeight;
    look[0] = own.x;
    look[1] = own.y;
    look[2] = own.z + kIntroFrontHeight * 0.5f;
    return true;
}

// the fly over advances by the stock rate and turns round once it reaches the far row
void RaceScreen::updateIntro(float dt) {
    if (!m_panReady || m_stage != Stage::Countdown) return;
    if (m_stageTime >= m_panSeconds) return;
    m_panPos += m_panRate * dt * static_cast<float>(m_panDir);
    if (m_panPos >= m_panSpan) { m_panPos = m_panSpan; m_panDir = -1; }
    if (m_panPos <= 0.f) { m_panPos = 0.f; m_panDir = 1; }
}

void RaceScreen::readKeys(InputFlags& flags, bool& drift, bool& item) {
    flags = InputFlags();
    // the Input ini rows of the settings the stock defaults are the arrows control and shift
    flags.accel = m_app.raceKeyDown(RaceKey::Up) ? 1 : 0;
    flags.brake = m_app.raceKeyDown(RaceKey::Down) ? 1 : 0;
    flags.steerLeft = m_app.raceKeyDown(RaceKey::Left) ? 1 : 0;
    flags.steerRight = m_app.raceKeyDown(RaceKey::Right) ? 1 : 0;
    drift = m_app.raceKeyDown(RaceKey::Drift);
    item = m_app.raceKeyDown(RaceKey::Item);
}

CarPose RaceScreen::localPose() const {
    if (!m_sim.hasLocal()) return m_chase;
    return m_sim.pose(m_sim.localIndex());
}

// a script times its race shots from the GO the grid comes five seconds after the launch
bool RaceScreen::ready() const { return !m_loading && (!m_worldLoaded || m_stage >= Stage::Countdown); }

bool RaceScreen::holdsPrevious() const { return m_loading; }

void RaceScreen::update(float dt) {
    m_time += dt;
    m_frameDt = dt;
    m_stageTime += dt;
    if (m_loading && m_load && m_load->done.load()) finishLoad();
    // one capture a second into a load the held stage shows in it
    if (m_loading && m_app.captureMode() && !m_capturedLoading && App::uptimeMs() - m_enterMs > 1000.0) {
        m_capturedLoading = true;
        m_app.captureStage("loading");
    }
    if (!m_worldLoaded) return;
    adoptEffects();
    const Options& o = m_app.options();

    if (m_stage == Stage::Countdown) {
        updateIntro(dt);
        if (m_app.captureMode() && !m_capturedCountdown && m_stageTime >= kCountdownCaptureSeconds) {
            m_capturedCountdown = true;
            m_app.captureStage("countdown");
        }
        // one ready cue per digit then the go cue at the green light
        const float left = kCountdownSeconds - m_stageTime;
        const int digit = left > 3.f ? 0 : left > 2.f ? 3 : left > 1.f ? 2 : 1;
        updateCountdownProp(digit);
        if (digit > 0 && digit != m_cueStage) {
            m_cueStage = digit;
            m_app.sound().play("countdown_ready_snd");
        }
        if (m_stageTime >= kCountdownSeconds) {
            m_stage = Stage::Racing;
            m_stageTime = 0.f;
            m_raceClock = 0.0;
            m_sim.setGreenLight(true);
            m_sim.setSessionRunning(true);
            m_lapStartClock = 0.0;
            m_status = "green light";
            m_app.sound().play("countdown_go_snd");
            // the sample race holds a rabbit from the light so the slot shows as the stock capture
            if (m_app.sampleMode()) m_wire.sendItemGrant(7);
        }
    }
    // the GO plate of the nif holds its last second then the prop goes away
    if (m_stage == Stage::Racing) updateCountdownProp(m_stageTime < 1.f ? 4 : 0);
    if (m_stage == Stage::Racing || m_stage == Stage::Finished) m_raceClock += dt;
    if (m_stage == Stage::Racing && m_app.captureMode() && !m_capturedRace && m_stageTime >= kRaceCaptureSeconds) {
        std::printf("[race] band %.1f of 127 spark %d flash %d at %.0f km per hour\n", m_sim.gaugeFill(),
                    m_sim.gaugeSpark() ? 1 : 0, m_sim.gaugeFlash(), localPose().speedKmh);
        m_capturedRace = true;
        m_app.captureStage("race");
        if (o.stopAt == "race") m_app.finishRun();
    }
    if (m_stage == Stage::Finished) updateFinish(dt);
    if (m_stage == Stage::Podium) updatePodium(dt);

    // the six flags come from the keyboard or the line follower the port reads them each tick
    InputFlags flags;
    bool drift = false;
    bool itemKey = false;
    if (m_sim.hasLocal()) {
        const bool driving = m_stage == Stage::Racing;
        if (m_autoDrive) {
            if (driving) m_auto.drive(localPose(), flags, drift);
        } else if (!m_paused) {
            readKeys(flags, drift, itemKey);
            if (!driving) flags = InputFlags();
        }
        if (m_stage >= Stage::Finished) { flags = InputFlags(); flags.brake = 1; drift = false; }
        const bool accelPressed = flags.accel != 0 && !m_accelWas;
        m_accelWas = flags.accel != 0;
        m_sim.setLocalInput(flags, drift, accelPressed);
    }

    const int ticks = m_sim.advance(dt);
    if (ticks > 0 && m_sim.hasLocal()) {
        for (float& c : m_boxCooldown) if (c > 0.f) c -= 0.02f * static_cast<float>(ticks);
        if (m_stage == Stage::Racing) {
            watchCheckpoints();
            watchDrums();
            watchGimmicks();
            logRemoteLean();
            const bool useNow = (itemKey && !m_itemWas) || (m_autoDrive && m_useItemIn >= 0.f && (m_useItemIn -= 0.02f * static_cast<float>(ticks)) <= 0.f);
            watchItems(useNow);
        }
        m_itemWas = itemKey;
    }
    tickWire(ticks);

    // the engine loop follows the speed of the local car while it drives
    if (m_sim.hasLocal() && (m_stage == Stage::Racing || m_stage == Stage::Countdown)) {
        const CarPose pose = localPose();
        m_app.sound().engine("accel_my_snd_03", 0.6f + std::min(pose.speedKmh, 200.f) / 200.f * 1.2f, m_paused ? 0.f : 0.5f);
    }

    updateItemBoxes();
    // every visual follows its car the local one leads the camera the podium seats the ranked ones
    const float clock = m_app.renderer().animation().seconds();
    for (Slot& s : m_slots) {
        if (s.carIndex < 0 || s.viewHandle < 0) continue;
        CarPose pose = m_sim.pose(s.carIndex);
        if (m_stage == Stage::Podium && m_podium.loaded()) {
            const Racer* me = nullptr;
            int slot = 0;
            for (const Racer& r : m_wire.racers()) {
                if (r.playerId == s.playerId) me = &r;
            }
            if (me && me->finishRank >= 0) {
                // the ranks past three take the spots behind in rank order
                for (const Racer& r : m_wire.racers())
                    if (r.finishRank >= 3 && r.finishRank < me->finishRank) ++slot;
                m_podium.seat(me->finishRank, slot, pose);
            }
        }
        m_view.setPose(s.viewHandle, pose, dt, clock);
        // the name plate of the hud reads this pose so a podium seat is not the sim pose
        s.shown = pose;
    }
    if (m_sim.hasLocal()) m_chase = localPose();
    // the weather sheet rides the camera so it is placed after the camera of the last frame
    const CarPose own = localPose();
    const float carAt[3] = {own.x, own.y, own.z};
    m_view.updateWeather(m_app.renderer(), dt, carAt);
    if (m_view.takeThunder()) m_app.sound().play("weather_thunder");
    // the rain loop of sound 0xD4 is 3 13 s long one shot on the sheet window avoids a gap
    if (m_view.weather() == RaceWeather::Rain) {
        m_rainLoopIn -= dt;
        if (m_rainLoopIn <= 0.f) { m_rainLoopIn = kRainLoopSeconds; m_app.sound().play("weather_stream", 0.7f); }
    }
}

void RaceScreen::updateFinish(float dt) {
    const Options& o = m_app.options();
    if (m_app.captureMode() && !m_capturedResult && m_stageTime >= kFinishCaptureSeconds) {
        m_capturedResult = true;
        m_app.captureStage("result");
    }
    if (m_boardAt < 0.0) return;
    // state 2010 waits six seconds after the board then the fade out then the podium
    if (m_fadeDir == 0 && m_time - m_boardAt >= kFinishHoldSeconds) m_fadeDir = 1;
    if (m_fadeDir == 1) {
        m_fade += dt / kFadeSeconds;
        if (m_fade >= 1.f) {
            m_fade = 1.f;
            beginPodium();
            if (m_stage != Stage::Podium && (o.stopAt == "result" || o.autoRaceTrack > 0)) m_app.finishRun();
        }
    }
}

void RaceScreen::beginPodium() {
    int finishers = 0;
    for (const Racer& r : m_wire.racers()) if (r.finishRank >= 0) ++finishers;
    const uint32_t mode = raceGameMode();
    const bool team = mode == 1 || mode == 3;
    if (!m_podium.load(m_view, m_app.renderer(), m_app.options().gameDir, m_world, team, finishers)) {
        m_status = "no podium the race end fades back";
        m_fadeDir = -1;
        return;
    }
    m_stage = Stage::Podium;
    m_stageTime = 0.f;
    m_fadeDir = -1;
    // state 2020 hides the board with FUN 004B6820 before the podium loads
    m_boardOpen = false;
    m_status = "the podium";
}

void RaceScreen::updatePodium(float dt) {
    const Options& o = m_app.options();
    // FUN 0043D7E0 mode 1 the fade in over 300 ms
    if (m_fadeDir == -1) {
        m_fade -= dt / kFadeSeconds;
        if (m_fade <= 0.f) { m_fade = 0.f; m_fadeDir = 0; }
    }
    std::vector<PodiumEvent> events;
    m_podium.update(dt, events);
    for (PodiumEvent e : events) {
        switch (e) {
        case PodiumEvent::DropStart:
            m_view.camera().kick(kPodiumShake);
            m_app.sound().play("goal_drop_podium_snd");
            break;
        case PodiumEvent::Landing:
            m_view.camera().kick(kLandingShake);
            m_app.sound().play("goal_drop_player_snd");
            break;
        case PodiumEvent::OthersLanding:
            m_app.sound().play("goal_drop_podium_snd");
            break;
        case PodiumEvent::WinClips:
            // drivers play result clip 0x48C420 every ranked finisher wins clip 9 an unranked one loses clip 10
            for (const Slot& s : m_slots) {
                for (const Racer& r : m_wire.racers()) {
                    if (r.playerId != s.playerId) continue;
                    m_view.setDriverClip(s.viewHandle, r.finishRank >= 0 ? KnC::Tools::kDriverSeqWin : KnC::Tools::kDriverSeqLose);
                }
            }
            break;
        case PodiumEvent::Ceremony:
            m_app.sound().play("goal_ceremony_snd");
            break;
        }
    }
    // KNC PODIUM SHOT moves the podium capture to that second a capture aid for the clips
    static const float captureAt = std::getenv("KNC_PODIUM_SHOT") != nullptr
                                       ? static_cast<float>(std::atof(std::getenv("KNC_PODIUM_SHOT")))
                                       : kPodiumCaptureSeconds;
    if (m_app.captureMode() && !m_capturedPodium && m_podium.seconds() >= captureAt) {
        m_capturedPodium = true;
        m_app.captureStage("podium");
        if (o.stopAt == "result" || o.autoRaceTrack > 0) m_app.finishRun();
    }
    if (m_podium.seconds() >= kPodiumFallbackSeconds && !m_podiumWanted) {
        m_podiumWanted = true;
        leaveRace();
    }
}

void RaceScreen::tickWire(int ticks) {
    if (!m_sim.hasLocal()) return;
    (void)ticks;
    // motion send 0x49BEB0 fires once the real clock clears the period frame clock is capped since server judges the wall
    const double now = App::uptimeMs() / 1000.0;
    if (m_stage != Stage::Loading && now - m_motionAt >= kMotionPeriod) {
        m_motionAt = now;
        m_wire.sendMotion(m_sim.localMotion());
    }
    if (m_stage == Stage::Racing) m_wire.sendProgress(progressScore(), m_time);
}

void RaceScreen::watchCheckpoints() {
    const int count = m_world.checkpointCount;
    if (count <= 0) return;
    const int face = m_sim.localCheckpointFace();
    if (face < 0 || face == m_lastFace) { if (face >= 0) m_lastFace = face; return; }
    m_lastFace = face;
    const int expected = (m_checkpoint + 1) % count;
    if (face != expected) return;
    m_wire.sendCheckpoint(static_cast<uint32_t>(m_checkpoint), static_cast<uint32_t>(face));
    m_checkpoint = face;
    if (face == 0) {
        ++m_laps;
        std::printf("[race] lap %d closed at %.1f s\n", m_laps, m_raceClock);
        const double lapTime = m_raceClock - m_lapStartClock;
        m_lapStartClock = m_raceClock;
        if (m_bestLap < 0.0 || lapTime < m_bestLap) m_bestLap = lapTime;
        const int total = static_cast<int>(m_world.files.laps);
        const bool finalLap = m_laps + 1 == total;
        m_app.sound().play(finalLap ? "lap_check_finallap_snd" : "lap_check_snd");
        // the stock words slide in at the start of lap two and of the final lap
        if (m_laps < total) {
            m_lapFlash = finalLap ? 2 : (m_laps == 1 ? 1 : 0);
            m_lapFlashAt = m_time;
        }
    }
}

uint32_t RaceScreen::progressScore() const {
    const int n = m_world.checkpointCount;
    if (n <= 0) return static_cast<uint32_t>(m_laps) * 5000u;
    const float bucket = 5000.f / static_cast<float>(n);
    const int next = (m_checkpoint + 1) % n;
    int wrapped = (next - 2) % n;
    if (wrapped < 0) wrapped += n;
    float frac = 0.f;
    const auto& pts = m_world.checkpointPoints;
    if (static_cast<int>(pts.size()) == n) {
        const CarPose pose = localPose();
        const CheckpointPoint3& a = pts[static_cast<size_t>(m_checkpoint)];
        const CheckpointPoint3& b = pts[static_cast<size_t>(next)];
        const float dx = b.x - a.x, dy = b.y - a.y;
        const float len2 = dx * dx + dy * dy;
        if (len2 > 1e-3f) frac = ((pose.x - a.x) * dx + (pose.y - a.y) * dy) / len2;
        if (frac < 0.f) frac = 0.f;
        if (frac > 1.f) frac = 1.f;
    }
    const float score = frac * bucket + static_cast<float>(wrapped) * bucket + static_cast<float>(m_laps) * 5000.f;
    return score <= 0.f ? 0u : static_cast<uint32_t>(score);
}

// itemdrum hit test 0x4bed40 the barrel under the swept heading point slows bounces or cancels the boost
void RaceScreen::watchDrums() {
    if (m_world.itemDrums.empty() || !m_sim.hasLocal()) return;
    const CarPose pose = localPose();
    const KnC::Kart::Client::GimmickDrumResult hit = KnC::Kart::Client::itemdrum_hit_test(
        m_world.itemDrums, m_drumBroken, pose.x, pose.y, pose.yawDeg, m_sim.localDriftGaugeSmoothed(),
        pose.speedKmh, pose.boosting);
    if (hit.kind == KnC::Kart::Client::GimmickDrumHit::None || hit.row < 0) return;
    m_sim.applyDrumHit(hit);
    // the barrel breaks once the stock hides the model and never brings it back in the same race
    if (static_cast<size_t>(hit.row) < m_drumBroken.size()) m_drumBroken[static_cast<size_t>(hit.row)] = 1;
    m_app.sound().play("itembox_block_snd");
    std::printf("[race] drum %d kind %d push %.1f at %.1f km per hour\n", hit.row, static_cast<int>(hit.kind),
                hit.pushStrength, pose.speedKmh);
}

void RaceScreen::watchGimmicks() {
    if (m_gimmicks.rows.empty() || !m_sim.hasLocal()) return;
    const int32_t row = KnC::Kart::Client::world_gimmick_hit_dispatch(m_sim.game(), 0, m_gimmicks, m_sim.nowMs());
    if (row < 0) return;
    const KnC::Kart::Client::GimmickInstance& hit = m_gimmicks.rows[static_cast<size_t>(row)];
    const int code = m_sim.car(0).effect.activeCode;
    std::printf("[race] gimmick row %d class %d code %d at %.1f %.1f %.1f\n", row, hit.gimmickClass, code, hit.x,
                hit.y, hit.z);
    // FUN 00481B60 the packet 105 notice of car gimmick hit our wire carries it on 0x0069
    if (code != 0) {
        m_wire.sendHit(static_cast<int16_t>(code));
        m_hitAt = m_time;
        raceEffects().hit(m_slots.empty() ? -1 : m_slots.front().viewHandle, 1000,
                          m_app.renderer().animation().seconds());
    }
    m_app.sound().play("itembox_block_snd");
}

// KNC REMOTE LEAN prints the ground under the four wheel corners of a bot and the roll it implies
void RaceScreen::logRemoteLean() {
    static const bool on = std::getenv("KNC_REMOTE_LEAN") != nullptr;
    if (!on) return;
    static double last = -1.0;
    if (m_time - last < 1.0) return;
    last = m_time;
    for (const Slot& s : m_slots) {
        if (s.carIndex <= 0) continue;
        const CarPose p = m_sim.pose(s.carIndex);
        // the four corners of a basic kart front left front right rear left rear right
        const float half = 0.9f, base = 1.4f;
        const float yaw = p.yawDeg * 3.14159265f / 180.f;
        const float c = std::cos(yaw), sn = std::sin(yaw);
        float z[4] = {p.z, p.z, p.z, p.z};
        KnC::Kart::Client::BspQuery probe;
        for (int i = 0; i < 4; ++i) {
            const float lx = (i < 2 ? base : -base), ly = (i % 2 == 0 ? half : -half);
            const float wx = p.x + lx * c - ly * sn;
            const float wy = p.y + lx * sn + ly * c;
            float ground = p.z;
            if (KnC::Kart::Client::world_locate_piece_by_height(probe, m_world.scene.collision, wx, wy, p.z,
                                                                &ground, nullptr))
                z[i] = ground;
            else
                z[i] = 9999.f;
        }
        const float roll = std::atan2((z[1] + z[3]) * 0.5f - (z[0] + z[2]) * 0.5f, half * 2.f) * 180.f / 3.14159265f;
        std::printf("[lean] car %d at %.1f %.1f %.2f wheels %.2f %.2f %.2f %.2f roll %.1f\n", s.carIndex, p.x, p.y,
                    p.z, z[0], z[1], z[2], z[3], roll);
    }
}

void RaceScreen::watchItems(bool useKey) {
    const CarPose pose = localPose();
    if (m_wire.heldItem() < 0) {
        const std::vector<ItemBoxSpot>& boxes = m_view.itemBoxes();
        for (size_t i = 0; i < boxes.size() && i < m_boxCooldown.size(); ++i) {
            if (m_boxCooldown[i] > 0.f) continue;
            const float dx = boxes[i].position[0] - pose.x;
            const float dy = boxes[i].position[1] - pose.y;
            if (dx * dx + dy * dy > kBoxReach * kBoxReach) continue;
            m_boxCooldown[i] = kBoxCooldown;
            // the box goes away on the touch and comes back once the cooldown runs out
            m_view.hideItemBox(i, m_app.renderer().animation().seconds() + kBoxCooldown);
            m_itemRng = m_itemRng * 1103515245u + 12345u;
            const int32_t kind = kItemKinds[(m_itemRng >> 16) % (sizeof(kItemKinds) / sizeof(kItemKinds[0]))];
            m_wire.sendItemGrant(kind);
            m_pickupAt = m_time;
            m_pickupItem = kind;
            m_useItemIn = 2.f + static_cast<float>(m_itemRng % 2000) / 1000.f;
            std::printf("[race] item box %zu gives %d\n", i, kind);
            m_app.sound().play("itembox_get_snd");
            break;
        }
        return;
    }
    if (useKey) {
        m_app.sound().play(itemUseSound(m_wire.heldItem()));
        m_wire.sendItemUse(m_wire.heldItem(), pose.x, pose.y, pose.z, pose.yawDeg);
        m_useItemIn = -1.f;
        std::printf("[race] item used\n");
    }
}

// the item boxes come off the race view the session keeps one cooldown per spot beside them
void RaceScreen::loadItemBoxes() {
    m_boxCooldown.assign(m_view.itemBoxes().size(), 0.f);
    std::printf("[race] %zu item boxes on the track\n", m_view.itemBoxes().size());
}

// the boxes that served their cooldown draw again the view holds the respawn clock
void RaceScreen::updateItemBoxes() {
    m_view.refreshItemBoxes(m_app.renderer().animation().seconds());
}

bool RaceScreen::drawScene() {
    if (!m_worldLoaded || !m_view.loaded()) return false;
    float introEye[3] = {0.f, 0.f, 0.f};
    float introLook[3] = {0.f, 0.f, 0.f};
    if (m_stage == Stage::Podium && m_podium.loaded()) {
        float eye[3], look[3], fov = 0.f;
        m_podium.camera(eye, look, fov);
        m_view.drawFixed(m_app.renderer(), eye, look, fov, m_frameDt);
    } else if (m_stage == Stage::Finished) {
        m_view.drawFinish(m_app.renderer(), m_chase, m_frameDt);
    } else if (introCamera(introEye, introLook)) {
        // the intro of FUN 004024f0 the fly over then the front view the chase eases in after it
        m_view.drawFixed(m_app.renderer(), introEye, introLook, kIntroFieldRadians, m_frameDt);
    } else if (m_stage == Stage::Racing && m_app.raceKeyDown(RaceKey::Back)) {
        // camera update 0x43F040 stage 11 before the finish the back row looks behind the kart
        float eye[3], look[3];
        lookBackCamera(localPose(), eye, look);
        m_view.drawFixed(m_app.renderer(), eye, look, kLookBackFieldRadians, m_frameDt);
    } else {
        m_view.draw(m_app.renderer(), m_chase, m_frameDt);
    }
    // sub 4D1BE0 the weather veil lands over the world and under the hud the gauge scene included
    const uint8_t veil = m_view.weatherVeil();
    m_veilInScene = veil != 0 && m_stage < Stage::Finished;
    if (m_veilInScene)
        m_app.renderer().draw_frame_veil(KnC::Render::HourColour{0.f, 0.f, 0.f}, static_cast<float>(veil) / 255.f);
    // sub 4AE230 the gauge nifs into the gauge rect the needle the disc and the digits go over them
    if (m_stage < Stage::Finished) {
        HudState state;
        fillHud(state);
        m_gauge.draw(m_app.renderer(), state, m_frameDt, m_app.width(), m_app.height(), m_app.canvasWidth(),
                     m_app.canvasHeight());
    }
    return true;
}

void RaceScreen::fillHud(HudState& s) const {
    const CarPose pose = localPose();
    s.speedKmh = pose.speedKmh;
    s.position = m_wire.localPosition() + 1;
    s.racers = static_cast<int>(m_wire.racers().size());
    s.totalLaps = static_cast<int>(m_world.files.laps);
    s.lap = std::min(m_laps + 1, s.totalLaps);
    s.raceSeconds = m_raceClock;
    s.bestLapSeconds = m_bestLap;
    s.heldItem = m_wire.heldItem();
    // the Panel Stream label of the port draft below zero keeps it hidden
    s.slipStream = m_sim.slipStreamBonus();
    // 0x4AE230 the band shows in the race stage of a speed mode the item modes keep the plain board
    const bool speedMode = raceGameMode() == 2 || raceGameMode() == 3;
    if (speedMode && m_stage >= Stage::Countdown && m_stage < Stage::Finished) {
        s.driftGauge = m_sim.gaugeFill();
        // KNC GAUGE TEST paints a fixed band 0 to 127 a capture aid for the art
        if (const char* forced = std::getenv("KNC_GAUGE_TEST")) s.driftGauge = static_cast<float>(std::atof(forced));
        s.driftGaugeBlue = m_sim.gaugeFillBlue();
        s.driftGaugeSpark = m_sim.gaugeSpark();
        s.gaugeFlash = m_sim.gaugeFlash();
        s.gaugeFlashAge = m_sim.gaugeFlashAge();
    }
    s.clock = m_time;
    s.gameMode = raceGameMode();
    s.minimap = &m_world.minimap;
    // the countdown nif draws the digits in the world when it loaded
    s.countdownProp = m_countdownProp >= 0;
    if (m_stage == Stage::Countdown) {
        const float left = kCountdownSeconds - m_stageTime;
        s.waiting = left > 3.f;
        // start 03 is the three start 01 the one start 04 the GO each grows over its second
        s.countdownStage = left > 3.f ? 0 : left > 2.f ? 3 : left > 1.f ? 2 : 1;
        if (s.countdownStage > 0) s.countdownAge = static_cast<double>(static_cast<float>(s.countdownStage) - left);
    } else if (m_stage == Stage::Racing && m_stageTime < 1.f) {
        s.countdownStage = 4;
        s.countdownAge = m_stageTime;
    } else if (m_stage == Stage::Loading || m_stage == Stage::Grid) {
        s.waiting = true;
    }
    s.reverse = pose.reversing && m_stage == Stage::Racing;
    s.finalLap = m_laps + 1 == s.totalLaps && m_stage == Stage::Racing;
    s.finished = m_stage >= Stage::Finished;
    s.finishRank = m_wire.reward().valid ? m_wire.reward().finishRank : -1;
    s.paused = m_paused;
    // FUN 004B73F0 draws the board in the game state 0xB while the flag of the 0x0046 handler stands
    s.boardOpen = m_boardOpen;
    if (m_lapFlash != 0) {
        s.lapFlash = m_lapFlash;
        s.lapFlashAge = m_time - m_lapFlashAt;
        if (s.lapFlashAge > 4.0) s.lapFlash = 0;
    }
    if (m_pickupAt >= 0.0) { s.pickupAge = m_time - m_pickupAt; s.pickupItem = m_pickupItem; }
    if (m_hitAt >= 0.0) { s.hitAge = m_time - m_hitAt; s.hitItem = -1; }
    for (const Racer& r : m_wire.racers()) {
        HudStanding row;
        row.name = u16ToUtf8(r.name);
        row.driverAsset = driverAsset(r.driverKey);
        row.position = r.position;
        row.pingMs = r.pingMs;
        row.team = r.team;
        row.local = r.local;
        row.finished = r.finishRank >= 0 || r.animState == 9;
        s.standings.push_back(row);
    }
    // the view of the last draw and the lens of the frame put a name tag over every other kart
    float view[16], eye[3], proj[16];
    const bool camera = m_view.camera().valid;
    if (camera) {
        m_view.lastView(view, eye);
        m_app.renderer().projection(proj);
    }
    const float scale = std::min(static_cast<float>(m_app.width()) / m_app.canvasWidth(), static_cast<float>(m_app.height()) / m_app.canvasHeight());
    const float offX = (static_cast<float>(m_app.width()) - m_app.canvasWidth() * scale) * 0.5f;
    const float offY = (static_cast<float>(m_app.height()) - m_app.canvasHeight() * scale) * 0.5f;
    for (const Slot& slot : m_slots) {
        if (slot.carIndex < 0) continue;
        const Racer* racer = nullptr;
        for (const Racer& r : m_wire.racers()) if (r.playerId == slot.playerId) racer = &r;
        HudCar car;
        // the podium seats the karts so the plate reads the pose the view was given not the sim one
        const CarPose p = m_stage == Stage::Podium ? slot.shown : m_sim.pose(slot.carIndex);
        car.x = p.x;
        car.y = p.y;
        car.driverAsset = racer ? driverAsset(racer->driverKey) : std::string("Cosmo");
        car.local = racer ? racer->local : false;
        s.cars.push_back(car);
        // FUN 00499180 skips the own kart unless the podium flag is set
        if (!camera || !racer || (racer->local && m_stage != Stage::Podium)) continue;
        const float world[4] = {p.x, p.y, p.z + 2.2f, 1.f};
        float e[4], c[4];
        bx::vec4MulMtx(e, world, view);
        bx::vec4MulMtx(c, e, proj);
        if (c[3] <= 0.001f) continue;
        const float nx = c[0] / c[3];
        const float ny = c[1] / c[3];
        if (nx < -1.f || nx > 1.f || ny < -1.f || ny > 1.f) continue;
        HudNameTag tag;
        tag.x = ((nx * 0.5f + 0.5f) * static_cast<float>(m_app.width()) - offX) / scale;
        tag.y = ((0.5f - ny * 0.5f) * static_cast<float>(m_app.height()) - offY) / scale;
        // FUN 00499180 raises the plate by 0x32 on the podium
        if (m_stage == Stage::Podium) tag.y -= 50.f;
        tag.name = u16ToUtf8(racer->name);
        s.tags.push_back(tag);
    }
    s.status = m_status;
}

void RaceScreen::draw(SpriteBatch& batch) {
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    const float w = m_app.canvasWidth();
    const float h = m_app.canvasHeight();
    if (!m_worldLoaded) {
        batch.fill(0.f, 0.f, w, h, rgba(20, 20, 30, 255));
        ctx.font.drawCentered(batch, m_status, w * 0.5f, h * 0.5f - 10.f, 18.f, kWhite);
        ctx.font.drawCentered(batch, "Escape leaves the race", w * 0.5f, h * 0.5f + 30.f, 14.f, rgba(255, 220, 90, 255));
        return;
    }
    HudState state;
    fillHud(state);
    // sub 4D1BE0 the weather veil lands over the scene and under the hud the world frame drew it
    const uint8_t veil = m_view.weatherVeil();
    if (veil != 0 && !m_veilInScene) batch.fill(0.f, 0.f, w, h, rgba(0, 0, 0, veil));
    // the band is the gauge nif of the world frame the sprite hud leaves the board and the arc out
    state.gaugeArt = m_gauge.loaded();
    // the stock hud goes away with the finish camera the board of 0x0046 comes over it
    if (m_stage < Stage::Finished) {
        m_hud.draw(ctx, m_app.assets(), state, w, h);
        if (m_paused) m_hud.drawPause(ctx, m_app.assets(), w, h);
        return;
    }
    drawFinishOverlay(ctx, state, w, h);
}

// the 0x0046 rows the board draws as the stock sub 4B6D00 and sub 4B6D50 list them
std::vector<HudResultRow> RaceScreen::boardRows() const {
    std::vector<HudResultRow> rows;
    for (const Racer& r : m_wire.racers()) {
        if (!r.onBoard) continue;
        HudResultRow row;
        row.name = u16ToUtf8(r.name);
        row.driverAsset = driverAsset(r.driverKey);
        row.rank = r.finishRank;
        row.timeMs = r.finishTimeMs;
        row.gold = r.gold;
        row.exp = r.exp;
        row.goldBonus = r.goldBonus;
        row.expBonus = r.expBonus;
        row.local = r.local;
        rows.push_back(row);
    }
    return rows;
}

void RaceScreen::drawFinishOverlay(DrawContext& ctx, const HudState& state, float w, float h) {
    SpriteBatch& batch = ctx.batch;
    // the name plates over the karts the same plate the race hud draws bold 11 on 16 px of dark
    for (const HudNameTag& tag : state.tags) {
        const float width = ctx.bold.measure(tag.name, 11.f) + 8.f;
        const float x = std::floor(tag.x - width * 0.5f);
        const float y = std::floor(tag.y - 8.f);
        batch.fill(x, y, width, 16.f, rgba(30, 30, 30, 150));
        ctx.bold.draw(batch, tag.name, x + 4.f, y + 2.f, 11.f, kWhite);
    }
    // FUN 004B73F0 the board of the game state 0xB over a 0x60 veil while the flag of 0x0046 stands
    if (m_boardOpen) m_hud.drawResult(ctx, m_app.assets(), boardRows(), state.gameMode, w, h);
    // the system line of the last game message at the top right as the stock message area shows it
    if (m_systemLineAt >= 0.0 && m_time - m_systemLineAt < kSystemLineSeconds && !m_systemLine.empty()) {
        const float width = ctx.bold.measure(m_systemLine, 13.f);
        ctx.bold.draw(batch, m_systemLine, w - 24.f - width + 1.f, 11.f, 13.f, rgba(0, 0, 0, 160));
        ctx.bold.draw(batch, m_systemLine, w - 24.f - width, 10.f, 13.f, kWhite);
    }
    // FUN 0043D7E0 the black fade mode 2 out before the podium mode 1 in after
    if (m_fade > 0.f) {
        const float alpha = std::min(1.f, m_fade) * 255.f;
        batch.fill(0.f, 0.f, w, h, rgba(0, 0, 0, static_cast<uint8_t>(alpha)));
    }
}

// the quit box OK leaves the race Cancel closes it
void RaceScreen::onMouseButton(int button, int action, float x, float y) {
    if (!m_paused || button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    if (RaceHud::pauseRow(0).contains(x, y)) { m_app.click(); leaveRace(); return; }
    if (RaceHud::pauseRow(1).contains(x, y)) { m_app.click(); m_paused = false; return; }
}

void RaceScreen::leaveRace() {
    m_status = "leaving 0x003B";
    m_wire.sendLeave();
}

void RaceScreen::onKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) {
        if (!m_worldLoaded || m_stage >= Stage::Finished) { leaveRace(); return; }
        m_paused = !m_paused;
        return;
    }
    if (m_paused && (key == GLFW_KEY_Q || key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)) { leaveRace(); return; }
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_3) m_sim.pressKey(kVkOne + (key - GLFW_KEY_1));
}

void RaceScreen::onSession(SessionEvent event) {
    (void)event;
}

}
