#include "packets/gen/ResultsPackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"
#include <map>
#include <string>

namespace knc {

namespace {

// per rank payout row loaded from reward rules
struct RewardRule {
    int32_t gold      = 0;
    int32_t xp        = 0;
    int32_t goldBonus = 0;
    int32_t xpBonus   = 0;
};

// invented ladder keeps a fresh db playable until reward rules is seeded
const int32_t FALLBACK_GOLD[8] = { 100, 80, 65, 50, 40, 30, 20, 10 };
const int32_t FALLBACK_XP[8]   = {  50, 40, 33, 25, 20, 15, 10,  5 };

int32_t toI32(const std::map<std::string, std::string>& row, const char* key, int32_t def) {
    return static_cast<int32_t>(rowInt64Throwing(row, key, def));
}

bool loadRules(int32_t gameMode, std::map<int32_t, RewardRule>& out) {
    auto rows = Database::instance().queryPrepared(
        "SELECT finish_rank, gold_reward, exp_reward, gold_bonus, exp_bonus "
        "FROM reward_rules WHERE game_mode = ? ORDER BY finish_rank",
        { gameMode });

    for (const auto& row : rows) {
        RewardRule rule;
        rule.gold      = toI32(row, "gold_reward", 0);
        rule.xp        = toI32(row, "exp_reward", 0);
        rule.goldBonus = toI32(row, "gold_bonus", 0);
        rule.xpBonus   = toI32(row, "exp_bonus", 0);
        out[toI32(row, "finish_rank", 0)] = rule;
    }
    return !out.empty();
}

// nearest rule at or below the rank else the invented ladder
RewardRule pickRule(const std::map<int32_t, RewardRule>& rules, int32_t rank) {
    if (!rules.empty()) {
        auto it = rules.upper_bound(rank);
        if (it != rules.begin()) {
            --it;
            return it->second;
        }
        return rules.begin()->second;
    }
    RewardRule rule;
    const int32_t idx = rank < 0 ? 0 : (rank < 8 ? rank : 7);
    rule.gold = FALLBACK_GOLD[idx];
    rule.xp   = FALLBACK_XP[idx];
    return rule;
}

uint32_t clampU32(int32_t v) {
    return v < 0 ? 0u : static_cast<uint32_t>(v);
}

} // namespace

Packet ResultsPackets::finishReward(uint32_t playerId, uint32_t goldAfter,
                                    uint8_t levelAfter, uint32_t expAfter,
                                    int32_t finishRank) {
    // unicast only these land in the recipient own wallet globals unguarded
    Packet pkt(CMD::S_RACE_END);

    pkt.writeUInt32(playerId);
    pkt.writeUInt32(goldAfter);
    pkt.writeUInt8(levelAfter);
    pkt.writeUInt32(expAfter);   // genuinely packed no pad after the byte
    pkt.writeInt32(finishRank);

    const size_t expected = 17;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "finishReward size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet ResultsPackets::rankBroadcast(uint32_t playerId, int32_t rank) {
    // protocol h name is stale the value is the rank broadcast
    Packet pkt(CMD::S_ROOM_STATE_3D);

    pkt.writeUInt32(playerId);
    pkt.writeInt32(rank);

    const size_t expected = 8;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "rankBroadcast size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet ResultsPackets::scoreboard(uint32_t myTeam, const std::vector<ScoreRow>& rows) {
    Packet pkt(CMD::S_LARGE_GAME_STATE);

    // thirty one rows walk the client car table out of bounds and corrupt it
    bool taken[MAX_ROWS] = { false };
    std::vector<ScoreRow> keep;
    keep.reserve(rows.size());

    for (const auto& r : rows) {
        if (r.rank < 0 || r.rank >= MAX_ROWS) {
            LOG_WARN("PACKET", "scoreboard drop rank " + std::to_string(r.rank) +
                               " outside board range");
            continue;
        }
        if (taken[r.rank]) {
            LOG_WARN("PACKET", "scoreboard drop duplicate rank " + std::to_string(r.rank));
            continue;
        }
        taken[r.rank] = true;
        keep.push_back(r);

        // long name spills into the next board row and can clear the loaded flag
        if (keep.back().displayName.size() > MAX_NAME_UNITS) {
            LOG_WARN("PACKET", "scoreboard name truncated for player " +
                               std::to_string(keep.back().playerId));
            keep.back().displayName.resize(MAX_NAME_UNITS);
        }
    }

    pkt.writeUInt32(myTeam);
    pkt.writeInt32(static_cast<int32_t>(keep.size()));

    for (const auto& r : keep) {
        pkt.writeInt32(r.rank);
        pkt.writeInt32(r.finishTimeMs);
        pkt.writeUInt32(r.playerId);
        pkt.writeWString(r.displayName);
        pkt.writeInt8(r.levelIndex);
        pkt.writeUInt32(r.team);
        pkt.writeUInt32(r.col2Base);
        pkt.writeUInt32(r.col1Base);
        pkt.writeUInt32(r.col2Bonus);
        pkt.writeUInt32(r.col1Bonus);
        pkt.writeInt8(r.pccafeIconIndex);
        pkt.writeUInt32(r.characterBaseKey);
        pkt.writeUInt32(0);              // no reader anywhere in the client
        pkt.writeUInt32(r.bonusItemKey);
    }

    size_t expected = 8;
    for (const auto& r : keep) {
        expected += 46 + 2 * (r.displayName.size() + 1);
    }
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "scoreboard size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet ResultsPackets::driverAnimState(uint32_t playerId, uint8_t animState) {
    Packet pkt(CMD::S_PLAYER_STATUS);

    pkt.writeUInt32(playerId);
    pkt.writeUInt8(animState);

    const size_t expected = 5;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "driverAnimState size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

bool ResultsPackets::parseAnimStateEcho(const Packet& pkt, uint8_t& outAnimState) {
    const std::vector<uint8_t>& body = pkt.payload();
    if (body.size() != 1) {
        LOG_WARN("PACKET", "animStateEcho size " + std::to_string(body.size()) + " expected one");
        return false;
    }

    outAnimState = body[0];

    if (outAnimState != ANIM_SPIN && outAnimState != ANIM_RECOVER &&
        outAnimState != ANIM_HIT && outAnimState != ANIM_STATE_8 &&
        outAnimState != ANIM_FINISH) {
        LOG_WARN("PACKET", "animStateEcho unobserved state " +
                           std::to_string(static_cast<int>(outAnimState)));
    }
    return true;
}

std::vector<ResultsPackets::Reward> ResultsPackets::computeRewards(
        const std::vector<Finisher>& order, int32_t gameMode) {
    std::map<int32_t, RewardRule> rules;
    if (!loadRules(gameMode, rules) && gameMode != 0) {
        loadRules(0, rules);
    }
    if (rules.empty()) {
        LOG_WARN("REWARD", "reward_rules empty for mode " + std::to_string(gameMode) +
                           " using built in ladder");
    }

    std::vector<Reward> out;
    out.reserve(order.size());

    for (const auto& f : order) {
        Reward r;
        r.playerId   = f.playerId;
        r.finishRank = f.finishRank;

        // retire earns nothing the leave race dock is applied separately
        if (f.finishRank < 0) {
            out.push_back(r);
            continue;
        }

        const RewardRule rule = pickRule(rules, f.finishRank);
        r.gold = rule.gold;
        r.xp   = rule.xp;

        // total up carries both halves
        if (f.bonusItemKey == BONUS_GOLD_UP || f.bonusItemKey == BONUS_TOTAL_UP) {
            r.goldBonus = rule.goldBonus;
        }
        if (f.bonusItemKey == BONUS_EXP_UP || f.bonusItemKey == BONUS_TOTAL_UP) {
            r.xpBonus = rule.xpBonus;
        }

        out.push_back(r);
    }
    return out;
}

void ResultsPackets::applyReward(ScoreRow& row, const Reward& reward) {
    // every other reward packet writes gold before exp so col two is gold
    row.col2Base  = clampU32(reward.gold);
    row.col2Bonus = clampU32(reward.goldBonus);
    row.col1Base  = clampU32(reward.xp);
    row.col1Bonus = clampU32(reward.xpBonus);
}

void ResultsPackets::applyRetirePenalty(int32_t& gold, int32_t& xp, int32_t activeCarCount) {
    // client only docks itself when more than one car runs
    if (activeCarCount <= 1) return;

    xp -= RETIRE_EXP_PENALTY;
    if (xp < 0) xp = 0;
    gold -= RETIRE_GOLD_PENALTY;
    if (gold < 0) gold = 0;
}

} // namespace knc
