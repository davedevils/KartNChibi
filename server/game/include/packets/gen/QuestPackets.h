/// quest wire builders and C2S parsers for client stage 26

#pragma once
#include "net/Packet.h"
#include "net/Protocol.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

/// quest index must equal four times theme id plus row or sub 43CCC0 dereferences a null pointer
struct QuestPackets {

    static constexpr size_t QUEST_DEF_MAX      = 50;   ///< defMax sub 452180 returns -1 on 51st stateMax sub 451FB0 returns -1 on 51st
    static constexpr size_t QUEST_STATE_MAX    = 50;
    static constexpr size_t QUEST_THEME_MAX    = 4;    ///< themeMax quest step1 to step4 radio group rowsPerTheme 12 button sets 4 rows by 3 states
    static constexpr size_t QUEST_ROWS_PER_THEME = 4;
    static constexpr uint32_t QUEST_INDEX_MAX  = 15;   ///< four times three plus three nothing higher is reachable

    static constexpr size_t QUEST_DEF_SIZE     = 112;  ///< defSize 0x70 one 0xFB payload flat blob stateRow one 0xFC row
    static constexpr size_t QUEST_STATE_ROW    = 12;
    static constexpr size_t QUEST_STR_KEY_OFF  = 44;   ///< strKeyOff 0x2C start of string table key strKeySlot 112 minus 44 leaves room for key and its NUL
    static constexpr size_t QUEST_STR_KEY_SLOT = 68;

    static constexpr uint32_t STATE_ACCEPTABLE  = 0;   ///< acceptable is quest acceptance art inProgress is quest progress art
    static constexpr uint32_t STATE_IN_PROGRESS = 1;
    static constexpr uint32_t STATE_COMPLETE    = 2;   ///< quest complete art

    /// one S2C 0xFB quest definition always 112 fixed bytes no strings on the wire
    struct QuestDefWire {
        uint32_t questIndex    = 0;  ///< questIndex 0x04 key for sub 452240 4 times themeId plus row themeId 0x08 sub 4521C0 0 to 3 selectable
        uint32_t themeId       = 0;
        uint32_t goalCount     = 0;  ///< goalCount 0x0C goal drawn by sub 43CCC0 as d of d s detailValueLower 0x14 sub 43CCC0 panel y plus 0x1F7
        uint32_t detailValueLower = 0;
        uint32_t detailValueUpper = 0;  ///< detailValueUpper 0x18 sub 43CCC0 panel y plus 0x1D4 strKeyTitle 0x2C ascii key into off 7272E0 max 67 chars plus NUL
        std::string strKeyTitle;
        // 0x00 is the valid flag a zero row is skipped by sub 4521C0 and shifts later rows
    };

    /// one 12 byte row of S2C 0xFC in the shape sub 451FB0 stores
    struct QuestStateEntry {
        uint32_t questIndex    = 0;  ///< questIndex 0x00 key for sub 452110 must exist in catalog state 0x04 0 1 or 2 only
        uint32_t state         = 0;
        uint32_t progressCount = 0;  ///< 0x08 stage 26 prints only the low 16 bits
    };

    /// C2S 0xFE C2S 0x100 and C2S 0x102 payload all three are one u32
    struct QuestIndexReq {
        uint32_t questIndex = 0;
    };

    /// the only legal quest index for a theme and row row must be 0 to 3
    static uint32_t questIndexFor(uint32_t themeId, uint32_t row);

    /// theme id implied by a quest index
    static uint32_t themeOf(uint32_t questIndex);

    /// row 0 to 3 implied by a quest index
    static uint32_t rowOf(uint32_t questIndex);

    /// drops catalog rows that break the 4 times theme plus row contiguity rule keeping only the safe run
    static std::vector<QuestDefWire> sanitizeCatalog(const std::vector<QuestDefWire>& defs);

    /// drops state rows the client cannot survive keeping only ones whose quest index exists in the catalog
    static std::vector<QuestStateEntry> sanitizeStateList(const std::vector<QuestStateEntry>& rows,
                                                          const std::vector<QuestDefWire>& catalog);

    /// S2C 0xFB one quest definition always 112 bytes appends to the catalog
    static Packet questDefinition(const QuestDefWire& def);

    /// S2C 0xFB from a captured 112 byte record with no field interpretation
    static Packet questDefinitionRaw(const uint8_t* record112);

    /// S2C 0xFB times N for a whole catalog sanitized first one packet per row
    static std::vector<Packet> questCatalog(const std::vector<QuestDefWire>& defs);

    /// S2C 0xFC full replace of the state list clamped to 50 rows
    static Packet questStateList(const std::vector<QuestStateEntry>& rows);

    /// S2C 0xFE accept ack appends index 1 0 to the state list 4 bytes
    static Packet questAcceptAck(uint32_t questIndex);

    /// S2C 0x100 discard ack removes the row from the state list 4 bytes
    static Packet questDiscardAck(uint32_t questIndex);

    /// S2C 0x101 sets state 2 and progress on an existing row 8 bytes
    static Packet questCompleted(uint32_t questIndex, uint32_t progressCount);

    /// S2C 0x102 sets progress only on an existing row 8 bytes
    static Packet questProgress(uint32_t questIndex, uint32_t progressCount);

    /// C2S 0xFE accept quest 4 bytes from the quest accept button
    static bool parseAcceptRequest(const Packet& pkt, QuestIndexReq& out);

    /// C2S 0x100 discard quest 4 bytes from the quest discard button
    static bool parseDiscardRequest(const Packet& pkt, QuestIndexReq& out);

    /// C2S 0x102 progress report 4 bytes fired by the race result screen
    static bool parseProgressReport(const Packet& pkt, QuestIndexReq& out);

    /// quest def rows already sanitized at most 50
    static std::vector<QuestDefWire> loadQuestDefs();

    /// char quest state rows for one character raw sanitize before sending
    static std::vector<QuestStateEntry> loadQuestState(int32_t characterId);

    /// server side goal for a quest zero when unknown never on the wire
    static uint32_t loadQuestGoal(uint32_t questIndex);
};

} // namespace knc
