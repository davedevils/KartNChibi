/// S2C 0x000A is 38 bytes grade level exp astro gold ids role floor next pendant

#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include "packets/gen/ProgressionPackets.h"
#include "packets/gen/CharCreatePackets.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace knc {

class GameServer;

/// every method is static and safe to call anywhere like other handlers
class ProgressionHandler {
public:

    /// init loads and validates level curve once at server start and logs failures
    static bool init();

    /// reloads level curve after an admin edit using the same validation as init
    static bool reloadCurve();

    /// true when a validated curve is cached
    static bool curveReady();

    /// audits level curve straight from the DB bypassing the process cache
    static bool auditCurveTable();

    /// level 50 is the client draw cap per sub 443420 FUN 004425F0 and FUN 00429990
    static constexpr int32_t CLIENT_LEVEL_ICON_MAX =
        ProgressionPackets::DB_LEVEL_MIN +
        static_cast<int32_t>(ProgressionPackets::WIRE_LEVEL_MAX_RENDER);

    // FUN 00429990 and FUN 0047D3B0 prove the client never derives level from exp itself

    /// highest level whose cum exp is at or below the given exp
    static int32_t levelForExp(int64_t exp);

    /// cumulative exp at the start of a level or 0 when unknown
    static int32_t expFloorForLevel(int32_t lvl);

    /// cumulative exp opening the next level always above the floor
    static int32_t expNextForLevel(int32_t lvl);

    /// highest level present in level curve which is the real cap
    static int32_t levelCap();

    /// number of cached curve rows 0 when the curve never loaded
    static size_t curveRows();

    /// sanitized snapshot of every S2C 0x000A field false when missing
    static bool loadStats(uint32_t characterId, ProgressionPackets::StatBlock& out);

    /// builds the 38 byte S2C 0x000A without sending it for batched login
    static bool buildStatsRefresh(uint32_t characterId, Packet& out);

    /// recomputes level floor and next then persists and pushes 0x000A
    static void refreshPlayerProgress(Session::Ptr s, GameServer* srv);

    /// same as refreshPlayerProgress but finds the session by character id
    static void refreshPlayerProgressById(uint32_t characterId, GameServer* srv);

    /// unicasts one stat block as S2C 0x000A sanitized by the builder
    static void pushStats(const Session::Ptr& s, const ProgressionPackets::StatBlock& stats);

    /// race payout applies exp and gold then pushes 0x000A and the banner
    static void awardRace(Session::Ptr s, int32_t gold, int32_t exp, GameServer* srv);

    /// push order is 0x000A then 0x0135 level up then 0x00CE and 0x00D0
    static ProgressionPackets::AwardResult awardAndPush(Session::Ptr s,
                                                        int32_t expDelta,
                                                        int32_t goldDelta,
                                                        int32_t astroDelta,
                                                        GameServer* srv);

    /// DB only award for callers that must interleave their own packets
    static ProgressionPackets::AwardResult applyAward(uint32_t characterId,
                                                      int32_t expDelta,
                                                      int32_t goldDelta,
                                                      int32_t astroDelta);

    /// sends 0x0135 level up and 0x00CE only when the level moved
    static void pushLevelUp(const Session::Ptr& s,
                            const ProgressionPackets::AwardResult& res);

    /// the level race count and rookie grade earn pendants inserts missing owned rows and returns their keys no packet sent
    static std::vector<uint32_t> grantEarnedPendants(uint32_t characterId);

    /// the same then one S2C 0x011B per new pendant so the popup grid opens it at once
    static void pushEarnedPendants(const Session::Ptr& s);

    /// one undrained progression grant row
    struct PendingGrant {
        uint64_t    id = 0;
        int32_t     expDelta = 0;
        int32_t     goldDelta = 0;
        int32_t     astroDelta = 0;
        std::string source;
    };

    /// queues an award for a character the server cannot reach right now
    static bool queueGrant(uint32_t characterId, int32_t expDelta, int32_t goldDelta,
                           int32_t astroDelta, const std::string& source);

    /// undrained rows for one character empty when the table is absent
    static std::vector<PendingGrant> pendingGrants(uint32_t characterId);

    /// applies every undrained grant once claimed so two logins cannot pay twice
    static int32_t drainPendingGrants(uint32_t characterId);

    /// drains grants then pushes 0x000A the level banner and astro if moved
    static int32_t drainPendingGrantsAndPush(Session::Ptr s, GameServer* srv);

    /// awards now if online otherwise queues the grant safely
    static bool awardOffline(uint32_t characterId, int32_t expDelta, int32_t goldDelta,
                             int32_t astroDelta, const std::string& source, GameServer* srv);

    /// drains the inbox then repairs the stored level floor and next
    static void reconcileOnLogin(uint32_t characterId);

    /// server side gold price gate that pushes 0x000A on success
    static bool spendGold(Session::Ptr s, int32_t cost, GameServer* srv);

    /// server side astro price gate that pushes 0x000A then 0x00D0
    static bool spendAstro(Session::Ptr s, int32_t cost, GameServer* srv);

    /// pushes S2C 0x00D0 astro balance only
    static void pushAstroBalance(Session::Ptr s);

    /// answers the C2S 0x00D0 astro poll reading the balance from the DB
    static void handleAstroPoll(Session::Ptr s, Packet& packet, GameServer* srv);

    /// persists a licence grade and pushes 0x000A pairing with S2C 0x00A4 elsewhere
    static bool setLicenceGrade(Session::Ptr s, uint8_t grade, GameServer* srv);

    /// sends S2C 0x00F8 reward then the mandatory trailing 0x000A
    static void sendAchievementReward(Session::Ptr s,
                                      const ProgressionPackets::AchievementReward& reward,
                                      GameServer* srv);

    /// sends S2C 0x0135 reward popup after validating the blob size
    static void sendRewardPopup(Session::Ptr s, const ProgressionPackets::RewardPopup& popup);

    /// fills progression fields of a 0x0007 or 0x00A7 login profile
    static bool fillLoginProfile(uint32_t characterId,
                                 CharCreatePackets::LoginProfile& out);

    /// sub 405D60 quick join mode for a character where 14 is open and 8 gated
    static int32_t quickJoinMode(uint32_t characterId);

    /// padlock state of the advanced licence row
    static bool advancedLicenceUnlocked(uint32_t characterId);

    /// padlock state of the master licence row
    static bool masterLicenceUnlocked(uint32_t characterId);

private:
    /// character id a session owns 0 means not logged in yet
    static uint32_t characterOf(const Session::Ptr& s, const char* what);

    /// runs init once every public entry point calls it
    static bool ensureBooted();

    /// probes progression grant once so a missing table cannot spam the log
    static bool grantTableReady();
};

} // namespace knc
