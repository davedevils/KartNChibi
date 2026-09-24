#include "packets/gen/ProgressionPackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"
#include "util/LevelCurve.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace knc {

namespace {

using DbRow = std::map<std::string, std::string>;
using QueryFn = std::function<std::vector<DbRow>(const std::string&, const DbParams&)>;

// curve cache lives here so a lookup never hits the db per packet
std::mutex g_curveMutex;
std::vector<ProgressionPackets::CurveRow> g_curve;
bool g_curveOk = false;
bool g_curveTried = false;

int64_t colI64(const DbRow& row, const char* key, int64_t fallback) {
    return rowInt64NoThrow(row, key, fallback);
}

int32_t colI32(const DbRow& row, const char* key, int32_t fallback) {
    const int64_t v = colI64(row, key, fallback);
    if (v > ProgressionPackets::EXP_MAX) return ProgressionPackets::EXP_MAX;
    if (v < -ProgressionPackets::EXP_MAX) return -ProgressionPackets::EXP_MAX;
    return static_cast<int32_t>(v);
}

// saturating so a huge reward can never wrap the signed wire field
int32_t clampAdd(int32_t base, int32_t delta) {
    const int64_t sum = static_cast<int64_t>(base) + static_cast<int64_t>(delta);
    if (sum < 0) return 0;
    if (sum > ProgressionPackets::EXP_MAX) return ProgressionPackets::EXP_MAX;
    return static_cast<int32_t>(sum);
}

// db column is unsigned but a bad row must never wrap the wire byte
uint8_t clampU8(int64_t raw) {
    if (raw < 0) return 0;
    if (raw > 255) return 255;
    return static_cast<uint8_t>(raw);
}

// zero means owns nothing and the client wants minus one there
int32_t invIdOrNone(int64_t raw) {
    if (raw <= 0) return -1;
    if (raw > ProgressionPackets::EXP_MAX) return -1;
    return static_cast<int32_t>(raw);
}

void ensureCurve() {
    {
        std::lock_guard<std::mutex> lock(g_curveMutex);
        if (g_curveTried) return;
        g_curveTried = true;
    }
    ProgressionPackets::reloadLevelCurve();
}

bool readStatBlock(const QueryFn& q, uint32_t characterId, ProgressionPackets::StatBlock& out) {
    auto rows = q("SELECT account_id, level, experience, gold, cash, license_class, "
                  "exp_floor, exp_next, selected_kart_instance_id, pendant_key, "
                  "equipped_driver_id FROM characters WHERE id = ? LIMIT 1",
                  {static_cast<int32_t>(characterId)});
    if (rows.empty()) {
        LOG_ERROR("PROGRESS", "no character row id " + std::to_string(characterId));
        return false;
    }

    const DbRow& r = rows[0];
    out.playerId     = characterId;
    out.licenceGrade = clampU8(colI64(r, "license_class", 0));
    out.levelDisplay = colI32(r, "level", ProgressionPackets::DB_LEVEL_MIN);
    out.expCurrent   = colI32(r, "experience", 0);
    out.gold         = colI32(r, "gold", 0);
    out.astro        = colI32(r, "cash", 0);
    out.expFloor     = colI32(r, "exp_floor", 0);
    out.expNext      = colI32(r, "exp_next", 1);
    out.kartInvId    = invIdOrNone(colI64(r, "selected_kart_instance_id", 0));
    out.pendantKey   = colI32(r, "pendant_key", 0);
    out.characterInvId = -1;
    out.staffFlag    = 0;

    // wire wants the owned instance id not the catalog key
    const int64_t driverBase = colI64(r, "equipped_driver_id", 0);
    if (driverBase > 0) {
        auto owned = q("SELECT id FROM owned_character WHERE character_id = ? "
                       "AND base_key = ? LIMIT 1",
                       {static_cast<int32_t>(characterId), static_cast<int32_t>(driverBase)});
        if (!owned.empty()) {
            out.characterInvId = invIdOrNone(colI64(owned[0], "id", 0));
        }
    }

    // selected kart instance id was negative one for seeded accounts so the lobby stand drew neither driver nor kart
    if (out.kartInvId < 0) {
        auto ownedKart = q("SELECT id FROM owned_kart WHERE character_id = ? "
                           "AND active_flag = 1 ORDER BY id LIMIT 1",
                           {static_cast<int32_t>(characterId)});
        if (ownedKart.empty()) {
            ownedKart = q("SELECT id FROM owned_kart WHERE character_id = ? ORDER BY id LIMIT 1",
                          {static_cast<int32_t>(characterId)});
        }
        if (!ownedKart.empty()) {
            out.kartInvId = invIdOrNone(colI64(ownedKart[0], "id", 0));
            LOG_INFO("PROGRESS", "character " + std::to_string(characterId) +
                                 " had no selected kart so the wire falls back to owned kart " +
                                 std::to_string(out.kartInvId));
        } else {
            LOG_WARN("PROGRESS", "character " + std::to_string(characterId) +
                                 " owns no kart at all so the lobby stand stays empty");
        }
    }

    // staff flag is an account fact never the level
    const int64_t accountId = colI64(r, "account_id", 0);
    if (accountId > 0) {
        auto acc = q("SELECT `role` FROM accounts WHERE id = ? LIMIT 1",
                     {static_cast<int32_t>(accountId)});
        if (!acc.empty()) {
            out.staffFlag = clampU8(colI64(acc[0], "role", 0));
        }
    }

    return true;
}

} // namespace

std::vector<ProgressionPackets::CurveRow> ProgressionPackets::loadLevelCurveRows() {
    std::vector<CurveRow> rows;

    auto res = Database::instance().queryPrepared(
        "SELECT level, cum_exp FROM level_curve ORDER BY level ASC", {});

    rows.reserve(res.size());
    for (const auto& r : res) {
        CurveRow c;
        c.level  = colI32(r, "level", 0);
        c.cumExp = colI32(r, "cum_exp", 0);
        rows.push_back(c);
    }
    return rows;
}

bool ProgressionPackets::reloadLevelCurve() {
    std::vector<CurveRow> rows = loadLevelCurveRows();

    if (rows.empty()) {
        LOG_ERROR("PROGRESS", "level_curve empty or missing so exp bar bounds stay whatever "
                              "the character row holds");
        std::lock_guard<std::mutex> lock(g_curveMutex);
        g_curveTried = true;
        return false;
    }

    if (rows.front().level < DB_LEVEL_MIN) {
        LOG_ERROR("PROGRESS", "level_curve first level " + std::to_string(rows.front().level) +
                              " below " + std::to_string(DB_LEVEL_MIN) + " rejected");
        return false;
    }

    for (size_t i = 1; i < rows.size(); ++i) {
        if (rows[i].level <= rows[i - 1].level) {
            LOG_ERROR("PROGRESS", "level_curve level not increasing at row " + std::to_string(i) +
                                  " value " + std::to_string(rows[i].level) + " rejected");
            return false;
        }
        // equal bounds make FDIVP produce the integer indefinite and an empty bar
        if (rows[i].cumExp <= rows[i - 1].cumExp) {
            LOG_ERROR("PROGRESS", "level_curve cum_exp not increasing at level " +
                                  std::to_string(rows[i].level) + " rejected");
            return false;
        }
    }

    if (rows.front().cumExp != 0) {
        LOG_WARN("PROGRESS", "level_curve first cum_exp " + std::to_string(rows.front().cumExp) +
                             " not zero so a fresh character starts mid bar");
    }

    const int32_t topWire = rows.back().level - DB_LEVEL_MIN;
    if (topWire > WIRE_LEVEL_MAX_RENDER) {
        LOG_WARN("PROGRESS", "level_curve top level draws wrong, this build only has "
                             "icons up to wire " + std::to_string(WIRE_LEVEL_MAX_RENDER));
    }

    {
        std::lock_guard<std::mutex> lock(g_curveMutex);
        g_curve = std::move(rows);
        g_curveOk = true;
        g_curveTried = true;
    }

    LOG_INFO("PROGRESS", "level_curve loaded rows " + std::to_string(curveSize()) +
                         " top level " + std::to_string(maxCurveLevel()));
    return true;
}

bool ProgressionPackets::curveLoaded() {
    std::lock_guard<std::mutex> lock(g_curveMutex);
    return g_curveOk && !g_curve.empty();
}

size_t ProgressionPackets::curveSize() {
    std::lock_guard<std::mutex> lock(g_curveMutex);
    return g_curve.size();
}

int32_t ProgressionPackets::maxCurveLevel() {
    std::lock_guard<std::mutex> lock(g_curveMutex);
    if (g_curve.empty()) return DB_LEVEL_MIN;
    return g_curve.back().level;
}

int32_t ProgressionPackets::cumExpForLevel(int32_t levelDisplay) {
    ensureCurve();
    std::lock_guard<std::mutex> lock(g_curveMutex);
    for (const auto& row : g_curve) {
        if (row.level == levelDisplay) return row.cumExp;
    }
    return 0;
}

int32_t ProgressionPackets::levelForExp(int32_t exp) {
    ensureCurve();
    std::lock_guard<std::mutex> lock(g_curveMutex);
    if (!g_curveOk || g_curve.empty()) return DB_LEVEL_MIN;
    return levelOnCurve(g_curve, exp, DB_LEVEL_MIN);
}

bool ProgressionPackets::expBoundsForLevel(int32_t levelDisplay, int32_t& floorOut,
                                           int32_t& nextOut) {
    ensureCurve();
    std::lock_guard<std::mutex> lock(g_curveMutex);
    floorOut = 0;
    nextOut = 1;
    if (!g_curveOk || g_curve.empty()) return false;
    return boundsOnCurve(g_curve, levelDisplay, floorOut, nextOut, EXP_MAX);
}

bool ProgressionPackets::loadStats(uint32_t characterId, StatBlock& out) {
    ensureCurve();
    QueryFn q = [](const std::string& sql, const DbParams& params) {
        return Database::instance().queryPrepared(sql, params);
    };
    if (!readStatBlock(q, characterId, out)) return false;
    sanitize(out);
    return true;
}

void ProgressionPackets::sanitize(StatBlock& stats) {
    if (stats.licenceGrade > LICENCE_GRADE_MAX) {
        LOG_ERROR("PROGRESS", "licence grade " + std::to_string(stats.licenceGrade) +
                              " out of range for player " + std::to_string(stats.playerId) +
                              " clamped");
        stats.licenceGrade = LICENCE_GRADE_MAX;
    }

    if (stats.levelDisplay < DB_LEVEL_MIN) {
        LOG_WARN("PROGRESS", "level " + std::to_string(stats.levelDisplay) + " below base for "
                             "player " + std::to_string(stats.playerId) + " clamped");
        stats.levelDisplay = DB_LEVEL_MIN;
    }
    const int32_t maxDisplay = DB_LEVEL_MIN + static_cast<int32_t>(WIRE_LEVEL_MAX_SEND);
    if (stats.levelDisplay > maxDisplay) {
        LOG_WARN("PROGRESS", "level " + std::to_string(stats.levelDisplay) + " above what the "
                             "wire byte can carry clamped to " + std::to_string(maxDisplay));
        stats.levelDisplay = maxDisplay;
    }

    if (stats.expCurrent < 0) {
        LOG_ERROR("PROGRESS", "negative exp for player " + std::to_string(stats.playerId) +
                              " clamped to zero");
        stats.expCurrent = 0;
    }
    if (stats.gold < 0) {
        LOG_ERROR("PROGRESS", "negative gold for player " + std::to_string(stats.playerId) +
                              " clamped to zero");
        stats.gold = 0;
    }
    if (stats.astro < 0) {
        LOG_ERROR("PROGRESS", "negative astro for player " + std::to_string(stats.playerId) +
                              " clamped to zero");
        stats.astro = 0;
    }

    if (stats.expFloor < 0) stats.expFloor = 0;
    if (stats.expFloor >= EXP_MAX) stats.expFloor = EXP_MAX - 1;

    // numerator goes negative here and the fill loop draws nothing
    if (stats.expCurrent < stats.expFloor) {
        LOG_ERROR("PROGRESS", "exp below floor for player " + std::to_string(stats.playerId) +
                              " exp " + std::to_string(stats.expCurrent) + " floor " +
                              std::to_string(stats.expFloor) + " floor pulled down");
        stats.expFloor = stats.expCurrent;
    }

    // zero divisor here is what empties the bar and prints the bad exp line
    if (stats.expNext <= stats.expFloor) {
        LOG_ERROR("PROGRESS", "exp bar divisor not above floor for player " +
                              std::to_string(stats.playerId) + " floor " +
                              std::to_string(stats.expFloor) + " next " +
                              std::to_string(stats.expNext) + " forced to floor plus one");
        stats.expNext = stats.expFloor + 1;
    }

    // only zero two and six are understood and any non zero draws the GM crown
    if (stats.staffFlag != 0 && stats.staffFlag != 2 && stats.staffFlag != 6) {
        LOG_WARN("PROGRESS", "staff flag " + std::to_string(stats.staffFlag) + " on player " +
                             std::to_string(stats.playerId) +
                             " is not a known value and still draws the GM crown");
    }
}

int8_t ProgressionPackets::wireLevelFrom(int32_t levelDisplay) {
    int32_t wire = levelDisplay - DB_LEVEL_MIN;
    if (wire < WIRE_LEVEL_MIN) wire = WIRE_LEVEL_MIN;
    if (wire > WIRE_LEVEL_MAX_SEND) wire = WIRE_LEVEL_MAX_SEND;
    if (wire > WIRE_LEVEL_MAX_RENDER) {
        LOG_WARN("PROGRESS", "wire level " + std::to_string(wire) +
                             " has no icon in this build and pins the bar full");
    }
    return static_cast<int8_t>(wire);
}

int8_t ProgressionPackets::wireLevel(const StatBlock& stats) {
    return wireLevelFrom(stats.levelDisplay);
}

bool ProgressionPackets::validateExpBar(const StatBlock& stats) {
    bool ok = true;
    if (stats.expNext <= stats.expFloor) {
        LOG_ERROR("PROGRESS", "player " + std::to_string(stats.playerId) + " next " +
                              std::to_string(stats.expNext) + " not above floor " +
                              std::to_string(stats.expFloor));
        ok = false;
    }
    if (stats.expCurrent < stats.expFloor) {
        LOG_ERROR("PROGRESS", "player " + std::to_string(stats.playerId) + " exp " +
                              std::to_string(stats.expCurrent) + " below floor " +
                              std::to_string(stats.expFloor));
        ok = false;
    }
    if (stats.expCurrent > stats.expNext) {
        LOG_WARN("PROGRESS", "player " + std::to_string(stats.playerId) + " exp " +
                             std::to_string(stats.expCurrent) + " above next " +
                             std::to_string(stats.expNext) + " bar renders full");
    }
    return ok;
}

bool ProgressionPackets::resyncExpBounds(uint32_t characterId, StatBlock& out) {
    ensureCurve();

    auto tx = Database::instance().beginTransaction();
    if (!tx.valid()) {
        LOG_ERROR("PROGRESS", "resyncExpBounds no transaction for " + std::to_string(characterId));
        return false;
    }

    auto locked = tx.query("SELECT id FROM characters WHERE id = ? FOR UPDATE",
                           {static_cast<int32_t>(characterId)});
    if (locked.empty()) {
        LOG_ERROR("PROGRESS", "resyncExpBounds unknown character " + std::to_string(characterId));
        return false;
    }

    QueryFn q = [&tx](const std::string& sql, const DbParams& params) {
        return tx.query(sql, params);
    };
    if (!readStatBlock(q, characterId, out)) return false;

    if (!curveLoaded()) {
        LOG_ERROR("PROGRESS", "resyncExpBounds without a curve keeps the stored bounds");
        sanitize(out);
        return false;
    }

    int32_t level = levelForExp(out.expCurrent);
    if (LEVEL_NEVER_DECREASES && level < out.levelDisplay) level = out.levelDisplay;

    int32_t floorVal = out.expFloor;
    int32_t nextVal = out.expNext;
    expBoundsForLevel(level, floorVal, nextVal);

    if (!tx.execute("UPDATE characters SET level = ?, exp_floor = ?, exp_next = ? WHERE id = ?",
                    {level, floorVal, nextVal, static_cast<int32_t>(characterId)})) {
        LOG_ERROR("PROGRESS", "resyncExpBounds update failed for " + std::to_string(characterId));
        return false;
    }
    if (!tx.commit()) {
        LOG_ERROR("PROGRESS", "resyncExpBounds commit failed for " + std::to_string(characterId));
        return false;
    }

    out.levelDisplay = level;
    out.expFloor = floorVal;
    out.expNext = nextVal;
    sanitize(out);
    return true;
}

ProgressionPackets::AwardResult ProgressionPackets::award(uint32_t characterId, int32_t expDelta,
                                                          int32_t goldDelta, int32_t astroDelta) {
    AwardResult res;
    ensureCurve();

    auto tx = Database::instance().beginTransaction();
    if (!tx.valid()) {
        LOG_ERROR("PROGRESS", "award no transaction for " + std::to_string(characterId));
        return res;
    }

    // lock before reading else payout and refresh disagree
    auto locked = tx.query("SELECT id FROM characters WHERE id = ? FOR UPDATE",
                           {static_cast<int32_t>(characterId)});
    if (locked.empty()) {
        LOG_ERROR("PROGRESS", "award unknown character " + std::to_string(characterId));
        return res;
    }

    QueryFn q = [&tx](const std::string& sql, const DbParams& params) {
        return tx.query(sql, params);
    };

    StatBlock s;
    if (!readStatBlock(q, characterId, s)) return res;

    res.levelBefore = s.levelDisplay;
    const int32_t oldExp = s.expCurrent;
    const int32_t oldGold = s.gold;
    const int32_t oldAstro = s.astro;

    const int32_t newExp = clampAdd(oldExp, expDelta);
    const int32_t newGold = clampAdd(oldGold, goldDelta);
    const int32_t newAstro = clampAdd(oldAstro, astroDelta);

    int32_t newLevel = s.levelDisplay;
    int32_t floorVal = s.expFloor;
    int32_t nextVal = s.expNext;

    if (curveLoaded()) {
        int32_t derived = levelForExp(newExp);
        if (LEVEL_NEVER_DECREASES && derived < s.levelDisplay) derived = s.levelDisplay;
        newLevel = derived;
        expBoundsForLevel(newLevel, floorVal, nextVal);
    } else if (expDelta != 0) {
        LOG_ERROR("PROGRESS", "exp awarded with no level curve so level and bar bounds stay "
                              "stale for player " + std::to_string(characterId));
    }

    if (!tx.execute("UPDATE characters SET level = ?, experience = ?, gold = ?, cash = ?, "
                    "exp_floor = ?, exp_next = ? WHERE id = ?",
                    {newLevel, newExp, newGold, newAstro, floorVal, nextVal,
                     static_cast<int32_t>(characterId)})) {
        LOG_ERROR("PROGRESS", "award update failed for " + std::to_string(characterId));
        return res;
    }
    if (!tx.commit()) {
        LOG_ERROR("PROGRESS", "award commit failed for " + std::to_string(characterId));
        return res;
    }

    s.levelDisplay = newLevel;
    s.expCurrent = newExp;
    s.gold = newGold;
    s.astro = newAstro;
    s.expFloor = floorVal;
    s.expNext = nextVal;
    sanitize(s);

    res.ok = true;
    res.stats = s;
    res.expApplied = newExp - oldExp;
    res.goldApplied = newGold - oldGold;
    res.astroApplied = newAstro - oldAstro;
    res.levelsGained = newLevel - res.levelBefore;
    res.leveledUp = res.levelsGained > 0;

    if (res.leveledUp) {
        LOG_INFO("PROGRESS", "player " + std::to_string(characterId) + " level " +
                             std::to_string(res.levelBefore) + " to " + std::to_string(newLevel) +
                             " exp " + std::to_string(newExp));
    }
    return res;
}

ProgressionPackets::AwardResult ProgressionPackets::awardExpAndGold(uint32_t characterId,
                                                                    int32_t expDelta,
                                                                    int32_t goldDelta) {
    return award(characterId, expDelta, goldDelta, 0);
}

ProgressionPackets::AwardResult ProgressionPackets::awardExp(uint32_t characterId,
                                                             int32_t expDelta) {
    return award(characterId, expDelta, 0, 0);
}

ProgressionPackets::AwardResult ProgressionPackets::awardGold(uint32_t characterId,
                                                              int32_t goldDelta) {
    return award(characterId, 0, goldDelta, 0);
}

ProgressionPackets::AwardResult ProgressionPackets::awardAstro(uint32_t characterId,
                                                               int32_t astroDelta) {
    return award(characterId, 0, 0, astroDelta);
}

bool ProgressionPackets::spendGold(uint32_t characterId, int32_t cost, StatBlock& out) {
    if (cost < 0) {
        LOG_ERROR("PROGRESS", "spendGold negative cost " + std::to_string(cost));
        return false;
    }
    ensureCurve();

    auto tx = Database::instance().beginTransaction();
    if (!tx.valid()) {
        LOG_ERROR("PROGRESS", "spendGold no transaction for " + std::to_string(characterId));
        return false;
    }

    auto rows = tx.query("SELECT gold FROM characters WHERE id = ? FOR UPDATE",
                         {static_cast<int32_t>(characterId)});
    if (rows.empty()) {
        LOG_ERROR("PROGRESS", "spendGold unknown character " + std::to_string(characterId));
        return false;
    }

    const int32_t gold = colI32(rows[0], "gold", 0);
    // client never checks so this is the only gate
    if (gold < cost) return false;

    if (!tx.execute("UPDATE characters SET gold = ? WHERE id = ?",
                    {gold - cost, static_cast<int32_t>(characterId)})) {
        return false;
    }

    QueryFn q = [&tx](const std::string& sql, const DbParams& params) {
        return tx.query(sql, params);
    };
    if (!readStatBlock(q, characterId, out)) return false;
    if (!tx.commit()) {
        LOG_ERROR("PROGRESS", "spendGold commit failed for " + std::to_string(characterId));
        return false;
    }

    sanitize(out);
    return true;
}

bool ProgressionPackets::spendAstro(uint32_t characterId, int32_t cost, StatBlock& out) {
    if (cost < 0) {
        LOG_ERROR("PROGRESS", "spendAstro negative cost " + std::to_string(cost));
        return false;
    }
    ensureCurve();

    auto tx = Database::instance().beginTransaction();
    if (!tx.valid()) {
        LOG_ERROR("PROGRESS", "spendAstro no transaction for " + std::to_string(characterId));
        return false;
    }

    auto rows = tx.query("SELECT cash FROM characters WHERE id = ? FOR UPDATE",
                         {static_cast<int32_t>(characterId)});
    if (rows.empty()) {
        LOG_ERROR("PROGRESS", "spendAstro unknown character " + std::to_string(characterId));
        return false;
    }

    const int32_t astro = colI32(rows[0], "cash", 0);
    if (astro < cost) return false;

    if (!tx.execute("UPDATE characters SET cash = ? WHERE id = ?",
                    {astro - cost, static_cast<int32_t>(characterId)})) {
        return false;
    }

    QueryFn q = [&tx](const std::string& sql, const DbParams& params) {
        return tx.query(sql, params);
    };
    if (!readStatBlock(q, characterId, out)) return false;
    if (!tx.commit()) {
        LOG_ERROR("PROGRESS", "spendAstro commit failed for " + std::to_string(characterId));
        return false;
    }

    sanitize(out);
    return true;
}

bool ProgressionPackets::setLicenceGrade(uint32_t characterId, uint8_t grade) {
    uint8_t g = grade;
    if (g > LICENCE_GRADE_MAX) {
        LOG_ERROR("PROGRESS", "licence grade " + std::to_string(grade) +
                              " out of range clamped for " + std::to_string(characterId));
        g = LICENCE_GRADE_MAX;
    }
    // the first licence ends the tutorial login then lands on the lobby instead of the licence stage
    return Database::instance().executePrepared(
        "UPDATE characters SET license_class = ?, "
        "tutorial_completed = IF(? >= 1, 1, tutorial_completed) WHERE id = ?",
        {static_cast<int32_t>(g), static_cast<int32_t>(g), static_cast<int32_t>(characterId)});
}

void ProgressionPackets::fillLoginProfile(const StatBlock& stats,
                                          CharCreatePackets::LoginProfile& out) {
    StatBlock s = stats;
    sanitize(s);

    out.channelLevelBand      = s.licenceGrade;
    out.level                 = static_cast<uint8_t>(wireLevel(s));
    out.expCurrent            = static_cast<uint32_t>(s.expCurrent);
    out.astro                 = static_cast<uint32_t>(s.astro);
    out.gold                  = static_cast<uint32_t>(s.gold);
    out.selectedCharacterId   = static_cast<uint32_t>(s.characterInvId);
    out.selectedKartId        = static_cast<uint32_t>(s.kartInvId);
    out.sessionRole           = s.staffFlag;
    out.expFloorCurrentLevel  = static_cast<uint32_t>(s.expFloor);
    out.expRequiredNextLevel  = static_cast<uint32_t>(s.expNext);
    out.equippedTitleKey      = static_cast<uint32_t>(s.pendantKey);
}

Packet ProgressionPackets::statsRefresh(const StatBlock& stats) {
    StatBlock s = stats;
    sanitize(s);

    CharCreatePackets::LoginProfile profile;
    fillLoginProfile(s, profile);

    // one writer for these bytes else the blob tail and the refresh drift apart
    Packet pkt = CharCreatePackets::profileRefresh(profile);

    if (pkt.payload().size() != STATS_REFRESH_SIZE) {
        LOG_ERROR("PACKET", "statsRefresh size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(STATS_REFRESH_SIZE));
    }
    return pkt;
}

Packet ProgressionPackets::astroBalance(int32_t astro) {
    int32_t value = astro;
    if (value < 0) {
        LOG_ERROR("PACKET", "astroBalance negative " + std::to_string(astro) + " clamped");
        value = 0;
    }

    Packet pkt = Packet::fromCmdFull(OP_S_ASTRO_BALANCE);
    pkt.writeInt32(value);

    if (pkt.payload().size() != ASTRO_BALANCE_SIZE) {
        LOG_ERROR("PACKET", "astroBalance size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(ASTRO_BALANCE_SIZE));
    }
    return pkt;
}

size_t ProgressionPackets::popupBlobSize(int32_t kind) {
    switch (kind) {
        case 0: return 0x2C;
        case 1: return 0x38;
        case 2: return 0x1C;
        case 3: return 0x1C;
        case 5: return 0x30;
        case 6: return 0x84;
        default: return 0;    // four and seven have no case here unlike the reward packets
    }
}

bool ProgressionPackets::validatePopupBlob(int32_t kind, const std::vector<uint8_t>& bytes) {
    if (kind == 6) {
        const size_t base = 0x85;
        if (bytes.size() < base) {
            LOG_ERROR("PACKET", "popup kind six size " + std::to_string(bytes.size()) +
                                " below " + std::to_string(base));
            return false;
        }
        const size_t want = (bytes[0x84] == 1) ? (base + 0x34) : base;
        if (bytes.size() != want) {
            LOG_ERROR("PACKET", "popup kind six size " + std::to_string(bytes.size()) +
                                " expected " + std::to_string(want));
            return false;
        }
        return true;
    }

    const size_t want = popupBlobSize(kind);
    if (bytes.size() != want) {
        LOG_ERROR("PACKET", "popup kind " + std::to_string(kind) + " size " +
                            std::to_string(bytes.size()) + " expected " + std::to_string(want));
        return false;
    }
    return true;
}

Packet ProgressionPackets::rewardPopup(const RewardPopup& popup) {
    const bool blobOk = validatePopupBlob(popup.rewardKind, popup.blob);

    Packet pkt = Packet::fromCmdFull(OP_S_REWARD_POPUP);
    pkt.writeUInt32(popup.unused0);
    pkt.writeInt32(popup.sourceCode);
    pkt.writeUInt32(popup.ctxA);
    pkt.writeInt32(popup.ctxB);
    pkt.writeInt32(popup.rewardKind);
    pkt.writeUInt32(popup.unused14);
    pkt.writeUInt32(popup.unused18);

    if (blobOk && !popup.blob.empty()) {
        pkt.writeBytes(popup.blob.data(), popup.blob.size());
    }

    const size_t expected = REWARD_POPUP_SIZE + ((blobOk) ? popup.blob.size() : 0);
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "rewardPopup size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    if (!blobOk) {
        LOG_ERROR("PACKET", "rewardPopup kind " + std::to_string(popup.rewardKind) +
                            " blob dropped so the client reads the next frame instead");
    }
    return pkt;
}

Packet ProgressionPackets::levelUpBanner(uint32_t ctxA, int32_t ctxB) {
    // banner prints the level already in the blob so push the new one first
    RewardPopup p;
    p.sourceCode = SRC_LEVEL_UP;
    p.ctxA = ctxA;
    p.ctxB = ctxB;
    p.rewardKind = 4;
    return rewardPopup(p);
}

Packet ProgressionPackets::achievementReward(const AchievementReward& reward) {
    const bool carriesWallet = (reward.kind == 2 || reward.kind == 3);
    const bool wantItem = (reward.kind == 3) && reward.hasItem &&
                          MissionPackets::validateRewardBlob(reward.item);

    Packet pkt = Packet::fromCmdFull(OP_S_ACHIEVEMENT_REWARD);
    pkt.writeInt32(reward.kind);
    pkt.writeUInt8(reward.flag);
    pkt.writeUInt32(static_cast<uint32_t>(reward.achievementId & 0xFFFFFFFFu));
    pkt.writeUInt32(static_cast<uint32_t>((reward.achievementId >> 32) & 0xFFFFFFFFu));

    if (carriesWallet) {
        pkt.writeInt32(reward.goldAfter);
        pkt.writeInt32(reward.expAfter);
    }
    if (wantItem) {
        pkt.writeBytes(reward.item.bytes.data(), reward.item.bytes.size());
    }

    size_t expected = carriesWallet ? ACHIEVEMENT_PAID_SIZE : ACHIEVEMENT_BASE_SIZE;
    if (wantItem) expected += reward.item.bytes.size();
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "achievementReward size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    if (reward.hasItem && !wantItem) {
        LOG_ERROR("PACKET", "achievementReward kind " + std::to_string(reward.kind) +
                            " item dropped bad blob or wrong kind");
    }
    if (!carriesWallet && (reward.goldAfter != 0 || reward.expAfter != 0)) {
        LOG_WARN("PACKET", "achievementReward kind " + std::to_string(reward.kind) +
                           " never carries the wallet pair so those values were dropped");
    }
    return pkt;
}

int32_t ProgressionPackets::quickJoinMode(uint8_t licenceGrade, int32_t wireLevelValue) {
    if (licenceGrade == 0) return QUICKJOIN_MODE_OPEN;
    if (licenceGrade == 1) {
        return (wireLevelValue > LICENCE_ADV_MIN_WIRE_LEVEL) ? QUICKJOIN_MODE_OPEN
                                                             : QUICKJOIN_MODE_GATED;
    }
    if (licenceGrade == 2) {
        return (wireLevelValue > LICENCE_MASTER_MIN_WIRE_LEVEL) ? QUICKJOIN_MODE_OPEN
                                                                : QUICKJOIN_MODE_GATED;
    }
    return QUICKJOIN_MODE_GATED;
}

bool ProgressionPackets::advancedLicenceUnlocked(uint8_t licenceGrade, int32_t wireLevelValue) {
    return licenceGrade >= 1 && wireLevelValue >= LICENCE_ADV_MIN_WIRE_LEVEL;
}

bool ProgressionPackets::masterLicenceUnlocked(uint8_t licenceGrade, int32_t wireLevelValue) {
    return licenceGrade >= 2 && wireLevelValue >= LICENCE_MASTER_MIN_WIRE_LEVEL;
}

} // namespace knc
