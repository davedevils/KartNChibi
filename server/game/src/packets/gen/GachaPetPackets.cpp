#include "packets/gen/GachaPetPackets.h"

#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <map>
#include <random>
#include <stdexcept>
#include <string>

namespace knc {

namespace {

uint32_t getU32(const uint8_t* p, size_t off) {
    return static_cast<uint32_t>(p[off + 0]) |
           (static_cast<uint32_t>(p[off + 1]) << 8) |
           (static_cast<uint32_t>(p[off + 2]) << 16) |
           (static_cast<uint32_t>(p[off + 3]) << 24);
}

// wrong size here silently desyncs the whole stream so shout
void checkSize(const Packet& pkt, size_t expected, const char* what) {
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", std::string(what) + " size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
}

// client reads with strlen into a small stack buffer so long string smashes its frame
std::string clampAscii(const std::string& in, size_t maxChars, const char* what) {
    size_t nulProcessedLen = 0;
    std::string out = clampAsciiCore(in, maxChars, AsciiNulMode::kStopAtNul, nulProcessedLen);
    if (nulProcessedLen > maxChars) {
        LOG_WARN("PACKET", std::string("GachaPetPackets clamp ") + what + " from " +
                           std::to_string(nulProcessedLen) + " to " + std::to_string(maxChars));
    }
    return out;
}

uint32_t rowU32(const std::map<std::string, std::string>& row, const char* key) {
    return static_cast<uint32_t>(rowUInt64Throwing(row, key, 0));
}

int32_t rowI32(const std::map<std::string, std::string>& row, const char* key) {
    return static_cast<int32_t>(rowInt64Throwing(row, key, 0));
}

std::string rowStr(const std::map<std::string, std::string>& row, const char* key) {
    return rowStrCore(row, key);
}

GachaPetPackets::ItemRow itemRowFromDb(const std::map<std::string, std::string>& row) {
    GachaPetPackets::ItemRow r;
    r.instanceId  = rowU32(row, "id");
    r.baseKey     = rowU32(row, "base_key");
    r.priceKey    = rowU32(row, "price_key");
    r.periodMode  = rowU32(row, "period_mode");
    r.periodValue = rowI32(row, "period_value");
    r.activeFlag  = rowU32(row, "active_flag");
    r.inUseFlag   = rowI32(row, "in_use_flag");
    return r;
}

GachaPetPackets::PetRow petRowFromDb(const std::map<std::string, std::string>& row) {
    GachaPetPackets::PetRow r;
    r.instanceId   = rowU32(row, "id");
    r.baseKey      = rowU32(row, "base_key");
    r.equippedFlag = rowU32(row, "equipped_flag");
    r.priceKey     = rowU32(row, "price_key");
    r.periodMode   = rowU32(row, "period_mode");
    r.periodValue  = rowU32(row, "period_value");
    r.activeFlag   = rowU32(row, "active_flag");
    return r;
}

// bounded reader so a crafted payload can never walk off the buffer
class Cursor {
public:
    Cursor(const uint8_t* data, size_t len) : m_data(data), m_len(len) {}

    bool readU32(uint32_t& out) {
        if (m_pos + 4 > m_len) return false;
        out = getU32(m_data, m_pos);
        m_pos += 4;
        return true;
    }

    bool readI32(int32_t& out) {
        uint32_t raw = 0;
        if (!readU32(raw)) return false;
        out = static_cast<int32_t>(raw);
        return true;
    }

    bool readBytes(uint8_t* dst, size_t n) {
        if (m_pos + n > m_len) return false;
        std::memcpy(dst, m_data + m_pos, n);
        m_pos += n;
        return true;
    }

    bool readWString(std::u16string& out, size_t maxChars) {
        out.clear();
        while (true) {
            if (m_pos + 2 > m_len) return false;
            const uint16_t c = static_cast<uint16_t>(m_data[m_pos]) |
                               (static_cast<uint16_t>(m_data[m_pos + 1]) << 8);
            m_pos += 2;
            if (c == 0) return true;
            if (out.size() >= maxChars) return false;
            out.push_back(static_cast<char16_t>(c));
        }
    }

    size_t pos() const { return m_pos; }
    size_t len() const { return m_len; }

private:
    const uint8_t* m_data = nullptr;
    size_t m_len = 0;
    size_t m_pos = 0;
};

uint64_t nextRandom() {
    // one engine per thread else two rolls in the same tick can agree
    thread_local std::mt19937_64 engine([] {
        std::random_device rd;
        const uint64_t seed =
            (static_cast<uint64_t>(rd()) << 32) ^ static_cast<uint64_t>(rd()) ^
            static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        return std::mt19937_64(seed);
    }());
    return engine();
}

// four folders shipped under Data Public Pet Body and Pet Facial
const char* const kShippedFolders[] = { "Pet_01", "Pet_02", "Pet_03", "Pet_04" };

}  // namespace

bool GachaPetPackets::parseRoll(const Packet& pkt, GachaRollRequest& out) {
    const std::vector<uint8_t>& body = pkt.payload();
    if (body.size() != SIZE_TICKET_REC) {
        LOG_WARN("PACKET", "gacha roll payload " + std::to_string(body.size()) +
                           " expected " + std::to_string(SIZE_TICKET_REC));
        return false;
    }

    std::memcpy(out.raw.data(), body.data(), SIZE_TICKET_REC);
    out.instanceId        = getU32(body.data(), 0x00);
    out.itemBaseKey       = getU32(body.data(), 0x04);
    out.purchaseOptionKey = getU32(body.data(), 0x08);
    out.periodMode        = getU32(body.data(), 0x0C);
    out.remainingRolls    = static_cast<int32_t>(getU32(body.data(), 0x10));
    out.activeFlag        = getU32(body.data(), 0x14);
    out.inUseFlag         = static_cast<int32_t>(getU32(body.data(), 0x18));
    return true;
}

bool GachaPetPackets::rollRequestMatchesTicket(const GachaRollRequest& req,
                                               const ItemRow& serverTicket) {
    if (req.itemBaseKey != serverTicket.baseKey) {
        LOG_WARN("PACKET", "gacha roll base key " + std::to_string(req.itemBaseKey) +
                           " server holds " + std::to_string(serverTicket.baseKey));
        return false;
    }
    if (serverTicket.activeFlag == 0) {
        LOG_WARN("PACKET", "gacha ticket inactive for base key " +
                           std::to_string(serverTicket.baseKey));
        return false;
    }
    // mode one would make the value an absolute day index and never run out
    if (serverTicket.periodMode != InventoryPackets::PERIOD_COUNT) {
        LOG_WARN("PACKET", "gacha ticket period mode " +
                           std::to_string(serverTicket.periodMode) + " expected 2");
        return false;
    }
    if (serverTicket.periodValue <= 0) {
        LOG_WARN("PACKET", "gacha ticket empty for character request instance " +
                           std::to_string(req.instanceId));
        return false;
    }
    return true;
}

uint64_t GachaPetPackets::totalWeight(const std::vector<GachaPrizeEntry>& table) {
    uint64_t total = 0;
    for (const GachaPrizeEntry& e : table) total += e.weight;
    return total;
}

GachaPetPackets::GachaRollOutcome
GachaPetPackets::rollPrizeWithValue(const std::vector<GachaPrizeEntry>& table, uint64_t rollValue) {
    GachaRollOutcome outcome;

    const uint64_t total = totalWeight(table);
    if (total == 0) {
        LOG_WARN("PACKET", "gacha prize table has no weight, no prize");
        return outcome;
    }

    uint64_t pick = rollValue % total;
    for (size_t i = 0; i < table.size(); ++i) {
        const uint64_t w = table[i].weight;
        if (w == 0) continue;
        if (pick < w) {
            outcome.hasPrize = true;
            outcome.index = i;
            outcome.entry = table[i];
            return outcome;
        }
        pick -= w;
    }

    // unreachable while total is the sum but never leave a fallthrough silent
    LOG_ERROR("PACKET", "gacha weighted pick fell through total " + std::to_string(total));
    return outcome;
}

GachaPetPackets::GachaRollOutcome
GachaPetPackets::rollPrize(const std::vector<GachaPrizeEntry>& table) {
    return rollPrizeWithValue(table, nextRandom());
}

size_t GachaPetPackets::sanitizePrizePool(std::vector<GachaPrizeEntry>& table,
                                          uint32_t ticketBaseKey) {
    const size_t before = table.size();

    table.erase(std::remove_if(table.begin(), table.end(),
        [ticketBaseKey](const GachaPrizeEntry& e) {
            if (e.prizeCategory > GC_CARCRAFT) {
                LOG_WARN("PACKET", "gacha prize category " +
                                   std::to_string(e.prizeCategory) + " dropped");
                return true;
            }
            if (e.prizeBaseKey == 0) {
                LOG_WARN("PACKET", "gacha prize base key zero dropped");
                return true;
            }
            if (e.periodMode > GP_SILENT) {
                LOG_WARN("PACKET", "gacha prize period mode " +
                                   std::to_string(e.periodMode) + " dropped");
                return true;
            }
            // item prize shares the ticket container so a key clash wipes the ticket
            if (e.prizeCategory == GC_ITEM && e.prizeBaseKey == ticketBaseKey) {
                LOG_ERROR("PACKET", "gacha item prize key " + std::to_string(e.prizeBaseKey) +
                                    " equals the ticket key, dropped");
                return true;
            }
            return false;
        }), table.end());

    return before - table.size();
}

GachaPetPackets::GachaHeader GachaPetPackets::headerFor(const GachaPrizeEntry& entry) {
    GachaHeader h;
    h.rareFlag         = entry.rareFlag;
    h.prizeCategory    = entry.prizeCategory;
    h.prizeBaseKey     = entry.prizeBaseKey;
    h.prizePeriodMode  = entry.periodMode;
    h.prizePeriodValue = entry.periodValue;
    return h;
}

size_t GachaPetPackets::prizeTailSize(uint32_t category, bool hasSlotRecord) {
    switch (category) {
        case GC_CHARACTER: return SIZE_CHAR_REC;
        case GC_KART:      return SIZE_KART_REC;
        case GC_ITEM:      return SIZE_SMALL_REC;
        case GC_PART:      return SIZE_SMALL_REC;
        case GC_ROOMCRAFT: return SIZE_ROOMCRAFT_REC;
        case GC_CARCRAFT:
            return SIZE_CARCRAFT_REC + 1 + (hasSlotRecord ? SIZE_CARCRAFT_SLOT_REC : 0);
        default:           return 0;
    }
}

Packet GachaPetPackets::result(const GachaHeader& header, const ItemRow& updatedTicket,
                               const uint8_t* tail, size_t tailLen) {
    Packet pkt = Packet::fromCmdFull(OP_GACHA);

    pkt.writeUInt32(header.rareFlag);
    pkt.writeUInt32(header.prizeCategory);
    pkt.writeUInt32(header.prizeBaseKey);
    pkt.writeUInt32(header.prizePeriodMode);
    pkt.writeInt32(header.prizePeriodValue);

    // echo is unconditional and the client matches it on rec plus four the base key
    const std::array<uint8_t, SIZE_TICKET_REC> ticket = InventoryPackets::itemBlob(updatedTicket);
    pkt.writeBytes(ticket.data(), ticket.size());

    if (header.prizeCategory > GC_CARCRAFT) {
        // client reads nothing past the echo so any tail byte desyncs the frame
        if (tail != nullptr && tailLen != 0) {
            LOG_ERROR("PACKET", "gacha result category " +
                                std::to_string(header.prizeCategory) +
                                " carries a tail of " + std::to_string(tailLen) + " bytes");
        }
        if (header.prizePeriodMode == GP_DAYS || header.prizePeriodMode == GP_TIMES) {
            // period sprite still draws from the header even with no prize
            LOG_WARN("PACKET", "gacha refusal period mode " +
                               std::to_string(header.prizePeriodMode) +
                               " still draws digits, use 0 or 3");
        }
        checkSize(pkt, GACHA_HEADER_SIZE + SIZE_TICKET_REC, "gachaResult");
        return pkt;
    }

    if (tail == nullptr || tailLen == 0) {
        LOG_ERROR("PACKET", "gacha result category " + std::to_string(header.prizeCategory) +
                            " needs a tail record");
        return pkt;
    }

    const size_t plain = prizeTailSize(header.prizeCategory, false);
    const size_t withSlot = prizeTailSize(header.prizeCategory, true);
    if (tailLen != plain && tailLen != withSlot) {
        LOG_ERROR("PACKET", "gacha result category " + std::to_string(header.prizeCategory) +
                            " tail " + std::to_string(tailLen) + " expected " +
                            std::to_string(plain));
    }

    pkt.writeBytes(tail, tailLen);
    checkSize(pkt, GACHA_HEADER_SIZE + SIZE_TICKET_REC + tailLen, "gachaResult");
    return pkt;
}

Packet GachaPetPackets::resultCharacter(const GachaHeader& header, const ItemRow& updatedTicket,
                                        const std::array<uint8_t, SIZE_CHAR_REC>& record) {
    GachaHeader h = header;
    h.prizeCategory = GC_CHARACTER;
    return result(h, updatedTicket, record.data(), record.size());
}

Packet GachaPetPackets::resultKart(const GachaHeader& header, const ItemRow& updatedTicket,
                                   const std::array<uint8_t, SIZE_KART_REC>& record) {
    GachaHeader h = header;
    h.prizeCategory = GC_KART;
    return result(h, updatedTicket, record.data(), record.size());
}

Packet GachaPetPackets::resultItem(const GachaHeader& header, const ItemRow& updatedTicket,
                                   const std::array<uint8_t, SIZE_SMALL_REC>& record) {
    GachaHeader h = header;
    h.prizeCategory = GC_ITEM;

    // same container as the ticket so a key clash erases the ticket row
    if (getU32(record.data(), 0x04) == updatedTicket.baseKey) {
        LOG_ERROR("PACKET", "gacha item prize base key " +
                            std::to_string(updatedTicket.baseKey) +
                            " equals the ticket key and would wipe it");
    }
    return result(h, updatedTicket, record.data(), record.size());
}

Packet GachaPetPackets::resultPart(const GachaHeader& header, const ItemRow& updatedTicket,
                                   const std::array<uint8_t, SIZE_SMALL_REC>& record) {
    GachaHeader h = header;
    h.prizeCategory = GC_PART;
    return result(h, updatedTicket, record.data(), record.size());
}

Packet GachaPetPackets::resultRoomCraft(const GachaHeader& header, const ItemRow& updatedTicket,
                                        const std::array<uint8_t, SIZE_ROOMCRAFT_REC>& record) {
    GachaHeader h = header;
    h.prizeCategory = GC_ROOMCRAFT;
    return result(h, updatedTicket, record.data(), record.size());
}

Packet GachaPetPackets::resultCarCraft(const GachaHeader& header, const ItemRow& updatedTicket,
                                       const std::array<uint8_t, SIZE_CARCRAFT_REC>& record,
                                       const std::array<uint8_t, SIZE_CARCRAFT_SLOT_REC>* slotRecord) {
    Packet pkt = Packet::fromCmdFull(OP_GACHA);

    pkt.writeUInt32(header.rareFlag);
    pkt.writeUInt32(GC_CARCRAFT);
    pkt.writeUInt32(header.prizeBaseKey);
    pkt.writeUInt32(header.prizePeriodMode);
    pkt.writeInt32(header.prizePeriodValue);

    const std::array<uint8_t, SIZE_TICKET_REC> ticket = InventoryPackets::itemBlob(updatedTicket);
    pkt.writeBytes(ticket.data(), ticket.size());

    // blob first then the flag here the buy ack uses the opposite order
    pkt.writeBytes(record.data(), record.size());
    pkt.writeUInt8(slotRecord != nullptr ? 1 : 0);
    if (slotRecord != nullptr) {
        pkt.writeBytes(slotRecord->data(), slotRecord->size());
    }

    checkSize(pkt, GACHA_HEADER_SIZE + SIZE_TICKET_REC +
                   prizeTailSize(GC_CARCRAFT, slotRecord != nullptr), "gachaResultCarCraft");
    return pkt;
}

Packet GachaPetPackets::resultNoPrize(const ItemRow& unchangedTicket) {
    GachaHeader h;
    h.rareFlag         = 0;
    h.prizeCategory    = GC_NONE;
    h.prizeBaseKey     = 0;
    h.prizePeriodMode  = GP_PERMANENT;  // one or two would still draw the period digits
    h.prizePeriodValue = 0;
    return result(h, unchangedTicket, nullptr, 0);
}

GachaPetPackets::ItemRow GachaPetPackets::decrementedTicket(const ItemRow& before) {
    ItemRow after = before;
    if (after.periodValue > 0) {
        after.periodValue -= 1;
    }
    // client never deletes the row itself so leave it at zero and let it grey out
    return after;
}

size_t GachaPetPackets::randomRewardTailSize(uint32_t category, bool hasSlotRecord) {
    switch (category) {
        case 0: return SIZE_CHAR_REC;
        case 1: return SIZE_KART_REC;
        case 2: return SIZE_SMALL_REC;
        case 3: return SIZE_SMALL_REC;
        case 5: return SIZE_ROOMCRAFT_REC;
        case 6: return SIZE_CARCRAFT_REC + 1 + (hasSlotRecord ? SIZE_CARCRAFT_SLOT_REC : 0);
        default: return 0;  // four is pet and has no case at all
    }
}

Packet GachaPetPackets::randomReward(const RandomRewardHeader& header,
                                     const uint8_t* tail, size_t tailLen) {
    Packet pkt = Packet::fromCmdFull(OP_RANDOM_REWARD);

    pkt.writeUInt32(header.unknown00);
    pkt.writeInt32(header.resultCode);
    pkt.writeUInt32(header.popupArgA);
    pkt.writeUInt32(header.popupArgB);
    pkt.writeUInt32(header.category);
    pkt.writeUInt32(header.unknown14);
    pkt.writeUInt32(header.unknown18);

    if (header.category == CAT_PET || header.category > 6) {
        // pet has no case at all so the client reads nothing and any tail desyncs
        if (header.category == CAT_PET) {
            LOG_ERROR("PACKET", "random reward category 4 is pet and has no client case");
        }
        if (tail != nullptr && tailLen != 0) {
            LOG_ERROR("PACKET", "random reward category " + std::to_string(header.category) +
                                " reads no tail but " + std::to_string(tailLen) +
                                " bytes were given");
        }
        checkSize(pkt, 28, "randomReward");
        return pkt;
    }

    if (tail == nullptr || tailLen == 0) {
        LOG_ERROR("PACKET", "random reward category " + std::to_string(header.category) +
                            " needs a tail record");
        checkSize(pkt, 28, "randomReward");
        return pkt;
    }

    const size_t plain = randomRewardTailSize(header.category, false);
    const size_t withSlot = randomRewardTailSize(header.category, true);
    if (tailLen != plain && tailLen != withSlot) {
        LOG_ERROR("PACKET", "random reward category " + std::to_string(header.category) +
                            " tail " + std::to_string(tailLen) + " expected " +
                            std::to_string(plain));
    }

    pkt.writeBytes(tail, tailLen);
    checkSize(pkt, 28 + tailLen, "randomReward");
    return pkt;
}

Packet GachaPetPackets::randomRewardCarCraft(const RandomRewardHeader& header,
                                             const std::array<uint8_t, SIZE_CARCRAFT_REC>& record,
                                             const std::array<uint8_t, SIZE_CARCRAFT_SLOT_REC>* slotRecord) {
    std::vector<uint8_t> tail;
    tail.reserve(SIZE_CARCRAFT_REC + 1 + SIZE_CARCRAFT_SLOT_REC);
    // blob first then the flag here the buy ack uses the opposite order
    tail.insert(tail.end(), record.begin(), record.end());
    tail.push_back(slotRecord != nullptr ? 1 : 0);
    if (slotRecord != nullptr) {
        tail.insert(tail.end(), slotRecord->begin(), slotRecord->end());
    }

    RandomRewardHeader h = header;
    h.category = 6;
    return randomReward(h, tail.data(), tail.size());
}

bool GachaPetPackets::petKeyHasRaceEffect(uint32_t petBaseKey) {
    return petBaseKey == PET_KEY_ROSIE || petBaseKey == PET_KEY_CHAI ||
           petBaseKey == PET_KEY_PORKI || petBaseKey == PET_KEY_DIMDIM;
}

bool GachaPetPackets::modelFolderIsShipped(const std::string& folder) {
    for (const char* f : kShippedFolders) {
        if (folder == f) return true;
    }
    return false;
}

Packet GachaPetPackets::petDefinition(const PetDef& def) {
    Packet pkt = Packet::fromCmdFull(OP_PET_DEF);

    if (!petKeyHasRaceEffect(def.petBaseKey)) {
        LOG_WARN("PACKET", "pet base key " + std::to_string(def.petBaseKey) +
                           " is outside 10 20 30 40 so it has no race effect");
    }
    // key in the catalog with no model makes the loader return zero and bind slot zero
    if (!modelFolderIsShipped(def.modelFolder)) {
        LOG_ERROR("PACKET", "pet model folder '" + def.modelFolder +
                            "' is not shipped, the driver would bind to entity slot 0");
    }

    const std::string s1 = clampAscii(def.modelFolder, MAX_STR1, "pet str1");
    const std::string s2 = clampAscii(def.titleKey, MAX_STR2, "pet str2");
    const std::string s3 = clampAscii(def.infoKey, MAX_STR3, "pet str3");

    pkt.writeUInt32(def.shopVisibleFlag);
    pkt.writeUInt32(def.badge);
    pkt.writeUInt32(def.petBaseKey);
    pkt.writeUInt32(def.requiredPendantKey);
    pkt.writeString(s1);
    pkt.writeString(s2);
    pkt.writeString(s3);

    size_t count = def.options.size();
    if (count > CAP_PET_OPTIONS) {
        LOG_WARN("PACKET", "pet key " + std::to_string(def.petBaseKey) + " options " +
                           std::to_string(count) + " trimmed to " +
                           std::to_string(CAP_PET_OPTIONS));
        count = CAP_PET_OPTIONS;
    }
    // same null deref trap as every other tile list never send zero
    if (count == 0) {
        pkt.writeInt32(1);
        for (int i = 0; i < 4; ++i) pkt.writeUInt32(0);
    } else {
    pkt.writeInt32(static_cast<int32_t>(count));
    for (size_t i = 0; i < count; ++i) {
        const PetOption& o = def.options[i];
        pkt.writeUInt32(o.priceOptionKey);
        pkt.writeUInt32(o.periodMode);
        pkt.writeInt32(o.periodValue);
        pkt.writeUInt32(o.unused);
    }
    }

    checkSize(pkt, 16 + (s1.size() + 1) + (s2.size() + 1) + (s3.size() + 1) + 4 +
                   16 * (count == 0 ? 1 : count),
              "petDefinition");
    return pkt;
}

std::vector<Packet> GachaPetPackets::petCatalog(const std::vector<PetDef>& defs) {
    std::vector<Packet> out;
    size_t count = defs.size();
    if (count > CAP_PET_DEFS) {
        LOG_ERROR("PACKET", "pet catalog " + std::to_string(count) + " over cap " +
                            std::to_string(CAP_PET_DEFS) + " extra rows dropped");
        count = CAP_PET_DEFS;
    }
    out.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        out.push_back(petDefinition(defs[i]));
    }
    return out;
}

Packet GachaPetPackets::ownedPetList(const std::vector<PetRow>& rows) {
    return InventoryPackets::ownedPetList(rows);
}

size_t GachaPetPackets::enforceSingleEquipped(std::vector<PetRow>& rows) {
    bool seen = false;
    size_t cleared = 0;
    for (PetRow& r : rows) {
        if (r.equippedFlag != 1) continue;
        if (!seen) {
            seen = true;
            continue;
        }
        // effect scans take the first index match so later flags are silent liars
        r.equippedFlag = 0;
        ++cleared;
    }
    if (cleared != 0) {
        LOG_WARN("PACKET", "owned pets had " + std::to_string(cleared + 1) +
                           " equipped rows, kept the first");
    }
    return cleared;
}

uint32_t GachaPetPackets::equippedPetBaseKey(const std::vector<PetRow>& rows) {
    for (const PetRow& r : rows) {
        if (r.equippedFlag == 1) return r.baseKey;
    }
    return 0;
}

bool GachaPetPackets::ackRowIsSafe(const std::vector<PetRow>& clientRows, uint32_t instanceId) {
    for (const PetRow& r : clientRows) {
        if (r.instanceId == instanceId) return true;
    }
    LOG_ERROR("PACKET", "pet ack instance id " + std::to_string(instanceId) +
                        " is not in the client list, the ack would write to 0x00000008");
    return false;
}

Packet GachaPetPackets::buyAck(uint32_t moneyGp, uint32_t moneyCash, const PetRow& newPet) {
    return InventoryPackets::buyAckPet(moneyGp, moneyCash, newPet);
}

Packet GachaPetPackets::equipAck(const PetRow& newPet) {
    // flag must be one else the pet stays off and the race effect never arms
    if (newPet.equippedFlag != 1) {
        LOG_WARN("PACKET", "pet equip ack sends equipped flag " +
                           std::to_string(newPet.equippedFlag) + " expected 1");
    }
    return InventoryPackets::equipPetAck(1, newPet);
}

Packet GachaPetPackets::equipAckReplace(const PetRow& previousPet, const PetRow& newPet) {
    if (newPet.equippedFlag != 1) {
        LOG_WARN("PACKET", "pet equip ack sends equipped flag " +
                           std::to_string(newPet.equippedFlag) + " expected 1");
    }
    if (previousPet.equippedFlag != 0) {
        LOG_WARN("PACKET", "pet equip ack previous row flag " +
                           std::to_string(previousPet.equippedFlag) + " expected 0");
    }
    return InventoryPackets::equipPetAckReplace(previousPet, newPet);
}

Packet GachaPetPackets::unequipAck(const PetRow& row) {
    if (row.equippedFlag != 0) {
        LOG_WARN("PACKET", "pet unequip ack sends equipped flag " +
                           std::to_string(row.equippedFlag) + " expected 0");
    }
    return InventoryPackets::unequipPetAck(row);
}

bool GachaPetPackets::parseBuy(const Packet& pkt, PetBuyRequest& out) {
    const std::vector<uint8_t>& body = pkt.payload();
    Cursor cur(body.data(), body.size());

    uint32_t category = 0;
    uint32_t baseKey = 0;
    int32_t priceKey = -1;
    if (!cur.readU32(category) || !cur.readU32(baseKey) || !cur.readI32(priceKey)) {
        LOG_WARN("PACKET", "pet buy payload too short " + std::to_string(body.size()));
        return false;
    }

    std::u16string token;
    if (!cur.readWString(token, 512)) {
        LOG_WARN("PACKET", "pet buy token unterminated or too long");
        return false;
    }

    out.category = category;
    out.petBaseKey = baseKey;
    out.priceKey = priceKey;
    // launcher token not a username never key an account off this
    out.sessionToken = token;
    return true;
}

bool GachaPetPackets::parseEquip(const Packet& pkt, PetEquipRequest& out) {
    const std::vector<uint8_t>& body = pkt.payload();
    if (body.size() != 12) {
        LOG_WARN("PACKET", "pet equip payload " + std::to_string(body.size()) + " expected 12");
        return false;
    }
    out.category   = getU32(body.data(), 0x00);
    out.petBaseKey = getU32(body.data(), 0x04);
    out.aux        = static_cast<int32_t>(getU32(body.data(), 0x08));
    return true;
}

bool GachaPetPackets::parseUnequip(const Packet& pkt, PetUnequipRequest& out) {
    const std::vector<uint8_t>& body = pkt.payload();
    if (body.size() != 8) {
        LOG_WARN("PACKET", "pet unequip payload " + std::to_string(body.size()) + " expected 8");
        return false;
    }
    out.category   = getU32(body.data(), 0x00);
    out.petBaseKey = getU32(body.data(), 0x04);
    return true;
}

std::vector<GachaPetPackets::PetDef> GachaPetPackets::loadPetDefs() {
    std::vector<PetDef> defs;

    auto rows = Database::instance().queryPrepared(
        "SELECT base_key, shop_visible_flag, badge, required_pendant_key, "
        "str1_name, str2, str3_desc "
        "FROM shop_definition WHERE category = 4 ORDER BY base_key", {});

    defs.reserve(rows.size());
    for (const auto& row : rows) {
        PetDef d;
        d.petBaseKey         = rowU32(row, "base_key");
        d.shopVisibleFlag    = rowU32(row, "shop_visible_flag");
        d.badge              = rowU32(row, "badge");
        d.requiredPendantKey = rowU32(row, "required_pendant_key");
        d.modelFolder        = rowStr(row, "str1_name");
        d.titleKey           = rowStr(row, "str2");
        d.infoKey            = rowStr(row, "str3_desc");
        defs.push_back(std::move(d));
    }

    auto optRows = Database::instance().queryPrepared(
        "SELECT base_key, price_key, opt_word1, opt_word2, opt_word3 "
        "FROM shop_option WHERE category = 4 ORDER BY base_key, slot", {});

    // seeds four option slots a pet and three day rentals by price order pair holds unit type and amount
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> priceUnit;
    for (const auto& pr : Database::instance().queryPrepared(
             "SELECT price_key, unit_type, unit_amount FROM shop_price", {})) {
        priceUnit[rowU32(pr, "price_key")] = { rowU32(pr, "unit_type"), rowU32(pr, "unit_amount") };
    }
    std::map<uint32_t, std::vector<PetOption>> seeded;
    for (const auto& row : optRows) {
        PetOption o;
        o.priceOptionKey = rowU32(row, "price_key");
        o.periodMode     = rowU32(row, "opt_word1");
        o.periodValue    = rowI32(row, "opt_word2");
        o.unused         = rowU32(row, "opt_word3");
        seeded[rowU32(row, "base_key")].push_back(o);
    }
    static const std::pair<uint32_t, uint32_t> kWanted[3] = { {1, 1}, {1, 7}, {0, 0} };
    for (PetDef& d : defs) {
        auto it = seeded.find(d.petBaseKey);
        if (it == seeded.end()) continue;
        for (const auto& want : kWanted) {
            for (const PetOption& o : it->second) {
                auto pu = priceUnit.find(o.priceOptionKey);
                if (pu != priceUnit.end() && pu->second == want) { d.options.push_back(o); break; }
            }
        }
        if (d.options.empty()) {
            // keys with no price row whatever the seed has three at most
            for (const PetOption& o : it->second) {
                if (d.options.size() < 3) d.options.push_back(o);
            }
        }
    }

    LOG_INFO("PACKET", "loaded " + std::to_string(defs.size()) + " pet definitions");
    return defs;
}

std::vector<GachaPetPackets::PetRow> GachaPetPackets::loadOwnedPets(uint32_t characterId) {
    std::vector<PetRow> out;

    auto rows = Database::instance().queryPrepared(
        "SELECT id, base_key, equipped_flag, price_key, period_mode, period_value, active_flag "
        "FROM owned_pet WHERE character_id = ? ORDER BY id",
        { characterId });

    out.reserve(rows.size());
    for (const auto& row : rows) {
        out.push_back(petRowFromDb(row));
    }
    if (out.size() > CAP_OWNED_PETS) {
        LOG_ERROR("PACKET", "character " + std::to_string(characterId) + " owns " +
                            std::to_string(out.size()) + " pets over the client cap of " +
                            std::to_string(CAP_OWNED_PETS));
    }
    enforceSingleEquipped(out);
    return out;
}

bool GachaPetPackets::loadTicket(uint32_t characterId, uint32_t ticketBaseKey, ItemRow& out) {
    auto rows = Database::instance().queryPrepared(
        "SELECT id, base_key, price_key, period_mode, period_value, active_flag, in_use_flag "
        "FROM owned_item WHERE character_id = ? AND base_key = ? LIMIT 1",
        { characterId, ticketBaseKey });

    if (rows.empty()) return false;
    out = itemRowFromDb(rows[0]);
    return true;
}

std::vector<GachaPetPackets::GachaPrizeEntry>
GachaPetPackets::loadPrizePool(uint32_t ticketBaseKey) {
    std::vector<GachaPrizeEntry> table;

    auto rows = Database::instance().queryPrepared(
        "SELECT weight, rare_flag, prize_category, prize_base_key, period_mode, period_value "
        "FROM gacha_items WHERE ticket_base_key = ? AND enabled = 1 ORDER BY id",
        { ticketBaseKey });

    table.reserve(rows.size());
    for (const auto& row : rows) {
        GachaPrizeEntry e;
        e.weight        = rowU32(row, "weight");
        e.rareFlag      = rowU32(row, "rare_flag");
        e.prizeCategory = rowU32(row, "prize_category");
        e.prizeBaseKey  = rowU32(row, "prize_base_key");
        e.periodMode    = rowU32(row, "period_mode");
        e.periodValue   = rowI32(row, "period_value");
        table.push_back(e);
    }

    const size_t dropped = sanitizePrizePool(table, ticketBaseKey);
    if (dropped != 0) {
        LOG_WARN("PACKET", "gacha pool for ticket " + std::to_string(ticketBaseKey) +
                           " dropped " + std::to_string(dropped) + " bad rows");
    }
    return table;
}

bool GachaPetPackets::consumeTicketInTx(Transaction& tx, uint32_t characterId,
                                        uint32_t ticketBaseKey, ItemRow& updatedOut) {
    if (!tx.valid()) {
        // no transaction means no way to prove one row moved so refuse to spend
        LOG_ERROR("PACKET", "gacha consume needs a valid transaction");
        return false;
    }

    const bool ok = tx.execute(
        "UPDATE owned_item SET period_value = period_value - 1 "
        "WHERE character_id = ? AND base_key = ? AND active_flag = 1 "
        "AND period_mode = 2 AND period_value > 0",
        { characterId, ticketBaseKey });
    if (!ok) {
        LOG_ERROR("PACKET", "gacha ticket decrement failed for character " +
                            std::to_string(characterId));
        return false;
    }
    if (tx.affectedRows() != 1) {
        LOG_WARN("PACKET", "gacha ticket decrement touched " +
                           std::to_string(tx.affectedRows()) + " rows, no roll");
        return false;
    }

    auto rows = tx.query(
        "SELECT id, base_key, price_key, period_mode, period_value, active_flag, in_use_flag "
        "FROM owned_item WHERE character_id = ? AND base_key = ? LIMIT 1",
        { characterId, ticketBaseKey });
    if (rows.empty()) {
        LOG_ERROR("PACKET", "gacha ticket vanished after decrement for character " +
                            std::to_string(characterId));
        return false;
    }

    updatedOut = itemRowFromDb(rows[0]);
    return true;
}

bool GachaPetPackets::logRollInTx(Transaction& tx, uint32_t characterId,
                                  const GachaRollRequest& req, const GachaRollOutcome& outcome,
                                  int32_t remainingAfter) {
    if (!tx.valid()) return false;

    return tx.execute(
        "INSERT INTO gacha_history (character_id, ticket_instance_id, ticket_base_key, "
        "remaining_after, rare_flag, prize_category, prize_base_key) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)",
        { characterId,
          req.instanceId,
          req.itemBaseKey,
          remainingAfter,
          outcome.hasPrize ? static_cast<int>(outcome.entry.rareFlag) : 0,
          outcome.hasPrize ? static_cast<int>(outcome.entry.prizeCategory)
                           : static_cast<int>(GC_NONE),
          outcome.hasPrize ? static_cast<int>(outcome.entry.prizeBaseKey) : 0 });
}

bool GachaPetPackets::consumeTicketAndLog(uint32_t characterId, const GachaRollRequest& req,
                                          const GachaRollOutcome& outcome, ItemRow& updatedOut) {
    Transaction tx = Database::instance().beginTransaction();
    if (!tx.valid()) {
        LOG_ERROR("PACKET", "gacha roll cannot open a transaction, no roll");
        return false;
    }

    if (!consumeTicketInTx(tx, characterId, req.itemBaseKey, updatedOut)) {
        tx.rollback();
        return false;
    }
    if (!logRollInTx(tx, characterId, req, outcome, updatedOut.periodValue)) {
        LOG_ERROR("PACKET", "gacha log insert failed for character " +
                            std::to_string(characterId));
        tx.rollback();
        return false;
    }
    return tx.commit();
}

bool GachaPetPackets::setEquippedPet(uint32_t characterId, uint32_t petBaseKey) {
    const char* clearSql = "UPDATE owned_pet SET equipped_flag = 0 WHERE character_id = ?";
    const char* setSql =
        "UPDATE owned_pet SET equipped_flag = 1 "
        "WHERE character_id = ? AND base_key = ? AND active_flag = 1";
    const char* mirrorSql = "UPDATE characters SET active_pet_id = ? WHERE id = ?";

    Transaction tx = Database::instance().beginTransaction();
    if (tx.valid()) {
        if (!tx.execute(clearSql, { characterId })) return false;
        if (!tx.execute(setSql, { characterId, petBaseKey })) return false;
        if (tx.affectedRows() != 1) {
            LOG_WARN("PACKET", "pet equip touched " + std::to_string(tx.affectedRows()) +
                               " rows for base key " + std::to_string(petBaseKey));
            tx.rollback();
            return false;
        }
        if (!tx.execute(mirrorSql, { petBaseKey, characterId })) return false;
        return tx.commit();
    }

    LOG_WARN("PACKET", "pet equip running without a transaction");
    bool ok = Database::instance().executePrepared(clearSql, { characterId });
    ok = Database::instance().executePrepared(setSql, { characterId, petBaseKey }) && ok;
    ok = Database::instance().executePrepared(mirrorSql, { petBaseKey, characterId }) && ok;
    return ok;
}

bool GachaPetPackets::clearEquippedPet(uint32_t characterId, uint32_t petBaseKey) {
    const char* clearSql =
        "UPDATE owned_pet SET equipped_flag = 0 WHERE character_id = ? AND base_key = ?";
    const char* mirrorSql = "UPDATE characters SET active_pet_id = NULL WHERE id = ?";

    Transaction tx = Database::instance().beginTransaction();
    if (tx.valid()) {
        if (!tx.execute(clearSql, { characterId, petBaseKey })) return false;
        if (!tx.execute(mirrorSql, { characterId })) return false;
        return tx.commit();
    }

    LOG_WARN("PACKET", "pet unequip running without a transaction");
    bool ok = Database::instance().executePrepared(clearSql, { characterId, petBaseKey });
    ok = Database::instance().executePrepared(mirrorSql, { characterId }) && ok;
    return ok;
}

uint32_t GachaPetPackets::loadEquippedPetBaseKey(uint32_t characterId) {
    auto rows = Database::instance().queryPrepared(
        "SELECT base_key FROM owned_pet "
        "WHERE character_id = ? AND equipped_flag = 1 AND active_flag = 1 ORDER BY id LIMIT 1",
        { characterId });

    if (rows.empty()) return 0;
    return rowU32(rows[0], "base_key");
}

bool GachaPetPackets::hasPetCondition(uint32_t characterId, uint32_t conditionKey) {
    if (conditionKey == 0) return true;

    // the client tests its 0x11A owned pendant list so owned pendant is the truth pet condition stays a fallback
    auto owned = Database::instance().queryPrepared(
        "SELECT pendant_key FROM owned_pendant WHERE character_id = ? AND pendant_key = ? LIMIT 1",
        { characterId, conditionKey });
    if (!owned.empty()) return true;

    auto rows = Database::instance().queryPrepared(
        "SELECT condition_key FROM pet_condition "
        "WHERE character_id = ? AND condition_key = ? LIMIT 1",
        { characterId, conditionKey });

    return !rows.empty();
}

}  // namespace knc
