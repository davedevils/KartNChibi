/// race finish results send order is 0x3C then relayed 0x58 then 0x3D then 0x46 built per connection last

#pragma once
#include "net/Packet.h"
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

struct ResultsPackets {

    /// bonus consumable keys the scoreboard draws an icon for
    static constexpr uint32_t BONUS_NONE     = 0;
    static constexpr uint32_t BONUS_GOLD_UP  = 4001;
    static constexpr uint32_t BONUS_EXP_UP   = 4002;
    static constexpr uint32_t BONUS_TOTAL_UP = 4003;

    /// driver anim states observed on the wire
    static constexpr uint8_t ANIM_SPIN    = 3;
    static constexpr uint8_t ANIM_RECOVER = 6;
    static constexpr uint8_t ANIM_HIT     = 7;
    static constexpr uint8_t ANIM_STATE_8 = 8;
    static constexpr uint8_t ANIM_FINISH  = 9;

    /// board holds 30 rows and forces a name NUL at unit 12
    static constexpr int32_t MAX_ROWS       = 30;
    static constexpr size_t  MAX_NAME_UNITS = 12;

    /// rank zero is the winner and this value means retire
    static constexpr int32_t RANK_RETIRE = -1;

    /// client docks itself these on leave race when more than one car runs
    static constexpr int32_t RETIRE_GOLD_PENALTY = 6;
    static constexpr int32_t RETIRE_EXP_PENALTY  = 3;

    /// one scoreboard row for S2C 0x46
    struct ScoreRow {
        int32_t        rank             = 0;   ///< rank zero based must be unique in 0 to 29 finishTimeMs zero or less renders as dashes
        int32_t        finishTimeMs     = 0;
        uint32_t       playerId         = 0;
        std::u16string displayName;            ///< displayName truncated to 12 utf16 units levelIndex no reader in client send level anyway
        int8_t         levelIndex       = 0;
        uint32_t       team             = 0;   ///< team compared against header team col2Base right column base drawn at x plus 0x2D0
        uint32_t       col2Base         = 0;
        uint32_t       col1Base         = 0;   ///< col1Base left column base drawn at x plus 0x230 col2Bonus right column plus N half
        uint32_t       col2Bonus        = 0;
        uint32_t       col1Bonus        = 0;   ///< col1Bonus left column plus N half pccafeIconIndex only 0 1 2 draw an icon
        int8_t         pccafeIconIndex  = -1;
        uint32_t       characterBaseKey = 0;   ///< characterBaseKey must exist in 0xBF driver catalog bonusItemKey 4001 gold up 4002 exp up 4003 total up
        uint32_t       bonusItemKey     = 0;
    };

    /// one racer as fed into the reward computation
    struct Finisher {
        uint32_t playerId     = 0;
        int32_t  finishRank   = RANK_RETIRE;  ///< finishRank zero based or RANK RETIRE for dnf finishTimeMs zero or less means dnf
        int32_t  finishTimeMs = 0;
        uint32_t bonusItemKey = BONUS_NONE;   ///< consumable actually burnt by that racer
    };

    /// gold and xp owed to one racer
    struct Reward {
        uint32_t playerId   = 0;
        int32_t  finishRank = RANK_RETIRE;
        int32_t  gold       = 0;
        int32_t  xp         = 0;
        int32_t  goldBonus  = 0;  ///< goldBonus plus N half when gold up active xpBonus plus N half when exp up active
        int32_t  xpBonus    = 0;
    };

    /// S2C 0x3C finish plus reward unicast to the finisher fields land with no id guard so persist before send
    static Packet finishReward(uint32_t playerId, uint32_t goldAfter,
                               uint8_t levelAfter, uint32_t expAfter,
                               int32_t finishRank);

    /// S2C 0x3D rank broadcast to everyone must reach the client before 0x46 which reads the stored rank
    static Packet rankBroadcast(uint32_t playerId, int32_t rank);

    /// S2C 0x46 scoreboard myTeam is recipient specific rows outside 0 to 29 or duplicate ranks are dropped capped at 30
    static Packet scoreboard(uint32_t myTeam, const std::vector<ScoreRow>& rows);

    /// S2C 0x58 driver anim relay sent to every racer but the sender
    static Packet driverAnimState(uint32_t playerId, uint8_t animState);

    /// C2S 0x58 driver anim echo one byte no id on the wire attribute to the sender then relay
    static bool parseAnimStateEcho(const Packet& pkt, uint8_t& outAnimState);

    /// gold and xp per racer falls back to a built in ladder retired racers earn nothing
    static std::vector<Reward> computeRewards(const std::vector<Finisher>& order,
                                              int32_t gameMode);

    /// single place that decides which scoreboard column carries gold
    static void applyReward(ScoreRow& row, const Reward& reward);

    /// mirrors the client side dock so both wallets stay in sync
    static void applyRetirePenalty(int32_t& gold, int32_t& xp, int32_t activeCarCount);
};

} // namespace knc
