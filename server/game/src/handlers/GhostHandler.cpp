/// ghost records replays ghost stage 15 and the quest stage 17 ghost

#include "handlers/GhostHandler.h"
#include "handlers/RaceHandler.h"
#include "packets/gen/GhostPackets.h"
#include "packets/gen/ShopPackets.h"
#include "GameServer.h"
#include "packets/PacketBuilder.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace knc {

namespace {

using DbRow = std::map<std::string, std::string>;

// client recv buffer is a fixed 0x2000 and a bigger frame kills its parse state
constexpr size_t kFrameCap = 0x2000;

// only three board rows are ever drawn so a wider burst is dead weight
constexpr size_t kBoardEntriesSent = 3;

// picker holds 55 slots per theme and the record cache holds 55 tracks
constexpr size_t kBoardTrackCap = 55;

// stage 13 recordings play back ten times too slow in stage 15
constexpr size_t kRepDecimation = 10;

// encoder splits the full steer span into fifteen buckets so one is this wide
constexpr float kSteerBucketDegrees = 2.0f * GhostPackets::kMaxSteerDegrees / 15.0f;

std::mutex g_sessionsMutex;
std::unordered_map<uint32_t, GhostUploadSession> g_sessions;

GhostUploadSession& sessionFor(uint32_t sessionId) {
    return g_sessions[sessionId];
}

bool sessionActive(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(g_sessionsMutex);
    auto it = g_sessions.find(sessionId);
    return it != g_sessions.end() && it->second.trackId >= 0;
}

int64_t rowI64(const DbRow& row, const char* key, int64_t fallback) {
    return rowInt64NoThrow(row, key, fallback);
}

uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

// oversized frame is fatal on the client so drop it here and shout
void sendChecked(const Session::Ptr& session, const Packet& pkt, const char* what) {
    if (pkt.payload().size() + 8 >= kFrameCap) {
        LOG_ERROR("GHOST", std::string(what) + " frame " +
                           std::to_string(pkt.payload().size() + 8) +
                           " past the client buffer " + std::to_string(kFrameCap) + " dropped");
        return;
    }
    session->send(pkt);
}

bool blockIsZero(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (data[i] != 0) return false;
    }
    return true;
}

bool readWholeFile(const std::string& path, std::vector<uint8_t>& out) {
    out.clear();

    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;

    uint8_t buf[8192];
    while (true) {
        const size_t got = std::fread(buf, 1, sizeof(buf), fp);
        if (got == 0) break;
        out.insert(out.end(), buf, buf + got);
    }
    const bool bad = std::ferror(fp) != 0;
    std::fclose(fp);

    if (bad) out.clear();
    return !bad;
}

// client decode adds one bucket so take it back off to land in the encoder domain
float steerDegreesOf(uint32_t steerSpeed) {
    return GhostPackets::steerDegreesFromBucket(GhostPackets::steerOf(steerSpeed))
         - kSteerBucketDegrees;
}

// client decode drops the floor so put it back
float rawSpeedOf(uint32_t steerSpeed) {
    return GhostPackets::speedFromBucket(GhostPackets::speedOf(steerSpeed))
         + GhostPackets::kSpeedFloor;
}

/// keeps one frame per step both bucket inverses round trip exactly on all 16 buckets and all 9218 shipped records
std::vector<ReplayFrame> resampleForGhostRate(const std::vector<ReplayFrame>& raw, size_t step) {
    std::vector<ReplayFrame> out;
    if (step == 0) step = 1;
    out.reserve(raw.size() / step + 1);

    for (size_t i = 0; i < raw.size(); i += step) {
        const size_t end = (i + step) < raw.size() ? (i + step) : raw.size();

        double steerSum = 0.0;
        double speedSum = 0.0;
        for (size_t k = i; k < end; ++k) {
            steerSum += static_cast<double>(steerDegreesOf(raw[k].steerSpeed));
            speedSum += static_cast<double>(rawSpeedOf(raw[k].steerSpeed));
        }
        const double span = static_cast<double>(end - i);

        ReplayFrame f = raw[i];
        // kept frame stands for the whole window so quantise the average not the sample
        f.steerSpeed = GhostPackets::makeSteerSpeed(
            GhostPackets::steerBucket(static_cast<float>(steerSum / span)),
            GhostPackets::speedBucket(static_cast<float>(speedSum / span)));
        out.push_back(f);
    }
    return out;
}

/// S2C 0xF6 0xF7 for the quest stage and 0xAE 0xAF for the ghost stage count goes first even at zero
void pushReplay(const Session::Ptr& session, const std::vector<ReplayFrame>& frames,
                bool questStage) {
    const uint32_t total = static_cast<uint32_t>(frames.size());

    // wrong start pose is the usual ghost symptom so put it in the log
    if (total != 0) {
        LOG_DEBUG("GHOST", std::string(questStage ? "quest" : "race") + " ghost starts at " +
                           std::to_string(frames[0].posX) + " " +
                           std::to_string(frames[0].posY) + " " +
                           std::to_string(frames[0].posZ) + " heading " +
                           std::to_string(GhostPackets::yawFromByte(frames[0].yaw)));
    }

    sendChecked(session,
                questStage ? GhostPackets::altFrameCount(total)
                           : GhostPackets::ghostFrameCount(total),
                questStage ? "altFrameCount" : "ghostFrameCount");

    for (size_t off = 0; off < frames.size(); off += GhostPackets::kChunkFrames) {
        const size_t left = frames.size() - off;
        const size_t take = left < GhostPackets::kChunkFrames ? left
                                                             : GhostPackets::kChunkFrames;
        const Packet chunk = questStage
            ? GhostPackets::altFrameChunk(frames.data() + off, take)
            : GhostPackets::ghostFrameChunk(frames.data() + off, take);
        sendChecked(session, chunk, questStage ? "altFrameChunk" : "ghostFrameChunk");
    }
}

} // namespace

bool GhostHandler::isBoardRequest(const Packet& packet) {
    uint32_t menuKind = 0;
    uint32_t subKind = 0;
    if (!GhostPackets::parseMenuSelect(packet, menuKind, subKind)) return false;
    return menuKind == GhostPackets::kMenuKindGhost && subKind == 0;
}

void GhostHandler::handleMenuSelect(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;

    uint32_t menuKind = 0;
    uint32_t subKind = 0;
    if (!GhostPackets::parseMenuSelect(packet, menuKind, subKind)) {
        LOG_WARN("GHOST", "menu select short payload from " + session->remoteAddress());
        return;
    }
    if (menuKind != GhostPackets::kMenuKindGhost || subKind != 0) {
        LOG_WARN("GHOST", "menu select kind " + std::to_string(menuKind) + " sub " +
                          std::to_string(subKind) + " is not the ghost board");
        return;
    }
    if (session->characterId == 0) {
        LOG_WARN("GHOST", "board request with no character from " + session->remoteAddress());
        session->send(ShopPackets::rejectAscii("MSG_UNKNOWN_ERROR", 1));
        return;
    }

    sendRecordBoard(session, false, server);
}

// the client asks for the board twice on one click so a second pass builds on top of the first
namespace {
std::mutex g_boardMutex;
std::map<uint32_t, int64_t> g_lastBoardMs;
constexpr int64_t kBoardDedupeMs = 3000;

bool boardAlreadySent(uint32_t sessionId, int64_t now) {
    std::lock_guard<std::mutex> lock(g_boardMutex);
    auto it = g_lastBoardMs.find(sessionId);
    if (it != g_lastBoardMs.end() && now - it->second < kBoardDedupeMs) return true;
    g_lastBoardMs[sessionId] = now;
    return false;
}
}  // namespace

void GhostHandler::sendRecordBoard(Session::Ptr session, bool withPopup,
                                   GameServer* server) {
    if (session && boardAlreadySent(session->id(), static_cast<int64_t>(RaceHandler::nowMs()))) {
        LOG_INFO("GHOST", "board already sent to session " + std::to_string(session->id()) +
                 ", skipping the twin so the screen is not built twice");
        return;
    }
    // empty record table on a fresh install shows a blank board so fill it first
    seedShippedReplaysOnce();

    std::vector<int32_t> tracks = boardTrackOrder();
    // KNC GHOST BOARD TRACKS caps the board compose always defines the variable so empty means no cap
    const char* capEnv = getenv("KNC_GHOST_BOARD_TRACKS");
    if (capEnv && capEnv[0] != '\0') {
        const size_t cap = static_cast<size_t>(std::max(0, atoi(capEnv)));
        if (tracks.size() > cap) tracks.resize(cap);
        LOG_WARN("GHOST", "board capped to " + std::to_string(tracks.size()) + " tracks by env");
    }

    if (tracks.empty()) {
        LOG_WARN("GHOST", "board has no tracks, maps table empty or every row disabled");
        // still open the board else the MSG WAIT modal never closes
        sendChecked(session, GhostPackets::boardHeader(0), "boardHeader");
        if (withPopup) sendChecked(session, GhostPackets::boardOpen(), "boardOpen");
        return;
    }

    // one flat burst caused Unknown error -4 since sub 476CC0 recv buffer is a fixed 0x2000 collect and drip instead
    std::vector<Packet> burst;
    burst.push_back(GhostPackets::boardHeader(static_cast<uint32_t>(tracks.size())));

    size_t sent = 0;
    for (int32_t trackId : tracks) {
        GhostRecordEntry best;
        GhostRecord bestRecord;
        if (GhostPackets::trackBest(trackId, bestRecord)) {
            best.name = bestRecord.name;
            best.timeMs = bestRecord.timeMs;
        }

        const std::vector<GhostRecordEntry> entries =
            GhostPackets::trackEntries(trackId, kBoardEntriesSent);

        burst.push_back(GhostPackets::boardTrack(static_cast<uint32_t>(trackId), best, entries));
        ++sent;
    }

    // the fill index guard drops every 0xAC past what 0xAB announced
    if (sent != tracks.size()) {
        LOG_ERROR("GHOST", "board announced " + std::to_string(tracks.size()) +
                           " tracks but sent " + std::to_string(sent));
    }

    // only send 0xAD for the standalone popup sub 47CAE0 sub 4538B0 sub 458510 the 0x011D handler already closes it
    if (withPopup) burst.push_back(GhostPackets::boardOpen());

    if (server) {
        server->sendDripped(session, std::move(burst));
    } else {
        LOG_WARN("GHOST", "board sent without a server so it goes out in one write, "
                          "over 0x2000 the client answers -4");
        for (auto& p : burst) sendChecked(session, p, "boardFallback");
    }

    LOG_INFO("GHOST", "board sent " + std::to_string(sent) + " tracks to char " +
                      std::to_string(session->characterId));
}

bool GhostHandler::isGhostEnter(const Packet& packet) {
    // tutorial complete on the same opcode is three int32
    return packet.payload().size() == 4;
}

void GhostHandler::handleGhostEnter(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;

    uint32_t trackId = 0;
    if (!GhostPackets::parseGhostEnter(packet, trackId)) {
        LOG_WARN("GHOST", "ghost enter short payload from " + session->remoteAddress());
        return;
    }
    if (session->characterId == 0) {
        LOG_WARN("GHOST", "ghost enter with no character from " + session->remoteAddress());
        session->send(ShopPackets::rejectAscii("MSG_UNKNOWN_ERROR", 1));
        return;
    }

    auto& db = Database::instance();

    // reads the exact catalogue row the client saw since it sent trackRec offset 0x04 and gates on trackRec offset 0x48
    auto trackRows = db.queryPrepared(
        "SELECT track_id AS id, COALESCE(required_license, 0) AS required_license "
        "FROM track_catalog WHERE track_id = ? LIMIT 1",
        { static_cast<int32_t>(trackId) });

    if (trackRows.empty()) {
        LOG_WARN("GHOST", "ghost enter track " + std::to_string(trackId) +
                          " unknown or disabled");
        session->send(ShopPackets::rejectAscii("MSG_UNKNOWN_ERROR", 1));
        return;
    }

    auto charRows = db.queryPrepared(
        "SELECT COALESCE(level, 1) AS level FROM characters WHERE id = ? LIMIT 1",
        { static_cast<int32_t>(session->characterId) });

    if (charRows.empty()) {
        LOG_WARN("GHOST", "ghost enter char " + std::to_string(session->characterId) +
                          " absent from characters");
        session->send(PacketBuilder::displayMessage(u"MSG_DB_ACCESS_FAIL", 2));
        return;
    }

    // one of the two client send sites has no gate at all so re check here
    const int64_t needed = rowI64(trackRows[0], "required_license", 1);
    const int64_t level = rowI64(charRows[0], "level", 1);
    if (level < needed) {
        LOG_WARN("GHOST", "ghost enter track " + std::to_string(trackId) + " needs level " +
                          std::to_string(needed) + " char has " + std::to_string(level));
        session->send(PacketBuilder::displayMessage(u"MSG_MAP_LEVEL_HIGH", 2));
        return;
    }

    GhostSessionInfo info;
    info.trackId = trackId;
    info.carKind = GhostPackets::kCarKindA;
    info.charBlock = GhostPackets::defaultCharBlock();
    info.kartBlock = GhostPackets::defaultKartBlock();

    std::vector<ReplayFrame> frames;

    // the ghost is one place ahead of this character not the track best
    GhostRecord best;
    if (GhostPackets::ghostAhead(static_cast<int32_t>(trackId), session->characterId, best)) {
        info.name = best.name;
        info.carKind = best.carKind;
        info.recordTimeMs = best.timeMs;
        if (!blockIsZero(best.charBlock.data(), best.charBlock.size())) {
            info.charBlock = best.charBlock;
        }
        if (!blockIsZero(best.kartBlock.data(), best.kartBlock.size())) {
            info.kartBlock = best.kartBlock;
        }
        if (!GhostPackets::loadReplay(static_cast<int32_t>(trackId), best.charId, frames)) {
            LOG_WARN("GHOST", "track " + std::to_string(trackId) +
                              " has a record but no readable replay so the ghost car is skipped");
            frames.clear();
        }
    } else {
        LOG_INFO("GHOST", "track " + std::to_string(trackId) +
                          " has no record so the run is a plain time attack");
    }

    // client cursor is not bounds checked and record 24000 lands on the count field
    if (frames.size() > GhostPackets::kMaxFramesArray) {
        LOG_ERROR("GHOST", "ghost replay " + std::to_string(frames.size()) +
                           " frames past capacity " +
                           std::to_string(GhostPackets::kMaxFramesArray) + " dropped");
        frames.clear();
    }

    pushReplay(session, frames, false);

    // stage 15 init only spawns the ghost when the count is already above zero
    sendChecked(session, GhostPackets::ghostSession(info), "ghostSession");

    {
        std::lock_guard<std::mutex> lock(g_sessionsMutex);
        GhostUploadSession& state = sessionFor(session->id());
        GhostPackets::resetUpload(state);
        state.trackId = static_cast<int32_t>(trackId);
    }

    LOG_INFO("GHOST", "ghost race track " + std::to_string(trackId) + " char " +
                      std::to_string(session->characterId) + " frames " +
                      std::to_string(frames.size()));
}

bool GhostHandler::isStageEvent(const Packet& packet) {
    // friend remove and block on the same opcodes both carry one int32
    return packet.payload().empty();
}

void GhostHandler::handleStageBegin(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;
    GhostPackets::parseStageBegin(packet);

    std::lock_guard<std::mutex> lock(g_sessionsMutex);
    GhostUploadSession& state = sessionFor(session->id());
    if (state.trackId < 0) {
        LOG_WARN("GHOST", "stage begin with no ghost race running from " +
                          session->remoteAddress());
        return;
    }
    state.started = true;
    LOG_DEBUG("GHOST", "stage begin track " + std::to_string(state.trackId));
}

void GhostHandler::handleFinalLap(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;
    GhostPackets::parseFinalLap(packet);

    std::lock_guard<std::mutex> lock(g_sessionsMutex);
    GhostUploadSession& state = sessionFor(session->id());
    if (state.trackId < 0) {
        LOG_WARN("GHOST", "final lap with no ghost race running from " +
                          session->remoteAddress());
        return;
    }
    if (!state.started) {
        LOG_WARN("GHOST", "final lap before stage begin on track " +
                          std::to_string(state.trackId));
    }
    state.finishedLap = true;
    LOG_DEBUG("GHOST", "final lap track " + std::to_string(state.trackId));
}

void GhostHandler::handleUploadCount(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;

    uint32_t frameCount = 0;
    if (!GhostPackets::parseUploadCount(packet, frameCount)) {
        LOG_WARN("GHOST", "upload count short payload from " + session->remoteAddress());
        return;
    }

    std::lock_guard<std::mutex> lock(g_sessionsMutex);
    GhostUploadSession& state = sessionFor(session->id());
    if (state.trackId < 0) {
        LOG_WARN("GHOST", "upload count with no ghost race running from " +
                          session->remoteAddress());
        return;
    }
    if (frameCount > GhostPackets::kUploadClamp) {
        LOG_WARN("GHOST", "upload count " + std::to_string(frameCount) +
                          " past the client clamp " +
                          std::to_string(GhostPackets::kUploadClamp) + " so it is forged");
    }
    if (!GhostPackets::beginUpload(state, frameCount)) {
        LOG_WARN("GHOST", "upload count " + std::to_string(frameCount) + " refused");
        return;
    }
    LOG_DEBUG("GHOST", "upload announces " + std::to_string(frameCount) + " frames");
}

void GhostHandler::handleUploadChunk(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;

    std::vector<ReplayFrame> frames;
    if (!GhostPackets::parseUploadChunk(packet, frames)) {
        LOG_WARN("GHOST", "upload chunk unparseable from " + session->remoteAddress());
        return;
    }

    std::lock_guard<std::mutex> lock(g_sessionsMutex);
    GhostUploadSession& state = sessionFor(session->id());
    if (state.trackId < 0) {
        LOG_WARN("GHOST", "upload chunk with no ghost race running from " +
                          session->remoteAddress());
        return;
    }
    if (!GhostPackets::appendUpload(state, frames)) {
        LOG_WARN("GHOST", "upload chunk of " + std::to_string(frames.size()) +
                          " frames refused, have " + std::to_string(state.frames.size()) +
                          " of " + std::to_string(state.announcedCount));
    }
}

bool GhostHandler::isSubmit(Session::Ptr session, const Packet& packet) {
    // add friend on the same opcode is a wstring that can also be 12 bytes
    if (packet.payload().size() != 12) return false;
    return sessionActive(session->id());
}

void GhostHandler::handleSubmit(Session::Ptr session, Packet& packet, GameServer* server) {
    uint32_t trackId = 0;
    uint32_t totalTimeMs = 0;
    uint32_t carKind = 0;

    GhostUploadSession state;
    {
        std::lock_guard<std::mutex> lock(g_sessionsMutex);
        state = sessionFor(session->id());
    }

    const bool parsed = GhostPackets::parseSubmit(packet, trackId, totalTimeMs, carKind);

    // record write walks every replay chunk slowest query the server runs goes on the pool keyed by session id
    const uint32_t charId = session->characterId;
    const std::u16string charName = session->characterName;
    auto work = [session, server, state, parsed, trackId, totalTimeMs, carKind,
                 charId, charName]() mutable {
    GhostRecordEntry mine;
    uint32_t rank = 0;
    if (!parsed) {
        LOG_WARN("GHOST", "submit short payload from " + session->remoteAddress());
    } else if (state.trackId < 0) {
        LOG_WARN("GHOST", "submit with no ghost race running from " + session->remoteAddress());
    } else if (static_cast<int32_t>(trackId) != state.trackId) {
        LOG_WARN("GHOST", "submit track " + std::to_string(trackId) + " does not match the " +
                          std::to_string(state.trackId) + " the race started on");
    } else if (!GhostPackets::uploadComplete(state)) {
        LOG_WARN("GHOST", "submit on an incoherent burst, started " +
                          std::to_string(state.started ? 1 : 0) + " finalLap " +
                          std::to_string(state.finishedLap ? 1 : 0) + " frames " +
                          std::to_string(state.frames.size()) + " announced " +
                          std::to_string(state.announcedCount));
    } else if (totalTimeMs == 0) {
        LOG_WARN("GHOST", "submit time zero renders as an empty row so it is not stored");
    } else {
        GhostRecord record;
        record.trackId = state.trackId;
        record.charId = charId;
        record.name = charName;
        record.timeMs = static_cast<int32_t>(totalTimeMs);
        record.carKind = carKind;
        record.frameCount = static_cast<uint32_t>(state.frames.size());
        record.charBlock = GhostPackets::defaultCharBlock();
        record.kartBlock = GhostPackets::defaultKartBlock();

        if (GhostPackets::saveRecord(record, state.frames)) {
            // read the chunks straight back so a bad write shows now not next race
            if (!exportRecordReplay(state.trackId, charId)) {
                LOG_WARN("GHOST", "record export failed for track " +
                                  std::to_string(state.trackId) + " char " +
                                  std::to_string(charId));
            }
        } else {
            LOG_INFO("GHOST", "record not stored for track " + std::to_string(state.trackId) +
                              " char " + std::to_string(charId));
        }

        // the panel shows the record the board keeps a slower run does not replace the better one
        mine.name = charName;
        mine.timeMs = static_cast<int32_t>(totalTimeMs);
        GhostRecord kept;
        if (GhostPackets::playerBest(state.trackId, charId, kept) &&
            kept.timeMs > 0 && kept.timeMs < mine.timeMs) {
            mine.timeMs = kept.timeMs;
        }
        rank = GhostPackets::rankOf(state.trackId, mine.timeMs);
    }

    // state 2013 is the switch default so without this reply the client never leaves
    const int32_t resultTrack = state.trackId >= 0 ? state.trackId
                                                   : static_cast<int32_t>(trackId);
    std::vector<GhostRecordEntry> top;
    if (resultTrack > 0) {
        top = GhostPackets::trackEntries(resultTrack, GhostPackets::kMaxTopRecords);
    }

    // only the io thread may send Session send has no lock of its own
    Packet reply = GhostPackets::submitResult(rank, mine, top);
    auto deliver = [session, reply, resultTrack, totalTimeMs, rank]() {
        session->send(reply);
        LOG_INFO("GHOST", "submit answered track " + std::to_string(resultTrack) + " time " +
                          std::to_string(totalTimeMs) + " rank " + std::to_string(rank));
        std::lock_guard<std::mutex> lock(g_sessionsMutex);
        GhostPackets::resetUpload(sessionFor(session->id()));
    };
    if (server) server->postIo(deliver); else deliver();
    };

    if (!server || !server->postDb(session->id(), work)) work();
}

bool GhostHandler::isQuestGhostStart(const Packet& packet) {
    return packet.payload().size() == 4;
}

void GhostHandler::handleQuestGhostStart(Session::Ptr session, Packet& packet,
                                         GameServer* server) {
    (void)server;

    const std::vector<uint8_t>& body = packet.payload();
    if (body.size() < 4) {
        LOG_WARN("GHOST", "quest start short payload from " + session->remoteAddress());
        return;
    }
    if (session->characterId == 0) {
        LOG_WARN("GHOST", "quest start with no character from " + session->remoteAddress());
        session->send(ShopPackets::rejectAscii("MSG_UNKNOWN_ERROR", 1));
        return;
    }

    seedShippedReplaysOnce();

    const uint32_t questIndex = readU32LE(body.data());
    const size_t pushed = sendQuestGhost(session, questIndex);

    LOG_INFO("GHOST", "quest start " + std::to_string(questIndex) + " char " +
                      std::to_string(session->characterId) + " ghost frames " +
                      std::to_string(pushed));
}

size_t GhostHandler::sendQuestGhost(Session::Ptr session, uint32_t questIndex) {
    std::vector<ReplayFrame> frames;

    auto rows = Database::instance().queryPrepared(
        "SELECT track_id, char_id FROM ghost_quest_replay "
        "WHERE quest_index = ? AND COALESCE(is_enabled, 1) = 1 LIMIT 1",
        { questIndex });

    if (rows.empty()) {
        LOG_WARN("GHOST", "quest " + std::to_string(questIndex) +
                          " has no ghost_quest_replay row so the quest runs with no ghost");
    } else {
        const int32_t trackId = static_cast<int32_t>(rowI64(rows[0], "track_id", 0));
        uint32_t charId = static_cast<uint32_t>(rowI64(rows[0], "char_id", 0));

        // sub 47DD10 writes 0xF3 rec offset 0x0C into dword C70A4C which stage 17 reads so this row must match it
        auto known = Database::instance().queryPrepared(
            "SELECT track_id FROM track_catalog WHERE track_id = ? LIMIT 1", { trackId });
        if (known.empty()) {
            LOG_ERROR("GHOST", "quest " + std::to_string(questIndex) + " names track " +
                               std::to_string(trackId) + " which has no track_catalog row so "
                               "sub_4531F0 misses and stage 17 dies on Track initialize fail");
        }

        if (charId == 0) {
            GhostRecord best;
            if (GhostPackets::trackBest(trackId, best)) charId = best.charId;
        }
        if (charId != 0 && !GhostPackets::loadReplay(trackId, charId, frames)) {
            LOG_WARN("GHOST", "quest " + std::to_string(questIndex) +
                              " maps to track " + std::to_string(trackId) +
                              " which has no readable replay");
            frames.clear();
        }
    }

    if (frames.size() > GhostPackets::kMaxFramesGhost) {
        LOG_WARN("GHOST", "quest ghost " + std::to_string(frames.size()) +
                          " frames past the recorder cap " +
                          std::to_string(GhostPackets::kMaxFramesGhost));
    }
    if (frames.size() > GhostPackets::kMaxFramesArray) {
        LOG_ERROR("GHOST", "quest ghost " + std::to_string(frames.size()) +
                           " frames past capacity " +
                           std::to_string(GhostPackets::kMaxFramesArray) + " dropped");
        frames.clear();
    }

    // send even at zero else the ghost of the last stage 15 race drives the quest
    pushReplay(session, frames, true);
    return frames.size();
}

void GhostHandler::onDisconnect(Session::Ptr session) {
    std::lock_guard<std::mutex> lock(g_sessionsMutex);
    g_sessions.erase(session->id());
}

std::vector<int32_t> GhostHandler::boardTrackOrder() {
    std::vector<int32_t> out;

    // the client stores 0xAC by arrival index against 0xC3 row order emit one entry per row in order disabled included
    auto rows = Database::instance().queryPrepared(
        "SELECT track_id AS id FROM track_catalog ORDER BY track_id ASC LIMIT 55", {});

    out.reserve(rows.size());
    for (const auto& row : rows) {
        out.push_back(static_cast<int32_t>(rowI64(row, "id", 0)));
    }

    if (out.size() > kBoardTrackCap) {
        LOG_ERROR("GHOST", "board track order " + std::to_string(out.size()) +
                           " past the client array " + std::to_string(kBoardTrackCap));
        out.resize(kBoardTrackCap);
    }
    return out;
}

std::string GhostHandler::replayDir() {
    if (const char* env = std::getenv("KNC_REPLAY_DIR")) {
        if (env[0] != '\0') return std::string(env);
    }
    return "DevClient";
}

std::string GhostHandler::exportDir() {
    if (const char* env = std::getenv("KNC_GHOST_EXPORT_DIR")) {
        if (env[0] != '\0') return std::string(env);
    }
    return ".";
}

bool GhostHandler::exportRepFile(const std::string& path,
                                 const std::vector<ReplayFrame>& frames) {
    if (frames.size() > GhostPackets::kMaxFramesArray) {
        LOG_ERROR("GHOST", "rep export " + std::to_string(frames.size()) +
                           " frames past capacity " +
                           std::to_string(GhostPackets::kMaxFramesArray) + " refused");
        return false;
    }

    const std::vector<uint8_t> image = GhostPackets::encodeRep(frames);
    const std::vector<uint8_t> body  = GhostPackets::encodeFrames(frames);

    // loader sub 4A04F0 reads one count then a packed run so the two must agree
    if (image.size() != 4 + body.size()) {
        LOG_ERROR("GHOST", "rep image " + std::to_string(image.size()) +
                           " is not the count plus the packed run " +
                           std::to_string(4 + body.size()));
        return false;
    }
    if (!body.empty() && std::memcmp(image.data() + 4, body.data(), body.size()) != 0) {
        LOG_ERROR("GHOST", "rep image body does not match the packed frame run");
        return false;
    }

    if (!GhostPackets::writeRepFile(path, frames)) return false;

    std::vector<uint8_t> onDisk;
    if (!readWholeFile(path, onDisk)) {
        LOG_ERROR("GHOST", "rep export could not read back " + path);
        return false;
    }
    if (onDisk != image) {
        LOG_ERROR("GHOST", "rep on disk " + std::to_string(onDisk.size()) +
                           " bytes differs from the image " + std::to_string(image.size()));
        return false;
    }

    std::vector<ReplayFrame> back;
    if (!GhostPackets::decodeRep(onDisk.data(), onDisk.size(), back)) {
        LOG_ERROR("GHOST", "rep export " + path + " does not load back");
        return false;
    }
    if (back.size() != frames.size()) {
        LOG_ERROR("GHOST", "rep export " + path + " loads " + std::to_string(back.size()) +
                           " frames of " + std::to_string(frames.size()));
        return false;
    }

    if (!frames.empty()) {
        ReplayFrame first;
        if (!GhostPackets::decodeFrame(onDisk.data() + 4, GhostPackets::kFrameSize, first)) {
            LOG_ERROR("GHOST", "rep export " + path + " first record is short");
            return false;
        }
        if (first.yaw != frames[0].yaw || first.flags != frames[0].flags ||
            first.steerSpeed != frames[0].steerSpeed ||
            first.inputMask != frames[0].inputMask) {
            LOG_ERROR("GHOST", "rep export " + path + " first record does not match the source");
            return false;
        }
    }

    LOG_INFO("GHOST", "rep exported " + path + " frames " + std::to_string(frames.size()) +
                      " bytes " + std::to_string(onDisk.size()));
    return true;
}

bool GhostHandler::exportRecordReplay(int32_t trackId, uint32_t charId) {
    std::vector<ReplayFrame> frames;
    if (!GhostPackets::loadReplay(trackId, charId, frames)) {
        LOG_WARN("GHOST", "record export found no replay for track " +
                          std::to_string(trackId) + " char " + std::to_string(charId));
        return false;
    }

    std::string base = exportDir();
    if (!base.empty() && base.back() != '/' && base.back() != '\\') base += '/';

    const std::string path = base + "ghost_t" + std::to_string(trackId) +
                             "_c" + std::to_string(charId) + ".rep";
    return exportRepFile(path, frames);
}

bool GhostHandler::importRepFile(const std::string& path, int32_t trackId, uint32_t charId,
                                 const std::u16string& name, uint32_t carKind) {
    std::vector<ReplayFrame> raw;
    if (!GhostPackets::readRepFile(path, raw)) {
        LOG_WARN("GHOST", "rep import could not read " + path);
        return false;
    }
    if (raw.empty()) {
        LOG_WARN("GHOST", "rep import " + path + " has no frames");
        return false;
    }

    // shipped rep is stage 13 at 20 ms per frame and ghost playback is 200 ms
    const std::vector<ReplayFrame> frames = resampleForGhostRate(raw, kRepDecimation);

    // a replay the chunker refuses could never reach a client so do not store it
    const std::vector<Packet> wire = GhostPackets::ghostFrameChunks(frames);
    if (wire.empty() && !frames.empty()) {
        LOG_ERROR("GHOST", "rep import " + path + " cannot be chunked for the wire so it is dropped");
        return false;
    }

    const int32_t timeMs = static_cast<int32_t>(raw.size()) * GhostPackets::kFrameMsStage13;

    GhostRecord record;
    record.trackId = trackId;
    record.charId = charId;
    record.name = name;
    record.timeMs = timeMs;
    record.carKind = carKind;
    record.frameCount = static_cast<uint32_t>(frames.size());
    record.charBlock = GhostPackets::defaultCharBlock();
    record.kartBlock = GhostPackets::defaultKartBlock();

    if (!GhostPackets::saveRecord(record, frames)) {
        LOG_WARN("GHOST", "rep import " + path + " not stored, a faster time is already there");
        return false;
    }

    LOG_INFO("GHOST", "rep import " + path + " track " + std::to_string(trackId) + " raw " +
                      std::to_string(raw.size()) + " kept " + std::to_string(frames.size()) +
                      " packets " + std::to_string(wire.size()) +
                      " time " + std::to_string(timeMs));

    // keep the 200 ms ghost we will actually serve next to the 20 ms original
    std::string base = exportDir();
    if (!base.empty() && base.back() != '/' && base.back() != '\\') base += '/';
    const std::string outPath = base + "ghost_t" + std::to_string(trackId) +
                                "_c" + std::to_string(charId) + ".rep";
    if (!exportRepFile(outPath, frames)) {
        LOG_WARN("GHOST", "rep import could not export the decimated replay " + outPath);
    }
    return true;
}

size_t GhostHandler::seedShippedReplays(const std::string& devClientDir) {
    // lesson id over ten picks the License world so 02 03 map to 90 and 11 12 to 91
    struct Seed {
        const char* file;
        int32_t     trackId;
        uint32_t    charId;
        const char16_t* name;
    };
    // db layer binds a u32 param as a signed LONG so a seed id must stay positive
    static const Seed seeds[] = {
        { "License_Track_02.rep", 90, 0x7F000002u, u"DevGhost A" },
        { "License_Track_03.rep", 90, 0x7F000003u, u"DevGhost B" },
        { "License_Track_11.rep", 91, 0x7F000011u, u"DevGhost C" },
        { "License_Track_12.rep", 91, 0x7F000012u, u"DevGhost D" },
    };

    std::string base = devClientDir;
    if (!base.empty() && base.back() != '/' && base.back() != '\\') base += '/';

    size_t stored = 0;
    for (const auto& seed : seeds) {
        if (importRepFile(base + seed.file, seed.trackId, seed.charId,
                          std::u16string(seed.name), GhostPackets::kCarKindA)) {
            ++stored;
        }
    }

    if (stored == 0) {
        LOG_WARN("GHOST", "no shipped replay was seeded from " + devClientDir);
    } else {
        LOG_INFO("GHOST", "seeded " + std::to_string(stored) + " shipped replays");
    }
    return stored;
}

void GhostHandler::seedShippedReplaysOnce() {
    static std::once_flag once;
    std::call_once(once, []() {
        auto rows = Database::instance().queryPrepared(
            "SELECT COUNT(*) AS n FROM ghost_record", {});

        const int64_t have = rows.empty() ? 0 : rowI64(rows[0], "n", 0);
        if (have > 0) {
            LOG_DEBUG("GHOST", "ghost_record already holds " + std::to_string(have) +
                               " rows so the shipped replays are not seeded");
            return;
        }

        const std::string dir = replayDir();
        LOG_INFO("GHOST", "ghost_record is empty so seeding shipped replays from " + dir);
        seedShippedReplays(dir);
    });
}

} // namespace knc
