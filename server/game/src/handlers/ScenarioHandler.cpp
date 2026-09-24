/// quest mode of the stock client stage 22 ScenarioMenu and stage 17 Quest game see docs packets systems scenario md

#include "handlers/ScenarioHandler.h"
#include "GameServer.h"
#include "packets/PacketBuilder.h"
#include "packets/gen/ScenarioPackets.h"
#include "packets/gen/GhostPackets.h"
#include "packets/gen/ShopPackets.h"
#include "handlers/ProgressionHandler.h"
#include "handlers/GhostHandler.h"
#include "handlers/ShopHandler.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <unordered_map>

namespace knc {

namespace {

using Clock = std::chrono::steady_clock;

/// one quest started by S2C 0xF5 and not yet reported
struct QuestRun {
    uint32_t key = 0;
    int32_t rivalTimeMs = 0;
    Clock::time_point startedAt;
};

/// level up and pendant catch up of a first clear wait for the menu off stage 17
struct QuestAfterglow {
    bool leveled = false;
    ProgressionPackets::AwardResult award;
};

std::mutex g_questMutex;
std::unordered_map<uint32_t, QuestRun> g_runs;
std::unordered_map<uint32_t, QuestAfterglow> g_afterglow;

uint32_t characterLevel(uint32_t characterId) {
    auto rows = Database::instance().queryPrepared(
        "SELECT level FROM characters WHERE id = ? LIMIT 1", {characterId});
    if (rows.empty()) return 1;
    const int64_t lvl = rowInt64NoThrow(rows[0], "level", 1);
    return lvl < 1 ? 1u : static_cast<uint32_t>(lvl);
}

std::vector<uint32_t> clearedKeys(uint32_t characterId) {
    std::vector<uint32_t> out;
    for (const auto& r : Database::instance().queryPrepared(
             "SELECT scenario_key FROM scenario_key_progress WHERE character_id = ? AND completed = 1",
             {characterId})) {
        out.push_back(static_cast<uint32_t>(rowUInt64NoThrow(r, "scenario_key", 0)));
    }
    return out;
}

std::vector<ScenarioPackets::ScenarioProgressRow> rowsAtLevel(
        uint32_t characterId, const std::vector<ScenarioPackets::ScenarioDefWire>& defs, uint32_t level) {
    return ScenarioPackets::visibleRows(defs, clearedKeys(characterId), level);
}

const ScenarioPackets::ScenarioDefWire* findDef(const std::vector<ScenarioPackets::ScenarioDefWire>& defs,
                                                uint32_t key) {
    for (const auto& d : defs)
        if (d.scenarioKey == key) return &d;
    return nullptr;
}

bool isCleared(const std::vector<ScenarioPackets::ScenarioProgressRow>& rows, uint32_t key) {
    for (const auto& r : rows)
        if (r.key == key) return r.cleared != 0;
    return false;
}

/// the time of the replay GhostHandler sendQuestGhost picked zero when none ran
int32_t rivalTimeFor(uint32_t key) {
    auto& db = Database::instance();
    auto rows = db.queryPrepared(
        "SELECT track_id, char_id FROM ghost_quest_replay "
        "WHERE quest_index = ? AND COALESCE(is_enabled, 1) = 1 LIMIT 1", {key});
    if (rows.empty()) return 0;
    const int32_t trackId = static_cast<int32_t>(rowInt64NoThrow(rows[0], "track_id", 0));
    const uint32_t charId = static_cast<uint32_t>(rowUInt64NoThrow(rows[0], "char_id", 0));
    if (charId != 0) {
        auto t = db.queryPrepared(
            "SELECT time_ms FROM ghost_record WHERE track_id = ? AND char_id = ? LIMIT 1",
            {trackId, charId});
        return t.empty() ? 0 : static_cast<int32_t>(rowInt64NoThrow(t[0], "time_ms", 0));
    }
    GhostRecord best;
    return GhostPackets::trackBest(trackId, best) ? best.timeMs : 0;
}

/// S2C 0x0001 turns the MSG WAIT box into an OK box so the menu stays usable
void refuse(const Session::Ptr& session, uint32_t key, const char* msgKey, const std::string& why) {
    session->send(ShopPackets::rejectAscii(msgKey, 1));
    LOG_WARN("SCENARIO", "quest " + std::to_string(key) + " refused for char " +
                         std::to_string(session->characterId) + ": " + why);
}

/// the item of a first clear granted and shaped as the 0x00F8 kind 3 tail empty when none
std::vector<uint8_t> grantItem(uint32_t characterId, const ScenarioPackets::ScenarioDefWire& def) {
    const uint32_t cat = ScenarioPackets::wireRewardCategory(def.rewardCategory, def.rewardKey);
    if (cat == ScenarioPackets::REWARD_NONE) return {};

    if (cat == ScenarioPackets::REWARD_PENDANT) {
        // inserted before pushEarnedPendants runs so the catch up finds it owned and sends no 0x011B
        Database::instance().executePrepared(
            "INSERT IGNORE INTO owned_pendant (character_id, pendant_key) VALUES (?, ?)",
            {characterId, def.rewardKey});
        return ScenarioPackets::pendantRewardTail(def.rewardKey);
    }

    std::vector<uint8_t> record;
    if (!ShopHandler::grantReward(characterId, cat, def.rewardKey, record)) return {};
    if (record.size() != ScenarioPackets::rewardTailSize(cat)) {
        LOG_ERROR("SCENARIO", "quest " + std::to_string(def.scenarioKey) + " reward record " +
                              std::to_string(record.size()) + " bytes the client reads " +
                              std::to_string(ScenarioPackets::rewardTailSize(cat)));
        return {};
    }
    return record;
}

} // namespace

std::vector<Packet> ScenarioHandler::loginFrames(uint32_t characterId, bool withDefs) {
    std::vector<Packet> out;
    const auto defs = ScenarioPackets::loadScenarioDefs();
    // 0xBE wipes the def container so the defs ride every catalogue pass and only it
    if (withDefs) {
        for (const auto& d : defs) out.push_back(ScenarioPackets::scenarioDefinition(d));
    }
    // sub 437FB0 reads the rows as the menu opens so they must already sit in the client
    if (characterId != 0) {
        out.push_back(ScenarioPackets::progressList(rowsAtLevel(characterId, defs, characterLevel(characterId))));
    }
    return out;
}

void ScenarioHandler::handleMenuOpen(Session::Ptr session, GameServer* server) {
    (void)server;
    const uint32_t charId = session->characterId;
    if (charId == 0) {
        // the ack alone clears MSG WAIT and the menu lists nothing
        session->send(ScenarioPackets::menuOpenAck());
        return;
    }

    const auto defs = ScenarioPackets::loadScenarioDefs();
    const auto rows = rowsAtLevel(charId, defs, characterLevel(charId));

    QuestAfterglow after;
    bool hadAfterglow = false;
    {
        std::lock_guard<std::mutex> lock(g_questMutex);
        // the menu only opens after a result or a quit so an open run was abandoned
        g_runs.erase(charId);
        auto it = g_afterglow.find(charId);
        if (it != g_afterglow.end()) {
            after = it->second;
            hadAfterglow = true;
            g_afterglow.erase(it);
        }
    }

    // full replace first sub 47E8E0 pushes stage 22 whose init reads the rows at once
    session->send(ScenarioPackets::progressList(rows));
    session->send(ScenarioPackets::menuOpenAck());

    if (hadAfterglow) {
        if (after.leveled) ProgressionHandler::pushLevelUp(session, after.award);
        ProgressionHandler::pushEarnedPendants(session);
    }

    LOG_INFO("SCENARIO", "quest menu char " + std::to_string(charId) + " rows " +
                         std::to_string(rows.size()) + " of " + std::to_string(defs.size()));
}

void ScenarioHandler::handleScenarioStageSelect(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;

    ScenarioPackets::StageSelectReq req;
    if (!ScenarioPackets::parseStageSelect(packet, req)) {
        refuse(session, 0, "MSG_UNKNOWN_ERROR", "short C2S 0xF5 payload");
        return;
    }
    const uint32_t charId = session->characterId;
    if (charId == 0) {
        refuse(session, req.key, "MSG_UNKNOWN_ERROR", "no character");
        return;
    }

    // S2C 0xF5 derefs the def of the key with no null test so only a loaded def may start
    const auto defs = ScenarioPackets::loadScenarioDefs();
    const ScenarioPackets::ScenarioDefWire* def = findDef(defs, req.key);
    if (!def) {
        refuse(session, req.key, "MSG_UNKNOWN_ERROR", "no safe scenario def");
        return;
    }
    const auto rows = rowsAtLevel(charId, defs, characterLevel(charId));
    if (!ScenarioPackets::rowPlayable(rows, req.key)) {
        refuse(session, req.key, "MSG_UNKNOWN_ERROR", "row locked or not listed");
        return;
    }

    // the fee is drawn only while the row is not cleared so only then is it charged
    const bool cleared = isCleared(rows, req.key);
    ProgressionPackets::StatBlock stats;
    if (!cleared && def->entryFee > 0) {
        if (!ProgressionPackets::spendGold(charId, static_cast<int32_t>(def->entryFee), stats)) {
            refuse(session, req.key, "MSG_NO_MONEY", "fee " + std::to_string(def->entryFee));
            return;
        }
    } else if (!ProgressionHandler::loadStats(charId, stats)) {
        refuse(session, req.key, "MSG_UNKNOWN_ERROR", "stats unreadable");
        return;
    }

    // 0xF6 and 0xF7 must land first stage 17 init spawns the rival only when car 1 holds frames
    GhostHandler::seedShippedReplaysOnce();
    const size_t frames = GhostHandler::sendQuestGhost(session, req.key);
    const int32_t rivalMs = frames > 0 ? rivalTimeFor(req.key) : 0;

    {
        std::lock_guard<std::mutex> lock(g_questMutex);
        g_runs[charId] = QuestRun{req.key, rivalMs, Clock::now()};
    }

    ScenarioPackets::ScenarioStartWire start;
    start.scenarioKey = req.key;
    start.goldCounter = static_cast<uint32_t>(stats.gold < 0 ? 0 : stats.gold);
    session->send(ScenarioPackets::scenarioStart(start));

    LOG_INFO("SCENARIO", "quest " + std::to_string(req.key) + " start char " + std::to_string(charId) +
                         " track " + std::to_string(def->trackId) + " fee " +
                         std::to_string(cleared ? 0u : def->entryFee) + " rival frames " +
                         std::to_string(frames) + " rival ms " + std::to_string(rivalMs));
}

void ScenarioHandler::handleScenarioResultReport(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;

    ScenarioPackets::ResultReportReq req;
    const bool parsed = ScenarioPackets::parseResultReport(packet, req);
    const uint32_t charId = session->characterId;

    // kind 0 reads no progress row so it is the safe answer to anything unexpected
    ScenarioPackets::ScenarioResultWire wire;
    wire.scenarioKey = req.scenarioKey;
    if (!parsed || charId == 0) {
        session->send(ScenarioPackets::scenarioResult(wire));
        LOG_WARN("SCENARIO", "quest result with no character or a short payload from " +
                             session->remoteAddress());
        return;
    }

    QuestRun run;
    bool hadRun = false;
    {
        std::lock_guard<std::mutex> lock(g_questMutex);
        auto it = g_runs.find(charId);
        if (it != g_runs.end()) {
            run = it->second;
            hadRun = true;
            g_runs.erase(it);
        }
    }

    auto& db = Database::instance();
    const auto defs = ScenarioPackets::loadScenarioDefs();
    const ScenarioPackets::ScenarioDefWire* def = findDef(defs, req.scenarioKey);
    const uint32_t levelBefore = characterLevel(charId);
    const auto rowsBefore = rowsAtLevel(charId, defs, levelBefore);

    const bool runMatches = hadRun && def && run.key == req.scenarioKey;
    const uint32_t elapsedMs = hadRun
        ? static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
              Clock::now() - run.startedAt).count())
        : 0;
    const bool hasItem = def &&
        ScenarioPackets::wireRewardCategory(def->rewardCategory, def->rewardKey) != ScenarioPackets::REWARD_NONE;
    ScenarioPackets::ResultDecision decision = ScenarioPackets::decideResult(
        runMatches, isCleared(rowsBefore, req.scenarioKey), req.resultValue, elapsedMs,
        run.rivalTimeMs, def ? def->goalTimeMs : 0, hasItem);

    if (def) {
        db.executePrepared(
            "INSERT INTO scenario_key_progress (character_id, scenario_key, completed, last_result, attempts) "
            "VALUES (?, ?, 0, ?, 1) "
            "ON DUPLICATE KEY UPDATE last_result = VALUES(last_result), attempts = attempts + 1",
            {charId, req.scenarioKey, req.resultValue});
        if (decision.success) {
            db.executePrepared(
                "UPDATE scenario_key_progress SET best_time_ms = ? "
                "WHERE character_id = ? AND scenario_key = ? AND (best_time_ms = 0 OR best_time_ms > ?)",
                {req.resultValue, charId, req.scenarioKey, req.resultValue});
        }
    }

    // the flag flips in one guarded update so two reports can never pay twice
    if (decision.firstClear) {
        bool claimed = false;
        auto tx = db.beginTransaction();
        if (tx.valid() &&
            tx.execute("UPDATE scenario_key_progress SET completed = 1, cleared_at = NOW() "
                       "WHERE character_id = ? AND scenario_key = ? AND completed = 0",
                       {charId, req.scenarioKey})) {
            claimed = tx.affectedRows() == 1;
            if (!tx.commit()) claimed = false;
        }
        if (!claimed) {
            decision.firstClear = false;
            decision.kind = ScenarioPackets::RESULT_CLEARED;
        }
    }

    wire.kind = decision.kind;
    wire.flag = decision.flag;

    ProgressionPackets::AwardResult award;
    if (decision.firstClear) {
        award = ProgressionHandler::applyAward(charId, static_cast<int32_t>(def->rewardExp),
                                               static_cast<int32_t>(def->rewardMileage), 0);
        if (!award.ok) {
            LOG_ERROR("SCENARIO", "quest " + std::to_string(req.scenarioKey) + " payout failed for char " +
                                  std::to_string(charId));
            ProgressionHandler::loadStats(charId, award.stats);
        }
        wire.goldAfter = award.stats.gold;
        wire.expAfter = award.stats.expCurrent;

        if (hasItem) {
            wire.tail = grantItem(charId, *def);
            if (wire.tail.empty()) {
                // no tail the client would read an item that never came so kind 2 pays the wallet only
                wire.kind = ScenarioPackets::RESULT_PAID;
                LOG_ERROR("SCENARIO", "quest " + std::to_string(req.scenarioKey) + " reward category " +
                                      std::to_string(def->rewardCategory) + " key " +
                                      std::to_string(def->rewardKey) + " was not granted");
            }
        }
    }

    session->send(ScenarioPackets::scenarioResult(wire));

    if (decision.firstClear) {
        // 0x00F8 writes gold and exp only the level and the bar ride 0x000A
        ProgressionHandler::pushStats(session, award.stats);
        // rows a level up opened go out now with the new badge the menu open resends them all
        const auto rowsAfter = rowsAtLevel(charId, defs, characterLevel(charId));
        for (const auto& r : ScenarioPackets::newRows(rowsBefore, rowsAfter))
            session->send(ScenarioPackets::scenarioProgressAppend(r));
        std::lock_guard<std::mutex> lock(g_questMutex);
        QuestAfterglow& after = g_afterglow[charId];
        if (award.leveledUp) {
            after.leveled = true;
            after.award = award;
        }
    }

    LOG_INFO("SCENARIO", "quest " + std::to_string(req.scenarioKey) + " result char " + std::to_string(charId) +
                         " time " + std::to_string(req.resultValue) + " ms elapsed " +
                         std::to_string(elapsedMs) + " rival " + std::to_string(run.rivalTimeMs) +
                         (runMatches ? "" : " no matching run") + " kind " + std::to_string(wire.kind) +
                         (decision.success ? " success" : " fail") +
                         (decision.firstClear ? " first clear paid" : ""));
}

} // namespace knc
