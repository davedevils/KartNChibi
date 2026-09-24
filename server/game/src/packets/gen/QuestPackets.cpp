#include "packets/gen/QuestPackets.h"
#include <algorithm>
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <cstdlib>
#include <map>
#include <string>

namespace knc {

namespace {

// Protocol h calls these buddy list and entity data which is a wrong earlier pass
constexpr uint16_t OP_S_QUEST_DEF       = 0x00FB;
constexpr uint16_t OP_S_QUEST_STATE     = 0x00FC;  // def sub 47DB60 qmemcpy 0x70 into catalog state sub 47DBB0 clear count rows
constexpr uint16_t OP_S_QUEST_ACCEPT    = 0x00FE;
constexpr uint16_t OP_S_QUEST_DISCARD   = 0x0100;  // accept sub 47F270 appends index 1 0 discard sub 47F2C0 sub 4520D0 removes row
constexpr uint16_t OP_S_QUEST_COMPLETE  = 0x0101;
constexpr uint16_t OP_S_QUEST_PROGRESS  = 0x0102;  // complete sub 47F2F0 writes offset 8 progress offset 4 equals 2 progress sub 47F340 offset 8 only

using DbRow = std::map<std::string, std::string>;

// wrong size here silently desyncs the whole stream so shout
void checkSize(const Packet& pkt, size_t expected, const char* what) {
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", std::string(what) + " size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
}

// sub 4E1B70 does strlen on the record so the slot must end NUL
void writeAsciiSlot(Packet& pkt, const std::string& s, size_t slot) {
    const size_t maxChars = slot - 1;
    size_t i = 0;
    for (; i < s.size() && i < maxChars; ++i) {
        pkt.writeUInt8(static_cast<uint8_t>(s[i]));
    }
    for (; i < slot; ++i) {
        pkt.writeUInt8(0);
    }
}

bool needBytes(const Packet& pkt, size_t want, const char* what) {
    if (pkt.payload().size() >= want) return true;
    LOG_WARN("PACKET", std::string(what) + " short payload " +
                       std::to_string(pkt.payload().size()) + " want " + std::to_string(want));
    return false;
}

uint32_t readU32LE(const std::vector<uint8_t>& b, size_t off) {
    return static_cast<uint32_t>(b[off]) |
           (static_cast<uint32_t>(b[off + 1]) << 8) |
           (static_cast<uint32_t>(b[off + 2]) << 16) |
           (static_cast<uint32_t>(b[off + 3]) << 24);
}

uint32_t rowU32(const DbRow& row, const char* key) {
    return static_cast<uint32_t>(rowUInt64NoThrow(row, key, 0));
}

std::string rowStr(const DbRow& row, const char* key) {
    return rowStrCore(row, key);
}

} // namespace

uint32_t QuestPackets::questIndexFor(uint32_t themeId, uint32_t row) {
    if (row >= QUEST_ROWS_PER_THEME) {
        LOG_ERROR("PACKET", "questIndexFor row " + std::to_string(row) + " over 3");
    }
    return themeId * static_cast<uint32_t>(QUEST_ROWS_PER_THEME) + row;
}

uint32_t QuestPackets::themeOf(uint32_t questIndex) {
    return questIndex / static_cast<uint32_t>(QUEST_ROWS_PER_THEME);
}

uint32_t QuestPackets::rowOf(uint32_t questIndex) {
    return questIndex % static_cast<uint32_t>(QUEST_ROWS_PER_THEME);
}

std::vector<QuestPackets::QuestDefWire>
QuestPackets::sanitizeCatalog(const std::vector<QuestDefWire>& defs) {
    std::vector<QuestDefWire> kept;

    for (const auto& d : defs) {
        if (d.themeId >= QUEST_THEME_MAX) {
            LOG_WARN("PACKET", "quest def index " + std::to_string(d.questIndex) + " theme " +
                               std::to_string(d.themeId) + " over 3 unreachable dropped");
        }
    }

    for (uint32_t theme = 0; theme < static_cast<uint32_t>(QUEST_THEME_MAX); ++theme) {
        const QuestDefWire* slot[QUEST_ROWS_PER_THEME] = { nullptr, nullptr, nullptr, nullptr };
        const uint32_t base = theme * static_cast<uint32_t>(QUEST_ROWS_PER_THEME);

        for (const auto& d : defs) {
            if (d.themeId != theme) continue;

            if (d.questIndex < base || d.questIndex >= base + QUEST_ROWS_PER_THEME) {
                LOG_ERROR("PACKET", "quest def index " + std::to_string(d.questIndex) +
                                    " does not belong to theme " + std::to_string(theme) +
                                    " dropped");
                continue;
            }

            const uint32_t row = d.questIndex - base;
            if (slot[row] != nullptr) {
                LOG_ERROR("PACKET", "quest def index " + std::to_string(d.questIndex) +
                                    " duplicated dropped");
                continue;
            }
            slot[row] = &d;
        }

        for (size_t row = 0; row < QUEST_ROWS_PER_THEME; ++row) {
            if (slot[row] == nullptr) {
                // gap here makes sub 452240 return 0 and the renderer derefs offset 0x2C
                for (size_t rest = row + 1; rest < QUEST_ROWS_PER_THEME; ++rest) {
                    if (slot[rest] != nullptr) {
                        LOG_ERROR("PACKET", "quest theme " + std::to_string(theme) +
                                            " has a gap at row " + std::to_string(row) +
                                            " later rows dropped");
                        break;
                    }
                }
                break;
            }

            QuestDefWire out = *slot[row];
            if (out.strKeyTitle.size() > QUEST_STR_KEY_SLOT - 1) {
                LOG_WARN("PACKET", "quest def index " + std::to_string(out.questIndex) +
                                   " str key cut to " + std::to_string(QUEST_STR_KEY_SLOT - 1));
                out.strKeyTitle = out.strKeyTitle.substr(0, QUEST_STR_KEY_SLOT - 1);
            }
            kept.push_back(out);
        }
    }

    if (kept.size() > QUEST_DEF_MAX) {
        LOG_ERROR("PACKET", "quest catalog " + std::to_string(kept.size()) +
                            " rows over client cap " + std::to_string(QUEST_DEF_MAX));
        kept.resize(QUEST_DEF_MAX);
    }
    return kept;
}

std::vector<QuestPackets::QuestStateEntry>
QuestPackets::sanitizeStateList(const std::vector<QuestStateEntry>& rows,
                                const std::vector<QuestDefWire>& catalog) {
    std::vector<QuestStateEntry> kept;
    size_t inProgress = 0;

    for (const auto& r : rows) {
        if (kept.size() >= QUEST_STATE_MAX) {
            LOG_ERROR("PACKET", "quest state list over client cap " +
                                std::to_string(QUEST_STATE_MAX) + " extra rows dropped");
            break;
        }

        if (r.state > STATE_COMPLETE) {
            LOG_ERROR("PACKET", "quest state index " + std::to_string(r.questIndex) + " state " +
                                std::to_string(r.state) + " over 2 dropped");
            continue;
        }

        bool inCatalog = false;
        for (const auto& d : catalog) {
            if (d.questIndex == r.questIndex) { inCatalog = true; break; }
        }
        if (!inCatalog) {
            LOG_ERROR("PACKET", "quest state index " + std::to_string(r.questIndex) +
                                " absent from catalog dropped");
            continue;
        }

        bool dupe = false;
        for (const auto& k : kept) {
            if (k.questIndex == r.questIndex) { dupe = true; break; }
        }
        if (dupe) {
            LOG_ERROR("PACKET", "quest state index " + std::to_string(r.questIndex) +
                                " duplicated dropped");
            continue;
        }

        if (r.progressCount > 0xFFFFu) {
            LOG_WARN("PACKET", "quest state index " + std::to_string(r.questIndex) +
                               " progress " + std::to_string(r.progressCount) +
                               " draws only the low 16 bits on stage 26");
        }

        if (r.state == STATE_IN_PROGRESS) ++inProgress;
        kept.push_back(r);
    }

    if (inProgress > 1) {
        LOG_WARN("PACKET", "quest state list has " + std::to_string(inProgress) +
                           " rows in progress, the client can never accept another");
    }
    return kept;
}

Packet QuestPackets::questDefinition(const QuestDefWire& def) {
    if (def.questIndex > QUEST_INDEX_MAX || themeOf(def.questIndex) != def.themeId) {
        LOG_ERROR("PACKET", "questDefinition index " + std::to_string(def.questIndex) +
                            " theme " + std::to_string(def.themeId) +
                            " breaks the 4 times theme plus row rule");
    }

    Packet pkt = Packet::fromCmdFull(OP_S_QUEST_DEF);
    pkt.writeUInt32(1);                    // offset 0x00 zero here hides the row and shifts the theme
    pkt.writeUInt32(def.questIndex);
    pkt.writeUInt32(def.themeId);
    pkt.writeUInt32(def.goalCount);        // sub 43CCC0 draws n over goal using the state field then this one so zero showed 1 over 0
    pkt.writeUInt32(0);
    pkt.writeUInt32(def.detailValueLower); // lower and upper numbers of the detail panel
    pkt.writeUInt32(def.detailValueUpper);
    pkt.writeUInt32(0);                    // offset 0x1C and 0x20 never read
    pkt.writeUInt32(0);
    pkt.writeUInt32(0);                    // offset 0x24 and 0x28 never read
    pkt.writeUInt32(0);

    writeAsciiSlot(pkt, def.strKeyTitle, QUEST_STR_KEY_SLOT);

    checkSize(pkt, QUEST_DEF_SIZE, "questDefinition");
    return pkt;
}

Packet QuestPackets::questDefinitionRaw(const uint8_t* record112) {
    Packet pkt = Packet::fromCmdFull(OP_S_QUEST_DEF);
    if (record112 == nullptr) {
        LOG_ERROR("PACKET", "questDefinitionRaw null record");
        for (size_t i = 0; i < QUEST_DEF_SIZE; ++i) pkt.writeUInt8(0);
        return pkt;
    }
    pkt.writeBytes(record112, QUEST_DEF_SIZE);
    checkSize(pkt, QUEST_DEF_SIZE, "questDefinitionRaw");
    return pkt;
}

std::vector<Packet> QuestPackets::questCatalog(const std::vector<QuestDefWire>& defs) {
    const std::vector<QuestDefWire> safe = sanitizeCatalog(defs);

    std::vector<Packet> out;
    out.reserve(safe.size());
    for (const auto& d : safe) {
        out.push_back(questDefinition(d));
    }
    return out;
}

Packet QuestPackets::questStateList(const std::vector<QuestStateEntry>& rows) {
    std::vector<QuestStateEntry> safe;
    safe.reserve(rows.size());

    for (const auto& r : rows) {
        if (safe.size() >= QUEST_STATE_MAX) {
            LOG_ERROR("PACKET", "questStateList over client cap " +
                                std::to_string(QUEST_STATE_MAX) + " extra rows dropped");
            break;
        }
        if (r.state > STATE_COMPLETE) {
            LOG_ERROR("PACKET", "questStateList index " + std::to_string(r.questIndex) +
                                " state " + std::to_string(r.state) + " over 2 dropped");
            continue;
        }
        bool dupe = false;
        for (const auto& k : safe) {
            if (k.questIndex == r.questIndex) { dupe = true; break; }
        }
        if (dupe) {
            LOG_ERROR("PACKET", "questStateList index " + std::to_string(r.questIndex) +
                                " duplicated dropped");
            continue;
        }
        safe.push_back(r);
    }

    Packet pkt = Packet::fromCmdFull(OP_S_QUEST_STATE);
    pkt.writeInt32(static_cast<int32_t>(safe.size()));
    for (const auto& r : safe) {
        pkt.writeUInt32(r.questIndex);
        pkt.writeUInt32(r.state);
        pkt.writeUInt32(r.progressCount);
    }

    checkSize(pkt, 4 + QUEST_STATE_ROW * safe.size(), "questStateList");
    return pkt;
}

Packet QuestPackets::questAcceptAck(uint32_t questIndex) {
    // sub 47F270 appends with no dupe check so never ack an index already in the list
    Packet pkt = Packet::fromCmdFull(OP_S_QUEST_ACCEPT);
    pkt.writeUInt32(questIndex);
    checkSize(pkt, 4, "questAcceptAck");
    return pkt;
}

Packet QuestPackets::questDiscardAck(uint32_t questIndex) {
    Packet pkt = Packet::fromCmdFull(OP_S_QUEST_DISCARD);
    pkt.writeUInt32(questIndex);
    checkSize(pkt, 4, "questDiscardAck");
    return pkt;
}

Packet QuestPackets::questCompleted(uint32_t questIndex, uint32_t progressCount) {
    // sub 47F2F0 derefs sub 452110 with no null test so the row must already exist
    Packet pkt = Packet::fromCmdFull(OP_S_QUEST_COMPLETE);
    pkt.writeUInt32(questIndex);
    pkt.writeUInt32(progressCount);
    checkSize(pkt, 8, "questCompleted");
    return pkt;
}

Packet QuestPackets::questProgress(uint32_t questIndex, uint32_t progressCount) {
    // sub 47F340 derefs sub 452110 with no null test so the row must already exist
    Packet pkt = Packet::fromCmdFull(OP_S_QUEST_PROGRESS);
    pkt.writeUInt32(questIndex);
    pkt.writeUInt32(progressCount);
    checkSize(pkt, 8, "questProgress");
    return pkt;
}

bool QuestPackets::parseAcceptRequest(const Packet& pkt, QuestIndexReq& out) {
    if (!needBytes(pkt, 4, "parseAcceptRequest")) return false;
    out.questIndex = readU32LE(pkt.payload(), 0);
    return true;
}

bool QuestPackets::parseDiscardRequest(const Packet& pkt, QuestIndexReq& out) {
    if (!needBytes(pkt, 4, "parseDiscardRequest")) return false;
    out.questIndex = readU32LE(pkt.payload(), 0);
    return true;
}

bool QuestPackets::parseProgressReport(const Packet& pkt, QuestIndexReq& out) {
    if (!needBytes(pkt, 4, "parseProgressReport")) return false;
    out.questIndex = readU32LE(pkt.payload(), 0);
    return true;
}

std::vector<QuestPackets::QuestDefWire> QuestPackets::loadQuestDefs() {
    std::vector<QuestDefWire> defs;

    auto rows = Database::instance().queryPrepared(
        "SELECT quest_index, theme_id, goal_count, detail_value_lower, detail_value_upper, "
        "str_key_title FROM quest_def ORDER BY theme_id, quest_index LIMIT 50",
        {});

    defs.reserve(rows.size());
    for (const auto& row : rows) {
        QuestDefWire d;
        d.questIndex    = rowU32(row, "quest_index");
        d.themeId       = rowU32(row, "theme_id");
        // a zero goal can never complete and the line reads n over 0
        d.goalCount     = std::max(1u, rowU32(row, "goal_count"));
        d.detailValueLower = rowU32(row, "detail_value_lower");
        d.detailValueUpper = rowU32(row, "detail_value_upper");
        d.strKeyTitle   = rowStr(row, "str_key_title");
        defs.push_back(std::move(d));
    }

    std::vector<QuestDefWire> safe = sanitizeCatalog(defs);
    LOG_INFO("QUEST", "loaded " + std::to_string(defs.size()) + " quest defs kept " +
                      std::to_string(safe.size()));
    return safe;
}

std::vector<QuestPackets::QuestStateEntry> QuestPackets::loadQuestState(int32_t characterId) {
    std::vector<QuestStateEntry> rows;

    auto dbRows = Database::instance().queryPrepared(
        "SELECT quest_index, state, progress_count FROM char_quest_state "
        "WHERE char_id = ? ORDER BY quest_index LIMIT 50",
        { characterId });

    rows.reserve(dbRows.size());
    for (const auto& row : dbRows) {
        QuestStateEntry e;
        e.questIndex    = rowU32(row, "quest_index");
        e.state         = rowU32(row, "state");
        e.progressCount = rowU32(row, "progress_count");
        rows.push_back(e);
    }

    LOG_DEBUG("QUEST", "loaded " + std::to_string(rows.size()) + " quest state rows for char " +
                       std::to_string(characterId));
    return rows;
}

uint32_t QuestPackets::loadQuestGoal(uint32_t questIndex) {
    auto rows = Database::instance().queryPrepared(
        "SELECT goal_count FROM quest_def WHERE quest_index = ? LIMIT 1",
        { questIndex });

    if (rows.empty()) return 0;
    return rowU32(rows[0], "goal_count");
}

} // namespace knc
