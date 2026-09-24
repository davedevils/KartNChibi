#include "packets/gen/ScenarioPackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>
#include <string>

namespace knc {

namespace {

uint32_t readU32LE(const std::vector<uint8_t>& b, size_t off) {
    return static_cast<uint32_t>(b[off]) |
           (static_cast<uint32_t>(b[off + 1]) << 8) |
           (static_cast<uint32_t>(b[off + 2]) << 16) |
           (static_cast<uint32_t>(b[off + 3]) << 24);
}

uint32_t rowU32(const DbRow& row, const char* key) {
    return static_cast<uint32_t>(rowUInt64NoThrow(row, key, 0));
}

// 33 byte ascii slot NUL padded the client reads it through the def quest index table
void writeAsciiSlot33(Packet& pkt, const std::string& s) {
    std::string v = s.substr(0, ScenarioPackets::KEY_SLOT_SIZE - 1);
    v.resize(ScenarioPackets::KEY_SLOT_SIZE, '\0');
    pkt.writeBytes(reinterpret_cast<const uint8_t*>(v.data()), v.size());
}

void checkSize(const Packet& pkt, size_t expected, const char* what) {
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", std::string(what) + " size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
}

std::vector<ScenarioPackets::ScenarioProgressRow> sortedDesc(
        std::vector<ScenarioPackets::ScenarioProgressRow> rows) {
    // sub 452C40 keeps the list sorted by key highest first
    std::stable_sort(rows.begin(), rows.end(),
                     [](const ScenarioPackets::ScenarioProgressRow& a,
                        const ScenarioPackets::ScenarioProgressRow& b) { return a.key > b.key; });
    return rows;
}

} // namespace

Packet ScenarioPackets::scenarioDefinition(const ScenarioDefWire& def) {
    Packet pkt = Packet::fromCmdFull(OP_S_SCENARIO_DEF);

    pkt.writeUInt32(0);                    // offset 0x00 pad offset 0x04 scenario key lookup sub 452E30
    pkt.writeUInt32(def.scenarioKey);
    pkt.writeUInt32(0);                    // offset 0x08 pad offset 0x0C track id for 0xC3 catalogue
    pkt.writeUInt32(def.trackId);
    pkt.writeUInt32(def.characterDefKey);  // offset 0x10 rival driver key for 0xBF catalogue offset 0x14 rival kart key for 0xC0 catalogue
    pkt.writeUInt32(def.kartDefKey);
    pkt.writeUInt32(0);                    // offset 0x18 pad lands in 0xC70874 offset 0x1C entry fee MSG QUEST PAY
    pkt.writeUInt32(def.entryFee);
    pkt.writeUInt32(def.rewardMileage);    // offset 0x20 reward mileage UNIT MILEAGE offset 0x24 reward exp UNIT EXP
    pkt.writeUInt32(def.rewardExp);
    pkt.writeUInt32(wireRewardCategory(def.rewardCategory, def.rewardKey));  // offset 0x28 reward category offset 0x2C reward catalogue key
    pkt.writeUInt32(def.rewardKey);
    pkt.writeUInt32(0);                    // offset 0x30 and 0x34 no reader in this build
    pkt.writeUInt32(0);
    writeAsciiSlot33(pkt, def.titleKey);   // offset 0x38 title key offset 0x59 desc key
    writeAsciiSlot33(pkt, def.descKey);
    writeAsciiSlot33(pkt, def.msgKeyBase.substr(0, STORY_KEY_MAX));  // offset 0x7A story key offset 0x9B pad byte
    pkt.writeUInt8(0);

    checkSize(pkt, SCENARIO_DEF_SIZE, "scenarioDefinition");
    return pkt;
}

Packet ScenarioPackets::progressList(const std::vector<ScenarioProgressRow>& rows) {
    Packet pkt = Packet::fromCmdFull(OP_S_SCENARIO_PROGRESS_LIST);
    const size_t n = std::min(rows.size(), PROGRESS_ROW_MAX);
    if (rows.size() > PROGRESS_ROW_MAX) {
        LOG_WARN("SCENARIO", "progress list of " + std::to_string(rows.size()) +
                             " rows cut to the client cap " + std::to_string(PROGRESS_ROW_MAX));
    }
    pkt.writeInt32(static_cast<int32_t>(n));
    for (size_t i = 0; i < n; ++i) {
        pkt.writeUInt32(rows[i].key);
        pkt.writeUInt32(rows[i].cleared);
    }
    checkSize(pkt, 4 + 8 * n, "progressList");
    return pkt;
}

Packet ScenarioPackets::scenarioStart(const ScenarioStartWire& start) {
    Packet pkt = Packet::fromCmdFull(OP_S_SCENARIO_START);
    pkt.writeUInt32(start.scenarioKey);
    pkt.writeUInt32(start.goldCounter);
    checkSize(pkt, 8, "scenarioStart");
    return pkt;
}

Packet ScenarioPackets::scenarioResult(const ScenarioResultWire& result) {
    const bool wallet = result.kind == RESULT_PAID || result.kind == RESULT_PAID_ITEM;
    const bool withTail = result.kind == RESULT_PAID_ITEM;

    Packet pkt = Packet::fromCmdFull(OP_S_SCENARIO_RESULT);
    pkt.writeInt32(result.kind);
    pkt.writeUInt8(result.flag);
    pkt.writeUInt32(result.scenarioKey);
    pkt.writeUInt32(0);  // offset 0x09 high half of the key no reader
    if (wallet) {
        pkt.writeInt32(result.goldAfter);
        pkt.writeInt32(result.expAfter);
    }
    if (withTail && !result.tail.empty()) {
        pkt.writeBytes(result.tail.data(), result.tail.size());
    }

    size_t expected = wallet ? RESULT_PAID_SIZE : RESULT_BASE_SIZE;
    if (withTail) expected += result.tail.size();
    checkSize(pkt, expected, "scenarioResult");
    if (!withTail && !result.tail.empty()) {
        LOG_ERROR("PACKET", "scenarioResult kind " + std::to_string(result.kind) +
                            " dropped a tail only kind 3 reads one");
    }
    return pkt;
}

Packet ScenarioPackets::scenarioProgressAppend(const ScenarioProgressRow& row) {
    Packet pkt = Packet::fromCmdFull(OP_S_SCENARIO_PROGRESS_APPEND);
    pkt.writeUInt32(row.key);
    pkt.writeUInt32(row.cleared);
    checkSize(pkt, 8, "scenarioProgressAppend");
    return pkt;
}

Packet ScenarioPackets::menuOpenAck() {
    Packet pkt = Packet::fromCmdFull(OP_S_SCENARIO_MENU);
    checkSize(pkt, 0, "menuOpenAck");
    return pkt;
}

size_t ScenarioPackets::rewardTailSize(uint32_t category, bool carcraftHasSlot) {
    switch (category) {
        case 0: return 0x2C;   // category 0 driver row category 1 kart row
        case 1: return 0x38;
        case 2: return 0x1C;   // category 2 item row category 3 part row
        case 3: return 0x1C;
        case 4: return 0x1C;   // category 4 pet row category 5 room object row
        case 5: return 0x30;
        case 6: return 0x84 + 1 + (carcraftHasSlot ? 0x34 : 0);  // category 6 car craft part then slot flag category 7 pendant instance then key
        case 7: return 0x08;
        default: return 0;
    }
}

uint32_t ScenarioPackets::wireRewardCategory(uint32_t category, uint32_t key) {
    // sub 438720 derefs the catalogue row of the key with no null test so no key means no icon
    if (key == 0 || category >= REWARD_NONE) return REWARD_NONE;
    return category;
}

std::vector<uint8_t> ScenarioPackets::pendantRewardTail(uint32_t pendantKey) {
    // one character owns a key once so the key is its own instance as on 0x011A
    std::vector<uint8_t> out(8, 0);
    for (int i = 0; i < 4; ++i) {
        out[static_cast<size_t>(i)] = static_cast<uint8_t>((pendantKey >> (i * 8)) & 0xFF);
        out[static_cast<size_t>(4 + i)] = static_cast<uint8_t>((pendantKey >> (i * 8)) & 0xFF);
    }
    return out;
}

std::vector<ScenarioPackets::ScenarioProgressRow> ScenarioPackets::visibleRows(
        const std::vector<ScenarioDefWire>& defs, const std::vector<uint32_t>& clearedKeys,
        uint32_t level) {
    const std::set<uint32_t> cleared(clearedKeys.begin(), clearedKeys.end());
    std::vector<ScenarioProgressRow> rows;
    for (const auto& d : defs) {
        const bool done = cleared.count(d.scenarioKey) != 0;
        // a new quest opens at every level and a cleared one never hides again
        if (d.requiredLevel > level && !done) continue;
        rows.push_back({d.scenarioKey, done ? 1u : 0u});
    }
    rows = sortedDesc(std::move(rows));
    if (rows.size() > PROGRESS_ROW_MAX) rows.resize(PROGRESS_ROW_MAX);
    return rows;
}

bool ScenarioPackets::rowPlayable(const std::vector<ScenarioProgressRow>& rowsIn, uint32_t key) {
    const std::vector<ScenarioProgressRow> rows = sortedDesc(rowsIn);
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].key != key) continue;
        // the lowest row always starts any other needs the row under it cleared
        if (i + 1 < rows.size()) return rows[i + 1].cleared != 0;
        return true;
    }
    return false;
}

std::vector<ScenarioPackets::ScenarioProgressRow> ScenarioPackets::newRows(
        const std::vector<ScenarioProgressRow>& before, const std::vector<ScenarioProgressRow>& after) {
    std::set<uint32_t> had;
    for (const auto& r : before) had.insert(r.key);
    std::vector<ScenarioProgressRow> out;
    for (const auto& r : after) {
        if (!had.count(r.key)) out.push_back(r);
    }
    return out;
}

std::string ScenarioPackets::defProblem(const ScenarioDefWire& def, bool driverKnown, bool kartKnown,
                                        bool trackKnown, bool rewardKnown,
                                        const std::string& rivalName) {
    if (def.scenarioKey == 0) return "scenario key 0";
    // sub 4B5EB0 derefs the rival driver row with no null test
    if (!driverKnown) return "rival driver " + std::to_string(def.characterDefKey) + " not in the 0xBF catalogue";
    // stage 17 init fails on a kart the 0xC0 catalogue does not hold
    if (!kartKnown) return "rival kart " + std::to_string(def.kartDefKey) + " not in the 0xC0 catalogue";
    // sub 42D690 pops Track initialize fail on an unknown track
    if (!trackKnown) return "track " + std::to_string(def.trackId) + " not in the 0xC3 catalogue";
    // sub 47DD10 swprintf of the name overruns a 14 wchar stack buffer
    if (rivalName.empty() || rivalName.size() > RIVAL_NAME_MAX)
        return "rival name '" + rivalName + "' longer than " + std::to_string(RIVAL_NAME_MAX);
    if (def.titleKey.empty() || def.titleKey.size() >= KEY_SLOT_SIZE) return "title key empty or too long";
    if (def.descKey.size() >= KEY_SLOT_SIZE) return "desc key too long";
    if (def.msgKeyBase.empty() || def.msgKeyBase.size() > STORY_KEY_MAX) return "story key empty or too long";
    if (wireRewardCategory(def.rewardCategory, def.rewardKey) != REWARD_NONE && !rewardKnown)
        return "reward category " + std::to_string(def.rewardCategory) + " key " +
               std::to_string(def.rewardKey) + " unknown";
    return std::string();
}

ScenarioPackets::ResultDecision ScenarioPackets::decideResult(bool runMatches, bool alreadyCleared,
                                                              uint32_t raceTimeMs, uint32_t elapsedMs,
                                                              int32_t rivalTimeMs, uint32_t goalTimeMs,
                                                              bool hasItemReward) {
    ResultDecision d;
    if (!runMatches) return d;
    if (raceTimeMs < MIN_RACE_TIME_MS) return d;
    if (static_cast<uint64_t>(raceTimeMs) > static_cast<uint64_t>(elapsedMs) + CLOCK_SLACK_MS) return d;
    if (rivalTimeMs > 0 && raceTimeMs > static_cast<uint32_t>(rivalTimeMs)) return d;
    if (goalTimeMs > 0 && raceTimeMs > goalTimeMs) return d;

    d.success = true;
    d.flag = 1;
    if (alreadyCleared) {
        d.kind = RESULT_CLEARED;
        return d;
    }
    d.firstClear = true;
    d.kind = hasItemReward ? RESULT_PAID_ITEM : RESULT_PAID;
    return d;
}

bool ScenarioPackets::parseStageSelect(const Packet& pkt, StageSelectReq& out) {
    const auto& b = pkt.payload();
    if (b.size() < 4) return false;
    out.key = readU32LE(b, 0);
    return true;
}

bool ScenarioPackets::parseResultReport(const Packet& pkt, ResultReportReq& out) {
    const auto& b = pkt.payload();
    if (b.size() < 4) return false;
    out.scenarioKey = readU32LE(b, 0);
    out.resultValue = (b.size() >= 8) ? readU32LE(b, 4) : 0;
    return true;
}

std::vector<ScenarioPackets::ScenarioDefWire> ScenarioPackets::loadScenarioDefs() {
    std::vector<ScenarioDefWire> defs;

    // the joins prove every key the client derefs sits in a catalogue the login burst ships
    auto rows = Database::instance().queryPrepared(
        "SELECT d.scenario_key, d.track_id, d.character_def_key, d.kart_def_key, d.entry_fee, "
        "d.gold_reward, d.exp_reward, d.reward_category, d.reward_key, d.title_key, d.info_key, "
        "d.story_key, d.required_level, d.goal_time_ms, "
        "COALESCE(NULLIF(dr.display_name, ''), dr.name, '') AS rival_name, "
        "dr.id AS driver_known, v.id AS kart_known, t.track_id AS track_known, "
        "CASE d.reward_category "
        "  WHEN 0 THEN (SELECT COUNT(*) FROM drivers x WHERE x.id = d.reward_key AND COALESCE(x.is_enabled, 1) = 1) "
        "  WHEN 1 THEN (SELECT COUNT(*) FROM vehicle_templates x WHERE x.id = d.reward_key AND COALESCE(x.is_enabled, 1) = 1) "
        "  WHEN 7 THEN (SELECT COUNT(*) FROM pendant_def x WHERE x.pendant_key = d.reward_key) "
        "  ELSE 0 END AS reward_known "
        "FROM scenario_def d "
        "LEFT JOIN drivers dr ON dr.id = d.character_def_key AND COALESCE(dr.is_enabled, 1) = 1 "
        "LEFT JOIN vehicle_templates v ON v.id = d.kart_def_key AND COALESCE(v.is_enabled, 1) = 1 "
        "LEFT JOIN track_catalog t ON t.track_id = d.track_id "
        "WHERE COALESCE(d.is_enabled, 1) = 1 "
        "ORDER BY d.scenario_key LIMIT 50",
        {});

    defs.reserve(rows.size());
    for (const auto& row : rows) {
        ScenarioDefWire d;
        d.scenarioKey     = rowU32(row, "scenario_key");
        d.trackId         = rowU32(row, "track_id");
        d.characterDefKey = rowU32(row, "character_def_key");
        d.kartDefKey      = rowU32(row, "kart_def_key");
        d.entryFee        = rowU32(row, "entry_fee");
        d.rewardMileage   = rowU32(row, "gold_reward");
        d.rewardExp       = rowU32(row, "exp_reward");
        d.rewardCategory  = rowU32(row, "reward_category");
        d.rewardKey       = rowU32(row, "reward_key");
        d.titleKey        = rowStrCore(row, "title_key");
        d.descKey         = rowStrCore(row, "info_key");
        d.msgKeyBase      = rowStrCore(row, "story_key");
        d.requiredLevel   = rowU32(row, "required_level");
        d.goalTimeMs      = rowU32(row, "goal_time_ms");

        // only the driver the kart and the pendant rewards are proven safe for the detail draw
        const bool supported = d.rewardCategory == REWARD_DRIVER || d.rewardCategory == REWARD_KART ||
                               d.rewardCategory == REWARD_PENDANT;
        if (wireRewardCategory(d.rewardCategory, d.rewardKey) != REWARD_NONE && !supported) {
            LOG_WARN("SCENARIO", "quest " + std::to_string(d.scenarioKey) + " reward category " +
                                 std::to_string(d.rewardCategory) + " is not granted by this server, shown as none");
            d.rewardCategory = REWARD_NONE;
            d.rewardKey = 0;
        }

        const std::string problem = defProblem(
            d, !rowStrCore(row, "driver_known").empty(), !rowStrCore(row, "kart_known").empty(),
            !rowStrCore(row, "track_known").empty(), rowU32(row, "reward_known") != 0,
            rowStrCore(row, "rival_name"));
        if (!problem.empty()) {
            LOG_ERROR("SCENARIO", "quest " + std::to_string(d.scenarioKey) + " left out: " + problem);
            continue;
        }
        defs.push_back(d);
    }

    if (defs.size() > SCENARIO_DEF_MAX) defs.resize(SCENARIO_DEF_MAX);
    LOG_DEBUG("SCENARIO", "loaded " + std::to_string(defs.size()) + " quest defs");
    return defs;
}

} // namespace knc
