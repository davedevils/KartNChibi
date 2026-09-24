/// quest mode wire of stage 22 ScenarioMenu and stage 17 Quest game proven on KnC exe

#pragma once
#include "net/Packet.h"
#include "net/Protocol.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

struct ScenarioPackets {

    /// sub 452DA0 refuses the 51st definition and sub 452C40 the 51st progress row
    static constexpr size_t SCENARIO_DEF_MAX  = 50;
    /// sub 47DAA0 reads one flat 156 byte record
    static constexpr size_t SCENARIO_DEF_SIZE = 0x9C;
    /// sub 452C40 cap of the 0x00F4 and 0x00F9 progress list
    static constexpr size_t PROGRESS_ROW_MAX  = 50;
    /// each ascii key slot of the record is 33 bytes NUL padded
    static constexpr size_t KEY_SLOT_SIZE     = 33;
    /// sub 4B5EB0 copies the story key into 36 bytes then appends SUCCESS so 27 is the room left
    static constexpr size_t STORY_KEY_MAX     = 27;
    /// sub 47DD10 swprintf of the rival name lands in 14 wchar so 13 letters plus the NUL
    static constexpr size_t RIVAL_NAME_MAX    = 13;

    /// reward category 8 and above draws nothing and reads no 0x00F8 tail
    static constexpr uint32_t REWARD_NONE     = 8;
    static constexpr uint32_t REWARD_DRIVER   = 0;
    static constexpr uint32_t REWARD_KART     = 1;
    static constexpr uint32_t REWARD_PENDANT  = 7;

    /// 0x00F8 kind 0 failed 1 cleared again 2 paid 3 paid with the item tail
    static constexpr int32_t RESULT_FAILED     = 0;
    static constexpr int32_t RESULT_CLEARED    = 1;
    static constexpr int32_t RESULT_PAID       = 2;
    static constexpr int32_t RESULT_PAID_ITEM  = 3;
    static constexpr size_t  RESULT_BASE_SIZE  = 13;
    static constexpr size_t  RESULT_PAID_SIZE  = 21;

    /// a 3 lap quest never ends under 20 seconds
    static constexpr uint32_t MIN_RACE_TIME_MS  = 20000;
    /// the reported race time may run past the server clock by this much
    static constexpr uint32_t CLOCK_SLACK_MS    = 5000;

    static constexpr uint16_t OP_S_SCENARIO_DEF             = 0x00F3;
    static constexpr uint16_t OP_S_SCENARIO_PROGRESS_LIST   = 0x00F4;
    static constexpr uint16_t OP_S_SCENARIO_START           = 0x00F5;
    static constexpr uint16_t OP_S_SCENARIO_RESULT          = 0x00F8;
    static constexpr uint16_t OP_S_SCENARIO_PROGRESS_APPEND = 0x00F9;
    /// S2C 0x011C sub 47E8E0 pushes stage 22 the ScenarioMenu
    static constexpr uint16_t OP_S_SCENARIO_MENU            = 0x011C;

    /// one quest definition the wire fields then the server only rules
    struct ScenarioDefWire {
        /// offset 0x04 key sub 452E30 resolves every quest lookup against
        uint32_t scenarioKey     = 0;
        /// offset 0x0C 0xC3 track the detail thumbnail and the stage 17 world
        uint32_t trackId         = 0;
        /// offset 0x10 0xBF rival driver HUD portrait and ghost car body
        uint32_t characterDefKey = 0;
        /// offset 0x14 0xC0 rival kart of the ghost car
        uint32_t kartDefKey      = 0;
        /// offset 0x1C drawn after MSG QUEST PAY while the row is not cleared
        uint32_t entryFee        = 0;
        /// offset 0x20 UNIT MILEAGE gold paid on the first clear
        uint32_t rewardMileage   = 0;
        /// offset 0x24 UNIT EXP exp paid on the first clear
        uint32_t rewardExp       = 0;
        /// offset 0x28 0 to 7 picks the icon and the 0x00F8 kind 3 tail 8 means none
        uint32_t rewardCategory  = REWARD_NONE;
        /// offset 0x2C catalogue key of the reward
        uint32_t rewardKey       = 0;
        /// offset 0x38 def quest index key the list row title
        std::string titleKey;
        /// offset 0x59 def quest index key the detail panel line
        std::string descKey;
        /// offset 0x7A def quest index key the HUD story START SUCCESS and FAIL get appended
        std::string msgKeyBase;
        /// server only the lowest character level that lists the row
        uint32_t requiredLevel   = 1;
        /// server only zero means any finish wins when no rival ghost runs
        uint32_t goalTimeMs      = 0;
    };

    /// one 8 byte row of 0x00F4 and 0x00F9 key then cleared flag
    struct ScenarioProgressRow {
        uint32_t key     = 0;
        uint32_t cleared = 0;
    };

    /// S2C 0x00F5 answers the Start click and opens stage 17
    struct ScenarioStartWire {
        uint32_t scenarioKey = 0;
        /// lands in the gold of the profile after the entry fee
        uint32_t goldCounter = 0;
    };

    /// S2C 0x00F8 the result the client waits for in state 2013
    struct ScenarioResultWire {
        int32_t  kind        = RESULT_FAILED;
        /// 1 plays result success and MSG QUEST SUCCESS anything else the fail
        uint8_t  flag        = 0;
        uint32_t scenarioKey = 0;
        int32_t  goldAfter   = 0;
        int32_t  expAfter    = 0;
        /// kind 3 only the typed record the client def category names
        std::vector<uint8_t> tail;
    };

    /// C2S 0xF5 payload a bare u32 scenario key
    struct StageSelectReq {
        uint32_t key = 0;
    };

    /// C2S 0xF8 payload scenario key then the race time in ms
    struct ResultReportReq {
        uint32_t scenarioKey = 0;
        uint32_t resultValue = 0;
    };

    /// what the server makes of one result report
    struct ResultDecision {
        bool     success    = false;
        bool     firstClear = false;
        int32_t  kind       = RESULT_FAILED;
        uint8_t  flag       = 0;
    };

    /// S2C 0x00F3 one definition always 156 bytes
    static Packet scenarioDefinition(const ScenarioDefWire& def);

    /// S2C 0x00F4 full replace 4 plus 8 per row
    static Packet progressList(const std::vector<ScenarioProgressRow>& rows);

    /// S2C 0x00F5 scenario start 8 bytes
    static Packet scenarioStart(const ScenarioStartWire& start);

    /// S2C 0x00F8 13 or 21 bytes or 21 plus the tail of kind 3
    static Packet scenarioResult(const ScenarioResultWire& result);

    /// S2C 0x00F9 one appended progress row 8 bytes
    static Packet scenarioProgressAppend(const ScenarioProgressRow& row);

    /// S2C 0x011C empty the menu open ack
    static Packet menuOpenAck();

    /// bytes sub 47DFA0 reads after the wallet pair for a def category carcraft counts its slot flag
    static size_t rewardTailSize(uint32_t category, bool carcraftHasSlot = false);

    /// the category the wire carries a zero key or an unknown category draws nothing
    static uint32_t wireRewardCategory(uint32_t category, uint32_t key);

    /// type 7 tail instance then key sub 451250 drops the key then sub 451140 appends
    static std::vector<uint8_t> pendantRewardTail(uint32_t pendantKey);

    /// rows a character sees every def at or under the level plus every cleared one
    static std::vector<ScenarioProgressRow> visibleRows(const std::vector<ScenarioDefWire>& defs,
                                                        const std::vector<uint32_t>& clearedKeys,
                                                        uint32_t level);

    /// sub 437F30 gate on rows sorted by key highest first the next row must be cleared
    static bool rowPlayable(const std::vector<ScenarioProgressRow>& rows, uint32_t key);

    /// keys in after that are missing from before the 0x00F9 appends of a level up
    static std::vector<ScenarioProgressRow> newRows(const std::vector<ScenarioProgressRow>& before,
                                                    const std::vector<ScenarioProgressRow>& after);

    /// the reason a def would crash or stall the client empty when the def is safe
    static std::string defProblem(const ScenarioDefWire& def, bool driverKnown, bool kartKnown,
                                  bool trackKnown, bool rewardKnown, const std::string& rivalName);

    /// success beats the rival ghost time or the goal and pays only on the first clear
    static ResultDecision decideResult(bool runMatches, bool alreadyCleared, uint32_t raceTimeMs,
                                       uint32_t elapsedMs, int32_t rivalTimeMs, uint32_t goalTimeMs,
                                       bool hasItemReward);

    // C2S parsers all return false on a short payload

    /// C2S 0xF5 stage select 4 bytes
    static bool parseStageSelect(const Packet& pkt, StageSelectReq& out);

    /// C2S 0xF8 result report 8 bytes tolerant of a short trailing field
    static bool parseResultReport(const Packet& pkt, ResultReportReq& out);

    /// every safe enabled def ordered by key clamped to 50 an unsafe one is logged and left out
    static std::vector<ScenarioDefWire> loadScenarioDefs();
};

} // namespace knc
