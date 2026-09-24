#include "packets/gen/SpawnPackets.h"
#include "packets/gen/MotionPackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace knc {

namespace {

uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

float readF32LE(const uint8_t* p) {
    float v = 0.0f;
    std::memcpy(&v, p, 4);
    return v;
}

uint32_t floatBits(float v) {
    uint32_t b = 0;
    std::memcpy(&b, &v, 4);
    return b;
}

void writeCappedAscii(Packet& pkt, const std::string& s, size_t cap, const char* what) {
    // dest is a fixed stack buffer and sub 44EB30 has no cap so a long string smashes it
    if (s.size() > cap) {
        LOG_ERROR("PACKET", std::string(what) + " over cap " + std::to_string(s.size()) +
                            " max " + std::to_string(cap) + " truncating");
        pkt.writeString(s.substr(0, cap));
        return;
    }
    pkt.writeString(s);
}

void writeWideNul(Packet& pkt, const std::u16string& s) {
    for (char16_t c : s) {
        pkt.writeUInt8(static_cast<uint8_t>(c & 0xFF));
        pkt.writeUInt8(static_cast<uint8_t>((c >> 8) & 0xFF));
    }
    pkt.writeUInt16(0);
}

bool finiteVec(float x, float y, float z, float w) {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(w);
}

// scanf per cent f skips leading whitespace then converts
bool scanFloat(const std::string& buf, size_t& pos, float& out) {
    const size_t n = buf.size();
    while (pos < n && std::isspace(static_cast<unsigned char>(buf[pos]))) ++pos;
    if (pos >= n) return false;
    const char* start = buf.c_str() + pos;
    char* end = nullptr;
    const float v = std::strtof(start, &end);
    if (end == start) return false;
    pos += static_cast<size_t>(end - start);
    out = v;
    return true;
}

bool readWholeFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

std::string rowStr(const std::map<std::string, std::string>& row, const char* key) {
    return rowStrCore(row, key);
}

int64_t rowInt(const std::map<std::string, std::string>& row, const char* key, int64_t fallback) {
    return rowInt64NoThrow(row, key, fallback);
}

float rowFloat(const std::map<std::string, std::string>& row, const char* key) {
    return rowFloatNoThrow(row, key, 0.0f);
}

const char* const kTrackCatalogColumns =
    "track_id, theme_id, folder_name, display_name_key, visible_flag, "
    "tuning_engine_setup_bits, tuning_engine_force_bits, "
    "tuning_turn_force, unknown_60, fall_off_timeout_ms, "
    "difficulty, required_license, special_mode_only, lap_count, fog_near, fog_far, "
    "lens_flare_x, lens_flare_y, lens_flare_z, checkpoint_count, start_row_count";

SpawnPackets::TrackCatalogRow trackRowFrom(const std::map<std::string, std::string>& row) {
    SpawnPackets::TrackCatalogRow r;
    r.visibleFlag       = static_cast<uint32_t>(rowInt(row, "visible_flag", 0));
    r.trackId           = static_cast<uint32_t>(rowInt(row, "track_id", 0));
    r.themeId           = static_cast<uint32_t>(rowInt(row, "theme_id", 0));
    r.folderName        = rowStr(row, "folder_name");
    r.tuningEngineSetupBits = static_cast<uint32_t>(rowInt(row, "tuning_engine_setup_bits", 1053609165));
    r.tuningEngineForceBits = static_cast<uint32_t>(rowInt(row, "tuning_engine_force_bits", 1058642330));
    r.displayNameKey    = rowStr(row, "display_name_key");
    r.tuningTurnForce   = rowFloat(row, "tuning_turn_force");
    r.unknown60         = static_cast<uint32_t>(rowInt(row, "unknown_60", 0));
    r.fallOffTimeoutMs  = static_cast<uint32_t>(
        rowInt(row, "fall_off_timeout_ms", SpawnPackets::FALL_TIMEOUT_MS));
    r.difficulty        = static_cast<uint32_t>(rowInt(row, "difficulty", 0));
    r.requiredLicense   = static_cast<uint32_t>(rowInt(row, "required_license", 0));
    r.specialModeOnly   = static_cast<uint32_t>(rowInt(row, "special_mode_only", 0));
    r.lapCount          = static_cast<uint32_t>(rowInt(row, "lap_count", 3));
    r.fogNear           = rowFloat(row, "fog_near");
    r.fogFar            = rowFloat(row, "fog_far");
    r.lensFlareX        = rowFloat(row, "lens_flare_x");
    r.lensFlareY        = rowFloat(row, "lens_flare_y");
    r.lensFlareZ        = rowFloat(row, "lens_flare_z");
    r.checkpointCount   = static_cast<int32_t>(rowInt(row, "checkpoint_count", 0));
    r.startRowCount     = static_cast<int32_t>(rowInt(row, "start_row_count", 0));
    return r;
}

} // namespace

Packet SpawnPackets::gridSpawn(const GridEntry& entry) {
    std::u16string name = entry.displayName;
    if (name.size() > MAX_NAME_UNITS) {
        // sub 44EB60 writes into a wide char String 35 with no bound
        LOG_ERROR("PACKET", "gridSpawn name " + std::to_string(name.size()) +
                            " units over client stack buffer truncating");
        name.resize(MAX_NAME_UNITS);
    }
    if (entry.gridIndex > MAX_GRID_INDEX) {
        LOG_ERROR("PACKET", "gridSpawn grid index " + std::to_string(entry.gridIndex) +
                            " past 15 rank board will drop player " +
                            std::to_string(entry.playerId));
    }

    Packet pkt(OP_GRID_SPAWN);
    pkt.writeUInt32(entry.playerId);
    writeWideNul(pkt, name);
    pkt.writeUInt32(entry.gridIndex);
    pkt.writeUInt32(entry.team);
    pkt.writeBytes(entry.character.data(), entry.character.size());
    pkt.writeBytes(entry.kart.data(), entry.kart.size());
    pkt.writeUInt32(entry.petBaseKey);
    pkt.writeBytes(entry.customCar.data(), entry.customCar.size());

    const size_t expected = SIZE_GRID_SPAWN_FIXED + 2 * (name.size() + 1);
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "gridSpawn size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet SpawnPackets::rankBoardRebuild() {
    Packet pkt(OP_RANK_BOARD);
    if (pkt.payload().size() != SIZE_RANK_BOARD) {
        LOG_ERROR("PACKET", "rankBoardRebuild size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(SIZE_RANK_BOARD));
    }
    return pkt;
}

Packet SpawnPackets::lapBoardAdvance() {
    Packet pkt(OP_LAP_BOARD);
    pkt.writeUInt32(0);  // sub 4B0CF0 never reads the argument

    if (pkt.payload().size() != SIZE_LAP_BOARD) {
        LOG_ERROR("PACKET", "lapBoardAdvance size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(SIZE_LAP_BOARD));
    }
    return pkt;
}

Packet SpawnPackets::cameraMode(uint32_t mode) {
    Packet pkt(OP_CAMERA_MODE);
    pkt.writeUInt32(mode);

    if (pkt.payload().size() != SIZE_CAMERA_MODE) {
        LOG_ERROR("PACKET", "cameraMode size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(SIZE_CAMERA_MODE));
    }
    return pkt;
}

Packet SpawnPackets::openResultBoard() {
    return cameraMode(CAMERA_RESULT_BOARD);
}

Packet SpawnPackets::respawnRelay(uint32_t playerId, const RespawnReport& report) {
    // same 20 byte 0x68 MotionPackets already owns so both paths cannot drift
    return MotionPackets::teleport(playerId, report.x, report.y, report.z, report.yawDegrees);
}

Packet SpawnPackets::trackCatalogEntry(const TrackCatalogRow& row) {
    if (row.lapCount < static_cast<uint32_t>(LAP_BOARD_MIN) ||
        row.lapCount > static_cast<uint32_t>(LAP_BOARD_MAX)) {
        LOG_WARN("PACKET", "track " + std::to_string(row.trackId) + " lap count " +
                           std::to_string(row.lapCount) + " outside the client 1 to 9 clamp");
    }
    if (row.folderName.empty()) {
        LOG_ERROR("PACKET", "track " + std::to_string(row.trackId) +
                            " has no folder name so the world path cannot be built");
    }

    Packet pkt(OP_TRACK_CATALOG);
    // track catalog recv 0x47F990 reads field0 trackid themeid folder then fourteen int32 then the name key
    pkt.writeUInt32(row.visibleFlag);
    pkt.writeUInt32(row.trackId);
    pkt.writeUInt32(row.themeId);
    writeCappedAscii(pkt, row.folderName, 35, "track folder_name");  // client buffer 36

    // the three tuning floats go to 0x5EB6F0 0x5EB6F4 0x5EB6F8 as raw dwords
    pkt.writeUInt32(row.tuningEngineSetupBits);
    pkt.writeUInt32(row.tuningEngineForceBits);
    pkt.writeUInt32(floatBits(row.tuningTurnForce));
    pkt.writeUInt32(row.unknown60);
    pkt.writeUInt32(row.fallOffTimeoutMs);
    pkt.writeUInt32(row.difficulty);
    pkt.writeUInt32(row.requiredLicense);
    pkt.writeUInt32(row.specialModeOnly);
    pkt.writeUInt32(row.lapCount);
    // record 0x54 to 0x64 are floats fog near fog far and the lens flare sun position
    pkt.writeUInt32(floatBits(row.fogNear));
    pkt.writeUInt32(floatBits(row.fogFar));
    pkt.writeUInt32(floatBits(row.lensFlareX));
    pkt.writeUInt32(floatBits(row.lensFlareY));
    pkt.writeUInt32(floatBits(row.lensFlareZ));

    // record 0x68 lands in a 36 byte buffer the def trans lookup reads it as the info label
    writeCappedAscii(pkt, row.displayNameKey, 35, "track display_name_key");

    const size_t folderLen = row.folderName.size() < MAX_FOLDER_CHARS
                           ? row.folderName.size() : MAX_FOLDER_CHARS;
    const size_t tailLen = row.displayNameKey.size() < MAX_FOLDER_CHARS
                         ? row.displayNameKey.size() : MAX_FOLDER_CHARS;
    const size_t expected = SIZE_TRACK_CATALOG_FIXED + folderLen + tailLen;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "trackCatalogEntry size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet SpawnPackets::themeCatalogEntry(const ThemeCatalogRow& row) {
    if (row.themeFolder.empty()) {
        LOG_ERROR("PACKET", "theme " + std::to_string(row.themeId) +
                            " has no folder so every track under it fails to load");
    }

    Packet pkt(OP_THEME_CATALOG);
    pkt.writeUInt32(row.recordField0);
    pkt.writeUInt32(row.themeId);
    writeCappedAscii(pkt, row.themeFolder, MAX_THEME_FOLDER_CHARS, "theme_folder");
    writeCappedAscii(pkt, row.displayName, MAX_THEME_NAME_CHARS, "theme display_name");

    const size_t folderLen = row.themeFolder.size() < MAX_THEME_FOLDER_CHARS
                           ? row.themeFolder.size() : MAX_THEME_FOLDER_CHARS;
    const size_t nameLen = row.displayName.size() < MAX_THEME_NAME_CHARS
                         ? row.displayName.size() : MAX_THEME_NAME_CHARS;
    const size_t expected = SIZE_THEME_CATALOG_FIXED + folderLen + nameLen;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "themeCatalogEntry size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

bool SpawnPackets::parseCheckpoint(const uint8_t* data, size_t len, CheckpointReport& out) {
    if (data == nullptr || len < SIZE_CHECKPOINT) {
        LOG_WARN("PACKET", "C2S checkpoint short body " + std::to_string(len));
        return false;
    }
    out.prev = readU32LE(data + 0);
    out.next = readU32LE(data + 4);
    return true;
}

bool SpawnPackets::parseCheckpoint(const Packet& pkt, CheckpointReport& out) {
    const std::vector<uint8_t>& body = pkt.payload();
    return parseCheckpoint(body.data(), body.size(), out);
}

bool SpawnPackets::parseProgress(const uint8_t* data, size_t len, uint32_t& outScore) {
    if (data == nullptr || len < SIZE_PROGRESS) {
        LOG_WARN("PACKET", "C2S progress short body " + std::to_string(len));
        return false;
    }
    outScore = readU32LE(data);
    return true;
}

bool SpawnPackets::parseProgress(const Packet& pkt, uint32_t& outScore) {
    const std::vector<uint8_t>& body = pkt.payload();
    return parseProgress(body.data(), body.size(), outScore);
}

bool SpawnPackets::parseRespawn(const uint8_t* data, size_t len, RespawnReport& out) {
    if (data == nullptr || len < SIZE_RESPAWN_C2S) {
        LOG_WARN("PACKET", "C2S respawn short body " + std::to_string(len));
        return false;
    }
    const float x = readF32LE(data + 0);
    const float y = readF32LE(data + 4);
    const float z = readF32LE(data + 8);
    const float yaw = readF32LE(data + 12);

    if (!finiteVec(x, y, z, yaw)) {
        // relaying this would NaN every other client interpolator
        LOG_WARN("PACKET", "C2S respawn non finite floats dropped");
        return false;
    }

    out.x = x;
    out.y = y;
    out.z = z;
    out.yawDegrees = yaw;
    return true;
}

bool SpawnPackets::parseRespawn(const Packet& pkt, RespawnReport& out) {
    const std::vector<uint8_t>& body = pkt.payload();
    return parseRespawn(body.data(), body.size(), out);
}

std::vector<uint32_t> SpawnPackets::assignGridIndices(size_t racerCount, int32_t startRowCount) {
    size_t cap = static_cast<size_t>(MAX_GRID_INDEX) + 1;
    if (startRowCount > 0 && static_cast<size_t>(startRowCount) < cap) {
        cap = static_cast<size_t>(startRowCount);
    }

    std::vector<uint32_t> out;
    if (cap == 0) {
        LOG_ERROR("PACKET", "assignGridIndices no start rows so every car lands at origin");
        return out;
    }
    if (racerCount > cap) {
        LOG_ERROR("PACKET", "assignGridIndices " + std::to_string(racerCount) +
                            " racers but only " + std::to_string(cap) + " grid slots");
    }

    const size_t n = racerCount < cap ? racerCount : cap;
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) out.push_back(static_cast<uint32_t>(i));
    return out;
}

bool SpawnPackets::validateGrid(const std::vector<GridEntry>& entries, int32_t startRowCount) {
    bool ok = true;

    if (entries.size() > MAX_CARS) {
        LOG_ERROR("PACKET", "grid has " + std::to_string(entries.size()) +
                            " racers but sub_495280 only has 30 slots");
        ok = false;
    }

    std::set<uint32_t> seen;
    for (const auto& e : entries) {
        if (e.gridIndex > MAX_GRID_INDEX) {
            LOG_ERROR("PACKET", "player " + std::to_string(e.playerId) + " grid index " +
                                std::to_string(e.gridIndex) + " past the 16 row rank board");
            ok = false;
        }
        if (startRowCount > 0 && e.gridIndex >= static_cast<uint32_t>(startRowCount)) {
            LOG_ERROR("PACKET", "player " + std::to_string(e.playerId) + " grid index " +
                                std::to_string(e.gridIndex) + " past start rows " +
                                std::to_string(startRowCount) + " car lands at origin");
            ok = false;
        }
        if (!seen.insert(e.gridIndex).second) {
            LOG_ERROR("PACKET", "grid index " + std::to_string(e.gridIndex) +
                                " reused by player " + std::to_string(e.playerId) +
                                " one racer will vanish from the board");
            ok = false;
        }
    }
    return ok;
}

bool SpawnPackets::resolveGridPose(const std::vector<TrackPoint>& startRows,
                                   int32_t gridIndex, TrackPoint& out) {
    out = TrackPoint{};
    if (gridIndex < 0 || static_cast<size_t>(gridIndex) >= startRows.size()) {
        // sub 486C20 early return gives the origin and never applies the lift
        LOG_WARN("PACKET", "grid index " + std::to_string(gridIndex) +
                           " out of range of " + std::to_string(startRows.size()) + " rows");
        return false;
    }

    const TrackPoint& row = startRows[static_cast<size_t>(gridIndex)];
    out = row;
    out.z = row.z + GRID_LIFT_Z;
    return true;
}

uint32_t SpawnPackets::progressLaps(uint32_t score) {
    return score / PROGRESS_PER_LAP;
}

float SpawnPackets::progressLapFraction(uint32_t score) {
    return static_cast<float>(score % PROGRESS_PER_LAP) / static_cast<float>(PROGRESS_PER_LAP);
}

uint32_t SpawnPackets::progressScore(uint32_t lap, uint32_t nextCheckpoint,
                                     float segmentFraction, int32_t checkpointCount) {
    if (checkpointCount <= 0) {
        LOG_WARN("PACKET", "progressScore with no checkpoints returns lap only");
        return lap * PROGRESS_PER_LAP;
    }
    if (!std::isfinite(segmentFraction)) segmentFraction = 0.0f;
    if (segmentFraction < 0.0f) segmentFraction = 0.0f;
    if (segmentFraction > 1.0f) segmentFraction = 1.0f;

    const int32_t n = checkpointCount;
    const float bucket = static_cast<float>(PROGRESS_PER_LAP) / static_cast<float>(n);
    int32_t wrapped = (static_cast<int32_t>(nextCheckpoint) - 2) % n;
    if (wrapped < 0) wrapped += n;

    const float score = segmentFraction * bucket
                      + static_cast<float>(wrapped) * bucket
                      + static_cast<float>(lap) * static_cast<float>(PROGRESS_PER_LAP);
    if (score <= 0.0f) return 0;
    return static_cast<uint32_t>(score);
}

bool SpawnPackets::withinClientWindow(uint32_t detected, uint32_t expected) {
    // no modulus here so no forward skip is ever accepted at the last checkpoint
    return detected >= expected && detected <= expected + CHECKPOINT_WINDOW;
}

void SpawnPackets::LapTracker::reset(int32_t checkpointCount, int32_t totalLaps) {
    m_checkpointCount = checkpointCount > 0 ? checkpointCount : 0;
    m_totalLaps = totalLaps > 0 ? totalLaps : 0;
    m_expected = 0;
    m_lapsCompleted = 0;
    m_crossings = 0;
    m_desynced = false;
}

SpawnPackets::LapTracker::Result
SpawnPackets::LapTracker::onCheckpoint(const CheckpointReport& report) {
    if (m_checkpointCount <= 0) {
        LOG_WARN("RACE", "checkpoint report on a track with no checkpoints");
        return Result::Ignored;
    }

    const uint32_t n = static_cast<uint32_t>(m_checkpointCount);
    if (report.prev >= n || report.next >= n) {
        LOG_WARN("RACE", "checkpoint pair " + std::to_string(report.prev) + " " +
                         std::to_string(report.next) + " outside " + std::to_string(n));
        return Result::Rejected;
    }
    if (report.next != (report.prev + 1) % n) {
        LOG_WARN("RACE", "checkpoint pair " + std::to_string(report.prev) + " " +
                         std::to_string(report.next) + " is not one step");
        return Result::Rejected;
    }

    // grid sits on start faces so the crossing report after spawn arrives before reset count the grid crossing as done
    if (m_crossings == 0 && report.prev != 0) {
        m_crossings = 1;
        m_desynced  = false;
        m_expected  = static_cast<int32_t>(report.next);
        return Result::Advanced;
    }

    // client sends its own pre increment pointer so a mismatch means a lost packet
    m_desynced = (static_cast<int32_t>(report.prev) != m_expected);
    m_expected = static_cast<int32_t>(report.next);

    if (report.prev != 0) return Result::Advanced;

    ++m_crossings;
    if (m_crossings == 1) {
        // grid sits on the START faces so this one is free
        return Result::GridCross;
    }
    ++m_lapsCompleted;
    return Result::LapComplete;
}

std::string SpawnPackets::dataRoot() {
    if (const char* env = std::getenv("KNC_DATA_ROOT")) {
        if (env[0] != '\0') return std::string(env);
    }
    return "DevClient/Data/Public";
}

std::string SpawnPackets::trackBase(const std::string& themeFolder,
                                    const std::string& trackFolder) {
    return dataRoot() + "/World/" + themeFolder + "/" + trackFolder;
}

std::vector<SpawnPackets::TrackPoint>
SpawnPackets::parseIniPoints(const std::string& text, size_t maxRows) {
    std::vector<TrackPoint> out;
    size_t pos = 0;
    size_t shortRows = 0;

    while (out.size() < maxRows) {
        float v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (!scanFloat(text, pos, v[0])) break;

        int got = 1;
        for (int k = 1; k < 4; ++k) {
            if (pos >= text.size() || text[pos] != ',') break;
            ++pos;
            if (!scanFloat(text, pos, v[k])) break;
            ++got;
        }

        TrackPoint p;
        p.x = v[0];
        p.y = v[1];
        p.z = v[2];
        p.yawDegrees = v[3];
        p.hadYawColumn = (got >= 4);
        out.push_back(p);
        if (got < 4) ++shortRows;
    }

    if (shortRows != 0) {
        // client keeps the previous track value here because sub 48A800 never clears
        LOG_WARN("RACE", std::to_string(shortRows) + " ini rows had no yaw column");
    }
    if (out.size() >= maxRows && pos < text.size()) {
        LOG_WARN("RACE", "ini row cap " + std::to_string(maxRows) + " reached rest dropped");
    }
    return out;
}

std::vector<SpawnPackets::TrackPoint>
SpawnPackets::loadIniPoints(const std::string& path, size_t maxRows) {
    std::string text;
    if (!readWholeFile(path, text)) {
        LOG_WARN("RACE", "ini not found " + path);
        return {};
    }
    if (text.empty()) {
        // sub 48A710 rejects a zero byte extract before the parser ever runs
        LOG_ERROR("RACE", "ini empty " + path);
        return {};
    }
    if (text.size() >= MAX_INI_BYTES) {
        // sub 48A710 hard fails at 40960 so the real client never sees this file
        LOG_ERROR("RACE", "ini " + path + " is " + std::to_string(text.size()) +
                          " bytes past the pack VFS cap of " + std::to_string(MAX_INI_BYTES));
        return {};
    }
    return parseIniPoints(text, maxRows);
}

std::vector<SpawnPackets::TrackPoint>
SpawnPackets::loadStartGrid(const std::string& themeFolder, const std::string& trackFolder) {
    const std::string path = trackBase(themeFolder, trackFolder) + "/start.ini";
    std::vector<TrackPoint> rows = loadIniPoints(path, MAX_START_ROWS);
    if (rows.empty()) {
        // sub 4875C0 returns 0 on a zero row start so the whole track load dies
        LOG_ERROR("RACE", "start grid empty for " + themeFolder + "/" + trackFolder);
    }
    return rows;
}

std::vector<SpawnPackets::TrackPoint>
SpawnPackets::loadFollowPath(const std::string& themeFolder, const std::string& trackFolder,
                             int pathIndex) {
    if (pathIndex < 1 || pathIndex > MAX_FOLLOW_PATHS) {
        LOG_WARN("RACE", "follow path index " + std::to_string(pathIndex) + " out of 1 to 4");
        return {};
    }
    char name[32];
    std::snprintf(name, sizeof(name), "/follow_%02d.ini", pathIndex);
    return loadIniPoints(trackBase(themeFolder, trackFolder) + name, MAX_FOLLOW_NODES);
}

std::vector<SpawnPackets::TrackPoint>
SpawnPackets::loadWarpPoints(const std::string& themeFolder, const std::string& trackFolder) {
    return loadIniPoints(trackBase(themeFolder, trackFolder) + "/warp.ini", MAX_WARP_ROWS);
}

bool SpawnPackets::loadColCheckpoints(const std::string& path, ColCheckpoints& out, bool audit) {
    out = ColCheckpoints{};

    std::string blob;
    if (!readWholeFile(path, blob)) {
        LOG_WARN("RACE", "col not found " + path);
        return false;
    }
    if (blob.size() < 16) {
        LOG_ERROR("RACE", "col too small " + path);
        return false;
    }

    const uint8_t* p = reinterpret_cast<const uint8_t*>(blob.data());
    const uint64_t a = readU32LE(p + 0);
    const uint64_t b = readU32LE(p + 4);
    const uint64_t c = readU32LE(p + 8);
    const uint64_t d = readU32LE(p + 12);

    const uint64_t expected = 16ull + 12ull * a + 56ull * b + 20ull * c + 12ull * d;
    if (expected != static_cast<uint64_t>(blob.size())) {
        // holds byte exact on every shipped col so a mismatch means a different format
        LOG_ERROR("RACE", "col " + path + " size " + std::to_string(blob.size()) +
                          " does not match header " + std::to_string(expected));
        return false;
    }

    out.checkpointCount = static_cast<int32_t>(a);
    out.faceCount = static_cast<int32_t>(b);
    out.edgeCount = static_cast<int32_t>(c);
    out.vertexCount = static_cast<int32_t>(d);

    out.points.reserve(static_cast<size_t>(a));
    for (uint64_t i = 0; i < a; ++i) {
        const uint8_t* q = p + 16 + 12 * i;
        TrackVec3 v;
        v.x = readF32LE(q + 0);
        v.y = readF32LE(q + 4);
        v.z = readF32LE(q + 8);
        out.points.push_back(v);
    }

    const uint8_t* faces = p + 16 + 12 * a;
    const uint8_t* edges = faces + 56 * b;
    std::set<int> checkNumbers;
    for (uint64_t i = 0; i < b; ++i) {
        const uint8_t* face = faces + 56 * i;
        char name[35];
        std::memcpy(name, face + 22, 34);
        name[34] = '\0';
        for (int k = 0; k < 34; ++k) {
            name[k] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[k])));
        }
        if (std::strstr(name, "START") != nullptr) {
            out.hasStartFace = true;
            continue;
        }
        int index = 0;
        if (std::sscanf(name, "CHECK_%03d", &index) == 1) {
            checkNumbers.insert(index);
            if (index > out.maxCheckNumber) out.maxCheckNumber = index;
            continue;
        }
        // world cell boost pad index 0x486CF0 strstr BOOST then sscanf BOOST %03d the client tests three edges
        if (std::strstr(name, "BOOST_") != nullptr && std::sscanf(name, "BOOST_%03d", &index) == 1) {
            ColPadCell cell;
            cell.row = index;
            bool ok = true;
            for (int e = 0; e < 3; ++e) {
                const uint16_t raw = static_cast<uint16_t>(face[16 + 2 * e]
                                   | (static_cast<uint16_t>(face[17 + 2 * e]) << 8));
                const uint64_t idx = raw & 0x7FFFu;
                if (idx >= c) { ok = false; break; }
                const uint8_t* edge = edges + 20 * idx;
                // the top bit of the index flips the half plane world bsp set piece 0x4EBCF0
                const float sign = (raw & 0x8000u) ? -1.0f : 1.0f;
                cell.a[e] = sign * readF32LE(edge + 4);
                cell.b[e] = sign * readF32LE(edge + 8);
                cell.c[e] = sign * readF32LE(edge + 12);
            }
            if (ok) out.pads.push_back(cell);
            else LOG_WARN("RACE", "col " + path + " pad " + std::string(name) + " edge index out of range");
        }
    }

    out.distinctCheckNames = static_cast<int32_t>(checkNumbers.size());
    if (out.checkpointCount == 0 || !audit) {
        // every room floor col is like this and it is fine there are no laps the overlays skip the audit
        return true;
    }
    if (out.distinctCheckNames + 1 != out.checkpointCount) {
        LOG_WARN("RACE", "col " + path + " has " + std::to_string(out.distinctCheckNames) +
                         " check names but count " + std::to_string(out.checkpointCount));
    }
    if (out.maxCheckNumber > out.distinctCheckNames) {
        // Race 02 and Toy 04 both number past their own distinct count
        LOG_WARN("RACE", "col " + path + " check numbering has a gap max " +
                         std::to_string(out.maxCheckNumber));
    }
    if (!out.hasStartFace) {
        LOG_WARN("RACE", "col " + path + " has no START face so lap zero never closes");
    }
    return true;
}

bool SpawnPackets::loadTrackCheckpoints(const std::string& themeFolder,
                                        const std::string& trackFolder,
                                        ColCheckpoints& out) {
    // overlay track1 to track8 never change the count sub 485A10 pins the main col
    const std::string base = trackBase(themeFolder, trackFolder);
    if (!loadColCheckpoints(base + "/track.COL", out)) return false;
    // the overlay pieces carry pad cells of their own world load track pieces 0x485580 reads them all
    for (int i = 1; i <= 8; ++i) {
        const std::string overlay = base + "/track" + std::to_string(i) + ".COL";
        if (!std::ifstream(overlay, std::ios::binary)) break;
        ColCheckpoints piece;
        if (!loadColCheckpoints(overlay, piece, false)) break;
        out.pads.insert(out.pads.end(), piece.pads.begin(), piece.pads.end());
    }
    if (!out.pads.empty()) loadBoostPadKinds(base + "/boost.ini", out.pads);
    return true;
}

bool SpawnPackets::loadBoostPadKinds(const std::string& path, std::vector<ColPadCell>& pads) {
    std::string text;
    if (!readWholeFile(path, text)) {
        LOG_WARN("RACE", "boost ini not found " + path + " pad kinds unknown");
        return false;
    }
    // gimmick load boost 0x48AB10 one line is kind then four floats at most 100 lines
    std::vector<int32_t> kinds;
    std::istringstream in(text);
    std::string line;
    while (kinds.size() < 100 && std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        char* end = nullptr;
        const long kind = std::strtol(line.c_str(), &end, 10);
        if (end == line.c_str()) continue;
        kinds.push_back(static_cast<int32_t>(kind));
    }
    for (ColPadCell& cell : pads) {
        const int32_t line = cell.row - 1;
        if (line >= 0 && static_cast<size_t>(line) < kinds.size()) {
            cell.kind = kinds[static_cast<size_t>(line)];
        } else {
            LOG_WARN("RACE", "boost ini " + path + " has no line for pad " + std::to_string(cell.row));
        }
    }
    return true;
}

bool SpawnPackets::padCellContains(const ColPadCell& cell, float x, float y, float tol) {
    // world wheel query surface 0x4A0680 negates both axes the edge normals are unit so tol is in units
    const float cx = -x;
    const float cy = -y;
    for (int e = 0; e < 3; ++e) {
        if (cx * cell.a[e] + cy * cell.b[e] + cell.c[e] < -tol) return false;
    }
    return true;
}

bool SpawnPackets::predictRescuePoint(const std::vector<TrackPoint>& followNodes,
                                      float fromX, float fromY, float fromZ,
                                      RespawnReport& out) {
    out = RespawnReport{};
    if (followNodes.empty()) return false;

    size_t best = 0;
    float bestDist = -1.0f;
    for (size_t i = 0; i < followNodes.size(); ++i) {
        const TrackPoint& n = followNodes[i];
        const float dx = n.x - fromX;
        const float dy = n.y - fromY;
        const float dz = n.z - fromZ;
        const float dist = dx * dx + dy * dy + dz * dz;
        if (bestDist < 0.0f || dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }

    const TrackPoint& node = followNodes[best];
    out.x = node.x;
    out.y = node.y;
    out.z = node.z + RESCUE_LIFT_Z;
    out.yawDegrees = node.yawDegrees;
    return true;
}

bool SpawnPackets::trackCatalog(int32_t trackId, TrackCatalogRow& out) {
    const std::string sql = std::string("SELECT ") + kTrackCatalogColumns +
                            " FROM track_catalog WHERE track_id = ?";
    auto rows = Database::instance().queryPrepared(sql, { trackId });
    if (rows.empty()) {
        LOG_WARN("RACE", "no track_catalog row for track " + std::to_string(trackId));
        return false;
    }
    out = trackRowFrom(rows.front());
    return true;
}

std::vector<SpawnPackets::TrackCatalogRow> SpawnPackets::trackCatalogAll() {
    const std::string sql = std::string("SELECT ") + kTrackCatalogColumns +
                            " FROM track_catalog ORDER BY track_id";
    auto rows = Database::instance().queryPrepared(sql, {});

    std::vector<TrackCatalogRow> out;
    out.reserve(rows.size());
    for (const auto& row : rows) {
        if (out.size() >= MAX_TRACK_ROWS) {
            LOG_ERROR("RACE", "track_catalog past the 128 row client table rest dropped");
            break;
        }
        out.push_back(trackRowFrom(row));
    }
    if (out.empty()) LOG_WARN("RACE", "track_catalog is empty so no track can load");
    return out;
}

std::vector<SpawnPackets::ThemeCatalogRow> SpawnPackets::themeCatalogAll() {
    auto rows = Database::instance().queryPrepared(
        "SELECT theme_id, theme_folder, display_name, record_field_0 "
        "FROM theme_catalog ORDER BY theme_id",
        {});

    std::vector<ThemeCatalogRow> out;
    out.reserve(rows.size());
    for (const auto& row : rows) {
        if (out.size() >= MAX_THEME_ROWS) {
            LOG_ERROR("RACE", "theme_catalog past the 16 row client table rest dropped");
            break;
        }
        ThemeCatalogRow r;
        r.recordField0 = static_cast<uint32_t>(rowInt(row, "record_field_0", 0));
        r.themeId      = static_cast<uint32_t>(rowInt(row, "theme_id", 0));
        r.themeFolder  = rowStr(row, "theme_folder");
        r.displayName  = rowStr(row, "display_name");
        out.push_back(r);
    }
    if (out.empty()) LOG_WARN("RACE", "theme_catalog is empty so no track can load");
    return out;
}

} // namespace knc
