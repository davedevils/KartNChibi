// level curve exp awards wallets and the S2C 0x000A push

#include "handlers/ProgressionHandler.h"
#include "GameServer.h"
#include "packets/PacketBuilder.h"
#include "packets/gen/ProgressionPackets.h"
#include "packets/gen/CharCreatePackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"
#include "util/PendantRules.h"

#include <cstdint>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace knc {

namespace {

constexpr const char* TAG = "PROGRESS";

// ctxA and ctxB are the reward reel slot 8 entries at 0x011B44E8 and 0x011B450C zero draws nothing rather than faulting
constexpr uint32_t LEVELUP_CTX_A = 0;
constexpr int32_t  LEVELUP_CTX_B = 0;

// one login must not replay an unbounded backlog into a single burst
constexpr int32_t GRANT_DRAIN_LIMIT = 64;

using DbRow = std::map<std::string, std::string>;

// boot state init runs the body once and reloadCurve is the retry
std::mutex g_bootMutex;
bool g_bootDone = false;
bool g_bootOk = false;

// probe once else a missing table logs on every single login
std::mutex g_grantMutex;
bool g_grantProbed = false;
bool g_grantPresent = false;

int32_t clampToWire(int64_t v) {
    if (v < 0) return 0;
    if (v > ProgressionPackets::EXP_MAX) return ProgressionPackets::EXP_MAX;
    return static_cast<int32_t>(v);
}

std::string idText(uint32_t characterId) {
    return std::to_string(characterId);
}

int64_t colI64(const DbRow& row, const char* key, int64_t fallback) {
    return rowInt64NoThrow(row, key, fallback);
}

int32_t colI32(const DbRow& row, const char* key, int32_t fallback) {
    const int64_t v = colI64(row, key, fallback);
    if (v > ProgressionPackets::EXP_MAX) return ProgressionPackets::EXP_MAX;
    if (v < -ProgressionPackets::EXP_MAX) return -ProgressionPackets::EXP_MAX;
    return static_cast<int32_t>(v);
}

std::string colStr(const DbRow& row, const char* key) {
    return rowStrCore(row, key);
}

// migration 009 seeds d n as 100 n squared plus 400 n so the cumulative closes here
int64_t syntheticCumExp(int64_t level) {
    if (level <= 1) return 0;
    const int64_t n = level - 1;
    return 100 * n * (n + 1) * (2 * n + 1) / 6 + 200 * n * (n + 1);
}

bool looksLikeSyntheticSeed(const std::vector<ProgressionPackets::CurveRow>& rows) {
    if (rows.size() < 4) return false;
    for (const auto& r : rows) {
        if (syntheticCumExp(r.level) != static_cast<int64_t>(r.cumExp)) return false;
    }
    return true;
}

} // namespace

uint32_t ProgressionHandler::characterOf(const Session::Ptr& s, const char* what) {
    if (!s) {
        LOG_WARN(TAG, std::string(what) + " called with a null session");
        return 0;
    }
    if (s->characterId == 0) {
        LOG_WARN(TAG, std::string(what) + " session " + std::to_string(s->id()) +
                      " has no character bound yet");
        return 0;
    }
    return s->characterId;
}

bool ProgressionHandler::init() {
    {
        std::lock_guard<std::mutex> lock(g_bootMutex);
        if (g_bootDone) return g_bootOk;
        g_bootDone = true;
    }

    const bool loaded = ProgressionPackets::reloadLevelCurve();
    // audit runs even on a failed load so the reason is named once not guessed
    const bool audited = auditCurveTable();
    const bool ok = loaded && audited;

    if (!loaded) {
        LOG_ERROR(TAG, "level_curve not usable so every exp bar falls back to the stored "
                       "exp_floor and exp_next of each character row, run reloadCurve after "
                       "applying migration 009");
    } else {
        const int32_t cap = ProgressionPackets::maxCurveLevel();
        LOG_INFO(TAG, "level curve ready rows " +
                      std::to_string(ProgressionPackets::curveSize()) + " cap " +
                      std::to_string(cap));

        // client art stops at wire 49 so anything past that draws the same icon
        const int32_t topWire = cap - ProgressionPackets::DB_LEVEL_MIN;
        if (topWire > ProgressionPackets::WIRE_LEVEL_MAX_RENDER) {
            LOG_WARN(TAG, "curve cap " + std::to_string(cap) +
                          " exceeds what this client renders wire " +
                          std::to_string(ProgressionPackets::WIRE_LEVEL_MAX_RENDER));
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_bootMutex);
        g_bootOk = ok;
    }
    return ok;
}

bool ProgressionHandler::ensureBooted() {
    {
        std::lock_guard<std::mutex> lock(g_bootMutex);
        if (g_bootDone) return g_bootOk;
    }
    return init();
}

bool ProgressionHandler::reloadCurve() {
    const bool loaded = ProgressionPackets::reloadLevelCurve();
    const bool audited = auditCurveTable();

    {
        std::lock_guard<std::mutex> lock(g_bootMutex);
        g_bootDone = true;
        g_bootOk = loaded && audited;
    }

    if (!loaded) {
        LOG_ERROR(TAG, "curve reload rejected so the previous cache stays in use");
    } else {
        LOG_INFO(TAG, "curve reload accepted rows " +
                      std::to_string(ProgressionPackets::curveSize()) + " cap " +
                      std::to_string(ProgressionPackets::maxCurveLevel()));
    }
    return loaded && audited;
}

bool ProgressionHandler::curveReady() {
    ensureBooted();
    return ProgressionPackets::curveLoaded();
}

bool ProgressionHandler::auditCurveTable() {
    // raw read bypasses the cache so an edit under a live server is visible
    const std::vector<ProgressionPackets::CurveRow> rows =
        ProgressionPackets::loadLevelCurveRows();

    if (rows.empty()) {
        LOG_ERROR(TAG, "level_curve audit found no rows, the ladder is missing and every "
                       "level lookup will answer with the base level");
        return false;
    }

    bool ok = true;

    if (rows.front().level != ProgressionPackets::DB_LEVEL_MIN) {
        LOG_ERROR(TAG, "level_curve audit first level is " +
                       std::to_string(rows.front().level) + " not " +
                       std::to_string(ProgressionPackets::DB_LEVEL_MIN) +
                       " so a fresh character has no floor row");
        ok = false;
    }
    if (rows.front().cumExp != 0) {
        LOG_WARN(TAG, "level_curve audit first cum_exp is " +
                      std::to_string(rows.front().cumExp) +
                      " not zero so a fresh character starts mid bar");
    }

    // reloadLevelCurve only rejects a non increasing level so a hole passes it
    for (size_t i = 1; i < rows.size(); ++i) {
        if (rows[i].level != rows[i - 1].level + 1) {
            LOG_ERROR(TAG, "level_curve audit gap between level " +
                           std::to_string(rows[i - 1].level) + " and " +
                           std::to_string(rows[i].level) +
                           " so every level inside the hole shares one bar");
            ok = false;
        }
        if (rows[i].cumExp <= rows[i - 1].cumExp) {
            LOG_ERROR(TAG, "level_curve audit cum_exp not increasing at level " +
                           std::to_string(rows[i].level) + " so the bar divides by zero");
            ok = false;
        }
    }

    // cache and table can drift when someone edits the rows on a live server
    if (ProgressionPackets::curveLoaded()) {
        if (ProgressionPackets::curveSize() != rows.size()) {
            LOG_WARN(TAG, "level_curve audit cache holds " +
                          std::to_string(ProgressionPackets::curveSize()) +
                          " rows but the table holds " + std::to_string(rows.size()) +
                          " so a reloadCurve is due");
        } else {
            for (const auto& r : rows) {
                if (ProgressionPackets::cumExpForLevel(r.level) != r.cumExp) {
                    LOG_WARN(TAG, "level_curve audit level " + std::to_string(r.level) +
                                  " table cum_exp " + std::to_string(r.cumExp) +
                                  " differs from the cached value, a reloadCurve is due");
                    break;
                }
            }
        }
    }

    const int32_t top = rows.back().level;
    if (top > CLIENT_LEVEL_ICON_MAX) {
        LOG_WARN(TAG, "level_curve audit top level " + std::to_string(top) +
                      " is past the client icon clamp at " +
                      std::to_string(CLIENT_LEVEL_ICON_MAX) +
                      " so those levels reuse Lv_icon_050 and draw a full exp bar");
    }
    // wireLevelFrom already clamps into 0 to 54 so a tall curve is degraded not broken
    if (top - ProgressionPackets::DB_LEVEL_MIN > ProgressionPackets::WIRE_LEVEL_MAX_SEND) {
        LOG_WARN(TAG, "level_curve audit top level " + std::to_string(top) +
                      " is past the wire send clamp so every level above it reports the "
                      "same wire byte, the curve still loads");
    }

    if (looksLikeSyntheticSeed(rows)) {
        // client holds no curve so the shape is ours to tune not to discover
        LOG_INFO(TAG, "level_curve holds the migration 009 seed, deliberate server policy, "
                      "the client never derives a level from exp so any strictly increasing "
                      "ladder is wire correct, the only client cliff is level " +
                      std::to_string(CLIENT_LEVEL_ICON_MAX) +
                      " where the icon clamps and the exp bar pins full");
    }

    LOG_INFO(TAG, "level_curve audit rows " + std::to_string(rows.size()) + " top level " +
                  std::to_string(top) + (ok ? " usable" : " NOT usable"));
    return ok;
}

int32_t ProgressionHandler::levelForExp(int64_t exp) {
    ensureBooted();
    if (exp < 0) {
        LOG_WARN(TAG, "levelForExp negative exp " + std::to_string(exp) + " treated as zero");
    }
    if (exp > ProgressionPackets::EXP_MAX) {
        LOG_WARN(TAG, "levelForExp exp " + std::to_string(exp) +
                      " above the signed wire limit clamped");
    }
    if (!ProgressionPackets::curveLoaded()) {
        LOG_WARN(TAG, "levelForExp without a curve returns the base level");
    }
    return ProgressionPackets::levelForExp(clampToWire(exp));
}

int32_t ProgressionHandler::expFloorForLevel(int32_t lvl) {
    ensureBooted();
    int32_t floorVal = 0;
    int32_t nextVal = 1;
    if (!ProgressionPackets::expBoundsForLevel(lvl, floorVal, nextVal)) {
        LOG_WARN(TAG, "expFloorForLevel level " + std::to_string(lvl) +
                      " is off the curve so the nearest end was used");
        return floorVal;
    }

    // row and bounds must agree else the cached curve got edited under us
    const int32_t exact = ProgressionPackets::cumExpForLevel(lvl);
    if (exact != floorVal) {
        LOG_ERROR(TAG, "level_curve row " + std::to_string(lvl) + " cum_exp " +
                       std::to_string(exact) + " disagrees with the computed floor " +
                       std::to_string(floorVal));
    }
    return floorVal;
}

int32_t ProgressionHandler::expNextForLevel(int32_t lvl) {
    ensureBooted();
    int32_t floorVal = 0;
    int32_t nextVal = 1;
    if (!ProgressionPackets::expBoundsForLevel(lvl, floorVal, nextVal)) {
        LOG_WARN(TAG, "expNextForLevel level " + std::to_string(lvl) +
                      " is off the curve so the nearest end was used");
    }
    // zero divisor is exactly what prints the bad exp line
    if (nextVal <= floorVal) {
        LOG_ERROR(TAG, "expNextForLevel level " + std::to_string(lvl) + " next " +
                       std::to_string(nextVal) + " not above floor " + std::to_string(floorVal) +
                       " forced to floor plus one");
        nextVal = (floorVal < ProgressionPackets::EXP_MAX) ? floorVal + 1
                                                           : ProgressionPackets::EXP_MAX;
    }
    return nextVal;
}

int32_t ProgressionHandler::levelCap() {
    ensureBooted();
    if (!ProgressionPackets::curveLoaded()) {
        LOG_WARN(TAG, "levelCap asked with no curve so the base level is reported");
    }
    return ProgressionPackets::maxCurveLevel();
}

size_t ProgressionHandler::curveRows() {
    ensureBooted();
    return ProgressionPackets::curveSize();
}

bool ProgressionHandler::loadStats(uint32_t characterId, ProgressionPackets::StatBlock& out) {
    ensureBooted();
    if (characterId == 0) {
        LOG_WARN(TAG, "loadStats with character id zero");
        return false;
    }
    if (!ProgressionPackets::loadStats(characterId, out)) {
        LOG_WARN(TAG, "loadStats found no row for character " + idText(characterId));
        return false;
    }
    ProgressionPackets::validateExpBar(out);
    return true;
}

bool ProgressionHandler::buildStatsRefresh(uint32_t characterId, Packet& out) {
    // login burst snapshots the containers so the inbox has to be paid first
    reconcileOnLogin(characterId);

    ProgressionPackets::StatBlock stats;
    if (!loadStats(characterId, stats)) {
        LOG_WARN(TAG, "buildStatsRefresh skipped for character " + idText(characterId) +
                      " so the client keeps its previous exp bar");
        return false;
    }
    out = ProgressionPackets::statsRefresh(stats);
    return true;
}

void ProgressionHandler::pushStats(const Session::Ptr& s,
                                   const ProgressionPackets::StatBlock& stats) {
    if (!s) {
        LOG_WARN(TAG, "pushStats with a null session for character " + idText(stats.playerId));
        return;
    }
    // never broadcast this one the handler writes the recipient own account
    s->send(ProgressionPackets::statsRefresh(stats));
}

void ProgressionHandler::refreshPlayerProgress(Session::Ptr s, GameServer* srv) {
    (void)srv;

    const uint32_t characterId = characterOf(s, "refreshPlayerProgress");
    if (characterId == 0) return;

    ProgressionPackets::StatBlock stats;

    if (ProgressionPackets::resyncExpBounds(characterId, stats)) {
        ProgressionPackets::validateExpBar(stats);
        pushStats(s, stats);
        LOG_INFO(TAG, "refresh character " + idText(characterId) + " level " +
                      std::to_string(stats.levelDisplay) + " wire " +
                      std::to_string(ProgressionPackets::wireLevelFrom(stats.levelDisplay)) +
                      " exp " + std::to_string(stats.expCurrent) + " bar " +
                      std::to_string(stats.expFloor) + " to " + std::to_string(stats.expNext));
        return;
    }

    LOG_WARN(TAG, "resync failed for character " + idText(characterId) +
                  " falling back to the stored bounds");

    if (!loadStats(characterId, stats)) {
        LOG_ERROR(TAG, "refreshPlayerProgress could not read character " + idText(characterId) +
                       " so no 0x000A was sent and the exp bar stays wrong");
        return;
    }
    pushStats(s, stats);
}

void ProgressionHandler::refreshPlayerProgressById(uint32_t characterId, GameServer* srv) {
    if (characterId == 0) {
        LOG_WARN(TAG, "refreshPlayerProgressById with character id zero");
        return;
    }
    if (!srv) {
        LOG_WARN(TAG, "refreshPlayerProgressById with no server for character " +
                      idText(characterId));
        return;
    }

    Session::Ptr s = srv->findSessionByCharacterId(static_cast<int32_t>(characterId));
    if (!s) {
        LOG_WARN(TAG, "character " + idText(characterId) +
                      " is offline so the 0x000A refresh was dropped");
        return;
    }
    refreshPlayerProgress(std::move(s), srv);
}

ProgressionPackets::AwardResult ProgressionHandler::applyAward(uint32_t characterId,
                                                               int32_t expDelta,
                                                               int32_t goldDelta,
                                                               int32_t astroDelta) {
    ProgressionPackets::AwardResult res;

    ensureBooted();

    if (characterId == 0) {
        LOG_WARN(TAG, "applyAward with character id zero");
        return res;
    }
    if (expDelta != 0 && !ProgressionPackets::curveLoaded()) {
        LOG_ERROR(TAG, "exp awarded to character " + idText(characterId) +
                       " with no level curve so level and bar bounds cannot move");
    }

    const bool movesExp   = expDelta != 0;
    const bool movesGold  = goldDelta != 0;
    const bool movesAstro = astroDelta != 0;

    // narrowest wrapper wins so each wallet has one named entry point
    if (movesAstro && !movesExp && !movesGold) {
        res = ProgressionPackets::awardAstro(characterId, astroDelta);
    } else if (movesGold && !movesExp && !movesAstro) {
        res = ProgressionPackets::awardGold(characterId, goldDelta);
    } else if (!movesGold && !movesAstro) {
        // zero delta still relocks the row and puts the ladder back on the curve
        res = ProgressionPackets::awardExp(characterId, expDelta);
    } else if (!movesAstro) {
        res = ProgressionPackets::awardExpAndGold(characterId, expDelta, goldDelta);
    } else {
        res = ProgressionPackets::award(characterId, expDelta, goldDelta, astroDelta);
    }
    if (!res.ok) {
        LOG_ERROR(TAG, "award rejected for character " + idText(characterId) + " exp " +
                       std::to_string(expDelta) + " gold " + std::to_string(goldDelta) +
                       " astro " + std::to_string(astroDelta) + " nothing was written");
        return res;
    }

    if (res.expApplied != expDelta) {
        LOG_WARN(TAG, "exp clamped for character " + idText(characterId) + " asked " +
                      std::to_string(expDelta) + " applied " + std::to_string(res.expApplied));
    }
    if (res.goldApplied != goldDelta) {
        LOG_WARN(TAG, "gold clamped for character " + idText(characterId) + " asked " +
                      std::to_string(goldDelta) + " applied " + std::to_string(res.goldApplied));
    }
    if (res.astroApplied != astroDelta) {
        LOG_WARN(TAG, "astro clamped for character " + idText(characterId) + " asked " +
                      std::to_string(astroDelta) + " applied " + std::to_string(res.astroApplied));
    }

    ProgressionPackets::validateExpBar(res.stats);
    return res;
}

void ProgressionHandler::pushLevelUp(const Session::Ptr& s,
                                     const ProgressionPackets::AwardResult& res) {
    if (!res.leveledUp) return;
    if (!s) {
        LOG_WARN(TAG, "level up for character " + idText(res.stats.playerId) +
                      " had no session so the banner was dropped");
        return;
    }

    // banner reads the level already in the blob so 0x000A must be out first
    s->send(ProgressionPackets::levelUpBanner(LEVELUP_CTX_A, LEVELUP_CTX_B));
    s->send(PacketBuilder::msgLevelUp(s->characterName));
    // level 10 20 30 40 and 50 each carry a pendant
    pushEarnedPendants(s);

    LOG_INFO(TAG, "character " + idText(res.stats.playerId) + " reached level " +
                  std::to_string(res.stats.levelDisplay) + " from " +
                  std::to_string(res.levelBefore));
}

std::vector<uint32_t> ProgressionHandler::grantEarnedPendants(uint32_t characterId) {
    std::vector<uint32_t> granted;
    if (characterId == 0) return granted;
    auto& db = Database::instance();
    auto rows = db.queryPrepared(
        "SELECT level, COALESCE(total_races, 0) AS total_races, COALESCE(license_class, 0) AS grade "
        "FROM characters WHERE id = ? LIMIT 1", {characterId});
    if (rows.empty()) return granted;
    const int64_t level = colI64(rows[0], "level", 1);
    const int64_t races = colI64(rows[0], "total_races", 0);
    const int64_t grade = colI64(rows[0], "grade", 0);

    std::vector<uint32_t> wanted = levelPendantsUpTo(static_cast<uint32_t>(level < 0 ? 0 : level));
    for (uint32_t k : racePendantsFor(static_cast<uint32_t>(races < 0 ? 0 : races))) wanted.push_back(k);
    if (grade >= 1) wanted.push_back(kTutorialPendant);
    // quest level 5 10 15 20 cleared on the scenario menu earn the four pet pendants
    for (const DbRow& r : db.queryPrepared(
             "SELECT scenario_key FROM scenario_key_progress WHERE character_id = ? AND completed = 1",
             {characterId})) {
        const uint32_t key = questPendantFor(static_cast<uint32_t>(rowInt64NoThrow(r, "scenario_key", 0)));
        if (key != 0) wanted.push_back(key);
    }
    // the five missions of chapter 1 cleared earn the hidden pendant 13
    std::vector<uint32_t> clearedMissions;
    for (const DbRow& r : db.queryPrepared(
             "SELECT mission_id FROM char_mission_progress WHERE char_id = ? AND cleared = 1",
             {characterId})) {
        clearedMissions.push_back(static_cast<uint32_t>(rowInt64NoThrow(r, "mission_id", 0)));
    }
    if (missionChapterOneCleared(clearedMissions)) wanted.push_back(kMissionChapterOnePendant);
    if (wanted.empty()) return granted;

    auto owned = db.queryPrepared("SELECT pendant_key FROM owned_pendant WHERE character_id = ?",
                                  {characterId});
    // a hidden row still takes an owned row the box counts it and the 0x11A append finds its def
    auto defs = db.queryPrepared("SELECT pendant_key FROM pendant_def", {});
    auto has = [](const std::vector<DbRow>& list, uint32_t key) {
        for (const DbRow& r : list)
            if (static_cast<uint32_t>(rowInt64NoThrow(r, "pendant_key", 0)) == key) return true;
        return false;
    };
    for (uint32_t key : wanted) {
        if (has(owned, key) || !has(defs, key)) continue;
        if (db.executePrepared("INSERT IGNORE INTO owned_pendant (character_id, pendant_key) VALUES (?, ?)",
                               {characterId, key})) {
            granted.push_back(key);
            LOG_INFO(TAG, "character " + idText(characterId) + " earned pendant " + std::to_string(key));
        }
    }
    return granted;
}

void ProgressionHandler::pushEarnedPendants(const Session::Ptr& s) {
    if (!s || s->characterId == 0) return;
    for (uint32_t key : grantEarnedPendants(s->characterId)) {
        // sub 47E880 appends the instance and key row the live grant rides 0x011B
        s->send(PacketBuilder::entitySimple(true, static_cast<int32_t>(pendantInstanceId(key)),
                                            static_cast<int32_t>(key)));
    }
}

ProgressionPackets::AwardResult ProgressionHandler::awardAndPush(Session::Ptr s,
                                                                 int32_t expDelta,
                                                                 int32_t goldDelta,
                                                                 int32_t astroDelta,
                                                                 GameServer* srv) {
    (void)srv;

    ProgressionPackets::AwardResult res;

    const uint32_t characterId = characterOf(s, "awardAndPush");
    if (characterId == 0) return res;

    res = applyAward(characterId, expDelta, goldDelta, astroDelta);
    if (!res.ok) return res;

    pushStats(s, res.stats);
    pushLevelUp(s, res);

    // premium wallet also has its own standalone carrier
    if (res.astroApplied != 0) {
        s->send(ProgressionPackets::astroBalance(res.stats.astro));
    }
    return res;
}

void ProgressionHandler::awardRace(Session::Ptr s, int32_t gold, int32_t exp, GameServer* srv) {
    const uint32_t characterId = characterOf(s, "awardRace");
    if (characterId == 0) return;

    if (exp == 0 && gold == 0) {
        LOG_WARN(TAG, "awardRace for character " + idText(characterId) +
                      " carried no exp and no gold so only the refresh is useful");
    }

    ProgressionPackets::AwardResult res = awardAndPush(s, exp, gold, 0, srv);
    if (!res.ok) {
        LOG_ERROR(TAG, "race payout lost for character " + idText(characterId) + " gold " +
                       std::to_string(gold) + " exp " + std::to_string(exp));
        // client already drew its predicted numbers so put the truth back
        refreshPlayerProgress(std::move(s), srv);
        return;
    }

    LOG_INFO(TAG, "race payout character " + idText(characterId) + " gold " +
                  std::to_string(res.goldApplied) + " exp " + std::to_string(res.expApplied) +
                  " level " + std::to_string(res.stats.levelDisplay));
}

bool ProgressionHandler::grantTableReady() {
    {
        std::lock_guard<std::mutex> lock(g_grantMutex);
        if (g_grantProbed) return g_grantPresent;
    }

    auto rows = Database::instance().queryPrepared(
        "SELECT COUNT(*) AS n FROM information_schema.tables "
        "WHERE table_schema = DATABASE() AND table_name = 'progression_grant'", {});

    const bool present = !rows.empty() && colI64(rows[0], "n", 0) > 0;

    {
        std::lock_guard<std::mutex> lock(g_grantMutex);
        g_grantProbed = true;
        g_grantPresent = present;
    }

    if (!present) {
        LOG_WARN(TAG, "progression_grant is missing so every out of band award stays "
                      "invisible to an online client until the migration is applied");
    } else {
        LOG_INFO(TAG, "progression_grant inbox is available");
    }
    return present;
}

std::vector<ProgressionHandler::PendingGrant>
ProgressionHandler::pendingGrants(uint32_t characterId) {
    std::vector<PendingGrant> out;
    if (characterId == 0 || !grantTableReady()) return out;

    // limit is a compile time constant so it is inlined not bound
    auto rows = Database::instance().queryPrepared(
        "SELECT id, exp_delta, gold_delta, astro_delta, source FROM progression_grant "
        "WHERE character_id = ? AND state = 'pending' ORDER BY id ASC LIMIT " +
            std::to_string(GRANT_DRAIN_LIMIT),
        {static_cast<int32_t>(characterId)});

    out.reserve(rows.size());
    for (const auto& r : rows) {
        PendingGrant g;
        g.id         = static_cast<uint64_t>(colI64(r, "id", 0));
        g.expDelta   = colI32(r, "exp_delta", 0);
        g.goldDelta  = colI32(r, "gold_delta", 0);
        g.astroDelta = colI32(r, "astro_delta", 0);
        g.source     = colStr(r, "source");
        if (g.id != 0) out.push_back(std::move(g));
    }
    return out;
}

bool ProgressionHandler::queueGrant(uint32_t characterId, int32_t expDelta, int32_t goldDelta,
                                    int32_t astroDelta, const std::string& source) {
    if (characterId == 0) {
        LOG_WARN(TAG, "queueGrant with character id zero");
        return false;
    }
    if (expDelta == 0 && goldDelta == 0 && astroDelta == 0) {
        LOG_WARN(TAG, "queueGrant for character " + idText(characterId) +
                      " carried no delta so nothing was queued");
        return false;
    }
    if (!grantTableReady()) {
        LOG_ERROR(TAG, "grant for character " + idText(characterId) + " exp " +
                       std::to_string(expDelta) + " gold " + std::to_string(goldDelta) +
                       " astro " + std::to_string(astroDelta) +
                       " is LOST because progression_grant does not exist");
        return false;
    }

    const bool ok = Database::instance().executePrepared(
        "INSERT INTO progression_grant (character_id, exp_delta, gold_delta, astro_delta, "
        "source, state) VALUES (?, ?, ?, ?, ?, 'pending')",
        {static_cast<int32_t>(characterId), expDelta, goldDelta, astroDelta, source});

    if (!ok) {
        LOG_ERROR(TAG, "grant insert failed for character " + idText(characterId));
        return false;
    }
    LOG_INFO(TAG, "grant queued for character " + idText(characterId) + " exp " +
                  std::to_string(expDelta) + " gold " + std::to_string(goldDelta) +
                  " astro " + std::to_string(astroDelta) + " source " + source);
    return true;
}

int32_t ProgressionHandler::drainPendingGrants(uint32_t characterId) {
    const std::vector<PendingGrant> queued = pendingGrants(characterId);
    if (queued.empty()) return 0;

    int32_t applied = 0;

    for (const auto& g : queued) {
        // claim first else a second login pays the same row again
        auto tx = Database::instance().beginTransaction();
        if (!tx.valid()) {
            LOG_ERROR(TAG, "grant claim has no transaction for character " +
                           idText(characterId));
            break;
        }
        if (!tx.execute("UPDATE progression_grant SET state = 'claimed' "
                        "WHERE id = ? AND state = 'pending'",
                        {static_cast<int64_t>(g.id)})) {
            LOG_ERROR(TAG, "grant claim failed for row " + std::to_string(g.id));
            continue;
        }
        if (tx.affectedRows() != 1) {
            LOG_WARN(TAG, "grant row " + std::to_string(g.id) +
                          " was already taken by another drain");
            continue;
        }
        if (!tx.commit()) {
            LOG_ERROR(TAG, "grant claim commit failed for row " + std::to_string(g.id));
            continue;
        }

        const ProgressionPackets::AwardResult res =
            applyAward(characterId, g.expDelta, g.goldDelta, g.astroDelta);

        if (!res.ok) {
            // release else the row is eaten without ever being paid
            Database::instance().executePrepared(
                "UPDATE progression_grant SET state = 'pending' WHERE id = ? AND state = 'claimed'",
                {static_cast<int64_t>(g.id)});
            LOG_ERROR(TAG, "grant row " + std::to_string(g.id) + " for character " +
                           idText(characterId) + " could not be applied and was released");
            continue;
        }

        if (!Database::instance().executePrepared(
                "UPDATE progression_grant SET state = 'applied', applied_at = NOW() "
                "WHERE id = ?",
                {static_cast<int64_t>(g.id)})) {
            LOG_ERROR(TAG, "grant row " + std::to_string(g.id) +
                           " was paid but could not be marked applied, it stays claimed and "
                           "will never be paid twice");
        }

        ++applied;
        LOG_INFO(TAG, "grant row " + std::to_string(g.id) + " paid to character " +
                      idText(characterId) + " exp " + std::to_string(res.expApplied) +
                      " gold " + std::to_string(res.goldApplied) + " astro " +
                      std::to_string(res.astroApplied) + " source " + g.source);
    }

    if (applied >= GRANT_DRAIN_LIMIT) {
        LOG_WARN(TAG, "grant drain for character " + idText(characterId) +
                      " hit the per pass limit so more rows wait for the next drain");
    }
    return applied;
}

int32_t ProgressionHandler::drainPendingGrantsAndPush(Session::Ptr s, GameServer* srv) {
    (void)srv;

    const uint32_t characterId = characterOf(s, "drainPendingGrantsAndPush");
    if (characterId == 0) return 0;

    // cheap indexed probe keeps the empty inbox off the read and push path
    if (pendingGrants(characterId).empty()) return 0;

    ProgressionPackets::StatBlock before;
    const bool hadBefore = loadStats(characterId, before);

    const int32_t applied = drainPendingGrants(characterId);
    if (applied == 0) return 0;

    ProgressionPackets::StatBlock stats;
    if (!loadStats(characterId, stats)) {
        LOG_ERROR(TAG, "grants paid to character " + idText(characterId) +
                       " but the row could not be read back so no 0x000A was sent");
        return applied;
    }

    pushStats(s, stats);

    if (hadBefore && stats.levelDisplay > before.levelDisplay) {
        ProgressionPackets::AwardResult moved;
        moved.ok = true;
        moved.stats = stats;
        moved.levelBefore = before.levelDisplay;
        moved.levelsGained = stats.levelDisplay - before.levelDisplay;
        moved.leveledUp = true;
        // 0x000A already went out so the banner reads the fresh level
        pushLevelUp(s, moved);
    }

    // premium wallet also has its own standalone carrier
    s->send(ProgressionPackets::astroBalance(stats.astro));
    return applied;
}

bool ProgressionHandler::awardOffline(uint32_t characterId, int32_t expDelta, int32_t goldDelta,
                                      int32_t astroDelta, const std::string& source,
                                      GameServer* srv) {
    if (characterId == 0) {
        LOG_WARN(TAG, "awardOffline with character id zero");
        return false;
    }

    Session::Ptr s = srv ? srv->findSessionByCharacterId(static_cast<int32_t>(characterId))
                         : nullptr;
    if (s) {
        const ProgressionPackets::AwardResult res =
            awardAndPush(std::move(s), expDelta, goldDelta, astroDelta, srv);
        if (res.ok) return true;
        LOG_WARN(TAG, "online award failed for character " + idText(characterId) +
                      " so it falls back to the inbox");
    }
    return queueGrant(characterId, expDelta, goldDelta, astroDelta, source);
}

void ProgressionHandler::reconcileOnLogin(uint32_t characterId) {
    if (characterId == 0) return;

    ensureBooted();

    const int32_t paid = drainPendingGrants(characterId);
    if (paid > 0) {
        LOG_INFO(TAG, "login drained " + std::to_string(paid) +
                      " queued grants for character " + idText(characterId));
    }

    // other handlers add exp with raw sql and never move level floor or next
    const ProgressionPackets::AwardResult res = applyAward(characterId, 0, 0, 0);
    if (!res.ok) {
        LOG_WARN(TAG, "login reconcile could not relock character " + idText(characterId) +
                      " so the stored exp bar is used as is");
        return;
    }
    if (res.leveledUp) {
        LOG_INFO(TAG, "login reconcile lifted character " + idText(characterId) +
                      " from level " + std::to_string(res.levelBefore) + " to " +
                      std::to_string(res.stats.levelDisplay) +
                      " after out of band exp writes");
        return;
    }
    LOG_DEBUG(TAG, "login reconcile character " + idText(characterId) + " level " +
                   std::to_string(res.stats.levelDisplay) + " bar " +
                   std::to_string(res.stats.expFloor) + " to " +
                   std::to_string(res.stats.expNext));
}

bool ProgressionHandler::spendGold(Session::Ptr s, int32_t cost, GameServer* srv) {
    (void)srv;

    const uint32_t characterId = characterOf(s, "spendGold");
    if (characterId == 0) return false;

    ProgressionPackets::StatBlock stats;
    if (!ProgressionPackets::spendGold(characterId, cost, stats)) {
        LOG_WARN(TAG, "gold spend refused for character " + idText(characterId) + " cost " +
                      std::to_string(cost));
        return false;
    }

    pushStats(s, stats);
    return true;
}

bool ProgressionHandler::spendAstro(Session::Ptr s, int32_t cost, GameServer* srv) {
    (void)srv;

    const uint32_t characterId = characterOf(s, "spendAstro");
    if (characterId == 0) return false;

    ProgressionPackets::StatBlock stats;
    if (!ProgressionPackets::spendAstro(characterId, cost, stats)) {
        LOG_WARN(TAG, "astro spend refused for character " + idText(characterId) + " cost " +
                      std::to_string(cost));
        return false;
    }

    pushStats(s, stats);
    s->send(ProgressionPackets::astroBalance(stats.astro));
    return true;
}

void ProgressionHandler::pushAstroBalance(Session::Ptr s) {
    const uint32_t characterId = characterOf(s, "pushAstroBalance");
    if (characterId == 0) return;

    ProgressionPackets::StatBlock stats;
    if (!loadStats(characterId, stats)) {
        LOG_WARN(TAG, "astro balance unknown for character " + idText(characterId) +
                      " so no 0x00D0 was sent");
        return;
    }
    s->send(ProgressionPackets::astroBalance(stats.astro));
}

void ProgressionHandler::handleAstroPoll(Session::Ptr s, Packet& packet, GameServer* srv) {
    (void)packet;
    // balance is a server fact the polled name and extra string are ignored this poll is the only periodic beat
    if (drainPendingGrantsAndPush(s, srv) > 0) return;
    pushAstroBalance(std::move(s));
}

bool ProgressionHandler::setLicenceGrade(Session::Ptr s, uint8_t grade, GameServer* srv) {
    (void)srv;

    const uint32_t characterId = characterOf(s, "setLicenceGrade");
    if (characterId == 0) return false;

    if (grade > ProgressionPackets::LICENCE_GRADE_MAX) {
        LOG_WARN(TAG, "licence grade " + std::to_string(grade) + " for character " +
                      idText(characterId) + " above the safe max and will be clamped");
    }

    if (!ProgressionPackets::setLicenceGrade(characterId, grade)) {
        LOG_ERROR(TAG, "licence grade write failed for character " + idText(characterId));
        return false;
    }

    ProgressionPackets::StatBlock stats;
    if (!loadStats(characterId, stats)) {
        LOG_WARN(TAG, "licence grade saved but character " + idText(characterId) +
                      " could not be read back so no 0x000A was sent");
        return true;
    }
    pushStats(s, stats);
    return true;
}

void ProgressionHandler::sendAchievementReward(Session::Ptr s,
                                               const ProgressionPackets::AchievementReward& reward,
                                               GameServer* srv) {
    const uint32_t characterId = characterOf(s, "sendAchievementReward");
    if (characterId == 0) return;

    s->send(ProgressionPackets::achievementReward(reward));
    // 0x00F8 carries no floor and no next so the bar needs the refresh
    refreshPlayerProgress(std::move(s), srv);
}

void ProgressionHandler::sendRewardPopup(Session::Ptr s,
                                         const ProgressionPackets::RewardPopup& popup) {
    if (!s) {
        LOG_WARN(TAG, "sendRewardPopup with a null session");
        return;
    }
    if (!ProgressionPackets::validatePopupBlob(popup.rewardKind, popup.blob)) {
        LOG_WARN(TAG, "reward popup kind " + std::to_string(popup.rewardKind) + " blob is " +
                      std::to_string(popup.blob.size()) + " bytes but the handler reads " +
                      std::to_string(ProgressionPackets::popupBlobSize(popup.rewardKind)) +
                      " so the client would read the next frame");
    }
    s->send(ProgressionPackets::rewardPopup(popup));
}

bool ProgressionHandler::fillLoginProfile(uint32_t characterId,
                                          CharCreatePackets::LoginProfile& out) {
    // blob tail carries level exp floor and next so repair before reading
    reconcileOnLogin(characterId);

    ProgressionPackets::StatBlock stats;
    if (!loadStats(characterId, stats)) {
        LOG_ERROR(TAG, "login profile for character " + idText(characterId) +
                       " keeps its default exp fields so the bar shows a zero denominator");
        return false;
    }
    ProgressionPackets::fillLoginProfile(stats, out);
    return true;
}

int32_t ProgressionHandler::quickJoinMode(uint32_t characterId) {
    ProgressionPackets::StatBlock stats;
    if (!loadStats(characterId, stats)) {
        LOG_WARN(TAG, "quick join mode for character " + idText(characterId) +
                      " unknown so the gated mode is reported");
        return ProgressionPackets::QUICKJOIN_MODE_GATED;
    }
    return ProgressionPackets::quickJoinMode(stats.licenceGrade,
                                             ProgressionPackets::wireLevel(stats));
}

bool ProgressionHandler::advancedLicenceUnlocked(uint32_t characterId) {
    ProgressionPackets::StatBlock stats;
    if (!loadStats(characterId, stats)) {
        LOG_WARN(TAG, "advanced licence gate for character " + idText(characterId) +
                      " unknown so it stays locked");
        return false;
    }
    return ProgressionPackets::advancedLicenceUnlocked(stats.licenceGrade,
                                                       ProgressionPackets::wireLevel(stats));
}

bool ProgressionHandler::masterLicenceUnlocked(uint32_t characterId) {
    ProgressionPackets::StatBlock stats;
    if (!loadStats(characterId, stats)) {
        LOG_WARN(TAG, "master licence gate for character " + idText(characterId) +
                      " unknown so it stays locked");
        return false;
    }
    return ProgressionPackets::masterLicenceUnlocked(stats.licenceGrade,
                                                     ProgressionPackets::wireLevel(stats));
}

} // namespace knc
