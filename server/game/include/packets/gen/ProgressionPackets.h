/// sub 429990 prints exp from the blob so push S2C 0x000A unprompted on any exp level grade or wallet change

#pragma once
#include "net/Packet.h"
#include "packets/gen/CharCreatePackets.h"
#include "packets/gen/MissionPackets.h"

#include <cstdint>
#include <string>
#include <vector>

namespace knc {

/// progression domain the level curve exp and gold awards and the two wallets
struct ProgressionPackets {

    static constexpr uint16_t OP_S_STATS_REFRESH      = 0x000A;  ///< statsRefresh 38 bytes server to client only astroBalance 4 bytes astro only
    static constexpr uint16_t OP_S_ASTRO_BALANCE      = 0x00D0;
    static constexpr uint16_t OP_S_ACHIEVEMENT_REWARD = 0x00F8;  ///< achievementReward 13 or 21 bytes plus blob rewardPopup 28 bytes plus optional blob
    static constexpr uint16_t OP_S_REWARD_POPUP       = 0x0135;

    static constexpr size_t STATS_REFRESH_SIZE     = 38;  ///< 0x26 packed no padding
    static constexpr size_t ASTRO_BALANCE_SIZE     = 4;
    static constexpr size_t ACHIEVEMENT_BASE_SIZE  = 13;  ///< baseSize kind flag and u64 id paidSize plus gold and exp kind 2 or 3 popupSize 0x1C before any blob
    static constexpr size_t ACHIEVEMENT_PAID_SIZE  = 21;
    static constexpr size_t REWARD_POPUP_SIZE      = 28;

    /// characters level and level curve level are 1 based the wire byte is 0 based
    static constexpr int32_t DB_LEVEL_MIN = 1;
    /// sub 4425F0 loads 50 icon pairs sub 443420 clamps the index at 0x31
    static constexpr int8_t  WIRE_LEVEL_MAX_RENDER = 49;
    /// levels 50 to 54 still send drawing Lv icon 050 and a pinned bar
    static constexpr int8_t  WIRE_LEVEL_MAX_SEND = 54;
    /// MOVSX plus the sub 443420 branch means a negative level draws the GM crown
    static constexpr int8_t  WIRE_LEVEL_MIN = 0;
    /// grade counts licences passed the tab picker only builds 3 widget arrays
    static constexpr uint8_t LICENCE_GRADE_MAX = 3;
    /// sub 437190 and sub 437820 gate the advanced row on this wire level
    static constexpr int32_t LICENCE_ADV_MIN_WIRE_LEVEL = 10;
    /// same two functions gate the master row
    static constexpr int32_t LICENCE_MASTER_MIN_WIRE_LEVEL = 40;
    /// every exp field is read with FILD and printed with %d so it stays signed
    static constexpr int32_t EXP_MAX = 0x7FFFFFFF;
    /// sub 405D60 quick join mode when the licence gate passes
    static constexpr int32_t QUICKJOIN_MODE_OPEN = 14;
    /// sub 405D60 quick join mode when it does not
    static constexpr int32_t QUICKJOIN_MODE_GATED = 8;

    static constexpr int32_t SRC_LEVEL_UP = -700;  ///< levelUp reads the level ALREADY in the blob latch800 sets byte D09E84 latch900 sets byte C70A3C neither chased
    static constexpr int32_t SRC_LATCH_800 = -800;
    static constexpr int32_t SRC_LATCH_900 = -900;

    /// every field S2C 0x000A carries wireLevel handles the clamping so never write levelDisplay to the wire
    struct StatBlock {
        uint32_t playerId = 0;                    ///< playerId is characters id not on the 0x000A wire licenceGrade wire 0x00 characters license class 0 to 3
        uint8_t  licenceGrade = 0;
        int32_t  levelDisplay = DB_LEVEL_MIN;     ///< levelDisplay is characters level 1 based expCurrent wire 0x02 cumulative lifetime exp
        int32_t  expCurrent = 0;
        int32_t  astro = 0;                       ///< astro wire 0x06 characters cash premium gold wire 0x0A characters gold earned
        int32_t  gold = 0;
        int32_t  characterInvId = -1;             ///< characterInvId wire 0x0E owned character id not a base key kartInvId wire 0x12 owned kart id
        int32_t  kartInvId = -1;
        uint8_t  staffFlag = 0;                   ///< staffFlag wire 0x17 accounts role only 0 is safe expFloor wire 0x1A cumulative exp at start of level
        int32_t  expFloor = 0;
        int32_t  expNext = 1;                     ///< expNext wire 0x1E cumulative exp of next level pendantKey wire 0x22 zero draws nothing
        int32_t  pendantKey = 0;
    };

    /// one row of the level curve table
    struct CurveRow {
        int32_t level = 0;    ///< level is 1 based same base as characters level cumExp is cumulative exp at start of that level
        int32_t cumExp = 0;
    };

    /// outcome of one transactional award
    struct AwardResult {
        bool     ok = false;          ///< false means database rejected nothing written stats is post award snapshot safe to feed statsRefresh
        StatBlock stats;
        int32_t  levelBefore = 0;     ///< levelBefore is 1 based levelsGained is 0 when no level changed
        int32_t  levelsGained = 0;
        int32_t  expApplied = 0;      ///< after clamping may differ from the requested delta
        int32_t  goldApplied = 0;
        int32_t  astroApplied = 0;
        bool     leveledUp = false;   ///< send S2C 0x0135 with SRC LEVEL UP only when true
    };

    /// S2C 0x0135 reward popup push the new level with 0x003C or 0x000A first or the banner shows the old number
    struct RewardPopup {
        uint32_t unused0 = 0;      ///< unused0 offset 0x00 read into local nothing consumes sourceCode offset 0x04 tail switch
        int32_t  sourceCode = 0;
        uint32_t ctxA = 0;         ///< ctxA offset 0x08 stored to dword 11B44E8 ctxB offset 0x0C stored to dword 11B450C
        int32_t  ctxB = 0;
        int32_t  rewardKind = 4;   ///< rewardKind offset 0x10 picks trailing blob 4 and 7 read nothing unused14 offset 0x14 never consumed
        uint32_t unused14 = 0;
        uint32_t unused18 = 0;     ///< unused18 offset 0x18 never consumed blob must be exactly popupBlobSize of rewardKind
        std::vector<uint8_t> blob;
    };

    /// S2C 0x00F8 achievement reward gold and exp only reach the wire on kind 2 or 3 follow with statsRefresh
    struct AchievementReward {
        int32_t  kind = 0;               ///< kind offset 0x00 2 and 3 carry wallet pair flag offset 0x04 lands 0xC70A34 unread success via client result byte
        uint8_t  flag = 0;
        uint64_t achievementId = 0;      ///< achievementId offset 0x05 key into client achievement table goldAfter offset 0x0D kind 2 or 3 only
        int32_t  goldAfter = 0;
        int32_t  expAfter = 0;           ///< expAfter offset 0x11 kind 2 or 3 only item offset 0x15 kind 3 only type from client table
        MissionPackets::RewardBlob item;
        bool     hasItem = false;
    };

    /// raw read of level curve ordered by level no cache no validation
    static std::vector<CurveRow> loadLevelCurveRows();

    /// re-reads level curve into the cache rejects an empty table or a level below DB LEVEL MIN
    static bool reloadLevelCurve();

    /// true when a validated curve is cached
    static bool curveLoaded();

    /// cached row count zero when the curve never loaded
    static size_t curveSize();

    /// highest level present in the curve DB LEVEL MIN when empty
    static int32_t maxCurveLevel();

    /// cumulative exp at the start of a level zero when unknown
    static int32_t cumExpForLevel(int32_t levelDisplay);

    /// highest level whose cum exp is still at or below exp
    static int32_t levelForExp(int32_t exp);

    /// the floor and next pair the client bar needs next is always a total not a delta
    static bool expBoundsForLevel(int32_t levelDisplay, int32_t& floorOut, int32_t& nextOut);

    /// reads every 0x000A field for one character false when missing
    static bool loadStats(uint32_t characterId, StatBlock& out);

    /// clamps grade level and wallets so a broken row can never divide by zero on the client
    static void sanitize(StatBlock& stats);

    /// the 0 based signed byte the wire wants clamped to 0 through 54
    static int8_t wireLevel(const StatBlock& stats);

    /// same conversion for a bare 1 based level
    static int8_t wireLevelFrom(int32_t levelDisplay);

    /// true when floor is less than next and floor is at or below exp
    static bool validateExpBar(const StatBlock& stats);

    /// recomputes level floor and next from the curve and persists them
    static bool resyncExpBounds(uint32_t characterId, StatBlock& out);

    /// adds exp and gold recomputes level and bar bounds in one transaction with one row lock
    static AwardResult awardExpAndGold(uint32_t characterId, int32_t expDelta, int32_t goldDelta);

    /// exp only
    static AwardResult awardExp(uint32_t characterId, int32_t expDelta);

    /// gold only no level recomputation
    static AwardResult awardGold(uint32_t characterId, int32_t goldDelta);

    /// astro only the premium wallet behind the 60 second S2C 0x00D0 poll
    static AwardResult awardAstro(uint32_t characterId, int32_t astroDelta);

    /// exp gold and astro in one transaction
    static AwardResult award(uint32_t characterId, int32_t expDelta,
                             int32_t goldDelta, int32_t astroDelta);

    /// takes gold only if the balance covers it the client never checks affordability
    static bool spendGold(uint32_t characterId, int32_t cost, StatBlock& out);

    /// same for astro
    static bool spendAstro(uint32_t characterId, int32_t cost, StatBlock& out);

    /// persists a licence grade clamped to 0 through 3 pair it with S2C 0x00A4
    static bool setLicenceGrade(uint32_t characterId, uint8_t grade);

    /// exp only ever grows a shrinking total keeps the old level
    static constexpr bool LEVEL_NEVER_DECREASES = true;

    /// S2C 0x000A stat refresh 38 bytes unicast to the owner only safe at any stage
    static Packet statsRefresh(const StatBlock& stats);

    /// S2C 0x00D0 astro balance 4 bytes the only field it writes
    static Packet astroBalance(int32_t astro);

    /// S2C 0x0135 reward popup 28 bytes plus the typed blob
    static Packet rewardPopup(const RewardPopup& popup);

    /// S2C 0x0135 with SRC LEVEL UP and no trailing blob send the new level first
    static Packet levelUpBanner(uint32_t ctxA, int32_t ctxB);

    /// S2C 0x00F8 achievement reward
    static Packet achievementReward(const AchievementReward& reward);

    /// copies progression fields into a login profile so the blob and the 0x000A refresh always agree
    static void fillLoginProfile(const StatBlock& stats,
                                 CharCreatePackets::LoginProfile& out);

    /// sub 47EEF0 bytes read for a reward kind kind 4 and 7 read nothing here
    static size_t popupBlobSize(int32_t kind);

    /// true when bytes match what the popup handler will read
    static bool validatePopupBlob(int32_t kind, const std::vector<uint8_t>& bytes);

    /// sub 405D60 quick join mode wireLevel is the 0 based byte
    static int32_t quickJoinMode(uint8_t licenceGrade, int32_t wireLevelValue);

    /// sub 437820 padlock state of the advanced licence row
    static bool advancedLicenceUnlocked(uint8_t licenceGrade, int32_t wireLevelValue);

    /// sub 437820 padlock state of the master licence row
    static bool masterLicenceUnlocked(uint8_t licenceGrade, int32_t wireLevelValue);
};

} // namespace knc
