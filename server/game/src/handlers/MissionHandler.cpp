/// the stock mission flow 0x8F open 0x90 start 0x8C finish the older missions table tracker is gone

#include "handlers/MissionHandler.h"
#include "GameServer.h"
#include "packets/PacketBuilder.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "handlers/RaceHandler.h"
#include "packets/gen/ProgressionPackets.h"
#include "packets/gen/SpawnPackets.h"
#include "packets/gen/ShopPackets.h"
#include "handlers/ProgressionHandler.h"
#include "handlers/ShopHandler.h"

namespace knc {

void MissionHandler::handleOpenMissionMenu(Session::Ptr session, GameServer* server) {
    if (!session || session->characterId == 0) return;
    const int32_t charId = static_cast<int32_t>(session->characterId);

    // the Escape box of stage 25 sends 0x8F too so a run left half way is dropped here
    Database::instance().executePrepared("DELETE FROM char_mission_state WHERE char_id = ?", {charId});

    // C2S 0x8F comes from sub 483980 the Mission button in sub 42BCE0 only its S2C ack opens stage 24
    std::vector<Packet> burst;
    // 0x87 is a blind append so a second open doubles the rows FUN 0043bad0 loads images for
    size_t defCount = 0;
    if (!session->missionDefsSent) {
        const auto defs = MissionPackets::loadMissionDefs();
        for (const auto& d : defs) burst.push_back(MissionPackets::missionDefinition(d));
        defCount = defs.size();
        session->missionDefsSent = true;
    }

    const auto progress = MissionPackets::chainProgress(MissionPackets::loadMissionProgress(charId));
    burst.push_back(MissionPackets::missionProgressList(progress));
    burst.push_back(MissionPackets::missionMenuAck());

    if (server) server->sendDripped(session, std::move(burst));
    else for (auto& p : burst) session->send(p);

    LOG_INFO("MISSION", "mission menu char " + std::to_string(charId) + " defs " +
             std::to_string(defCount) + " progress rows " + std::to_string(progress.size()));
}

void MissionHandler::handleStartMission(Session::Ptr session,
                                        const MissionPackets::MissionIdReq& req,
                                        GameServer* server) {
    (void)server;
    if (!session || session->characterId == 0) return;

    const int32_t charId = static_cast<int32_t>(session->characterId);
    const int32_t missionId = static_cast<int32_t>(req.missionId);
    auto& db = Database::instance();

    auto def = db.queryPrepared(
        "SELECT time_limit_ms, world_name, reward_extra FROM mission_def WHERE mission_id = ? LIMIT 1",
        {missionId});
    if (def.empty()) {
        LOG_WARN("MISSION", "start mission " + std::to_string(missionId) +
                 " has no definition, char " + std::to_string(charId));
        // the box replaces the MSG WAIT of the MISSION ENTER confirm
        session->send(ShopPackets::rejectAscii("MSG_UNKNOWN_ERROR", 1));
        return;
    }

    // sub 43B9A0 draws Start only on a playable row so a locked id is a forged start
    const auto progress = MissionPackets::chainProgress(MissionPackets::loadMissionProgress(charId));
    if (!MissionPackets::missionPlayable(progress, req.missionId)) {
        LOG_WARN("MISSION", "start mission " + std::to_string(missionId) + " is locked for char " +
                 std::to_string(charId));
        session->send(ShopPackets::rejectAscii("MSG_UNKNOWN_ERROR", 1));
        return;
    }
    bool cleared = false;
    for (const auto& row : progress)
        if (row.missionId == req.missionId) cleared = row.cleared == 1;

    int64_t limit = 0;
    {
        const std::string& v = def[0].at("time_limit_ms");
        if (!v.empty()) limit = std::stoll(v);
    }
    // a zero limit would make the mission expire on the same tick it starts
    if (limit <= 0) limit = 120000;

    // def 0x18 is the entry fee the menu shows it until the row is cleared and it is never refunded
    const std::string& feeText = def[0].at("reward_extra");
    const uint32_t fee = MissionPackets::entryFeeFor(
        feeText.empty() ? 0u : static_cast<uint32_t>(std::stoul(feeText)), cleared);
    int32_t gold = 0;
    if (fee > 0) {
        ProgressionPackets::StatBlock stats;
        if (!ProgressionPackets::spendGold(session->characterId, static_cast<int32_t>(fee), stats)) {
            LOG_INFO("MISSION", "start mission " + std::to_string(missionId) + " char " +
                     std::to_string(charId) + " cannot pay the fee " + std::to_string(fee));
            session->send(ShopPackets::rejectAscii("MSG_NO_MONEY", 1));
            return;
        }
        gold = stats.gold;
    } else {
        auto st = db.queryPrepared("SELECT gold FROM characters WHERE id = ? LIMIT 1", {charId});
        if (!st.empty() && !st[0].at("gold").empty()) gold = std::stoi(st[0].at("gold"));
    }

    const int64_t now = static_cast<int64_t>(RaceHandler::nowMs());

    // one row per character so a restart replaces the old one a counter of 0 is not answered yet
    db.executePrepared(
        "REPLACE INTO char_mission_state (char_id, mission_id, started_at_ms, deadline_ms, counter) "
        "VALUES (?, ?, ?, ?, 0)",
        {charId, missionId, static_cast<int64_t>(now), static_cast<int64_t>(now + limit)});

    // sub 47DF30 writes the gold opens stage 25 and closes MSG WAIT
    session->send(MissionPackets::missionStartAck(static_cast<uint32_t>(missionId),
                                                  static_cast<uint32_t>(gold)));
    // no 0x0120 or 0x0122 here both belong to the rally world of stage 11 and 0x0120 overruns 12 slots

    LOG_INFO("MISSION", "start mission " + std::to_string(missionId) + " char " +
             std::to_string(charId) + " fee " + std::to_string(fee) + " deadline " +
             std::to_string(now + limit));
}

void MissionHandler::handleMissionComplete(Session::Ptr session,
                                           const MissionPackets::MissionIdReq& req,
                                           GameServer* server) {
    if (!session || session->characterId == 0) return;

    const int32_t charId = static_cast<int32_t>(session->characterId);
    const int32_t missionId = static_cast<int32_t>(req.missionId);
    auto& db = Database::instance();

    // the client can claim any id so the started row is the only proof
    auto state = db.queryPrepared(
        "SELECT mission_id, deadline_ms, counter FROM char_mission_state WHERE char_id = ? LIMIT 1",
        {charId});
    const bool sameRun = !state.empty() && std::stoi(state[0].at("mission_id")) == missionId;
    if (sameRun && state[0].at("counter") == "-1") {
        // state 3000 of 0x43AB00 resends 0x8C every frame until the answer lands so repeats are dropped
        return;
    }
    if (!sameRun) {
        LOG_WARN("MISSION", "complete mission " + std::to_string(missionId) +
                 " was never started by char " + std::to_string(charId));
        // has reward 2 leaves state 3000 and marks nothing cleared
        session->send(MissionPackets::missionRefused(static_cast<uint32_t>(missionId)));
        return;
    }
    // the client arms its deadline at the GO after an open ended board so a late claim is only logged
    {
        const int64_t deadline = std::stoll(state[0].at("deadline_ms"));
        const int64_t now = static_cast<int64_t>(RaceHandler::nowMs());
        if (now > deadline + 60000)
            LOG_INFO("MISSION", "complete mission " + std::to_string(missionId) + " char " +
                     std::to_string(charId) + " came " + std::to_string((now - deadline) / 1000) +
                     " s after the server deadline");
    }
    db.executePrepared("UPDATE char_mission_state SET counter = -1 WHERE char_id = ?", {charId});

    auto def = db.queryPrepared(
        "SELECT reward_mileage, reward_exp, reward_item_type, reward_item_key FROM mission_def "
        "WHERE mission_id = ? LIMIT 1",
        {missionId});
    int32_t rewardGold = 0;
    int32_t rewardExp = 0;
    uint32_t itemType = 0;
    uint32_t itemKey = 0;
    if (!def.empty()) {
        auto num = [&](const char* c) -> int64_t {
            const std::string& v = def[0].at(c);
            return v.empty() ? 0 : std::stoll(v);
        };
        rewardGold = static_cast<int32_t>(num("reward_mileage"));
        rewardExp = static_cast<int32_t>(num("reward_exp"));
        itemType = static_cast<uint32_t>(num("reward_item_type"));
        itemKey = static_cast<uint32_t>(num("reward_item_key"));
    }

    // pay once a second claim finds cleared already set and pays nothing the board shows 0 then
    auto prev = db.queryPrepared(
        "SELECT cleared FROM char_mission_progress WHERE char_id = ? AND mission_id = ? LIMIT 1",
        {charId, missionId});
    const bool firstClear = prev.empty() || prev[0].at("cleared") == "0";
    if (!firstClear) {
        rewardGold = 0;
        rewardExp = 0;
    }

    // def 0x20 carries the UNIT EXP label so it pays as exp def 0x1C mileage pays as gold
    auto res = ProgressionPackets::award(session->characterId, rewardExp, rewardGold, 0);
    if (!res.ok) {
        LOG_ERROR("MISSION", "reward failed for mission " + std::to_string(missionId) +
                  " char " + std::to_string(charId));
    }

    const auto rowsBefore = MissionPackets::loadMissionProgress(charId);
    db.executePrepared(
        "REPLACE INTO char_mission_progress (char_id, mission_id, cleared) VALUES (?, ?, 1)",
        {charId, missionId});

    // sub 47B9E0 reads the blob by the def type and puts the item in the owned list it names
    MissionPackets::RewardBlob blob;
    bool withItem = false;
    if (firstClear && itemKey > 0) {
        if (itemType == 7) {
            db.executePrepared("INSERT IGNORE INTO owned_pendant (character_id, pendant_key) VALUES (?, ?)",
                               {charId, static_cast<int32_t>(itemKey)});
            blob = MissionPackets::pendantReward(itemKey);
            withItem = true;
        } else if (itemType <= 6) {
            std::vector<uint8_t> record;
            if (ShopHandler::grantReward(session->characterId, itemType, itemKey, record)) {
                blob.type = itemType;
                blob.bytes = std::move(record);
                withItem = MissionPackets::validateRewardBlob(blob);
            }
        }
        if (!withItem)
            LOG_WARN("MISSION", "mission " + std::to_string(missionId) + " reward type " +
                     std::to_string(itemType) + " key " + std::to_string(itemKey) + " was not granted");
    }

    const uint32_t goldAfter = static_cast<uint32_t>(res.stats.gold);
    const uint32_t expAfter = static_cast<uint32_t>(res.stats.expCurrent);
    session->send(withItem ? MissionPackets::missionCompleteWithReward(static_cast<uint32_t>(missionId),
                                                                       goldAfter, expAfter, blob)
                           : MissionPackets::missionComplete(static_cast<uint32_t>(missionId),
                                                             goldAfter, expAfter));
    // a chassis reward gets its slot and basic set after the board
    if (withItem && itemType == ShopPackets::CAT_KART && server) server->carCraft().pushFactoryLoadout(session);
    // 0x8C writes gold and exp only the level and the bar come with 0x000A then the level banner
    if (res.ok) {
        ProgressionHandler::pushStats(session, res.stats);
        ProgressionHandler::pushLevelUp(session, res);
    }
    // the fifth mission of chapter 1 earns pendant 13 live on 0x011B after the 0x8C board
    if (firstClear) ProgressionHandler::pushEarnedPendants(session);
    // the 0x88 list ends at the first row not cleared so the rows this clear opens are appended
    if (firstClear) {
        for (const auto& row : MissionPackets::rowsOpenedByClear(rowsBefore, req.missionId))
            session->send(MissionPackets::missionUnlocked(row.missionId, row.cleared));
    }

    LOG_INFO("MISSION", "complete mission " + std::to_string(missionId) + " char " +
             std::to_string(charId) + " gold " + std::to_string(rewardGold) +
             " exp " + std::to_string(rewardExp) +
             (withItem ? " item type " + std::to_string(itemType) + " key " + std::to_string(itemKey)
                       : std::string()) +
             (firstClear ? " first clear" : " repeat no pay"));
}

} // namespace knc
