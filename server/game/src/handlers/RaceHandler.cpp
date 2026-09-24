#include "handlers/RaceHandler.h"
#include "handlers/AntiCheatHandler.h"
#include "handlers/GachaHandler.h"
#include "handlers/InventoryHandler.h"
#include "handlers/MissionHandler.h"
#include "handlers/ProgressionHandler.h"
#include "packets/gen/PartStatPackets.h"
#include "util/KartDurability.h"
#include "util/SlotExchange.h"
#include "packets/gen/InventoryPackets.h"
#include "GameServer.h"
#include "GameServerInternal.h"
#include "packets/PacketBuilder.h"
#include "packets/gen/RacePackets.h"
#include "game/Room.h"
#include "logging/Logger.h"
#include "db/Database.h"
#include "util/DbRowWire.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>

namespace {
// track catalog 10000000 random links map 200 a row with no folders
constexpr uint8_t kRandomMapId = 200;
}
#include <tuple>
#include <utility>

namespace knc {

namespace {

// reject item use faster than this
constexpr int64_t ITEM_USE_MIN_INTERVAL_MS = 250;
// drop same victim plus item inside this window
constexpr int64_t HIT_DEDUPE_WINDOW_MS = 500;

// client self reports every 100 ms sub 495B20 faster only raises the discard rate
constexpr uint64_t MOTION_FANOUT_MS = 100;
// S2C 0x45 standings measured at 2 Hz on a live chibikart race 750 rows over 134 s
constexpr uint64_t STANDINGS_PERIOD_MS = 500;
// five missed samples then drop the car from the fan out and let it coast to rest
constexpr uint64_t MOTION_STALE_MS = 500;
// client recv buffer is a fixed 8192 one bigger write corrupts its parse state
constexpr size_t MAX_FRAME_BYTES = 0x2000;
constexpr size_t FRAME_HEADER_BYTES = 8;

// velocity clamp xy 120 z 60 wu s from 0x5a6a14 0x5a6a68 old 50 ms floor banned legal 100 ms frames
constexpr MotionGateLimits MOTION_LIMITS{};

// sub 479CC0 reads only the first dword unless kind is 3 or 8
constexpr int32_t RACE_SETUP_KIND = 3;

// hard cap from the anti cheat escalation ladder shared by every gate here
constexpr int32_t SUSPICION_KICK = 10;
// decays one strike per this many ms of clean race so honest play recovers slower than a real cheat spikes
constexpr uint64_t SUSPICION_DECAY_MS = 3000;

// client docks itself on leave race so the server must dock the same wallet
constexpr int32_t RETIRE_MIN_CARS = 2;

// the order every standings frame uses time first then the server place key then the 0x67 score lap and height
bool rankLess(const RacePlayer* a, const RacePlayer* b) {
    if (a->finished != b->finished) return a->finished;
    if (a->finished && b->finished) return a->totalTime < b->totalTime;
    if (a->rankKey >= 0.0f && b->rankKey >= 0.0f && a->rankKey != b->rankKey) return a->rankKey > b->rankKey;
    if (a->haveProgress && b->haveProgress && a->progressScore != b->progressScore) {
        return a->progressScore > b->progressScore;
    }
    if (a->lap != b->lap) return a->lap > b->lap;
    return a->z > b->z;
}

// humans from their checked 0x41 faces bots from their follower all on one scale
void refreshRankKeys(const RoomRaceLive& live, std::vector<RacePlayer>& roster) {
    for (auto& p : roster) {
        p.rankKey = -1.0f;
        if (live.checkpointPoints.size() < 2) continue;
        const BotRacer* bot = nullptr;
        for (const auto& b : live.bots) if (b.playerId == p.playerId) { bot = &b; break; }
        if (bot) {
            p.rankKey = raceRankKey(live.checkpointPoints, bot->checkpoints.laps, bot->checkpoints.checkpoint, p.x, p.y);
        } else if (p.lapTracker.checkpointCount() == static_cast<int32_t>(live.checkpointPoints.size())) {
            p.rankKey = raceRankKey(live.checkpointPoints, clientLapsFromTracker(p.lapTracker),
                                    p.lapTracker.nextCheckpoint(), p.x, p.y);
        }
    }
}

// exclude by character id because a bot has no session to exclude by
void sendToOthers(Room* room, const Packet& pkt, int32_t exceptCharacterId) {
    for (const auto& s : room->sessions()) {
        if (static_cast<int32_t>(s->characterId) == exceptCharacterId) continue;
        if (!s->isConnected()) continue;
        s->send(pkt);
    }
}

int32_t rowInt(const std::map<std::string, std::string>& row, const char* key, int32_t def) {
    return static_cast<int32_t>(rowInt64Throwing(row, key, def));
}


} // namespace

void RaceHandler::handleStartRace(Session::Ptr session, Room* room, GameServer* server) {
    if (!room) return;
    if (room->hostId() != session->id()) {
        LOG_WARN("RACE", "Non-host tried to start race");
        return;
    }

    if (!room->canStart()) {
        LOG_WARN("RACE", "Cannot start race - conditions not met");
        return;
    }

    // the seats close here so no match tick can seat one more bot between this roster and the grid
    room->setState(RoomState::Starting);

    // a throw below must hand the room back or the master can never press start again
    struct StartGuard {
        Room* room;
        bool armed = true;
        ~StartGuard() { if (armed && room) room->setState(RoomState::Waiting); }
    } guard{room};

    // race takes exactly the room's seats humans and bots from the one list the room screen drew
    const std::vector<RoomPlayer> seats = room->participants();

    clearRoom(room->id());
    for (const auto& seat : seats) {
        addPlayer(room->id(), seat.characterId);
    }

    // never broadcast 0xBE here sub 478B50 wipes twenty login catalog containers not just the race array

    // one 0xBF per seat bot or human the client cannot tell the two apart
    for (const auto& seat : seats) {
        std::u16string playerName = seat.name;
        if (!seat.isBot) {
            for (const auto& s : room->sessions()) {
                if (static_cast<int32_t>(s->characterId) != seat.characterId) continue;
                playerName = s->characterName;
                break;
            }
        }

        // vehicle then driver then slot then pad
        const int32_t vehicleId = seat.vehicleTemplateId;
        const int32_t driverId  = seat.driverId;
        uint8_t data20[20] = {};
        data20[0] = static_cast<uint8_t>(vehicleId & 0xFF);
        data20[1] = static_cast<uint8_t>((vehicleId >> 8) & 0xFF);
        data20[2] = static_cast<uint8_t>((vehicleId >> 16) & 0xFF);
        data20[3] = static_cast<uint8_t>((vehicleId >> 24) & 0xFF);
        data20[4] = static_cast<uint8_t>(driverId & 0xFF);
        data20[5] = static_cast<uint8_t>((driverId >> 8) & 0xFF);
        data20[6] = static_cast<uint8_t>((driverId >> 16) & 0xFF);
        data20[7] = static_cast<uint8_t>((driverId >> 24) & 0xFF);
        data20[8] = seat.slot;
        // tail stays zero

        room->broadcast(PacketBuilder::racePlayer1(
            seat.characterId,
            vehicleId,
            driverId,
            static_cast<int32_t>(seat.slot),
            0,
            playerName,
            data20,
            playerName,
            u"",
            {}
        ));
    }

    // random on the track panel is a fake row with no folder draw a real track the room map drives
    if (room->settings().mapId == kRandomMapId) {
        auto pick = Database::instance().queryPrepared(
            "SELECT t.track_id, t.map_id FROM track_catalog t JOIN maps m ON m.id = t.map_id "
            "WHERE t.track_id BETWEEN 10 AND 99 AND m.track_folder IS NOT NULL "
            "AND m.track_folder <> '' ORDER BY RAND() LIMIT 1", {});
        if (!pick.empty()) {
            const int32_t trackId = std::stoi(pick[0].at("track_id"));
            const int32_t mapId   = std::stoi(pick[0].at("map_id"));
            room->settings().mapId = static_cast<uint8_t>(mapId & 0xFF);
            room->broadcast(PacketBuilder::roomTrackSelect(trackId, 0));
            LOG_INFO("RACE", "random drew track " + std::to_string(trackId) + " map " +
                     std::to_string(mapId) + " for room " + std::to_string(room->id()));
        } else {
            LOG_ERROR("RACE", "random has no race track with a folder to draw from");
        }
    }

    initRaceLive(room);
    loadTrackData(room);

    // stage 11 request goes out now so the world has the whole countdown to load
    sendRaceSetup(room);

    guard.armed = false;
    startCountdown(room->id(), server);
}

// track spawn rows plus the track COL checkpoint count for this map
void RaceHandler::loadTrackData(Room* room) {
    if (!room) return;

    const int32_t trackId   = static_cast<int32_t>(room->settings().mapId);
    const int32_t totalLaps = static_cast<int32_t>(room->settings().laps);

    std::vector<GridSpawn> grid = MotionPackets::gridSpawns(trackId);
    if (grid.empty()) {
        LOG_WARN("RACE", "no track_spawn rows for track " + std::to_string(trackId) +
                 " no S2C 0x68 placement client start ini owns the grid");
    }

    // folders live on maps not on a track catalog which this schema has no table for
    std::string themeFolder;
    std::string trackFolder;
    auto rows = Database::instance().queryPrepared(
        "SELECT theme_folder, track_folder FROM maps WHERE id = ? LIMIT 1",
        {trackId});
    if (rows.empty()) {
        LOG_WARN("RACE", "map row missing for track " + std::to_string(trackId) +
                 " lap tracking off");
    } else {
        auto tf = rows[0].find("theme_folder");
        auto kf = rows[0].find("track_folder");
        if (tf != rows[0].end()) themeFolder = tf->second;
        if (kf != rows[0].end()) trackFolder = kf->second;
    }

    // a short track spawn table used to drop every seated racer past its row count widen it first
    size_t seatCount = 0;
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        seatCount = m_racePlayers[room->id()].size();
    }
    if (grid.size() < seatCount && !themeFolder.empty() && !trackFolder.empty()) {
        const std::vector<SpawnPackets::TrackPoint> ini =
            SpawnPackets::loadStartGrid(themeFolder, trackFolder);
        if (ini.size() > grid.size()) {
            LOG_WARN("RACE", "track " + std::to_string(trackId) +
                     " db grid short falling back to " + std::to_string(ini.size()) +
                     " start ini rows");
            grid.clear();
            grid.reserve(ini.size());
            for (size_t i = 0; i < ini.size(); ++i) {
                GridSpawn s;
                s.gridIndex  = static_cast<int32_t>(i);
                s.x          = ini[i].x;
                s.y          = ini[i].y;
                s.z          = ini[i].z;
                s.yawDegrees = ini[i].yawDegrees;
                grid.push_back(s);
            }
        }
    }
    if (grid.size() < seatCount) {
        LOG_WARN("RACE", "track " + std::to_string(trackId) + " grid still short generating " +
                 std::to_string(seatCount - grid.size()) + " rows so no racer is dropped");
        grid = MotionPackets::padGeneratedGrid(std::move(grid), seatCount);
    }

    int32_t checkpointCount = 0;
    std::vector<SpawnPackets::ColPadCell> pads;
    bool padsLoaded = false;
    std::vector<SpawnPackets::TrackVec3> checkpointPoints;
    if (themeFolder.empty() || trackFolder.empty()) {
        LOG_WARN("RACE", "track " + std::to_string(trackId) +
                 " has no theme or track folder lap tracking off");
    } else {
        SpawnPackets::ColCheckpoints col;
        if (SpawnPackets::loadTrackCheckpoints(themeFolder, trackFolder, col)) {
            checkpointCount = col.checkpointCount;
            if (static_cast<int32_t>(col.points.size()) == col.checkpointCount) checkpointPoints = col.points;
            // the client starts a pad boost alone the observer needs the cells to tell it from a cheat
            pads = std::move(col.pads);
            padsLoaded = true;
        } else {
            LOG_WARN("RACE", "track COL unreadable at " +
                     SpawnPackets::trackBase(themeFolder, trackFolder) +
                     " lap tracking off set KNC_DATA_ROOT");
        }
    }

    // the racing line and item boxes the CPU cars drive off a track with neither keeps the clock finishers
    BotTrack botTrack = BotTrack::load(themeFolder, trackFolder);

    std::lock_guard<std::mutex> lock(m_raceMutex);
    RoomRaceLive& live = m_roomLive[room->id()];
    live.trackId         = trackId;
    live.botTrack        = std::move(botTrack);
    live.totalLaps       = totalLaps > 0 ? totalLaps : 1;
    live.checkpointCount = checkpointCount;
    live.grid            = std::move(grid);
    live.lapTrackingLive = checkpointCount > 0;
    live.pads            = std::move(pads);
    live.checkpointPoints = std::move(checkpointPoints);
    live.padsLoaded      = padsLoaded;

    for (auto& p : m_racePlayers[room->id()]) {
        p.lapTracker.reset(live.checkpointCount, live.totalLaps);
    }

    LOG_INFO("RACE", "track " + std::to_string(trackId) + " spawns " +
             std::to_string(live.grid.size()) + " checkpoints " +
             std::to_string(live.checkpointCount) + " pad cells " +
             std::to_string(live.pads.size()) + " laps " +
             std::to_string(live.totalLaps));
}

// S2C 0xC4 theme rows then S2C 0xC3 track rows both from the maps table
void RaceHandler::sendTrackCatalog(Room* room) {
    if (!room) return;

    auto rows = Database::instance().queryPrepared(
        "SELECT id, theme_folder, track_folder, required_level FROM maps "
        "WHERE theme_folder IS NOT NULL AND track_folder IS NOT NULL ORDER BY id", {});
    if (rows.empty()) {
        LOG_WARN("RACE", "maps has no folder rows so no 0xC3 or 0xC4 goes out and "
                 "sub_4875C0 cannot build a world path");
        return;
    }

    const int32_t racedTrack = static_cast<int32_t>(room->settings().mapId);
    const int32_t roomLaps   = static_cast<int32_t>(room->settings().laps);

    // theme ids are ours the schema has no theme table the client only joins 0xC3 to 0xC4
    std::map<std::string, uint32_t> themeIds;
    for (const auto& r : rows) {
        auto it = r.find("theme_folder");
        if (it == r.end() || it->second.empty()) continue;
        if (themeIds.find(it->second) != themeIds.end()) continue;
        if (themeIds.size() >= SpawnPackets::MAX_THEME_ROWS) {
            LOG_WARN("RACE", "theme " + it->second + " past the 16 client rows dropped");
            continue;
        }
        themeIds[it->second] = static_cast<uint32_t>(themeIds.size() + 1);
    }

    size_t bytes = 0;

    // themes go first every track row names one of them
    for (const auto& kv : themeIds) {
        SpawnPackets::ThemeCatalogRow t;
        t.themeId     = kv.second;
        t.themeFolder = kv.first;
        t.displayName = kv.first;
        Packet pkt = SpawnPackets::themeCatalogEntry(t);
        bytes += pkt.payload().size() + FRAME_HEADER_BYTES;
        room->broadcast(pkt);
    }

    size_t sent = 0;
    for (const auto& r : rows) {
        if (sent >= SpawnPackets::MAX_TRACK_ROWS) {
            LOG_WARN("RACE", "track catalog capped at 128 rows the rest are dropped");
            break;
        }
        auto tf = r.find("theme_folder");
        auto kf = r.find("track_folder");
        if (tf == r.end() || kf == r.end() || tf->second.empty() || kf->second.empty()) continue;
        auto themeIt = themeIds.find(tf->second);
        if (themeIt == themeIds.end()) {
            LOG_WARN("RACE", "track " + kf->second + " has a dropped theme so it is dropped too");
            continue;
        }

        const int32_t trackId = rowInt(r, "id", 0);

        SpawnPackets::TrackCatalogRow t;
        t.trackId       = static_cast<uint32_t>(trackId);
        t.themeId       = themeIt->second;
        t.folderName    = kf->second;
        t.requiredLicense = static_cast<uint32_t>(rowInt(r, "required_level", 0));  // maps column the 0xC3 gate is the license class
        // lap count drives the board total so the raced track carries the room setting
        t.lapCount      = static_cast<uint32_t>(
            trackId == racedTrack && roomLaps >= SpawnPackets::LAP_BOARD_MIN &&
            roomLaps <= SpawnPackets::LAP_BOARD_MAX ? roomLaps : 3);
        // every remaining field has no reader or no source and stays zero

        Packet pkt = SpawnPackets::trackCatalogEntry(t);
        bytes += pkt.payload().size() + FRAME_HEADER_BYTES;
        room->broadcast(pkt);
        ++sent;
    }

    if (bytes >= MAX_FRAME_BYTES) {
        LOG_WARN("RACE", "track catalog burst is " + std::to_string(bytes) +
                 " bytes past the client 8192 recv buffer it needs dripping");
    }

    LOG_INFO("RACE", "track catalog " + std::to_string(themeIds.size()) + " themes " +
             std::to_string(sent) + " tracks " + std::to_string(bytes) + " bytes");
}

// S2C 0x33 0x34 then the catalogs then S2C 0x14 which asks the client for stage 11
void RaceHandler::sendRaceSetup(Room* room) {
    if (!room) return;

    room->broadcast(PacketBuilder::raceStart());
    room->broadcast(PacketBuilder::flag34());

    // login already owns S2C 0xC3 with the real track ids this second publisher used fake ids so it was removed

    int32_t racerCount = 0;
    int32_t trackId    = 0;
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        racerCount = static_cast<int32_t>(m_racePlayers[room->id()].size());
        trackId    = m_roomLive[room->id()].trackId;
    }
    if (trackId == 0) trackId = static_cast<int32_t>(room->settings().mapId);

    // field is track catalog track id not the maps id else map 1 loads a track with no world folder
    int32_t catalogTrack = trackId;
    {
        auto hit = Database::instance().queryPrepared(
            "SELECT track_id FROM track_catalog WHERE map_id = ? LIMIT 1", {trackId});
        if (!hit.empty()) catalogTrack = std::stoi(hit[0].at("track_id"));
        else LOG_WARN("RACE", "map " + std::to_string(trackId) +
                      " has no track_catalog row so the client loads the wrong folder");
    }

    // kind unk track mode count flag kind must be 3 or 8 else only the first dword is read
    room->broadcast(PacketBuilder::gameModeSetupFull(
        RACE_SETUP_KIND,
        0,
        catalogTrack,
        static_cast<int32_t>(room->settings().mode),
        racerCount,
        0));
}

void RaceHandler::initRaceLive(Room* room) {
    if (!room) return;

    struct Pre {
        uint32_t playerId = 0;
        bool     thirdSlot = false;
        int32_t  tickets = 0;
        Session::Ptr session;
    };

    // db read outside the lock one query pair per human per race
    std::vector<Pre> pre;
    for (const auto& s : room->sessions()) {
        if (Room::isBotId(static_cast<int32_t>(s->characterId))) continue;
        Pre p;
        p.playerId  = s->characterId;
        p.thirdSlot = ItemPackets::loadThirdSlotUnlocked(p.playerId);
        p.tickets   = ItemPackets::loadSwapTicketQuantity(p.playerId);
        p.session   = s;
        pre.push_back(p);
    }

    std::lock_guard<std::mutex> lock(m_raceMutex);
    RoomRaceLive& live = m_roomLive[room->id()];
    live = RoomRaceLive{};
    for (const auto& p : pre) {
        live.items.addPlayer(p.playerId, p.thirdSlot, p.tickets);
        motionSlot(live, static_cast<int32_t>(p.playerId), p.session);
    }
}

uint64_t RaceHandler::nowMs() {
    static const std::chrono::steady_clock::time_point base = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - base).count());
}

// body size picks the form not the room state
void RaceHandler::handleMotion(Session::Ptr session, Packet& packet, Room* room) {
    const bool rawWorld = packet.payload().size() >= MotionPackets::kSelfReportSizeRaw;
    CarState state;
    if (!MotionPackets::parseSelfReport(packet, rawWorld, state)) return;
    handleMotion(session, packet, room, state, rawWorld);
}

// C2S 0x40 from the waiting room the stock drives the kart on the room field in stage 9
void RaceHandler::handleRoomMotion(Session::Ptr session, Packet& packet, Room* room) {
    if (!room || room->state() != RoomState::Waiting) return;
    const bool rawWorld = packet.payload().size() >= MotionPackets::kSelfReportSizeRaw;
    CarState state;
    if (!MotionPackets::parseSelfReport(packet, rawWorld, state)) return;
    // no race roster and no judge here the room seat is cosmetic the relay is one entry
    MotionEntry entry;
    entry.playerId = static_cast<uint32_t>(session->characterId);
    entry.interpScale = MotionPackets::kInterpScaleDefault;
    entry.state = state;
    const std::vector<MotionEntry> one{entry};
    // never a client its own id the local car runs its own tick and skips the queue
    room->broadcastExcept(MotionPackets::motionBroadcast(one, rawWorld), session->id());
}

// C2S 0x40 in race own motion sub 4818A0 no player id on the wire
void RaceHandler::handleMotion(Session::Ptr session, Packet& packet, Room* room,
                               const CarState& state, bool rawWorld) {
    (void)packet;
    if (!room) return;
    if (room->state() != RoomState::Racing) return;

    // identity comes from the socket the packet never carries one
    const int32_t senderId = static_cast<int32_t>(session->characterId);
    const uint64_t nowStamp = nowMs();

    // unique so the kick below can drop the lock the leave path takes it again
    std::unique_lock<std::mutex> lock(m_raceMutex);
    auto* player = getPlayer(room->id(), senderId);
    if (!player) return;

    // leaky bucket first motion ticks run all race so this drains the ladder no matter which gate added the strike
    if (player->suspiciousCount > 0) {
        if (player->lastSuspicionDecayMs == 0) player->lastSuspicionDecayMs = nowStamp;
        const uint64_t since = nowStamp > player->lastSuspicionDecayMs
                             ? nowStamp - player->lastSuspicionDecayMs : 0;
        const int32_t bleed = static_cast<int32_t>(since / SUSPICION_DECAY_MS);
        if (bleed > 0) {
            player->suspiciousCount = player->suspiciousCount > bleed
                                    ? player->suspiciousCount - bleed : 0;
            player->lastSuspicionDecayMs = nowStamp;
        }
    } else {
        player->lastSuspicionDecayMs = nowStamp;
    }

    // S2C 0x45 standings chibikart sends one row per racer twice a second we had the builder never called
    {
        auto& live = m_roomLive[room->id()];
        if (nowStamp - live.lastStandingsMs >= STANDINGS_PERIOD_MS) {
            live.lastStandingsMs = nowStamp;
            auto& roster = m_racePlayers[room->id()];
            if (!roster.empty()) {
                // zero based place in the same order calculatePositions writes used to send only leader as one flipping the HUD
                refreshRankKeys(live, roster);
                std::vector<RacePlayer*> order;
                order.reserve(roster.size());
                for (auto& p : roster) order.push_back(&p);
                std::sort(order.begin(), order.end(),
                          [](const RacePlayer* a, const RacePlayer* b) { return rankLess(a, b); });
                for (size_t i = 0; i < order.size(); ++i) {
                    order[i]->position = static_cast<uint8_t>(i + 1);
                    room->broadcast(ItemPackets::standingsUpdate(
                        static_cast<uint32_t>(order[i]->playerId), static_cast<int32_t>(i)));
                }
            }
        }
    }

    auto now = std::chrono::steady_clock::now();
    bool forceSnap = false;

    // every sample becomes the new truth position is client authority the gate only judges it
    float motionBudget = 0.0f;
    const MotionVerdict motionVerdict = MotionPackets::motionStep(
        player->motionGate, MOTION_LIMITS, state.x, state.y, state.z, nowStamp, &motionBudget);
    switch (motionVerdict) {
        case MotionVerdict::FirstSample:
            forceSnap = true;
            break;
        case MotionVerdict::Snap:
            // a rescue lands on the nearest follow path node up to 80 units away and is not a cheat
            LOG_DEBUG("RACE", "motion snap char " + std::to_string(session->characterId) +
                      " budget " + std::to_string(motionBudget));
            DriftBoostPackets::resyncPosition(player->drift);
            forceSnap = true;
            break;
        case MotionVerdict::Teleport:
            LOG_WARN("RACE", "motion delta over budget char " + std::to_string(session->characterId) +
                     " budget=" + std::to_string(motionBudget) + " reach=" +
                     std::to_string(MOTION_LIMITS.snapAxisWu));
            player->suspiciousCount++;
            DriftBoostPackets::resyncPosition(player->drift);
            forceSnap = true;
            break;
        case MotionVerdict::Ok:
        default:
            break;
    }

    // distance based judge with its own stale resync so it can never freeze a car
    float speed = 0.0f;
    const SpeedVerdict verdict = DriftBoostPackets::checkSpeed(
        player->drift, state.x, state.y, state.z, nowStamp, speed);
    switch (verdict) {
        case SpeedVerdict::BadSample:
            // a non finite float poisons every other client interpolator
            LOG_WARN("RACE", "motion sample not finite from char " +
                     std::to_string(session->characterId));
            return;
        case SpeedVerdict::Rewind:
            // tcp keeps order same ms sample rides last read docker relay busy host joins two next period covers it
            LOG_DEBUG("RACE", "motion samples bunched in one read from char " +
                      std::to_string(session->characterId));
            return;
        case SpeedVerdict::FirstSample:
            forceSnap = true;
            break;
        case SpeedVerdict::Teleport:
            LOG_WARN("RACE", "motion teleport from char " + std::to_string(session->characterId));
            player->suspiciousCount++;
            forceSnap = true;
            break;
        case SpeedVerdict::OverCeiling:
            LOG_WARN("RACE", "speed " + std::to_string(speed) + " wu per s over ceiling char " +
                     std::to_string(session->characterId));
            player->suspiciousCount++;
            break;
        case SpeedVerdict::Ok:
        default:
            break;
    }

    // drift and boost are client authority the server only observes the state word and the pads under it
    const RoomRaceLive& roomLive = m_roomLive[room->id()];
    const std::vector<SpawnPackets::ColPadCell>* pads = roomLive.padsLoaded ? &roomLive.pads : nullptr;
    DriftBoostTuning noKart;
    noKart.driftChargeTimeMs = 0.0f;
    const DriftBoostEvents ev = DriftBoostPackets::observeState(
        player->drift, state.stateBits, nowStamp,
        player->tuningLoaded ? player->tuning : noKart, pads, state.x, state.y);
    if (ev.driftStarted || ev.driftCharged || ev.driftReleased || ev.boostStarted) {
        LOG_DEBUG("RACE", "drift char " + std::to_string(session->characterId) +
                  " start " + std::to_string(ev.driftStarted ? 1 : 0) +
                  " charged " + std::to_string(ev.driftCharged ? 1 : 0) +
                  " released " + std::to_string(ev.driftReleased ? 1 : 0) +
                  " boost " + std::to_string(ev.boostStarted ? 1 : 0) +
                  " source " + std::to_string(static_cast<int>(ev.source)));
    }
    if (ev.violations != 0) {
        LOG_WARN("RACE", "drift boost violation char " + std::to_string(session->characterId) +
                 " " + DriftBoostPackets::violationNames(ev.violations));
        player->suspiciousCount++;
    }

    if (player->suspiciousCount > SUSPICION_KICK) {
        const int32_t strikes = player->suspiciousCount;
        LOG_ERROR("RACE", "Player kicked for motion anomalies: char " +
                  std::to_string(session->characterId));
        // the kick walks the leave path which takes this same lock so let it go first
        lock.unlock();
        session->send(PacketBuilder::displayMessage(u"Disconnected: Speed anomaly detected", 0));
        AntiCheatHandler::reportViolation(session, ViolationType::SpeedHack,
            ViolationSeverity::Critical,
            "Auto-kick: " + std::to_string(strikes) + " violations");
        session->stop();
        return;
    }

    player->x = state.x;
    player->y = state.y;
    player->z = state.z;
    player->rot = MotionPackets::yawFromByte(state.yaw);
    player->lastUpdate = now;

    RoomRaceLive& live = m_roomLive[room->id()];
    if (!live.motion.empty() && live.rawWorld != rawWorld) {
        LOG_WARN("RACE", "motion form flip room " + std::to_string(room->id()) +
                 " rawWorld " + std::to_string(rawWorld ? 1 : 0));
    }
    live.rawWorld = rawWorld;

    MotionSlot& slot = motionSlot(live, senderId, session);
    // long gap means the remote car already coasted to a stop snap it before the next 0x40
    if (slot.hasSample && nowStamp > slot.lastSampleMs + MOTION_STALE_MS) slot.needSnap = true;
    if (forceSnap) slot.needSnap = true;
    slot.state        = state;
    slot.lastSampleMs = nowStamp;
    slot.hasSample    = true;
    slot.stale        = false;
    live.dirty        = true;
}

MotionSlot& RaceHandler::motionSlot(RoomRaceLive& live, int32_t playerId,
                                    const Session::Ptr& session) {
    for (auto& s : live.motion) {
        if (s.playerId != playerId) continue;
        if (session) s.session = session;
        return s;
    }
    live.motion.push_back(MotionSlot{});
    MotionSlot& s = live.motion.back();
    s.playerId = playerId;
    if (session) s.session = session;
    return s;
}

void RaceHandler::ensureItemState(RoomRaceLive& live, uint32_t playerId) {
    if (live.items.find(playerId)) return;
    live.items.addPlayer(playerId);
}

void RaceHandler::checkTickClock(uint64_t now) {
    if (m_tickClockChecked) return;
    m_tickClockChecked = true;
    const int64_t drift = static_cast<int64_t>(now) - static_cast<int64_t>(nowMs());
    if (drift > 5000 || drift < -5000) {
        LOG_ERROR("RACE", "tick clock off by " + std::to_string(drift) +
                  " ms item timers will misfire pass RaceHandler::nowMs()");
    }
}

void RaceHandler::tick(uint64_t now, GameServer* server) {
    // bots step before the fan out so their slots carry this tick
    if (server) tickBots(now, server);

    std::vector<std::pair<Session::Ptr, Packet>> out;

    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        checkTickClock(now);

        for (auto& kv : m_roomLive) {
            RoomRaceLive& live = kv.second;

            // state only the wire 0xCF comes from the client itself and there is no clear opcode
            const ItemPackets::TickResult res = live.items.tick(now);
            for (const auto& c : res.commits) {
                LOG_DEBUG("RACE", "item commit " + std::string(ItemPackets::itemName(c.itemId)) +
                          " slot " + std::to_string(c.slotIndex) +
                          " char " + std::to_string(c.playerId));
            }

            // a car nobody reports for coasts to rest by itself so stop feeding it
            for (auto& s : live.motion) {
                if (!s.hasSample || s.stale) continue;
                if (now <= s.lastSampleMs + MOTION_STALE_MS) continue;
                s.stale = true;
                LOG_WARN("RACE", "motion stale player " + std::to_string(s.playerId) +
                         " room " + std::to_string(kv.first));
            }

            if (!live.dirty) continue;
            if (now < live.lastFanOutMs + MOTION_FANOUT_MS) continue;
            live.lastFanOutMs = now;
            live.dirty = false;

            // 0x68 flushes the 0x40 queue so a resumed car lands instead of sliding
            std::vector<std::pair<int32_t, Packet>> snaps;
            for (auto& s : live.motion) {
                if (!s.needSnap) continue;
                s.needSnap = false;
                snaps.emplace_back(s.playerId,
                    MotionPackets::teleport(static_cast<uint32_t>(s.playerId),
                                            s.state.x, s.state.y, s.state.z,
                                            MotionPackets::yawFromByte(s.state.yaw)));
            }

            std::vector<MotionEntry> entries;
            entries.reserve(live.motion.size());
            for (const auto& s : live.motion) {
                if (!s.hasSample || s.stale) continue;
                MotionEntry e;
                e.playerId    = static_cast<uint32_t>(s.playerId);
                e.interpScale = MotionPackets::kInterpScaleDefault;
                e.state       = s.state;
                entries.push_back(e);
            }

            const size_t entrySize = live.rawWorld ? MotionPackets::kEntrySizeRaw
                                                   : MotionPackets::kEntrySizeCompressed;
            size_t maxPerFrame = (MAX_FRAME_BYTES - FRAME_HEADER_BYTES - 1) / entrySize;
            if (maxPerFrame > MotionPackets::kMaxWireEntries) maxPerFrame = MotionPackets::kMaxWireEntries;
            if (maxPerFrame == 0) maxPerFrame = 1;

            for (const auto& r : live.motion) {
                Session::Ptr sp = r.session.lock();
                if (!sp || !sp->isConnected()) continue;

                for (const auto& sn : snaps) {
                    if (sn.first == r.playerId) continue;
                    out.emplace_back(sp, sn.second);
                }

                // never a client its own id the local car runs sub 49C0D0 and skips the queue
                size_t others = 0;
                for (const auto& e : entries) {
                    if (e.playerId != static_cast<uint32_t>(r.playerId)) ++others;
                }
                if (others == 0) continue;

                if (others <= maxPerFrame) {
                    // builder drops the recipient own id itself
                    out.emplace_back(sp, MotionPackets::motionBroadcastFor(
                        static_cast<uint32_t>(r.playerId), entries, live.rawWorld));
                    continue;
                }

                std::vector<MotionEntry> mine;
                mine.reserve(entries.size());
                for (const auto& e : entries) {
                    if (e.playerId == static_cast<uint32_t>(r.playerId)) continue;
                    mine.push_back(e);
                }

                for (size_t i = 0; i < mine.size(); i += maxPerFrame) {
                    const size_t end = std::min(mine.size(), i + maxPerFrame);
                    std::vector<MotionEntry> chunk(mine.begin() + static_cast<std::ptrdiff_t>(i),
                                                   mine.begin() + static_cast<std::ptrdiff_t>(end));
                    out.emplace_back(sp, MotionPackets::motionBroadcast(chunk, live.rawWorld));
                }
            }
        }
    }

    for (auto& p : out) {
        if (p.first) p.first->send(p.second);
    }
}

// the finish of a racer the server counted every lap of from the 0x41 checkpoints
void RaceHandler::completeFinish(Session::Ptr session, Room* room, int32_t finishTime,
                                 GameServer* server) {
    if (!room || !session) return;

    const int32_t charId = static_cast<int32_t>(session->characterId);

    int rank = 1;
    size_t playerCount = 0;
    bool allFinished = false;
    uint32_t bonusKey = ResultsPackets::BONUS_NONE;
    {
        // tight lock only for m racePlayers mutation
        std::lock_guard<std::mutex> lock(m_raceMutex);
        auto* player = getPlayer(room->id(), charId);
        if (!player || player->finished) return;

        player->finished = true;
        player->totalTime = finishTime;

        for (const auto& p : m_racePlayers[room->id()]) {
            if (p.finished && p.playerId != charId && p.totalTime < finishTime) rank++;
        }
        player->position   = static_cast<uint8_t>(rank);
        player->finishRank = rank - 1;
        bonusKey    = player->bonusItemKey;
        playerCount = m_racePlayers[room->id()].size();

        allFinished = true;
        for (const auto& p : m_racePlayers[room->id()]) {
            if (!p.finished) { allFinished = false; break; }
        }
    }
    if (rank == 1 && !allFinished) scheduleFinishGrace(room->id(), server);

    const int32_t finishRank    = rank - 1;
    const int32_t mapIdSnap     = static_cast<int32_t>(room->settings().mapId);
    const int32_t gameModeSnap  = static_cast<int32_t>(room->settings().mode);
    const int32_t playerCountSnap = static_cast<int32_t>(playerCount);

    // amounts come from reward rules so a human tunes them without a rebuild
    std::vector<ResultsPackets::Finisher> order;
    ResultsPackets::Finisher fin;
    fin.playerId     = static_cast<uint32_t>(charId);
    fin.finishRank   = finishRank;
    fin.finishTimeMs = finishTime;
    fin.bonusItemKey = bonusKey;
    order.push_back(fin);

    const std::vector<ResultsPackets::Reward> rewards =
        ResultsPackets::computeRewards(order, gameModeSnap);
    if (rewards.empty()) {
        LOG_ERROR("RACE", "computeRewards gave no row for char " + std::to_string(charId));
        return;
    }
    const ResultsPackets::Reward& rw = rewards.front();
    const int32_t goldReward = rw.gold + rw.goldBonus;
    const int32_t xpReward   = rw.xp + rw.xpBonus;

    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        if (auto* p = getPlayer(room->id(), charId)) {
            p->score     = goldReward;
            p->xpReward  = xpReward;
            p->goldBonus = rw.goldBonus;
            p->xpBonus   = rw.xpBonus;
        }
    }

    LOG_INFO("RACE", "Player " + std::to_string(charId) +
             " finished rank " + std::to_string(rank) + "/" + std::to_string(playerCount) +
             " time=" + std::to_string(finishTime) + "ms reward=" + std::to_string(goldReward) + "g");

    auto& db = Database::instance();
    int32_t xp = 0;
    int32_t goldAfter = 0;
    int32_t currentLevel = 1;
    int32_t newLevel = 1;
    {
        auto tx = db.beginTransaction();
        if (!tx.valid()) {
            LOG_ERROR("RACE", "beginTransaction failed on finish for char "
                      + std::to_string(charId));
            return;
        }

        int32_t equippedVehicleId = 0;
        auto equipped = tx.query(
            "SELECT k.id FROM owned_kart k JOIN characters c ON c.id = k.character_id "
            "WHERE k.character_id = ? AND c.selected_kart_instance_id = k.id LIMIT 1",
            {charId});
        if (equipped.empty()) {
            equipped = tx.query(
                "SELECT id FROM owned_kart WHERE character_id = ? "
                "ORDER BY active_flag DESC, id ASC LIMIT 1",
                {charId});
        }
        if (!equipped.empty()) {
            try { equippedVehicleId = std::stoi(equipped[0]["id"]); } catch (...) {}
        }

        const char* sql = (rank == 1)
            ? "UPDATE characters SET gold = gold + ?, experience = experience + ?, "
              "total_races = total_races + 1, wins = wins + 1 WHERE id = ?"
            : "UPDATE characters SET gold = gold + ?, experience = experience + ?, "
              "total_races = total_races + 1, losses = losses + 1 WHERE id = ?";
        if (!tx.execute(sql, {goldReward, xpReward, charId})) {
            LOG_ERROR("RACE", "stat update failed for char " + std::to_string(charId));
            return;
        }

        tx.execute(
            "INSERT INTO race_history "
            "(character_id, room_id, map_id, vehicle_id, game_mode, "
            " finish_time_ms, rank_in_race, player_count, gold_reward, xp_reward) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            {charId, static_cast<int32_t>(room->id()),
             mapIdSnap, equippedVehicleId, gameModeSnap,
             finishTime, rank, playerCountSnap,
             goldReward, xpReward});

        auto charData = tx.query(
            "SELECT gold, experience, level FROM characters WHERE id = ? LIMIT 1",
            {charId});
        if (charData.empty()) {
            LOG_WARN("RACE", "character row missing after finish char " + std::to_string(charId));
        } else {
            goldAfter    = rowInt(charData[0], "gold", 0);
            xp           = rowInt(charData[0], "experience", 0);
            currentLevel = rowInt(charData[0], "level", 1);
            // sqrt of exp over a hundred is not the real ladder ProgressionHandler fought it over levels every login
            newLevel     = ProgressionHandler::levelForExp(xp);
            if (newLevel > currentLevel) {
                tx.execute("UPDATE characters SET level = ? WHERE id = ?", {newLevel, charId});
            }
        }

        if (!tx.commit()) {
            LOG_ERROR("RACE", "commit failed on finish for char " + std::to_string(charId));
            return;
        }
    }

    // raced kart is the persisted selection 0x1C full replace shows it 0x4795A0 durability warn reads owned copy on return
    {
        InventoryPackets::KartRow kr;
        if (InventoryHandler::selectedKartRow(charId, kr) &&
            PartStatPackets::consumeDurability(charId, kr.instanceId,
                                               KART_DURABILITY_WEAR_PER_RACE)) {
            session->send(InventoryHandler::ownedKartListPacket(charId));
        }
    }

    const int32_t levelAfter = newLevel > currentLevel ? newLevel : currentLevel;
    const int32_t levelWire  = levelAfter < 0 ? 0 : (levelAfter > 255 ? 255 : levelAfter);

    // unicast only these four land in the recipient own wallet globals with no id guard
    session->send(ResultsPackets::finishReward(
        static_cast<uint32_t>(charId),
        static_cast<uint32_t>(goldAfter < 0 ? 0 : goldAfter),
        static_cast<uint8_t>(levelWire),
        static_cast<uint32_t>(xp < 0 ? 0 : xp),
        finishRank));

    // finisher already plays it locally so only the other screens need the relay
    sendToOthers(room, ResultsPackets::driverAnimState(static_cast<uint32_t>(charId),
                                                       ResultsPackets::ANIM_FINISH), charId);

    // rank must land before the 0x46 board else the win mission report reads stale
    room->broadcast(ResultsPackets::rankBroadcast(static_cast<uint32_t>(charId), finishRank));

    // the race count and the level both carry pendants the new ones go out on 0x011B
    ProgressionHandler::pushEarnedPendants(session);

    if (newLevel > currentLevel) {
        LOG_INFO("RACE", "Player " + std::to_string(charId) +
                 " leveled up to " + std::to_string(newLevel) + "!");
        session->send(PacketBuilder::msgLevelUp(session->characterName));
        session->send(PacketBuilder::playerStatsUpdate(charId, xp, newLevel, 0));
    }

    // no per finisher 0x39 or it tears down every client hud mid race send once at endRace
    if (allFinished) {
        endRace(room, server);
    }
}

// in race item wire the proven C2S senders

// C2S 0x49 the client rolled locally and reports what it got
void RaceHandler::handleItemGrantReport(Session::Ptr session, Packet& packet, Room* room) {
    if (!room) return;
    if (room->state() != RoomState::Racing) return;

    ItemPackets::GrantReport rep;
    if (!ItemPackets::parseGrant(packet, rep)) return;
    if (!ItemPackets::itemKindValid(rep.itemId)) {
        LOG_WARN("RACE", "grant item " + std::to_string(rep.itemId) + " out of range from char " +
                 std::to_string(session->characterId));
        return;
    }

    int32_t slotIndex = rep.slotIndex;
    if (slotIndex < 0) slotIndex = 0;
    if (slotIndex >= ItemPackets::kSlotCount) slotIndex = ItemPackets::kSlotCount - 1;

    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        if (!getPlayer(room->id(), static_cast<int32_t>(session->characterId))) return;
        RoomRaceLive& live = m_roomLive[room->id()];
        ensureItemState(live, session->characterId);
        int32_t modelSlot = slotIndex;
        // client owns the roll so a refusal only means our view drifted
        if (live.items.applyGrant(session->characterId, rep.itemId, rep.slotIndex,
                                  nowMs(), modelSlot)) {
            slotIndex = modelSlot;
        }
    }

    LOG_DEBUG("RACE", "item grant " + std::string(ItemPackets::itemName(rep.itemId)) +
              " slot " + std::to_string(slotIndex) + " char " +
              std::to_string(session->characterId));

    // sender already draws its own icon
    room->broadcastExcept(
        ItemPackets::grantBroadcast(session->characterId, rep.itemId, slotIndex),
        session->id());
}

// C2S 0x47 item use no player id on the wire
void RaceHandler::handleItemSpawn(Session::Ptr session, Packet& packet, Room* room) {
    if (!room) return;
    if (room->state() != RoomState::Racing) return;

    ItemPackets::UseRequest req;
    if (!ItemPackets::parseUse(packet, req)) return;
    if (!ItemPackets::itemKindValid(req.kind)) {
        LOG_WARN("RACE", "item spawn kind " + std::to_string(req.kind) + " out of range from char " +
                 std::to_string(session->characterId));
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        // an id not in the roster bails the receiver mid stream and desyncs it
        auto* player = getPlayer(room->id(), static_cast<int32_t>(session->characterId));
        if (!player) return;

        auto now = std::chrono::steady_clock::now();
        if (player->lastItemUse.time_since_epoch().count() != 0) {
            auto since = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - player->lastItemUse).count();
            if (since < ITEM_USE_MIN_INTERVAL_MS) {
                player->suspiciousCount++;
                return;
            }
        }
        player->lastItemUse = now;

        // a later class one boost bit is only innocent when it pairs with this
        DriftBoostPackets::noteItemUse(player->drift, req.kind, nowMs());

        RoomRaceLive& live = m_roomLive[room->id()];
        ensureItemState(live, session->characterId);
        // logs when slot zero disagrees the client is still authoritative
        live.items.applyUse(session->characterId, req.kind, nowMs());
    }

    // sub 47cd20 plays the shooter's item sound on every remote screen with no per kind receiver switch
    room->broadcastExcept(ItemPackets::playSoundCue(session->characterId), session->id());

    // kinds 0 1 6 10 16 have no receiver case at all
    if (!ItemPackets::spawnHasClientCase(req.kind)) {
        LOG_DEBUG("RACE", "item spawn " + std::string(ItemPackets::itemName(req.kind)) +
                  " has no receiver case not relayed");
        return;
    }

    // a spike bomb or ice on the road is a hazard the CPU cars drive into
    noteHazard(room->id(), static_cast<int32_t>(session->characterId), req.kind, req.x, req.y, req.z);

    Packet spawn = ItemPackets::itemSpawnEcho(session->characterId, req);
    if (ItemPackets::spawnNeedsSelfEcho(req.kind)) {
        // online path has no local spawn drop the echo and the shooter sees nothing
        room->broadcast(spawn);
    } else {
        // shooter already spawned it locally a second one would double up
        room->broadcastExcept(spawn, session->id());
    }

}

// C2S 0x4B rocket or magnet second press
void RaceHandler::handleHomingLaunch(Session::Ptr session, Packet& packet, Room* room) {
    if (!room) return;
    if (room->state() != RoomState::Racing) return;

    ItemPackets::HomingLaunch req;
    if (!ItemPackets::parseHomingLaunch(packet, req)) return;
    if (req.kind != ItemPackets::ITEM_ROCKET && req.kind != ItemPackets::ITEM_MAGNET) {
        LOG_WARN("RACE", "homing kind " + std::to_string(req.kind) + " is a receiver no op");
        return;
    }

    const int32_t senderId = static_cast<int32_t>(session->characterId);
    if (req.shooterPlayerId != senderId) {
        LOG_WARN("RACE", "homing shooter " + std::to_string(req.shooterPlayerId) +
                 " does not match session char " + std::to_string(senderId));
    }

    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        auto* player = getPlayer(room->id(), senderId);
        if (!player) return;
        if (!getPlayer(room->id(), req.targetPlayerId)) {
            LOG_WARN("RACE", "homing target " + std::to_string(req.targetPlayerId) +
                     " not in roster from char " + std::to_string(senderId));
            player->suspiciousCount++;
            return;
        }
        RoomRaceLive& live = m_roomLive[room->id()];
        ensureItemState(live, session->characterId);
        // first press only started the search the launch is what consumes
        live.items.consumeSlot0(session->characterId, nowMs());
    }

    // no client answers for a CPU car so the server lands the rocket itself
    if (Room::isBotId(req.targetPlayerId)) {
        queueBotHit(room->id(), req.targetPlayerId, ItemPackets::EFFECT_CRASH, 1500);
    }

    LOG_DEBUG("RACE", "homing " + std::string(ItemPackets::itemName(req.kind)) +
              " " + std::to_string(senderId) + " to " + std::to_string(req.targetPlayerId));

    // shooter id comes from the socket sub 481320 has no local launch so echo self too
    room->broadcast(ItemPackets::homingLaunch(session->characterId, req.kind,
                                              senderId, req.targetPlayerId));
}

// C2S 0x5C turtle target picked client side from the standings
void RaceHandler::handleTurtleLaunch(Session::Ptr session, Packet& packet, Room* room) {
    if (!room) return;
    if (room->state() != RoomState::Racing) return;

    ItemPackets::TurtleLaunch req;
    if (!ItemPackets::parseTurtleLaunch(packet, req)) return;

    const int32_t senderId = static_cast<int32_t>(session->characterId);
    if (req.shooterPlayerId != senderId) {
        LOG_WARN("RACE", "turtle shooter " + std::to_string(req.shooterPlayerId) +
                 " does not match session char " + std::to_string(senderId));
    }

    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        auto* player = getPlayer(room->id(), senderId);
        if (!player) return;
        if (!getPlayer(room->id(), req.targetPlayerId)) {
            LOG_WARN("RACE", "turtle target " + std::to_string(req.targetPlayerId) +
                     " not in roster from char " + std::to_string(senderId));
            player->suspiciousCount++;
            return;
        }
        RoomRaceLive& live = m_roomLive[room->id()];
        ensureItemState(live, session->characterId);
        live.items.applyUse(session->characterId, ItemPackets::ITEM_TURTLE, nowMs());
    }

    if (Room::isBotId(req.targetPlayerId)) {
        queueBotHit(room->id(), req.targetPlayerId, ItemPackets::EFFECT_SPIN, 2500);
    }

    LOG_DEBUG("RACE", "turtle " + std::to_string(senderId) +
              " to " + std::to_string(req.targetPlayerId));

    // sub 4D1A50 already made the shooter its own turtle echoing back makes two
    room->broadcastExcept(
        ItemPackets::turtleLaunch(session->characterId, senderId, req.targetPlayerId),
        session->id());
}

// C2S 0x57 lock state pure relay no id prepended
void RaceHandler::handleLockState(Session::Ptr session, Packet& packet, Room* room) {
    if (!room) return;
    if (room->state() != RoomState::Racing) return;

    ItemPackets::LockState st;
    if (!ItemPackets::parseLockState(packet, st)) return;
    if (st.kind != ItemPackets::ITEM_ROCKET && st.kind != ItemPackets::ITEM_MAGNET) return;
    if (st.phase != ItemPackets::LOCK_LOST && st.phase != ItemPackets::LOCK_SEARCHING &&
        st.phase != ItemPackets::LOCK_LOCKED) {
        LOG_WARN("RACE", "lock phase " + std::to_string(st.phase) + " unknown");
        return;
    }

    const int32_t senderId = static_cast<int32_t>(session->characterId);
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        if (!getPlayer(room->id(), senderId)) return;
        if (st.targetPlayerId >= 0 && !getPlayer(room->id(), st.targetPlayerId)) return;

        RoomRaceLive& live = m_roomLive[room->id()];
        ensureItemState(live, session->characterId);
        auto* ps = live.items.find(session->characterId);
        if (ps && st.phase == ItemPackets::LOCK_SEARCHING && ps->lastLockSendMs != 0 &&
            nowMs() < ps->lastLockSendMs + ItemPackets::kLockResendMs) {
            return;
        }
        live.items.setLock(session->characterId, st.targetPlayerId, st.phase, nowMs());
    }

    // only the targeted client reacts an id in front would shift every field
    Packet relay = ItemPackets::lockStateRelay(st.targetPlayerId, st.phase, st.kind);
    for (const auto& s : room->sessions()) {
        if (static_cast<int32_t>(s->characterId) != st.targetPlayerId) continue;
        s->send(relay);
        break;
    }
}

// C2S 0x69 hit report the victim sends it after applying the effect itself
void RaceHandler::handleHitReport(Session::Ptr session, Packet& packet, Room* room) {
    if (!room) return;
    if (room->state() != RoomState::Racing) return;

    ItemPackets::HitReport hit;
    if (!ItemPackets::parseHit(packet, hit)) return;

    // 1100 flash is victim local and 200 is never sent receivers drop both
    if (!ItemPackets::hitShouldRebroadcast(hit.code)) {
        LOG_DEBUG("RACE", "hit code " + std::to_string(hit.code) + " not rebroadcast");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        if (!getPlayer(room->id(), static_cast<int32_t>(session->characterId))) return;
        RoomRaceLive& live = m_roomLive[room->id()];
        ensureItemState(live, session->characterId);
        live.items.applyEffect(session->characterId, hit.code, nowMs());
    }

    LOG_DEBUG("RACE", "hit code " + std::to_string(hit.code) + " victim " +
              std::to_string(session->characterId));

    // sender FUN 00481B60 skips sub 495C30 and only writes the frame handler FUN 0047AF00 applies it to any car
    room->broadcast(ItemPackets::hitBroadcast(session->characterId, hit.code, hit.flag));
}

// C2S 0xCF whole slot array after any change
void RaceHandler::handleSlotSync(Session::Ptr session, Packet& packet, Room* room) {
    if (!room) return;
    if (room->state() != RoomState::Racing) return;

    ItemPackets::SlotSync ss;
    if (!ItemPackets::parseSlotSync(packet, ss)) return;

    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        if (!getPlayer(room->id(), static_cast<int32_t>(session->characterId))) return;
        RoomRaceLive& live = m_roomLive[room->id()];
        ensureItemState(live, session->characterId);
        live.items.applySlotSync(session->characterId, ss.slot[0], ss.slot[1], ss.slot[2]);
    }

    // sender mirrored it locally through sub 49A860 already
    room->broadcastExcept(
        ItemPackets::slotSync(session->characterId, ss.slot[0], ss.slot[1], ss.slot[2]),
        session->id());
}

// C2S 0x5F rabbit or bluerabbit latched on pure relay
void RaceHandler::handlePetReached(Session::Ptr session, Packet& packet, Room* room) {
    if (!room) return;
    if (room->state() != RoomState::Racing) return;

    ItemPackets::PetReached pr;
    if (!ItemPackets::parsePetReached(packet, pr)) return;
    if (pr.kind != ItemPackets::ITEM_RABBIT && pr.kind != ItemPackets::ITEM_BLUERABBIT) {
        LOG_WARN("RACE", "pet reached kind " + std::to_string(pr.kind) + " has no receiver case");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        if (!getPlayer(room->id(), static_cast<int32_t>(session->characterId))) return;
        if (!getPlayer(room->id(), pr.victimPlayerId)) {
            LOG_WARN("RACE", "pet reached victim " + std::to_string(pr.victimPlayerId) +
                     " not in roster");
            return;
        }
    }

    // receiver writes a constant state so a self echo is idempotent
    room->broadcast(ItemPackets::petReachedRelay(pr.victimPlayerId, pr.kind));
}

// C2S 0x6A parsed and logged only
void RaceHandler::handleRaceValue(Session::Ptr session, Packet& packet, Room* room) {
    (void)room;
    int16_t value = 0;
    if (!ItemPackets::parseRaceValue(packet, value)) return;

    // S2C 0x6A sets a car flag which makes the client throw away every relayed 0x40
    LOG_DEBUG("RACE", "race value " + std::to_string(value) + " from char " +
              std::to_string(session->characterId) + " not relayed");
}

// C2S 0xCB swap ticket consume the client already swapped and sent its own 0xCF
void RaceHandler::handleSwapTicket(Session::Ptr session, Packet& packet, Room* room) {
    if (!session || session->characterId == 0) return;

    ItemPackets::SwapTicket ticket;
    if (!ItemPackets::parseSwapTicket(packet, ticket)) return;
    if (ticket.baseKey != SLOT_EXCHANGE_ITEM_KEY) {
        LOG_WARN("RACE", "swap ticket base key " + std::to_string(ticket.baseKey) +
                 " expected " + std::to_string(SLOT_EXCHANGE_ITEM_KEY));
        return;
    }

    const int32_t charId = static_cast<int32_t>(session->characterId);
    auto& db = Database::instance();
    auto rows = db.queryPrepared(
        "SELECT id, period_value FROM owned_item WHERE character_id = ? AND base_key = ? LIMIT 1",
        {charId, SLOT_EXCHANGE_ITEM_KEY});
    if (rows.empty()) {
        LOG_WARN("RACE", "swap ticket from char " + std::to_string(charId) + " who owns no item 1000");
        return;
    }
    const int32_t rowId = std::stoi(rows[0].at("id"));
    const std::string& storedText = rows[0].at("period_value");
    const int32_t stored = storedText.empty() ? 0 : std::stoi(storedText);
    if (ticket.instanceId != rowId) {
        LOG_WARN("RACE", "swap ticket instance " + std::to_string(ticket.instanceId) + " is not row " +
                 std::to_string(rowId) + " of char " + std::to_string(charId));
        return;
    }

    // the 0xCF before this frame already carries the swapped order so the model is not swapped again
    const SlotExchangeStep step =
        slotExchangeStep(ticket.flag, ticket.periodValue, ticket.activeFlag, stored);
    if (step.decrement) {
        // guard in sql so two tickets in flight cannot drive the count negative
        db.executePrepared(
            "UPDATE owned_item SET period_value = period_value - 1 "
            "WHERE id = ? AND character_id = ? AND period_value > 0",
            {rowId, charId});
        if (room && room->state() == RoomState::Racing) {
            std::lock_guard<std::mutex> lock(m_raceMutex);
            if (getPlayer(room->id(), charId)) {
                RoomRaceLive& live = m_roomLive[room->id()];
                ensureItemState(live, session->characterId);
                live.items.noteSwapTicketUsed(session->characterId);
            }
        }
    }
    if (step.resync) {
        // sub 47E680 writes the count into the row of that instance the client owns the id
        session->send(InventoryPackets::setRemainingUses(static_cast<uint32_t>(rowId), step.countAfter));
    }

    LOG_DEBUG("RACE", "swap ticket char " + std::to_string(charId) + " flag " +
              std::to_string(ticket.flag) + " wire " + std::to_string(ticket.periodValue) +
              " stored " + std::to_string(stored) + " after " + std::to_string(step.countAfter) +
              (step.resync ? " resync" : ""));
}

// spawn lap and results wire the proven C2S senders with no id on the wire

// C2S 0x41 checkpoint transition only the local car emits it
void RaceHandler::handleCheckpoint(Session::Ptr session, Packet& packet, Room* room,
                                   GameServer* server) {
    if (!room) return;
    if (room->state() != RoomState::Racing) return;

    SpawnPackets::CheckpointReport rep;
    if (!SpawnPackets::parseCheckpoint(packet, rep)) return;

    const int32_t charId = static_cast<int32_t>(session->characterId);

    bool boardStep = false;
    bool raceDone  = false;
    int32_t lapsDone = 0;
    int32_t elapsedMs = 0;
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        RoomRaceLive& live = m_roomLive[room->id()];
        if (!live.lapTrackingLive) {
            if (!live.checkpointWarned) {
                live.checkpointWarned = true;
                LOG_WARN("RACE", "checkpoint reports ignored track " +
                         std::to_string(live.trackId) + " has no COL count laps come from 0x36");
            }
            return;
        }
        auto* player = getPlayer(room->id(), charId);
        if (!player) return;

        const SpawnPackets::LapTracker::Result res = player->lapTracker.onCheckpoint(rep);
        if (res == SpawnPackets::LapTracker::Result::Rejected ||
            res == SpawnPackets::LapTracker::Result::Ignored) {
            return;
        }
        if (player->lapTracker.desynced()) {
            LOG_WARN("RACE", "checkpoint desync char " + std::to_string(charId) +
                     " prev " + std::to_string(rep.prev));
        }
        if (res != SpawnPackets::LapTracker::Result::LapComplete) return;

        lapsDone = player->lapTracker.lapsCompleted();
        player->lap = static_cast<uint8_t>(lapsDone);
        if (player->lapBoardRow < SpawnPackets::LAP_BOARD_MAX) {
            ++player->lapBoardRow;
            boardStep = true;
        }
        raceDone = player->lapTracker.raceComplete() && !player->finished;

        auto st = m_raceStartTimes.find(room->id());
        if (st != m_raceStartTimes.end()) {
            elapsedMs = static_cast<int32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - st->second).count());
        }
    }

    if (boardStep) session->send(SpawnPackets::lapBoardAdvance());

    LOG_INFO("RACE", "lap " + std::to_string(lapsDone) + " by char " + std::to_string(charId));

    calculatePositions(room->id());
    updatePositions(room);

    // nothing in stage 11 ever ends a race the server owns the finish
    if (raceDone) {
        if (elapsedMs <= 0) {
            LOG_WARN("RACE", "no race start time for room " + std::to_string(room->id()) +
                     " finish time falls back to zero");
        }
        completeFinish(session, room, elapsedMs, server);
    }
}

// C2S 0x67 client rank metric about one per client frame
void RaceHandler::handleProgress(Session::Ptr session, Packet& packet, Room* room) {
    if (!room) return;
    if (room->state() != RoomState::Racing) return;

    uint32_t score = 0;
    if (!SpawnPackets::parseProgress(packet, score)) return;

    const int32_t charId = static_cast<int32_t>(session->characterId);

    std::lock_guard<std::mutex> lock(m_raceMutex);
    auto* player = getPlayer(room->id(), charId);
    if (!player) return;

    player->progressScore = score;
    player->haveProgress  = true;

    // client counts laps itself so a wide gap means one side lost checkpoints
    const RoomRaceLive& live = m_roomLive[room->id()];
    if (live.lapTrackingLive) {
        const int32_t clientLaps = static_cast<int32_t>(SpawnPackets::progressLaps(score));
        const int32_t serverLaps = player->lapTracker.lapsCompleted();
        if (clientLaps > serverLaps + 1 && !player->progressWarned) {
            player->progressWarned = true;
            LOG_WARN("RACE", "progress laps " + std::to_string(clientLaps) +
                     " ahead of server " + std::to_string(serverLaps) +
                     " char " + std::to_string(charId));
        }
    }
}

// C2S 0x68 respawn or warp result the client picks its own point
void RaceHandler::handleRespawn(Session::Ptr session, Packet& packet, Room* room) {
    if (!room) return;
    if (room->state() != RoomState::Racing) return;

    SpawnPackets::RespawnReport rep;
    if (!SpawnPackets::parseRespawn(packet, rep)) return;

    const int32_t charId = static_cast<int32_t>(session->characterId);

    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        auto* player = getPlayer(room->id(), charId);
        if (!player) {
            LOG_WARN("RACE", "respawn from char " + std::to_string(charId) + " not in roster");
            return;
        }
        player->x = rep.x;
        player->y = rep.y;
        player->z = rep.z;
        player->rot = rep.yawDegrees;
        player->motionGate = MotionGateState{};
        // baseline is gone after a warp else the next sample reads as a teleport
        DriftBoostPackets::resyncPosition(player->drift);

        RoomRaceLive& live = m_roomLive[room->id()];
        for (auto& s : live.motion) {
            if (s.playerId != charId) continue;
            s.state.x = rep.x;
            s.state.y = rep.y;
            s.state.z = rep.z;
            s.state.tx = rep.x;
            s.state.ty = rep.y;
            s.state.tz = rep.z;
            s.state.yaw = MotionPackets::yawToByte(rep.yawDegrees);
            s.needSnap = true;
            break;
        }
    }

    // sending it back to the owner leaves its physics body untouched and the car snaps back
    sendToOthers(room, SpawnPackets::respawnRelay(static_cast<uint32_t>(charId), rep), charId);
}

// C2S 0x58 driver anim echo one byte no id on the wire
void RaceHandler::handleAnimState(Session::Ptr session, Packet& packet, Room* room) {
    if (!room) return;
    if (room->state() != RoomState::Racing) return;

    uint8_t anim = 0;
    if (!ResultsPackets::parseAnimStateEcho(packet, anim)) return;

    const int32_t charId = static_cast<int32_t>(session->characterId);
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        if (!getPlayer(room->id(), charId)) {
            LOG_WARN("RACE", "anim state from char " + std::to_string(charId) + " not in roster");
            return;
        }
    }

    sendToOthers(room, ResultsPackets::driverAnimState(static_cast<uint32_t>(charId), anim), charId);
}

// kart derived thresholds cached once per race they never change mid race
bool RaceHandler::loadTuning(int32_t characterId, int32_t kartId, DriftBoostTuning& out) {
    KartStatBlock stats{};
    bool factory = false;
    // the client computes off the 0xC0 block of the login burst so the thresholds read that same block
    if (kartId > 0 && kartWireStatsForTemplate(kartId, stats, factory)) {
        if (factory) {
            KartDef def;
            def.kartId      = static_cast<uint32_t>(kartId);
            def.modelScheme = 1;
            def.stats       = stats;
            const auto parts = DriftBoostPackets::loadEquippedParts(
                static_cast<uint32_t>(characterId), static_cast<uint32_t>(kartId));
            std::vector<KartStatBlock> partStats;
            partStats.reserve(parts.size());
            for (const auto& p : parts) {
                KartStatBlock b{};
                DriftBoostPackets::loadPartStats(p.partId, b);
                partStats.push_back(b);
            }
            stats = DriftBoostPackets::effectiveStats(def, parts, partStats);
        }
        out = DriftBoostPackets::tuning(stats);
        return true;
    }
    if (kartId > 0 && DriftBoostPackets::loadEffectiveStats(
            static_cast<uint32_t>(characterId), static_cast<uint32_t>(kartId), stats)) {
        out = DriftBoostPackets::tuning(stats);
        return true;
    }

    LOG_WARN("RACE", "no kart stats for char " + std::to_string(characterId) +
             " kart " + std::to_string(kartId) + " only the teleport test stays live");
    return false;
}

void RaceHandler::startCountdown(uint32_t roomId, GameServer* server) {
    if (!server) return;
    auto room = server->getRoom(roomId);
    if (!room) return;

    room->setState(RoomState::Starting);
    LOG_INFO("RACE", "race start armed for room " + std::to_string(roomId));

    // chain from a working server 0x14 then 0x3E per racer then 0x0D grid client answers 0x0D then gets 0x3A GO
    auto timer = std::make_shared<asio::steady_timer>(server->ioContext());
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        m_countdownTimers[roomId] = timer;
    }
    timer->expires_after(std::chrono::milliseconds(5000));
    timer->async_wait([this, roomId, server](const std::error_code& ec) {
        if (ec) return;
        sendGridPhase(roomId, server);
    });
}

void RaceHandler::sendGridPhase(uint32_t roomId, GameServer* server) {
    auto room = server->getRoom(roomId);
    if (!room) {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        m_countdownTimers.erase(roomId);
        return;
    }

    // one 0x3E per racer then the bare 0x0D and the camera mode we keep sending
    sendGrid(room.get());
    room->broadcast(SpawnPackets::cameraMode(0));

    // every human racer owes a C2S 0x0D before GO bots have nothing to load the stock answers too
    std::set<int32_t> waiting;
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        for (const auto& p : m_racePlayers[roomId]) {
            if (!Room::isBotId(p.playerId)) waiting.insert(p.playerId);
        }
        m_awaitLoaded[roomId].arm(waiting, nowMs());
    }
    LOG_INFO("RACE", "grid out for room " + std::to_string(roomId) + ", waiting up to " +
             std::to_string(SceneLoadWait::kWaitMs / 1000) + " s on " + std::to_string(waiting.size()) +
             " scene loaded acks");

    if (waiting.empty()) {
        goRace(roomId, server);
        return;
    }

    // a client that never answers must not hold the room forever the cap still covers a slow load
    auto fallback = std::make_shared<asio::steady_timer>(server->ioContext());
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        m_loadFallbackTimers[roomId] = fallback;
    }
    fallback->expires_after(std::chrono::milliseconds(SceneLoadWait::kWaitMs));
    fallback->async_wait([this, roomId, server](const std::error_code& ec) {
        if (ec) return;
        size_t late = 0;
        {
            std::lock_guard<std::mutex> lock(m_raceMutex);
            auto it = m_awaitLoaded.find(roomId);
            if (it == m_awaitLoaded.end()) return;   // GO already went out
            late = it->second.owed.size();
        }
        LOG_WARN("RACE", "room " + std::to_string(roomId) + " starts with " +
                 std::to_string(late) + " racer(s) that never said scene loaded in " +
                 std::to_string(SceneLoadWait::kWaitMs) + " ms");
        goRace(roomId, server);
    });
}

void RaceHandler::handleSceneLoaded(Session::Ptr session, Room* room, GameServer* server) {
    if (!session || !room || !server) return;
    bool go = false;
    uint64_t waited = 0;
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        auto it = m_awaitLoaded.find(room->id());
        if (it == m_awaitLoaded.end()) return;       // not between grid and GO
        go = it->second.clear(static_cast<int32_t>(session->characterId));
        waited = nowMs() - it->second.gridMs;
    }
    LOG_INFO("RACE", "char " + std::to_string(session->characterId) + " scene loaded in room " +
             std::to_string(room->id()) + " " + std::to_string(waited) + " ms after the grid" +
             (go ? ", all in, GO" : ""));
    if (go) goRace(room->id(), server);
}

void RaceHandler::goRace(uint32_t roomId, GameServer* server) {
    bool first = false;
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        first = m_awaitLoaded.erase(roomId) > 0;     // the once token ack and fallback both land here
        m_countdownTimers.erase(roomId);
        auto f = m_loadFallbackTimers.find(roomId);
        if (f != m_loadFallbackTimers.end()) {
            if (f->second) f->second->cancel();
            m_loadFallbackTimers.erase(f);
        }
    }
    if (!first) return;
    auto room = server->getRoom(roomId);
    if (!room) return;
    startRace(room.get());
    scheduleRaceWatchdog(roomId, server);
    // the CPU cars drive the line from here a track with no line keeps the clock
    if (!armBots(room.get())) scheduleBotFinishers(roomId, server);
}

void RaceHandler::startRace(Room* room) {
    room->setState(RoomState::Racing);
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        m_raceStartTimes[room->id()] = std::chrono::steady_clock::now();
    }
    
    LOG_INFO("RACE", "Race started in room " + std::to_string(room->id()));
    
    sendRaceStart(room);
}

void RaceHandler::endRace(Room* room, GameServer* server) {
    (void)server;

    // idempotent only end once from racing bot timer plus last human can both fire
    if (room->state() != RoomState::Racing) return;

    room->setState(RoomState::Results);

    LOG_INFO("RACE", "Race ended in room " + std::to_string(room->id()));

    sendResults(room);

    // no tutorial grant here that was for mode 4 but mode 4 is battle 0xAA and LicenseHandler already handle completion

    // no 0x3C here unicast per racer at their finish one never sent reads as retired since 0x14 preset minus one

    room->setState(RoomState::Waiting);

    // client shows podium and a rest counter then waits forever nothing leaves stage 11 so resend the room screen
    room->resultsShownMs = static_cast<int64_t>(nowMs());

    // drop bots so waiting room grid frees up
    room->clearBots();

    clearRoom(room->id());
}

void RaceHandler::updatePositions(Room* room) {
    // mutex not recursive so no outer lock
    calculatePositions(room->id());

    std::vector<Packet> standings;
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        auto liveIt = m_roomLive.find(room->id());
        for (const auto& p : m_racePlayers[room->id()]) {
            // no 0x57 here sub 47AB40 is the item lock relay chibikart never sends a race status standings feed the HUD

            // 0x45 standings entry one is the zero based place thunder and hammer read it
            const int32_t place = p.position > 0 ? static_cast<int32_t>(p.position) - 1 : 0;
            standings.push_back(ItemPackets::standingsUpdate(
                static_cast<uint32_t>(p.playerId), place));
            if (liveIt != m_roomLive.end()) {
                liveIt->second.items.setRank(static_cast<uint32_t>(p.playerId), place);
            }
        }
    }

    for (const auto& pkt : standings) room->broadcast(pkt);
}

RacePlayer* RaceHandler::getPlayer(uint32_t roomId, int32_t playerId) {
    auto it = m_racePlayers.find(roomId);
    if (it == m_racePlayers.end()) return nullptr;
    
    for (auto& p : it->second) {
        if (p.playerId == playerId) return &p;
    }
    return nullptr;
}

void RaceHandler::addPlayer(uint32_t roomId, int32_t playerId) {
    std::lock_guard<std::mutex> lock(m_raceMutex);
    RacePlayer p;
    p.playerId = playerId;
    p.lastUpdate = std::chrono::steady_clock::now();
    m_racePlayers[roomId].push_back(p);
}

void RaceHandler::removePlayer(uint32_t roomId, int32_t playerId) {
    std::lock_guard<std::mutex> lock(m_raceMutex);
    auto it = m_racePlayers.find(roomId);
    if (it == m_racePlayers.end()) return;
    
    auto& players = it->second;
    players.erase(std::remove_if(players.begin(), players.end(),
        [playerId](const RacePlayer& p) { return p.playerId == playerId; }
    ), players.end());

    // a gone id in the fan out bails the 0x47 and 0x69 readers mid stream
    auto lit = m_roomLive.find(roomId);
    if (lit == m_roomLive.end()) return;
    auto& slots = lit->second.motion;
    slots.erase(std::remove_if(slots.begin(), slots.end(),
        [playerId](const MotionSlot& s) { return s.playerId == playerId; }
    ), slots.end());
    lit->second.items.removePlayer(static_cast<uint32_t>(playerId));
}

void RaceHandler::clearRoom(uint32_t roomId) {
    std::lock_guard<std::mutex> lock(m_raceMutex);
    m_racePlayers.erase(roomId);
    m_raceStartTimes.erase(roomId);
    auto t = m_raceWatchdogTimers.find(roomId);
    if (t != m_raceWatchdogTimers.end()) {
        t->second->cancel();
        m_raceWatchdogTimers.erase(t);
    }
    auto bt = m_botFinishTimers.find(roomId);
    if (bt != m_botFinishTimers.end()) {
        for (auto& timer : bt->second) timer->cancel();
        m_botFinishTimers.erase(bt);
    }
    auto gt = m_finishGraceTimers.find(roomId);
    if (gt != m_finishGraceTimers.end()) {
        gt->second->cancel();
        m_finishGraceTimers.erase(gt);
    }
    m_roomLive.erase(roomId);
}

void RaceHandler::sendCountdown(Room* room, int32_t seconds) {
    room->broadcast(PacketBuilder::countdown(seconds));
}

void RaceHandler::sendRaceStart(Room* room) {
    if (!room) return;
    // GO is a bare 0x0D then 0x3A the grid went out five seconds earlier once the client said it loaded
    room->broadcast(SpawnPackets::rankBoardRebuild());
    room->broadcast(PacketBuilder::results({}));
}

// one S2C 0x3E per racer then S2C 0x0D then one S2C 0x68 per remote viewer
void RaceHandler::sendGrid(Room* room) {
    if (!room) return;

    std::vector<GridSpawn> grid;
    std::vector<int32_t> roster;
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        grid = m_roomLive[room->id()].grid;
        for (const auto& p : m_racePlayers[room->id()]) roster.push_back(p.playerId);
    }

    // out of range index drops the car at the world origin with no lift
    const int32_t startRows = static_cast<int32_t>(grid.size());
    const std::vector<uint32_t> indices =
        SpawnPackets::assignGridIndices(roster.size(), startRows);
    if (indices.size() < roster.size()) {
        LOG_WARN("RACE", "grid holds " + std::to_string(indices.size()) + " of " +
                 std::to_string(roster.size()) + " racers the rest never spawn");
    }

    struct Arm {
        int32_t playerId = 0;
        uint32_t gridIndex = 0;
        DriftBoostTuning tuning;
        bool tuned = false;
    };

    std::vector<SpawnPackets::GridEntry> entries;
    std::vector<Arm> arms;
    entries.reserve(indices.size());
    arms.reserve(indices.size());

    for (size_t i = 0; i < indices.size(); ++i) {
        const int32_t playerId = roster[i];
        RoomPlayer* rp = room->getPlayerByCharacter(playerId);
        if (!rp) {
            LOG_WARN("RACE", "racer " + std::to_string(playerId) +
                     " has no room row no 0x3E built");
            continue;
        }

        SpawnPackets::GridEntry e;
        e.playerId    = static_cast<uint32_t>(playerId);
        e.displayName = rp->name;
        e.gridIndex   = indices[i];
        // same 0 red 1 blue space the 0x21 spoke for this racer
        e.team        = wireTeam(rp->team);
        // same blobs the room member carried a synthesized record has zero skin keys sub 490A70 drops the socket
        GameServer::loadoutBlobs(*rp, e.character, e.kart, e.customCar);
        // sub 479D60 hands this key to sub 48CBB0 which loads Pet Body of that pet beside the racer
        e.petBaseKey  = GachaHandler::equippedPetBaseKey(static_cast<uint32_t>(playerId));
        entries.push_back(e);

        // db read stays outside the lock else tick stalls on the kart catalog
        Arm a;
        a.playerId  = playerId;
        a.gridIndex = indices[i];
        a.tuned     = loadTuning(playerId, rp->vehicleTemplateId, a.tuning);
        arms.push_back(a);
    }

    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        RoomRaceLive& live = m_roomLive[room->id()];
        for (const auto& a : arms) {
            auto* p = getPlayer(room->id(), a.playerId);
            if (!p) continue;
            p->gridIndex = a.gridIndex;
            p->lapTracker.reset(live.checkpointCount, live.totalLaps);
            p->lapBoardRow    = 0;
            p->haveProgress   = false;
            p->progressWarned = false;
            p->progressScore  = 0;
            p->motionGate     = MotionGateState{};
            p->suspiciousCount = 0;
            p->lastSuspicionDecayMs = 0;
            p->finishRank     = ResultsPackets::RANK_RETIRE;
            p->tuning         = a.tuning;
            p->tuningLoaded   = a.tuned;
            if (a.tuned) {
                DriftBoostPackets::resetTrack(p->drift, p->tuning);
            } else {
                DriftBoostPackets::resetTrack(p->drift);
            }
        }
    }

    if (!SpawnPackets::validateGrid(entries, startRows)) {
        LOG_ERROR("RACE", "grid rejected for room " + std::to_string(room->id()));
    }

    for (const auto& e : entries) room->broadcast(SpawnPackets::gridSpawn(e));

    // board grid index is only written for occupied cars so rebuild after the last 0x3E
    room->broadcast(SpawnPackets::rankBoardRebuild());

    // S2C 0x131 per human racer before item boxes are reachable no weight table exists so this is a plausible fill
    {
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<uint32_t> dist;
        for (const auto& e : entries) {
            if (Room::isBotId(static_cast<int32_t>(e.playerId))) continue;
            for (const auto& s : room->sessions()) {
                if (s->characterId != e.playerId) continue;
                if (!s->isConnected()) continue;
                std::array<uint32_t, 100> rolls{};
                for (auto& v : rolls) v = dist(rng);
                s->send(RacePackets::itemRollStream(rolls));
                break;
            }
        }
    }

    if (grid.empty()) {
        LOG_WARN("RACE", "no authored grid for room " + std::to_string(room->id()) +
                 " every car places itself from start ini");
        return;
    }

    // 0x68 flushes the remote 0x40 queue so send it before any motion lands
    for (size_t i = 0; i < entries.size(); ++i) {
        const uint32_t gi = entries[i].gridIndex;
        const GridSpawn* row = nullptr;
        for (const auto& g : grid) {
            if (static_cast<uint32_t>(g.gridIndex) != gi) continue;
            row = &g;
            break;
        }
        if (!row) {
            LOG_WARN("RACE", "no track_spawn row for grid index " + std::to_string(gi));
            continue;
        }

        const int32_t playerId = static_cast<int32_t>(entries[i].playerId);
        // owner physics body ignores 0x68 so only the other screens get the placement
        sendToOthers(room, MotionPackets::teleportToGrid(entries[i].playerId, *row), playerId);

        std::lock_guard<std::mutex> lock(m_raceMutex);
        auto* p = getPlayer(room->id(), playerId);
        if (!p) continue;
        // anticheat baseline else the first delta reads as a teleport
        p->x = row->x;
        p->y = row->y;
        p->z = row->z;
        p->rot = row->yawDegrees;
        p->motionGate = MotionGateState{};
        DriftBoostPackets::resyncPosition(p->drift);
    }
}

void RaceHandler::sendResults(Room* room) {
    // 0x39 teardown race hud first
    room->broadcast(PacketBuilder::finish(0, 0, 0));
    broadcastScoreboard(room);
}

// 0x46 header team is recipient specific so build one packet per connection
void RaceHandler::broadcastScoreboard(Room* room) {
    if (!room) return;

    std::vector<RacePlayer> players;
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        players = m_racePlayers[room->id()];
    }

    // finished first then unfinished
    std::sort(players.begin(), players.end(),
        [](const RacePlayer& a, const RacePlayer& b) {
            if (a.finished != b.finished) return a.finished > b.finished;
            return a.position < b.position;
        });

    std::vector<ResultsPackets::ScoreRow> rows;
    int32_t place = 0;
    for (const auto& p : players) {
        if (place >= ResultsPackets::MAX_ROWS) {
            LOG_WARN("RACE", "scoreboard capped at " + std::to_string(ResultsPackets::MAX_ROWS) +
                     " rows room " + std::to_string(room->id()));
            break;
        }

        ResultsPackets::ScoreRow r;
        r.rank         = place;
        r.finishTimeMs = p.finished ? p.totalTime : 0;
        r.playerId     = static_cast<uint32_t>(p.playerId);

        if (auto* rp = room->getPlayerByCharacter(p.playerId)) {
            r.displayName      = rp->name;
            r.team             = wireTeam(rp->team);
            r.characterBaseKey = static_cast<uint32_t>(rp->driverId);
        }

        // UNKNOWN levelIndex has no client reader pccafeIconIndex has no pccafe column both stay zero
        r.levelIndex      = 0;
        r.pccafeIconIndex = -1;
        r.bonusItemKey    = p.bonusItemKey;

        ResultsPackets::Reward rw;
        rw.playerId   = static_cast<uint32_t>(p.playerId);
        rw.finishRank = p.finishRank;
        rw.gold       = p.score - p.goldBonus;
        rw.xp         = p.xpReward - p.xpBonus;
        rw.goldBonus  = p.goldBonus;
        rw.xpBonus    = p.xpBonus;
        ResultsPackets::applyReward(r, rw);

        rows.push_back(r);
        ++place;
    }

    for (const auto& s : room->sessions()) {
        if (!s->isConnected()) continue;
        uint32_t myTeam = 0;
        if (auto* rp = room->getPlayerByCharacter(static_cast<int32_t>(s->characterId))) {
            myTeam = wireTeam(rp->team);
        }
        s->send(ResultsPackets::scoreboard(myTeam, rows));
    }

    // camera mode 5 is what opens the board the rows must already be in
    room->broadcast(SpawnPackets::openResultBoard());
}

void RaceHandler::calculatePositions(uint32_t roomId) {
    std::lock_guard<std::mutex> lock(m_raceMutex);
    auto it = m_racePlayers.find(roomId);
    if (it == m_racePlayers.end()) return;

    auto& players = it->second;
    refreshRankKeys(m_roomLive[roomId], players);

    // sort finished by time else by the server place key then the 0x67 score lap and z
    std::vector<RacePlayer*> sorted;
    for (auto& p : players) {
        sorted.push_back(&p);
    }

    std::sort(sorted.begin(), sorted.end(), rankLess);

    uint8_t pos = 1;
    for (auto* p : sorted) {
        p->position = pos++;
    }
}

bool RaceHandler::checkRaceTimeout(Room* room, GameServer* server) {
    if (!room) return false;
    if (room->state() != RoomState::Racing) return false;

    // ten minute hard cap
    constexpr int64_t MAX_RACE_DURATION_MS = 10 * 60 * 1000;

    int64_t elapsed = 0;
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        auto it = m_raceStartTimes.find(room->id());
        if (it == m_raceStartTimes.end()) return false;
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - it->second
        ).count();
    }

    if (elapsed < MAX_RACE_DURATION_MS) return false;

    LOG_WARN("RACE", "Race timeout in room " + std::to_string(room->id()) +
             " after " + std::to_string(elapsed / 1000) + "s forcing end");

    // flush unfinished
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        auto it = m_racePlayers.find(room->id());
        if (it != m_racePlayers.end()) {
            int32_t timeoutMs = static_cast<int32_t>(elapsed);
            for (auto& p : it->second) {
                if (!p.finished) {
                    p.finished  = true;
                    p.totalTime = timeoutMs;
                }
            }
        }
    }

    endRace(room, server);
    return true;
}

void RaceHandler::handlePlayerLeave(Room* room, int32_t playerId, GameServer* server) {
    if (!room) return;

    const bool racing = room->state() == RoomState::Racing;

    int32_t activeCars = 0;
    bool hadFinished = false;
    if (racing) {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        auto it = m_racePlayers.find(room->id());
        if (it != m_racePlayers.end()) {
            activeCars = static_cast<int32_t>(it->second.size());
            if (auto* p = getPlayer(room->id(), playerId)) hadFinished = p->finished;
        }
    }

    removePlayer(room->id(), playerId);

    // a racer who leaves between the grid and the GO owes no scene loaded answer any more
    bool goNow = false;
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        auto wait = m_awaitLoaded.find(room->id());
        if (wait != m_awaitLoaded.end() && wait->second.owes(playerId)) goNow = wait->second.clear(playerId);
    }
    if (goNow && server) {
        LOG_INFO("RACE", "char " + std::to_string(playerId) + " left room " + std::to_string(room->id()) +
                 " before the GO, every other racer is loaded, GO");
        goRace(room->id(), server);
        return;
    }

    // S2C 0xF0 despawn for a racer who left mid race FUN 0048DEA0 resolves the id id space unconfirmed here
    if (racing) {
        sendToOthers(room, PacketBuilder::entityRemove(playerId), playerId);
    }

    // client docks its own wallet on leave race so the server must dock the same amount
    if (racing && !hadFinished && activeCars >= RETIRE_MIN_CARS &&
        !Room::isBotId(playerId)) {
        auto& db = Database::instance();
        auto wallet = db.queryPrepared(
            "SELECT gold, experience FROM characters WHERE id = ? LIMIT 1", {playerId});
        if (wallet.empty()) {
            LOG_WARN("RACE", "no wallet row for retiring char " + std::to_string(playerId) +
                     " penalty skipped");
        } else {
            int32_t gold = rowInt(wallet[0], "gold", 0);
            int32_t xp   = rowInt(wallet[0], "experience", 0);
            const int32_t gold0 = gold;
            const int32_t xp0   = xp;
            ResultsPackets::applyRetirePenalty(gold, xp, activeCars);
            const int32_t goldDock = gold0 - gold;
            const int32_t xpDock   = xp0 - xp;
            if (goldDock > 0 || xpDock > 0) {
                db.executePrepared(
                    "UPDATE characters SET gold = GREATEST(gold - ?, 0), "
                    "experience = GREATEST(experience - ?, 0) WHERE id = ?",
                    {goldDock, xpDock, playerId});
                LOG_INFO("RACE", "retire dock char " + std::to_string(playerId) +
                         " gold " + std::to_string(goldDock) + " exp " + std::to_string(xpDock));
            }
        }
    }

    if (!racing) return;

    // remaining finishers must not wait on a gone player
    bool any = false;
    bool allFinished = true;
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        auto it = m_racePlayers.find(room->id());
        if (it != m_racePlayers.end() && !it->second.empty()) {
            any = true;
            for (const auto& p : it->second) {
                if (!p.finished) { allFinished = false; break; }
            }
        }
    }
    if (any && allFinished) endRace(room, server);
}

void RaceHandler::scheduleRaceWatchdog(uint32_t roomId, GameServer* server) {
    if (!server) return;
    auto timer = std::make_shared<asio::steady_timer>(server->ioContext());
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        m_raceWatchdogTimers[roomId] = timer;
    }
    timer->expires_after(std::chrono::seconds(30));
    timer->async_wait([this, roomId, server, timer](const std::error_code& ec) {
        if (ec) return;
        auto room = server->getRoom(roomId);
        if (!room || room->state() != RoomState::Racing) {
            std::lock_guard<std::mutex> lock(m_raceMutex);
            m_raceWatchdogTimers.erase(roomId);
            return;
        }
        // ten minute cap lives in checkRaceTimeout which ends and clears room
        if (checkRaceTimeout(room.get(), server)) return;
        scheduleRaceWatchdog(roomId, server);
    });
}

void RaceHandler::scheduleBotFinishers(uint32_t roomId, GameServer* server) {
    if (!server) return;
    auto room = server->getRoom(roomId);
    if (!room) return;

    auto botList = room->bots();
    if (botList.empty()) return;

    // TODO real bot pace from follow pathing deferred plausible server computed finish staggered around a base duration
    constexpr int32_t BOT_BASE_MS = 90000;
    constexpr int32_t BOT_STAGGER_MS = 4000;

    thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int32_t> jitter(-2000, 2000);

    int i = 0;
    for (const auto& b : botList) {
        int32_t finishMs = BOT_BASE_MS + i * BOT_STAGGER_MS + jitter(rng);
        if (finishMs < 30000) finishMs = 30000;
        int32_t botId = b.characterId;

        auto timer = std::make_shared<asio::steady_timer>(server->ioContext());
        timer->expires_after(std::chrono::milliseconds(finishMs));
        {
            std::lock_guard<std::mutex> lock(m_raceMutex);
            m_botFinishTimers[roomId].push_back(timer);
        }
        timer->async_wait([this, roomId, botId, finishMs, server, timer](const std::error_code& ec) {
            if (ec) return;  // cancelled on clearRoom
            finishBot(roomId, botId, finishMs, server);
        });
        ++i;
    }

    LOG_INFO("RACE", "Scheduled " + std::to_string(botList.size()) +
             " bot finishers room " + std::to_string(roomId));
}

// the 0x3D rank of zero runs FUN 004A9AC0 the rest time countdown cars still out retire after twenty seconds
void RaceHandler::scheduleFinishGrace(uint32_t roomId, GameServer* server) {
    if (!server) return;
    constexpr int64_t kGraceMs = 20000;
    auto timer = std::make_shared<asio::steady_timer>(server->ioContext());
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        if (m_finishGraceTimers.count(roomId)) return;   // one per race
        m_finishGraceTimers[roomId] = timer;
    }
    LOG_INFO("RACE", "first finisher in room " + std::to_string(roomId) +
             ", twenty seconds for the rest");
    timer->expires_after(std::chrono::milliseconds(kGraceMs));
    timer->async_wait([this, roomId, server, timer](const std::error_code& ec) {
        if (ec) return;   // cancelled by clearRoom the race ended on its own
        auto room = server->getRoom(roomId);
        if (!room || room->state() != RoomState::Racing) return;
        size_t retired = 0;
        {
            std::lock_guard<std::mutex> lock(m_raceMutex);
            int32_t elapsedMs = 0;
            auto st = m_raceStartTimes.find(roomId);
            if (st != m_raceStartTimes.end()) {
                elapsedMs = static_cast<int32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - st->second).count());
            }
            auto it = m_racePlayers.find(roomId);
            if (it != m_racePlayers.end()) {
                // the ones still out keep their running order behind the finishers
                std::vector<RacePlayer*> out;
                for (auto& p : it->second) if (!p.finished) out.push_back(&p);
                std::sort(out.begin(), out.end(), rankLess);
                int32_t place = 0;
                for (const auto& p : it->second) if (p.finished) ++place;
                for (auto* p : out) {
                    p->finished   = true;
                    p->totalTime  = elapsedMs + place;   // keeps the order strict
                    p->position   = static_cast<uint8_t>(place + 1);
                    p->finishRank = place;
                    ++place;
                    ++retired;
                }
            }
        }
        LOG_INFO("RACE", "rest time over in room " + std::to_string(roomId) + ", " +
                 std::to_string(retired) + " retired");
        endRace(room.get(), server);
    });
}

void RaceHandler::finishBot(uint32_t roomId, int32_t botId, int32_t finishMs, GameServer* server) {
    auto room = server->getRoom(roomId);
    if (!room || room->state() != RoomState::Racing) return;

    int rank = 1;
    bool allFinished = false;
    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        auto* p = getPlayer(roomId, botId);
        if (!p || p->finished) return;

        p->finished = true;
        p->totalTime = finishMs;

        for (const auto& o : m_racePlayers[roomId]) {
            if (o.finished && o.playerId != botId && o.totalTime < finishMs) rank++;
        }
        p->position = static_cast<uint8_t>(rank);
        p->finishRank = rank - 1;

        allFinished = true;
        for (const auto& o : m_racePlayers[roomId]) {
            if (!o.finished) { allFinished = false; break; }
        }
    }
    if (rank == 1 && !allFinished) scheduleFinishGrace(roomId, server);

    // bots never touch db reward wallet or race history see BOTS ASSESSMENT md
    LOG_INFO("RACE", "Bot " + std::to_string(botId) + " finished rank " +
             std::to_string(rank) + " time=" + std::to_string(finishMs) + "ms");

    // every screen still needs the rank else the board row shows a stale place
    room->broadcast(ResultsPackets::rankBroadcast(static_cast<uint32_t>(botId), rank - 1));
    room->broadcast(ResultsPackets::driverAnimState(static_cast<uint32_t>(botId),
                                                    ResultsPackets::ANIM_FINISH));

    if (allFinished) endRace(room.get(), server);
}

// CPU cars see RaceBots h

namespace {

// what a bot takes from what lands on it the same codes a client reports on 0x69
int16_t hazardEffect(int32_t kind) {
    switch (kind) {
        case ItemPackets::ITEM_SPIKE: return ItemPackets::EFFECT_SPIN;
        case ItemPackets::ITEM_BOMB:  return ItemPackets::EFFECT_CRASH;
        case ItemPackets::ITEM_ICE:   return ItemPackets::EFFECT_ICE;
        case ItemPackets::ITEM_HIVE:  return ItemPackets::EFFECT_HIVE;
        case ItemPackets::ITEM_DUNG:  return ItemPackets::EFFECT_SPIN;
        default: return ItemPackets::EFFECT_NONE;
    }
}

constexpr float    BOT_BOX_REACH       = 6.0f;    // world units a body overlap on the client
constexpr float    BOT_HAZARD_REACH    = 3.0f;
constexpr uint64_t BOT_BOX_COOLDOWN_MS = 4000;
constexpr uint64_t BOT_HAZARD_ARM_MS   = 400;     // a dropped item is not live under the dropper
constexpr uint64_t BOT_HAZARD_LIFE_MS  = 25000;
constexpr float    BOT_LAP_SECONDS     = 48.0f;   // the base pace aims at this on any line
constexpr float    BOT_MIN_SPEED       = 42.0f;
// human laps Forest at 150 u s anti cheat gate is 600 so bots pin to 73 s laps
constexpr float    BOT_MAX_SPEED       = 180.0f;
constexpr uint64_t BOT_GO_HOLD_MS      = 10000;  // real kart moves 9 5 to 11 5 s after 0x3A bots hold that long

} // namespace

bool RaceHandler::armBots(Room* room) {
    if (!room) return false;
    const uint32_t roomId = room->id();
    const uint64_t now = nowMs();

    std::lock_guard<std::mutex> lock(m_raceMutex);
    RoomRaceLive& live = m_roomLive[roomId];
    live.bots.clear();
    live.hazards.clear();
    live.pendingHits.clear();
    live.raceStartMs = now;
    live.lastBotTickMs = now;
    if (!live.botTrack.loaded || live.botTrack.line.empty()) return false;

    // a lap in about BOT LAP SECONDS on this line inside the anti cheat ceiling
    float base = live.botTrack.lineLength / BOT_LAP_SECONDS;
    base = std::max(BOT_MIN_SPEED, std::min(BOT_MAX_SPEED, base));

    std::mt19937 seedRng(static_cast<uint32_t>(now) ^ (roomId * 2654435761u));
    std::uniform_real_distribution<float> paceDist(0.90f, 1.06f);

    for (const auto& p : m_racePlayers[roomId]) {
        if (!Room::isBotId(p.playerId)) continue;
        // one driver per id or two sims fight over one motion slot and the car jitters
        bool already = false;
        for (const auto& b : live.bots) {
            if (b.playerId == p.playerId) { already = true; break; }
        }
        if (already) {
            LOG_WARN("BOT", "roster holds CPU car " + std::to_string(p.playerId) +
                     " twice in room " + std::to_string(roomId) + " second row dropped");
            continue;
        }
        const GridSpawn* row = nullptr;
        for (const auto& g : live.grid) {
            if (static_cast<uint32_t>(g.gridIndex) == p.gridIndex) { row = &g; break; }
        }
        float sx = p.x, sy = p.y, sz = p.z, syaw = p.rot;
        if (row) {
            sx = row->x; sy = row->y; sz = row->z; syaw = row->yawDegrees;
        } else {
            // no authored grid start on the line itself
            const auto& n0 = live.botTrack.line[0];
            sx = n0.x; sy = n0.y; sz = n0.z; syaw = n0.yawDegrees;
        }
        BotRacer bot;
        bot.playerId = p.playerId;
        bot.driver.arm(&live.botTrack.line, live.botTrack.lineLength, sx, sy, sz, syaw,
                       base * paceDist(seedRng), static_cast<int32_t>(live.totalLaps),
                       seedRng());
        live.bots.push_back(std::move(bot));

        // the slot every fan out reads a bot has no session so nothing comes back to it
        MotionSlot& slot = motionSlot(live, p.playerId, nullptr);
        slot.state.x = sx; slot.state.y = sy; slot.state.z = sz;
        slot.state.tx = sx; slot.state.ty = sy; slot.state.tz = sz;
        slot.state.yaw = MotionPackets::yawToByte(syaw);
        slot.state.stateBits = 0x7A02;
        slot.lastSampleMs = now;
        slot.hasSample = true;
        slot.stale = false;
        slot.needSnap = false;
    }
    LOG_INFO("BOT", std::to_string(live.bots.size()) + " CPU cars on the line in room " +
             std::to_string(roomId) + " base pace " + std::to_string(static_cast<int>(base)) +
             " u/s over " + std::to_string(static_cast<int>(live.botTrack.lineLength)) + " units");
    return !live.bots.empty();
}

void RaceHandler::queueBotHit(uint32_t roomId, int32_t victimId, int16_t code, uint64_t delayMs) {
    std::lock_guard<std::mutex> lock(m_raceMutex);
    auto it = m_roomLive.find(roomId);
    if (it == m_roomLive.end()) return;
    it->second.pendingHits.push_back(BotPendingHit{victimId, code, nowMs() + delayMs});
}

void RaceHandler::noteHazard(uint32_t roomId, int32_t ownerId, int32_t kind, float x, float y, float z) {
    if (hazardEffect(kind) == ItemPackets::EFFECT_NONE) return;
    std::lock_guard<std::mutex> lock(m_raceMutex);
    auto it = m_roomLive.find(roomId);
    if (it == m_roomLive.end() || it->second.bots.empty()) return;
    it->second.hazards.push_back(BotHazard{ownerId, kind, x, y, z, nowMs()});
}

void RaceHandler::tickBots(uint64_t now, GameServer* server) {
    if (!server) return;
    // broadcasts is room and frame unicasts is room character and frame finishes is room bot and elapsed ms
    std::vector<std::pair<uint32_t, Packet>> broadcasts;
    std::vector<std::tuple<uint32_t, int32_t, Packet>> unicasts;
    std::vector<std::tuple<uint32_t, int32_t, int32_t>> finishes;
    std::vector<uint32_t> lapRooms;

    {
        std::lock_guard<std::mutex> lock(m_raceMutex);
        for (auto& kv : m_roomLive) {
            const uint32_t roomId = kv.first;
            RoomRaceLive& live = kv.second;
            if (live.bots.empty()) continue;
            auto rosterIt = m_racePlayers.find(roomId);
            if (rosterIt == m_racePlayers.end()) continue;
            auto& roster = rosterIt->second;

            uint64_t dt = live.lastBotTickMs ? now - live.lastBotTickMs : 0;
            live.lastBotTickMs = now;
            if (dt == 0) continue;
            if (now < live.raceStartMs + BOT_GO_HOLD_MS) {
                // still on the grid keep the slot fresh or the fan out drops the car for ten seconds
                for (const auto& b : live.bots) {
                    MotionSlot& slot = motionSlot(live, b.playerId, nullptr);
                    slot.lastSampleMs = now;
                    slot.hasSample = true;
                    slot.stale = false;
                    live.dirty = true;
                }
                continue;
            }
            if (dt > 250) dt = 250;   // a stalled io thread must not teleport the field

            // the leading human by progress the rubber band pulls the pack toward it
            uint32_t bestHuman = 0;
            bool haveHuman = false;
            for (const auto& p : roster) {
                if (Room::isBotId(p.playerId) || !p.haveProgress) continue;
                haveHuman = true;
                bestHuman = std::max(bestHuman, static_cast<uint32_t>(p.progressScore));
            }

            live.hazards.erase(std::remove_if(live.hazards.begin(), live.hazards.end(),
                [now](const BotHazard& h) { return now > h.placedMs + BOT_HAZARD_LIFE_MS; }),
                live.hazards.end());

            // hits that came due a rocket or a turtle a client or a bot fired
            for (auto it = live.pendingHits.begin(); it != live.pendingHits.end();) {
                if (now < it->dueMs) { ++it; continue; }
                for (auto& b : live.bots) {
                    if (b.playerId != it->victimId) continue;
                    if (!b.driver.stunned(now)) {
                        b.driver.hit(it->code, now);
                        broadcasts.emplace_back(roomId, ItemPackets::hitBroadcast(
                            static_cast<uint32_t>(b.playerId), it->code));
                    }
                    break;
                }
                it = live.pendingHits.erase(it);
            }

            bool lapEvent = false;
            for (auto& b : live.bots) {
                BotDriver& d = b.driver;

                // rubber band far ahead of the best human eases off far behind pushes
                if (haveHuman && !d.raceDone()) {
                    const int64_t gap = static_cast<int64_t>(d.progressScore()) -
                                        static_cast<int64_t>(bestHuman);
                    float scale = 1.0f;
                    if (gap > 1500)       scale = 0.88f;
                    else if (gap > 600)   scale = 0.95f;
                    else if (gap < -1500) scale = 1.10f;
                    else if (gap < -600)  scale = 1.04f;
                    d.setPaceScale(scale);
                }

                const int32_t lapsBefore = d.lapsDone();
                const bool doneBefore = d.raceDone();
                if (!d.step(now, dt)) continue;

                // hazards on the road
                if (!d.stunned(now)) {
                    for (auto it = live.hazards.begin(); it != live.hazards.end(); ++it) {
                        if (it->ownerId == b.playerId && now < it->placedMs + 2000) continue;
                        if (now < it->placedMs + BOT_HAZARD_ARM_MS) continue;
                        const float dx = it->x - d.x(), dy = it->y - d.y();
                        if (dx * dx + dy * dy > BOT_HAZARD_REACH * BOT_HAZARD_REACH) continue;
                        const int16_t code = hazardEffect(it->kind);
                        d.hit(code, now);
                        broadcasts.emplace_back(roomId, ItemPackets::hitBroadcast(
                            static_cast<uint32_t>(b.playerId), code));
                        live.hazards.erase(it);
                        break;
                    }
                }

                // item boxes the pickup puts the icon on every other screen
                if (d.heldItem == ItemPackets::ITEM_EMPTY && !d.raceDone() &&
                    now >= d.lastBoxMs + BOT_BOX_COOLDOWN_MS) {
                    for (const auto& box : live.botTrack.boxes) {
                        const float dx = box.x - d.x(), dy = box.y - d.y();
                        if (dx * dx + dy * dy > BOT_BOX_REACH * BOT_BOX_REACH) continue;
                        static const int32_t kBotItems[] = {
                            ItemPackets::ITEM_BOOSTER, ItemPackets::ITEM_SPIKE,
                            ItemPackets::ITEM_ROCKET,  ItemPackets::ITEM_BOMB,
                            ItemPackets::ITEM_ICE,     ItemPackets::ITEM_BIG_BOOSTER,
                            ItemPackets::ITEM_SPIKE };
                        std::uniform_int_distribution<int> pick(0, 6);
                        std::uniform_int_distribution<int> delay(1500, 4500);
                        d.heldItem = kBotItems[pick(d.rng())];
                        d.useAtMs = now + static_cast<uint64_t>(delay(d.rng()));
                        d.lastBoxMs = now;
                        broadcasts.emplace_back(roomId, ItemPackets::grantBroadcast(
                            static_cast<uint32_t>(b.playerId), d.heldItem, 0));
                        break;
                    }
                }

                // use what it holds
                if (d.heldItem != ItemPackets::ITEM_EMPTY && now >= d.useAtMs && !d.stunned(now)) {
                    const int32_t kind = d.heldItem;
                    bool used = true;
                    if (kind == ItemPackets::ITEM_ROCKET) {
                        // the nearest racer ahead human or bot
                        int32_t target = 0;
                        uint32_t bestScore = 0xFFFFFFFFu;
                        const uint32_t mine = d.progressScore();
                        for (const auto& p : roster) {
                            if (p.playerId == b.playerId || p.finished) continue;
                            uint32_t score = static_cast<uint32_t>(p.progressScore);
                            for (const auto& ob : live.bots) {
                                if (ob.playerId == p.playerId) score = ob.driver.progressScore();
                            }
                            if (score <= mine || score >= bestScore) continue;
                            bestScore = score;
                            target = p.playerId;
                        }
                        if (target == 0) {
                            used = false;
                            d.useAtMs = now + 1500;
                        } else {
                            broadcasts.emplace_back(roomId, ItemPackets::homingLaunch(
                                static_cast<uint32_t>(b.playerId), kind, b.playerId, target));
                            if (Room::isBotId(target)) {
                                live.pendingHits.push_back(
                                    BotPendingHit{target, ItemPackets::EFFECT_CRASH, now + 1500});
                            } else {
                                // the targeted client draws its own lock warning off this
                                unicasts.emplace_back(roomId, target, ItemPackets::lockStateRelay(
                                    target, ItemPackets::LOCK_LOCKED, kind));
                            }
                        }
                    } else if (kind == ItemPackets::ITEM_BOOSTER || kind == ItemPackets::ITEM_BIG_BOOSTER) {
                        d.boost(now, kind == ItemPackets::ITEM_BOOSTER ? 1.30f : 1.45f,
                                kind == ItemPackets::ITEM_BOOSTER ? 2000 : 3000);
                    } else {
                        // a hazard dropped where the car is every receiver spawns it there
                        broadcasts.emplace_back(roomId, ItemPackets::itemSpawn(
                            static_cast<uint32_t>(b.playerId), kind, d.x(), d.y(), d.z(), d.yawDeg()));
                        live.hazards.push_back(BotHazard{b.playerId, kind, d.x(), d.y(), d.z(), now});
                    }
                    if (used) {
                        broadcasts.emplace_back(roomId,
                            ItemPackets::playSoundCue(static_cast<uint32_t>(b.playerId)));
                        d.heldItem = ItemPackets::ITEM_EMPTY;
                    }
                }

                // the roster row the standings and the targeting read
                if (auto* p = getPlayer(roomId, b.playerId)) {
                    p->x = d.x(); p->y = d.y(); p->z = d.z(); p->rot = d.yawDeg();
                    p->lap = static_cast<uint8_t>(std::min<int32_t>(d.lapsDone(), 255));
                    // the standings compare this with human 0x67 so it must be the same checkpoint formula
                    p->progressScore = static_cast<int32_t>(live.checkpointPoints.size() >= 2
                        ? b.checkpoints.update(live.checkpointPoints, d.x(), d.y())
                        : d.progressScore());
                    p->haveProgress = true;
                    p->lastUpdate = std::chrono::steady_clock::now();
                }

                // the motion slot the fan out publishes
                MotionSlot& slot = motionSlot(live, b.playerId, nullptr);
                CarState st;
                st.x = d.x(); st.y = d.y(); st.z = d.z();
                d.lookahead(st.tx, st.ty, st.tz);
                st.yaw = MotionPackets::yawToByte(d.yawDeg());
                // byte 17 is flags byte 18 is speed low nibble and drift charge high nibble seven is neutral
                const float turn = d.turnDeg();
                uint8_t flags = 0;
                if (turn > 2.5f || turn < -2.5f) flags |= 0x10;
                uint8_t lo = static_cast<uint8_t>(d.speedNow() / 85.0f * 11.0f);
                if (lo > 11) lo = 11;
                uint8_t hi = 7;
                if (turn > 2.5f) hi = 9; else if (turn < -2.5f) hi = 5;
                st.stateBits = static_cast<uint16_t>(flags | ((hi << 4 | lo) << 8));
                slot.state = st;
                slot.lastSampleMs = now;
                slot.hasSample = true;
                slot.stale = false;
                live.dirty = true;

                if (d.lapsDone() > lapsBefore) lapEvent = true;
                if (d.raceDone() && !doneBefore && !b.finished) {
                    b.finished = true;
                    finishes.emplace_back(roomId, b.playerId,
                                          static_cast<int32_t>(now - live.raceStartMs));
                }
            }
            if (lapEvent) lapRooms.push_back(roomId);
        }
    }

    for (auto& bc : broadcasts) {
        auto room = server->getRoom(bc.first);
        if (room) room->broadcast(bc.second);
    }
    for (auto& uc : unicasts) {
        auto room = server->getRoom(std::get<0>(uc));
        if (!room) continue;
        for (const auto& s : room->sessions()) {
            if (static_cast<int32_t>(s->characterId) != std::get<1>(uc)) continue;
            s->send(std::get<2>(uc));
            break;
        }
    }
    for (uint32_t roomId : lapRooms) {
        auto room = server->getRoom(roomId);
        if (!room) continue;
        calculatePositions(roomId);
        updatePositions(room.get());
    }
    for (auto& f : finishes) {
        LOG_INFO("BOT", "CPU car " + std::to_string(std::get<1>(f)) + " crossed the line in " +
                 std::to_string(std::get<2>(f)) + " ms room " + std::to_string(std::get<0>(f)));
        finishBot(std::get<0>(f), std::get<1>(f), std::get<2>(f), server);
    }
}

} // namespace knc

