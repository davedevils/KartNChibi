#include "packets/gen/MissionPackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <cstdlib>
#include <map>
#include <string>

namespace knc {

namespace {

// local opcodes because the Protocol names for these values are from an older wrong pass
constexpr uint8_t OP_S_MISSION_DEF        = 0x87;
constexpr uint8_t OP_S_MISSION_PROGRESS   = 0x88;
constexpr uint8_t OP_S_MISSION_UNLOCK     = 0x8A;
constexpr uint8_t OP_S_MISSION_COMPLETE   = 0x8C;
constexpr uint8_t OP_S_MISSION_MENU_ACK   = 0x8F;
constexpr uint8_t OP_S_MISSION_START_ACK  = 0x90;
constexpr uint8_t OP_S_LICENSE_SCREEN_ACK = 0x16;
constexpr uint8_t OP_S_LICENSE_PROGRESS   = 0xA2;
constexpr uint8_t OP_S_LICENSE_RESULT     = 0xA3;
constexpr uint8_t OP_S_LICENSE_GRADE      = 0xA4;
constexpr uint8_t OP_S_LICENSE_TEST_DEF   = 0xC5;

// opcodes above 0xFF need fromCmdFull else the byte ctor truncates
constexpr uint16_t OP_S_MISSION_CHECKPOINT_PATH = 0x0120;
constexpr uint16_t OP_S_MISSION_GO              = 0x0122;

using DbRow = std::map<std::string, std::string>;

// 0x87 strings are fixed slots not cstrs so pad and keep the last byte NUL
void writeAsciiSlot(Packet& pkt, const std::string& s, size_t slot) {
    const size_t maxChars = slot - 1;
    size_t i = 0;
    for (; i < s.size() && i < maxChars; ++i) {
        pkt.writeUInt8(static_cast<uint8_t>(s[i]));
    }
    for (; i < slot; ++i) {
        pkt.writeUInt8(0);
    }
}

// sub 44EB30 has no cap so cut the string here or the client stack smashes
std::string clampAscii(const std::string& s, size_t bufSize) {
    size_t nulProcessedLen = 0;
    return clampAsciiCore(s, bufSize - 1, AsciiNulMode::kIgnoreNul, nulProcessedLen);
}

uint32_t readU32LE(const std::vector<uint8_t>& b, size_t off) {
    return static_cast<uint32_t>(b[off]) |
           (static_cast<uint32_t>(b[off + 1]) << 8) |
           (static_cast<uint32_t>(b[off + 2]) << 16) |
           (static_cast<uint32_t>(b[off + 3]) << 24);
}

bool needBytes(const Packet& pkt, size_t want, const char* what) {
    if (pkt.payload().size() >= want) return true;
    LOG_WARN("PACKET", std::string(what) + " short payload " +
                       std::to_string(pkt.payload().size()) + " want " + std::to_string(want));
    return false;
}

void warnIfNotEmpty(const Packet& pkt, const char* what) {
    if (!pkt.payload().empty()) {
        LOG_WARN("PACKET", std::string(what) + " expected zero payload got " +
                           std::to_string(pkt.payload().size()));
    }
}

uint32_t rowU32(const DbRow& row, const char* key) {
    return static_cast<uint32_t>(rowUInt64NoThrow(row, key, 0));
}

int32_t rowI32(const DbRow& row, const char* key) {
    return static_cast<int32_t>(rowInt64NoThrow(row, key, 0));
}

std::string rowStr(const DbRow& row, const char* key) {
    return rowStrCore(row, key);
}

void putU32(std::vector<uint8_t>& b, size_t off, uint32_t v) {
    b[off + 0] = static_cast<uint8_t>(v & 0xFF);
    b[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    b[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    b[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

} // namespace

size_t MissionPackets::rewardBlobSize(uint32_t type) {
    switch (type) {
        case 0: return 0x2C;  // type 0 character inventory type 1 kart inventory 0x38
        case 1: return 0x38;
        case 2: return 0x1C;
        case 3: return 0x1C;
        case 4: return 0x1C;
        case 5: return 0x30;
        case 6: return 0x84;
        case 7: return 0x08;  // the pendant instance then key
        default: return 0;
    }
}

bool MissionPackets::validateRewardBlob(const RewardBlob& blob) {
    if (blob.type > 7) {
        LOG_ERROR("PACKET", "reward type out of range " + std::to_string(blob.type));
        return false;
    }

    if (blob.type == 6) {
        // 0x84 record then a flag byte then 0x34 more when the flag is one
        const size_t base = 0x85;
        const size_t extra = 0x34;
        if (blob.bytes.size() < base) {
            LOG_ERROR("PACKET", "reward type 6 size " + std::to_string(blob.bytes.size()) +
                                " expected at least " + std::to_string(base));
            return false;
        }
        const uint8_t flag = blob.bytes[0x84];
        const size_t want = (flag == 1) ? (base + extra) : base;
        if (blob.bytes.size() != want) {
            LOG_ERROR("PACKET", "reward type 6 size " + std::to_string(blob.bytes.size()) +
                                " expected " + std::to_string(want));
            return false;
        }
        return true;
    }

    const size_t want = rewardBlobSize(blob.type);
    if (blob.bytes.size() != want) {
        LOG_ERROR("PACKET", "reward type " + std::to_string(blob.type) + " size " +
                            std::to_string(blob.bytes.size()) + " expected " +
                            std::to_string(want));
        return false;
    }
    return true;
}

MissionPackets::RewardBlob MissionPackets::characterReward(int32_t instanceId, int32_t baseKey) {
    // same bytes as PacketBuilder characterRecord kept local so this file stands alone
    RewardBlob r;
    r.type = 0;
    r.bytes.assign(0x2C, 0);
    putU32(r.bytes, 0x00, static_cast<uint32_t>(instanceId));
    putU32(r.bytes, 0x04, static_cast<uint32_t>(baseKey));
    return r;   // rest is equipped accessories zero means none
}

MissionPackets::RewardBlob MissionPackets::kartReward(int32_t instanceId, int32_t baseKey,
                                                      int32_t paintKey, int32_t plateKey,
                                                      int32_t kartItemKey, int32_t periodMode,
                                                      int32_t periodValue) {
    // same 0x38 owned kart record InventoryPackets kartBlob writes durability is 0x30 not 0x08
    RewardBlob r;
    r.type = 1;
    r.bytes.assign(0x38, 0);
    putU32(r.bytes, 0x00, static_cast<uint32_t>(instanceId));
    putU32(r.bytes, 0x04, static_cast<uint32_t>(baseKey));
    // a zero paint or plate key makes sub 4510C0 return null and the kart never builds
    putU32(r.bytes, 0x08, static_cast<uint32_t>(paintKey));
    putU32(r.bytes, 0x0C, static_cast<uint32_t>(plateKey));
    putU32(r.bytes, 0x10, static_cast<uint32_t>(kartItemKey));
    // 0x14 to 0x24 are model slots skipped price key at 0x28 zero means not bought
    putU32(r.bytes, 0x28, 0u);
    putU32(r.bytes, 0x2C, static_cast<uint32_t>(periodMode));
    putU32(r.bytes, 0x30, static_cast<uint32_t>(periodValue));
    putU32(r.bytes, 0x34, 0u);  // active flag a reward lands unequipped
    return r;
}

MissionPackets::RewardBlob MissionPackets::pendantReward(uint32_t pendantKey) {
    // one character owns a key once so the key is the unique instance sub 451250 erases by
    RewardBlob r;
    r.type = 7;
    r.bytes.assign(0x08, 0);
    putU32(r.bytes, 0x00, pendantKey);
    putU32(r.bytes, 0x04, pendantKey);
    return r;
}

bool MissionPackets::missionPlayable(const std::vector<MissionProgressEntry>& rows, uint32_t missionId) {
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].missionId != missionId) continue;
        if (rows[i].cleared == 1 || i == 0) return true;
        return rows[i - 1].cleared == 1;
    }
    // sub 450A40 lists only the progress rows so a mission with no row can never be picked
    return false;
}

std::vector<MissionPackets::MissionProgressEntry>
MissionPackets::chainProgress(const std::vector<MissionProgressEntry>& allRows) {
    std::vector<MissionProgressEntry> out;
    for (const auto& row : allRows) {
        out.push_back(row);
        if (row.cleared != 1) break;
        if (out.size() >= MISSION_PROGRESS_MAX) break;
    }
    return out;
}

std::vector<MissionPackets::MissionProgressEntry>
MissionPackets::rowsOpenedByClear(const std::vector<MissionProgressEntry>& allRows, uint32_t clearedId) {
    const auto before = chainProgress(allRows);
    std::vector<MissionProgressEntry> after = allRows;
    for (auto& row : after)
        if (row.missionId == clearedId) row.cleared = 1;
    std::vector<MissionProgressEntry> opened;
    for (const auto& row : chainProgress(after)) {
        bool known = false;
        for (const auto& b : before) known = known || b.missionId == row.missionId;
        if (!known) opened.push_back(row);
    }
    return opened;
}

Packet MissionPackets::missionDefinition(const MissionDefWire& def) {
    Packet pkt(OP_S_MISSION_DEF);

    pkt.writeUInt32(0);                     // offset 0x00 never read
    pkt.writeUInt32(def.missionId);
    pkt.writeUInt32(def.missionKind);
    pkt.writeUInt32(0);                     // offset 0x0C never read
    pkt.writeInt32(def.goalCount);
    pkt.writeInt32(def.timeLimitMs);
    pkt.writeUInt32(def.rewardExtra);
    pkt.writeUInt32(def.rewardMileage);
    pkt.writeUInt32(def.rewardExp);
    pkt.writeUInt32(0);                     // offset 0x24 never read
    pkt.writeUInt32(def.rewardItemType);
    pkt.writeUInt32(def.rewardItemKey);
    pkt.writeUInt32(0);                     // offset 0x30 and 0x34 never read
    pkt.writeUInt32(0);

    writeAsciiSlot(pkt, def.worldName,   MISSION_STR_SLOT);
    writeAsciiSlot(pkt, def.strKeyTitle, MISSION_STR_SLOT);
    writeAsciiSlot(pkt, def.strKeySub,   MISSION_STR_SLOT);
    writeAsciiSlot(pkt, def.strKeyDesc,  MISSION_STR_SLOT);

    if (def.rewardItemType > 7) {
        LOG_ERROR("PACKET", "missionDefinition id " + std::to_string(def.missionId) +
                            " reward type out of range " + std::to_string(def.rewardItemType));
    }

    // one byte off shifts every later field in the clients own table
    if (pkt.payload().size() != MISSION_DEF_SIZE) {
        LOG_ERROR("PACKET", "missionDefinition size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(MISSION_DEF_SIZE));
    }
    return pkt;
}

Packet MissionPackets::missionProgressList(const std::vector<MissionProgressEntry>& rows) {
    size_t count = rows.size();
    if (count > MISSION_PROGRESS_MAX) {
        LOG_WARN("PACKET", "missionProgressList " + std::to_string(count) +
                           " rows clamped to " + std::to_string(MISSION_PROGRESS_MAX));
        count = MISSION_PROGRESS_MAX;
    }

    Packet pkt(OP_S_MISSION_PROGRESS);
    pkt.writeInt32(static_cast<int32_t>(count));
    for (size_t i = 0; i < count; ++i) {
        pkt.writeUInt32(rows[i].missionId);
        pkt.writeUInt32(rows[i].cleared);
    }

    const size_t expected = 4 + 8 * count;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "missionProgressList size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet MissionPackets::missionUnlocked(uint32_t missionId, uint32_t cleared) {
    Packet pkt(OP_S_MISSION_UNLOCK);
    pkt.writeUInt32(missionId);
    pkt.writeUInt32(cleared);

    if (pkt.payload().size() != 8) {
        LOG_ERROR("PACKET", "missionUnlocked size " + std::to_string(pkt.payload().size()) +
                            " expected 8");
    }
    return pkt;
}

Packet MissionPackets::missionMenuAck() {
    // send every 0x87 and 0x88 first else stage 24 builds an empty menu
    return Packet(OP_S_MISSION_MENU_ACK);
}

Packet MissionPackets::missionStartAck(uint32_t missionId, uint32_t goldAfter) {
    Packet pkt(OP_S_MISSION_START_ACK);
    pkt.writeUInt32(missionId);
    pkt.writeUInt32(goldAfter);

    if (pkt.payload().size() != 8) {
        LOG_ERROR("PACKET", "missionStartAck size " + std::to_string(pkt.payload().size()) +
                            " expected 8");
    }
    return pkt;
}

Packet MissionPackets::missionCheckpointPath(const std::vector<SpawnPackets::TrackVec3>& points) {
    Packet pkt = Packet::fromCmdFull(OP_S_MISSION_CHECKPOINT_PATH);
    pkt.writeInt32(static_cast<int32_t>(points.size()));
    for (const auto& p : points) {
        pkt.writeFloat(p.x);
        pkt.writeFloat(p.y);
        pkt.writeFloat(p.z);
    }

    const size_t expected = 4 + 12 * points.size();
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "missionCheckpointPath size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet MissionPackets::missionGo() {
    // FUN 0047eb70 makes zero read calls so any payload here is ignored
    return Packet::fromCmdFull(OP_S_MISSION_GO);
}

Packet MissionPackets::missionComplete(uint32_t missionId, uint32_t goldAfter, uint32_t expAfter) {
    Packet pkt(OP_S_MISSION_COMPLETE);

    pkt.writeUInt32(0);          // has reward must be zero or the client reads a tail
    pkt.writeUInt32(missionId);
    pkt.writeUInt32(0);          // missionId above is low dword of a u64 key high dword never read
    pkt.writeUInt32(goldAfter);
    pkt.writeUInt32(expAfter);

    if (pkt.payload().size() != MISSION_COMPLETE_BASE) {
        LOG_ERROR("PACKET", "missionComplete size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(MISSION_COMPLETE_BASE));
    }
    return pkt;
}

Packet MissionPackets::missionRefused(uint32_t missionId) {
    Packet pkt(OP_S_MISSION_COMPLETE);
    pkt.writeUInt32(2);
    pkt.writeUInt32(missionId);
    pkt.writeUInt32(0);
    return pkt;
}

Packet MissionPackets::missionCompleteWithReward(uint32_t missionId, uint32_t goldAfter,
                                                 uint32_t expAfter, const RewardBlob& reward) {
    // tail size comes from the definition not the wire so a mismatch desyncs the frame
    if (!validateRewardBlob(reward)) {
        LOG_ERROR("PACKET", "missionCompleteWithReward mission " + std::to_string(missionId) +
                            " bad blob falling back to no reward");
        return missionComplete(missionId, goldAfter, expAfter);
    }

    Packet pkt(OP_S_MISSION_COMPLETE);
    pkt.writeUInt32(1);
    pkt.writeUInt32(missionId);
    pkt.writeUInt32(0);
    pkt.writeUInt32(goldAfter);
    pkt.writeUInt32(expAfter);
    pkt.writeBytes(reward.bytes.data(), reward.bytes.size());

    const size_t expected = MISSION_COMPLETE_BASE + reward.bytes.size();
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "missionCompleteWithReward size " +
                            std::to_string(pkt.payload().size()) + " expected " +
                            std::to_string(expected));
    }
    return pkt;
}

Packet MissionPackets::licenseScreenAck() {
    // send every 0xC5 and the single 0xA2 first else stage 14 builds an empty screen
    return Packet(OP_S_LICENSE_SCREEN_ACK);
}

Packet MissionPackets::licenseProgressList(const std::vector<LicenseProgressEntry>& rows) {
    size_t count = rows.size();
    if (count > LICENSE_PROGRESS_MAX) {
        LOG_WARN("PACKET", "licenseProgressList " + std::to_string(count) +
                           " rows clamped to " + std::to_string(LICENSE_PROGRESS_MAX));
        count = LICENSE_PROGRESS_MAX;
    }

    Packet pkt(OP_S_LICENSE_PROGRESS);
    pkt.writeInt32(static_cast<int32_t>(count));
    for (size_t i = 0; i < count; ++i) {
        pkt.writeUInt32(rows[i].licenseKey);
        pkt.writeUInt32(rows[i].passed);
        pkt.writeUInt32(0);   // third dword no reader in this build
    }

    const size_t expected = 4 + 12 * count;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "licenseProgressList size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet MissionPackets::licenseTestDefinition(const LicenseTestDefWire& def) {
    // buffers on the reading side are 36 33 and 35 bytes so clamp to buffer size minus the terminator
    const std::string name = clampAscii(def.name, 35);
    const std::string keyA = clampAscii(def.strKeyA, 32);
    const std::string keyB = clampAscii(def.strKeyB, 34);

    Packet pkt(OP_S_LICENSE_TEST_DEF);
    pkt.writeUInt32(0);                  // struct offset 0x00 never read
    pkt.writeUInt32(def.licenseKey);
    pkt.writeString(name);
    for (size_t i = 0; i < def.params.size(); ++i) {
        pkt.writeUInt32(def.params[i]);
    }
    pkt.writeString(keyA);
    pkt.writeString(keyB);

    const size_t expected = 4 + 4 + (name.size() + 1) + 52 + (keyA.size() + 1) + (keyB.size() + 1);
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "licenseTestDefinition size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet MissionPackets::licenseTestResult(const LicenseResultWire& result) {
    // tails are gated on exactly one so anything else makes the client skip the read
    const bool wantItem = result.hasItem && validateRewardBlob(result.item);

    Packet pkt(OP_S_LICENSE_RESULT);
    pkt.writeUInt32(result.hasCurrency ? 1u : 0u);
    pkt.writeUInt32(wantItem ? 1u : 0u);
    pkt.writeUInt32(result.licenseKey);
    pkt.writeUInt32(result.passed);
    pkt.writeUInt32(0);                  // third dword of the entry stored and never read

    if (result.hasCurrency) {
        pkt.writeUInt32(result.currencyKey);
        pkt.writeUInt32(result.currencyCount);
    }
    if (wantItem) {
        pkt.writeUInt32(result.item.type);
        pkt.writeBytes(result.item.bytes.data(), result.item.bytes.size());
    }

    size_t expected = LICENSE_RESULT_BASE;
    if (result.hasCurrency) expected += 8;
    if (wantItem) expected += 4 + result.item.bytes.size();
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "licenseTestResult size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    if (result.hasItem && !wantItem) {
        LOG_ERROR("PACKET", "licenseTestResult key " + std::to_string(result.licenseKey) +
                            " item dropped bad blob");
    }
    return pkt;
}

Packet MissionPackets::licenseGradeUp(uint8_t grade) {
    Packet pkt(OP_S_LICENSE_GRADE);
    pkt.writeUInt8(grade);

    if (pkt.payload().size() != 1) {
        LOG_ERROR("PACKET", "licenseGradeUp size " + std::to_string(pkt.payload().size()) +
                            " expected 1");
    }
    return pkt;
}

bool MissionPackets::parseMissionComplete(const Packet& pkt, MissionIdReq& out) {
    if (!needBytes(pkt, 4, "C2S 0x8C missionComplete")) return false;
    out.missionId = readU32LE(pkt.payload(), 0);
    return true;
}

bool MissionPackets::parseMissionStart(const Packet& pkt, MissionIdReq& out) {
    if (!needBytes(pkt, 4, "C2S 0x90 missionStart")) return false;
    out.missionId = readU32LE(pkt.payload(), 0);
    return true;
}

bool MissionPackets::parseMissionMenuOpen(const Packet& pkt) {
    warnIfNotEmpty(pkt, "C2S 0x8F missionMenuOpen");
    return true;
}

bool MissionPackets::parseMissionListRequest(const Packet& pkt) {
    warnIfNotEmpty(pkt, "C2S 0x8E missionListRequest");
    return true;
}

bool MissionPackets::parseFullStateRequest(const Packet& pkt) {
    warnIfNotEmpty(pkt, "C2S 0xFA fullStateRequest");
    return true;
}

bool MissionPackets::parseLicenseScreenOpen(const Packet& pkt) {
    warnIfNotEmpty(pkt, "C2S 0x16 licenseScreenOpen");
    return true;
}

bool MissionPackets::parseLicenseTestSubmit(const Packet& pkt, LicenseSubmitReq& out) {
    if (!needBytes(pkt, 12, "C2S 0xA3 licenseTestSubmit")) return false;
    const auto& b = pkt.payload();
    out.licenseKey    = readU32LE(b, 0);
    out.echoTestdef2C = readU32LE(b, 4);
    out.echoTestdef34 = readU32LE(b, 8);
    return true;
}

bool MissionPackets::parseLicensePanelClose(const Packet& pkt, PanelCloseReq& out) {
    if (!needBytes(pkt, 4, "C2S 0x8D licensePanelClose")) return false;
    out.popupContext = readU32LE(pkt.payload(), 0);
    return true;
}

bool MissionPackets::parseStageRequest(const Packet& pkt, StageReq& out) {
    if (!needBytes(pkt, 8, "C2S 0x18 stageRequest")) return false;
    const auto& b = pkt.payload();
    out.targetStage = readU32LE(b, 0);
    out.arg         = readU32LE(b, 4);
    return true;
}

std::vector<MissionPackets::MissionDefWire> MissionPackets::loadMissionDefs() {
    std::vector<MissionDefWire> defs;

    auto rows = Database::instance().queryPrepared(
        "SELECT mission_id, mission_kind, goal_count, time_limit_ms, reward_extra, "
        "reward_mileage, reward_exp, reward_item_type, reward_item_key, "
        "world_name, str_key_title, str_key_sub, str_key_desc "
        "FROM mission_def ORDER BY mission_id LIMIT 20",
        {});

    defs.reserve(rows.size());
    for (const auto& row : rows) {
        MissionDefWire d;
        d.missionId       = rowU32(row, "mission_id");
        d.missionKind     = rowU32(row, "mission_kind");
        d.goalCount       = rowI32(row, "goal_count");
        d.timeLimitMs     = rowI32(row, "time_limit_ms");
        d.rewardExtra     = rowU32(row, "reward_extra");
        d.rewardMileage   = rowU32(row, "reward_mileage");
        d.rewardExp       = rowU32(row, "reward_exp");
        d.rewardItemType  = rowU32(row, "reward_item_type");
        d.rewardItemKey   = rowU32(row, "reward_item_key");
        d.worldName       = rowStr(row, "world_name");
        d.strKeyTitle     = rowStr(row, "str_key_title");
        d.strKeySub       = rowStr(row, "str_key_sub");
        d.strKeyDesc      = rowStr(row, "str_key_desc");
        defs.push_back(std::move(d));
    }

    LOG_INFO("MISSION", "loaded " + std::to_string(defs.size()) + " mission defs");
    return defs;
}

std::vector<MissionPackets::MissionProgressEntry>
MissionPackets::loadMissionProgress(int32_t characterId) {
    std::vector<MissionProgressEntry> out;

    // every mission the player can play needs a row or the menu lists nothing for a fresh character
    auto rows = Database::instance().queryPrepared(
        "SELECT d.mission_id, COALESCE(p.cleared, 0) AS cleared FROM mission_def d "
        "LEFT JOIN char_mission_progress p ON p.mission_id = d.mission_id AND p.char_id = ? "
        "ORDER BY d.mission_id LIMIT 20",
        {std::to_string(characterId)});

    out.reserve(rows.size());
    for (const auto& row : rows) {
        MissionProgressEntry e;
        e.missionId = rowU32(row, "mission_id");
        e.cleared   = rowU32(row, "cleared");
        out.push_back(e);
    }
    return out;
}

std::vector<MissionPackets::LicenseTestDefWire> MissionPackets::loadLicenseTestDefs() {
    std::vector<LicenseTestDefWire> defs;

    auto rows = Database::instance().queryPrepared(
        "SELECT license_key, name, param_00, param_01, param_02, param_03, param_04, "
        "param_05, param_06, param_07, param_08, param_09, param_10, param_11, param_12, "
        "str_key_a, str_key_b FROM license_test_def ORDER BY license_key LIMIT 16",
        {});

    static const char* kParamCols[13] = {
        "param_00", "param_01", "param_02", "param_03", "param_04", "param_05", "param_06",
        "param_07", "param_08", "param_09", "param_10", "param_11", "param_12"
    };

    defs.reserve(rows.size());
    for (const auto& row : rows) {
        LicenseTestDefWire d;
        d.licenseKey = rowU32(row, "license_key");
        d.name       = rowStr(row, "name");
        for (size_t i = 0; i < d.params.size(); ++i) {
            d.params[i] = rowU32(row, kParamCols[i]);
        }
        d.strKeyA = rowStr(row, "str_key_a");
        d.strKeyB = rowStr(row, "str_key_b");
        defs.push_back(std::move(d));
    }

    LOG_INFO("LICENSE", "loaded " + std::to_string(defs.size()) + " license test defs");
    return defs;
}

std::vector<MissionPackets::LicenseProgressEntry>
MissionPackets::loadLicenseProgress(int32_t characterId) {
    std::vector<LicenseProgressEntry> out;

    auto rows = Database::instance().queryPrepared(
        "SELECT license_key, passed FROM char_license_progress "
        "WHERE char_id = ? ORDER BY license_key LIMIT 64",
        {std::to_string(characterId)});

    out.reserve(rows.size());
    for (const auto& row : rows) {
        LicenseProgressEntry e;
        e.licenseKey = rowU32(row, "license_key");
        e.passed     = rowU32(row, "passed");
        out.push_back(e);
    }
    return out;
}

} // namespace knc
