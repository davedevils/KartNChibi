#include "packets/gen/InventoryPackets.h"
#include "logging/Logger.h"

#include <string>

// the stock client needs a paint and a plate on every kart record RED paint and the standard plate
constexpr uint32_t kDefaultKartPaintKey = 9007;
constexpr uint32_t kDefaultKartPlateKey = 9100;

namespace knc {

namespace {

void putU32(uint8_t* p, size_t off, uint32_t v) {
    p[off + 0] = static_cast<uint8_t>(v & 0xFF);
    p[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

void putI32(uint8_t* p, size_t off, int32_t v) {
    putU32(p, off, static_cast<uint32_t>(v));
}

// wrong size here silently desyncs the whole stream so shout
void checkSize(const Packet& pkt, size_t expected, const char* what) {
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", std::string(what) + " size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
}

// past the cap the client drops the record and shows a stale row
size_t capCount(size_t n, size_t cap, const char* what) {
    if (n > cap) {
        LOG_ERROR("PACKET", std::string(what) + " count " + std::to_string(n) +
                            " over client cap " + std::to_string(cap) + " extra rows dropped");
        return cap;
    }
    return n;
}

// dupe id makes the next buy erase an unrelated row on the client
template <typename RowT>
void warnDuplicateIds(const std::vector<RowT>& rows, size_t count, const char* what) {
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            if (rows[i].instanceId == rows[j].instanceId) {
                LOG_ERROR("PACKET", std::string(what) + " duplicate instance id " +
                                    std::to_string(rows[i].instanceId));
                return;
            }
        }
    }
}

class Cursor {
public:
    Cursor(const uint8_t* data, size_t len) : m_data(data), m_len(len) {}

    bool readU32(uint32_t& out) {
        if (m_pos + 4 > m_len) return false;
        out = static_cast<uint32_t>(m_data[m_pos]) |
              (static_cast<uint32_t>(m_data[m_pos + 1]) << 8) |
              (static_cast<uint32_t>(m_data[m_pos + 2]) << 16) |
              (static_cast<uint32_t>(m_data[m_pos + 3]) << 24);
        m_pos += 4;
        return true;
    }

    bool readWString(std::u16string& out, size_t maxChars) {
        out.clear();
        for (;;) {
            if (m_pos + 2 > m_len) return false;
            const uint16_t c = static_cast<uint16_t>(m_data[m_pos]) |
                               static_cast<uint16_t>(static_cast<uint16_t>(m_data[m_pos + 1]) << 8);
            m_pos += 2;
            if (c == 0) return true;
            if (out.size() >= maxChars) return false;
            out.push_back(static_cast<char16_t>(c));
        }
    }

private:
    const uint8_t* m_data;
    size_t m_len;
    size_t m_pos = 0;
};

Packet buyAckBase(uint32_t category, uint32_t currencyB, uint32_t currencyA) {
    Packet pkt = Packet::fromCmdFull(InventoryPackets::OP_BUY);
    pkt.writeUInt32(category);
    // client reads this one into the right hand counter first
    pkt.writeUInt32(currencyB);
    pkt.writeUInt32(currencyA);
    return pkt;
}

Packet installAckBase(uint32_t category) {
    Packet pkt = Packet::fromCmdFull(InventoryPackets::OP_INSTALL);
    pkt.writeUInt32(category);
    return pkt;
}

Packet removeAckBase(uint32_t category) {
    Packet pkt = Packet::fromCmdFull(InventoryPackets::OP_REMOVE);
    pkt.writeUInt32(category);
    return pkt;
}

constexpr size_t ACK_HEAD_BUY     = 12;
constexpr size_t ACK_HEAD_CAT     = 4;

}  // namespace

std::array<uint8_t, 0x2C> InventoryPackets::characterBlob(const CharacterRow& row) {
    auto blob = PacketBuilder::characterRecord(static_cast<int32_t>(row.instanceId),
                                               static_cast<int32_t>(row.baseKey));
    // empty cosmetic slot is -1 not 0 since 0 is read as a real part key
    auto slot = [](int32_t v) { return v == 0 ? -1 : v; };
    putI32(blob.data(), 0x08, slot(row.accBody));
    putI32(blob.data(), 0x0C, slot(row.accFace));
    putI32(blob.data(), 0x10, slot(row.accHead));
    putI32(blob.data(), 0x14, slot(row.accGlass));
    putI32(blob.data(), 0x18, slot(row.accBack));
    putU32(blob.data(), 0x1C, row.priceKey);
    putU32(blob.data(), 0x20, row.periodMode);
    putU32(blob.data(), 0x24, row.periodValue);
    putU32(blob.data(), 0x28, row.activeFlag);
    return blob;
}

std::array<uint8_t, 0x38> InventoryPackets::kartBlob(const KartRow& row) {
    VehicleInfo v{};
    v.templateId = static_cast<int32_t>(row.baseKey);
    auto blob = PacketBuilder::kartRecord(v);

    // helper stat block predates this sweep so rewrite every dword past base key
    putU32(blob.data(), 0x00, row.instanceId);
    // a zero paint or plate key makes sub 4510C0 return null and the stock car build gives up
    putU32(blob.data(), 0x08, row.skinPrimary != 0 ? row.skinPrimary : kDefaultKartPaintKey);
    putU32(blob.data(), 0x0C, row.skinSecondary != 0 ? row.skinSecondary : kDefaultKartPlateKey);
    putU32(blob.data(), 0x10, row.skinTertiary);
    putU32(blob.data(), 0x14, row.custom3);
    putU32(blob.data(), 0x18, row.custom4);
    putU32(blob.data(), 0x1C, row.custom5);
    putU32(blob.data(), 0x20, row.appliedItemA);
    putU32(blob.data(), 0x24, row.appliedItemB);
    putU32(blob.data(), 0x28, row.priceKey);
    putU32(blob.data(), 0x2C, row.periodMode);
    putU32(blob.data(), 0x30, row.periodValue);
    putU32(blob.data(), 0x34, row.activeFlag);
    return blob;
}

std::array<uint8_t, 0x1C> InventoryPackets::itemBlob(const ItemRow& row) {
    std::array<uint8_t, 0x1C> blob{};
    putU32(blob.data(), 0x00, row.instanceId);
    putU32(blob.data(), 0x04, row.baseKey);
    putU32(blob.data(), 0x08, row.priceKey);
    // period block sits earlier here than in the part and pet rows
    putU32(blob.data(), 0x0C, row.periodMode);
    putI32(blob.data(), 0x10, row.periodValue);
    putU32(blob.data(), 0x14, row.activeFlag);
    putI32(blob.data(), 0x18, row.inUseFlag);
    return blob;
}

std::array<uint8_t, 0x1C> InventoryPackets::partBlob(const PartRow& row) {
    std::array<uint8_t, 0x1C> blob{};
    putU32(blob.data(), 0x00, row.instanceId);
    putU32(blob.data(), 0x04, row.baseKey);
    // offset 0x08 has no reader the client only echoes it in C2S 0x00CC
    putU32(blob.data(), 0x08, row.unk08);
    putU32(blob.data(), 0x0C, row.priceKey);
    putU32(blob.data(), 0x10, row.periodMode);
    putU32(blob.data(), 0x14, row.periodValue);
    putU32(blob.data(), 0x18, row.activeFlag);
    return blob;
}

std::array<uint8_t, 0x1C> InventoryPackets::petBlob(const PetRow& row) {
    std::array<uint8_t, 0x1C> blob{};
    putU32(blob.data(), 0x00, row.instanceId);
    putU32(blob.data(), 0x04, row.baseKey);
    putU32(blob.data(), 0x08, row.equippedFlag);
    putU32(blob.data(), 0x0C, row.priceKey);
    putU32(blob.data(), 0x10, row.periodMode);
    putU32(blob.data(), 0x14, row.periodValue);
    putU32(blob.data(), 0x18, row.activeFlag);
    return blob;
}

Packet InventoryPackets::ownedCharacterList(const std::vector<CharacterRow>& rows) {
    const size_t count = capCount(rows.size(), CAP_CHARACTER, "ownedCharacterList");
    warnDuplicateIds(rows, count, "ownedCharacterList");

    Packet pkt = Packet::fromCmdFull(OP_OWNED_CHARACTER_LIST);
    pkt.writeInt32(static_cast<int32_t>(count));
    for (size_t i = 0; i < count; ++i) {
        const auto rec = characterBlob(rows[i]);
        pkt.writeBytes(rec.data(), rec.size());
    }

    checkSize(pkt, 4 + SIZE_CHARACTER_REC * count, "ownedCharacterList");
    return pkt;
}

Packet InventoryPackets::ownedKartList(const std::vector<KartRow>& rows) {
    const size_t count = capCount(rows.size(), CAP_KART, "ownedKartList");
    warnDuplicateIds(rows, count, "ownedKartList");

    Packet pkt = Packet::fromCmdFull(OP_OWNED_KART_LIST);
    pkt.writeInt32(static_cast<int32_t>(count));
    for (size_t i = 0; i < count; ++i) {
        const auto rec = kartBlob(rows[i]);
        pkt.writeBytes(rec.data(), rec.size());
    }

    checkSize(pkt, 4 + SIZE_KART_REC * count, "ownedKartList");
    return pkt;
}

Packet InventoryPackets::ownedItemList(const std::vector<ItemRow>& rows) {
    const size_t count = capCount(rows.size(), CAP_ITEM, "ownedItemList");
    warnDuplicateIds(rows, count, "ownedItemList");

    Packet pkt = Packet::fromCmdFull(OP_OWNED_ITEM_LIST);
    pkt.writeInt32(static_cast<int32_t>(count));
    for (size_t i = 0; i < count; ++i) {
        const auto rec = itemBlob(rows[i]);
        pkt.writeBytes(rec.data(), rec.size());
    }

    checkSize(pkt, 4 + SIZE_ITEM_REC * count, "ownedItemList");
    return pkt;
}

Packet InventoryPackets::ownedPartRecord(const PartRow& row) {
    Packet pkt = Packet::fromCmdFull(OP_OWNED_PART_RECORD);
    const auto rec = partBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, SIZE_PART_REC, "ownedPartRecord");
    return pkt;
}

std::vector<Packet> InventoryPackets::ownedPartRecords(const std::vector<PartRow>& rows) {
    const size_t count = capCount(rows.size(), CAP_PART, "ownedPartRecords");
    warnDuplicateIds(rows, count, "ownedPartRecords");

    std::vector<Packet> out;
    out.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        out.push_back(ownedPartRecord(rows[i]));
    }
    return out;
}

Packet InventoryPackets::ownedPetList(const std::vector<PetRow>& rows) {
    const size_t count = capCount(rows.size(), CAP_PET, "ownedPetList");
    warnDuplicateIds(rows, count, "ownedPetList");

    Packet pkt = Packet::fromCmdFull(OP_OWNED_PET_LIST);
    pkt.writeInt32(static_cast<int32_t>(count));
    for (size_t i = 0; i < count; ++i) {
        const auto rec = petBlob(rows[i]);
        pkt.writeBytes(rec.data(), rec.size());
    }

    checkSize(pkt, 4 + SIZE_PET_REC * count, "ownedPetList");
    return pkt;
}

Packet InventoryPackets::appendCharacter(const CharacterRow& row) {
    Packet pkt = Packet::fromCmdFull(OP_APPEND_CHARACTER);
    const auto rec = characterBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, SIZE_CHARACTER_REC, "appendCharacter");
    return pkt;
}

Packet InventoryPackets::appendKart(const KartRow& row) {
    Packet pkt = Packet::fromCmdFull(OP_APPEND_KART);
    const auto rec = kartBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, SIZE_KART_REC, "appendKart");
    return pkt;
}

Packet InventoryPackets::appendPart(const PartRow& row) {
    Packet pkt = Packet::fromCmdFull(OP_APPEND_PART);
    const auto rec = partBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, SIZE_PART_REC, "appendPart");
    return pkt;
}

Packet InventoryPackets::buyAckCharacter(uint32_t currencyB, uint32_t currencyA,
                                         const CharacterRow& row) {
    Packet pkt = buyAckBase(CAT_CHARACTER, currencyB, currencyA);
    const auto rec = characterBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, ACK_HEAD_BUY + SIZE_CHARACTER_REC, "buyAckCharacter");
    return pkt;
}

Packet InventoryPackets::buyAckKart(uint32_t currencyB, uint32_t currencyA, const KartRow& row) {
    Packet pkt = buyAckBase(CAT_KART, currencyB, currencyA);
    const auto rec = kartBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, ACK_HEAD_BUY + SIZE_KART_REC, "buyAckKart");
    return pkt;
}

Packet InventoryPackets::buyAckItem(uint32_t currencyB, uint32_t currencyA, const ItemRow& row) {
    Packet pkt = buyAckBase(CAT_ITEM, currencyB, currencyA);
    const auto rec = itemBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, ACK_HEAD_BUY + SIZE_ITEM_REC, "buyAckItem");
    return pkt;
}

Packet InventoryPackets::buyAckPart(uint32_t currencyB, uint32_t currencyA, const PartRow& row) {
    Packet pkt = buyAckBase(CAT_PART, currencyB, currencyA);
    const auto rec = partBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, ACK_HEAD_BUY + SIZE_PART_REC, "buyAckPart");
    return pkt;
}

Packet InventoryPackets::buyAckPet(uint32_t currencyB, uint32_t currencyA, const PetRow& row) {
    Packet pkt = buyAckBase(CAT_PET, currencyB, currencyA);
    const auto rec = petBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, ACK_HEAD_BUY + SIZE_PET_REC, "buyAckPet");
    return pkt;
}

Packet InventoryPackets::deleteAck(uint32_t category, uint32_t baseKey) {
    // pet and car craft do nothing here so caller must resend the pet list
    if (category == CAT_PET || category == CAT_CAR_CRAFT) {
        LOG_WARN("PACKET", "deleteAck category " + std::to_string(category) +
                           " is a client no-op, follow it with a full list replace");
    }

    Packet pkt = Packet::fromCmdFull(OP_DELETE);
    pkt.writeUInt32(category);
    pkt.writeUInt32(baseKey);

    checkSize(pkt, 8, "deleteAck");
    return pkt;
}

Packet InventoryPackets::selectCharacterAck(const CharacterRow& row) {
    Packet pkt = installAckBase(CAT_CHARACTER);
    const auto rec = characterBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, ACK_HEAD_CAT + SIZE_CHARACTER_REC, "selectCharacterAck");
    return pkt;
}

Packet InventoryPackets::selectKartAck(const KartRow& row) {
    Packet pkt = installAckBase(CAT_KART);
    const auto rec = kartBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, ACK_HEAD_CAT + SIZE_KART_REC, "selectKartAck");
    return pkt;
}

Packet InventoryPackets::useItemAck(const ItemRow& row) {
    Packet pkt = installAckBase(CAT_ITEM);
    const auto rec = itemBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, ACK_HEAD_CAT + SIZE_ITEM_REC, "useItemAck");
    return pkt;
}

Packet InventoryPackets::useItemAckWithKartPeriod(const ItemRow& row, const KartRow& selectedKart) {
    Packet pkt = installAckBase(CAT_ITEM);
    const auto rec = itemBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    // tail lands straight in the selected kart period block so send only for def types 4 to 7
    pkt.writeUInt32(selectedKart.priceKey);
    pkt.writeUInt32(selectedKart.periodMode);
    pkt.writeUInt32(selectedKart.periodValue);
    pkt.writeUInt32(selectedKart.activeFlag);

    checkSize(pkt, ACK_HEAD_CAT + SIZE_ITEM_REC + 16, "useItemAckWithKartPeriod");
    return pkt;
}

Packet InventoryPackets::equipPartAck(const PartRow& row) {
    Packet pkt = installAckBase(CAT_PART);
    const auto rec = partBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, ACK_HEAD_CAT + SIZE_PART_REC, "equipPartAck");
    return pkt;
}

Packet InventoryPackets::equipPetAck(uint32_t subOp, const PetRow& newPet) {
    // sub op two means a second record follows so refuse it here
    if (subOp == 2) {
        LOG_ERROR("PACKET", "equipPetAck called with subOp 2, use equipPetAckReplace");
    }

    Packet pkt = installAckBase(CAT_PET);
    pkt.writeUInt32(subOp);
    const auto rec = petBlob(newPet);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, ACK_HEAD_CAT + 4 + SIZE_PET_REC, "equipPetAck");
    return pkt;
}

Packet InventoryPackets::equipPetAckReplace(const PetRow& previousPet, const PetRow& newPet) {
    Packet pkt = installAckBase(CAT_PET);
    pkt.writeUInt32(2);

    const auto prev = petBlob(previousPet);
    pkt.writeBytes(prev.data(), prev.size());
    const auto rec = petBlob(newPet);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, ACK_HEAD_CAT + 4 + SIZE_PET_REC * 2, "equipPetAckReplace");
    return pkt;
}

Packet InventoryPackets::unequipItemAck(const ItemRow& row) {
    Packet pkt = removeAckBase(CAT_ITEM);
    const auto rec = itemBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, ACK_HEAD_CAT + SIZE_ITEM_REC, "unequipItemAck");
    return pkt;
}

Packet InventoryPackets::unequipPartAck(const PartRow& row) {
    Packet pkt = removeAckBase(CAT_PART);
    const auto rec = partBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, ACK_HEAD_CAT + SIZE_PART_REC, "unequipPartAck");
    return pkt;
}

Packet InventoryPackets::unequipPetAck(const PetRow& row) {
    Packet pkt = removeAckBase(CAT_PET);
    const auto rec = petBlob(row);
    pkt.writeBytes(rec.data(), rec.size());

    checkSize(pkt, ACK_HEAD_CAT + SIZE_PET_REC, "unequipPetAck");
    return pkt;
}

Packet InventoryPackets::unequipNoopAck(uint32_t category) {
    // any record after these categories stays in the frame and desyncs the parse
    if (category == CAT_ITEM || category == CAT_PART || category == CAT_PET) {
        LOG_ERROR("PACKET", "unequipNoopAck called with category " + std::to_string(category) +
                            " which expects a record");
    }

    Packet pkt = removeAckBase(category);

    checkSize(pkt, ACK_HEAD_CAT, "unequipNoopAck");
    return pkt;
}

Packet InventoryPackets::applyEquipmentSet(uint32_t selectedCharacterInstanceId,
                                           uint32_t selectedKartInstanceId,
                                           const CharacterRow& characterRow,
                                           const KartRow& kartRow) {
    Packet pkt = Packet::fromCmdFull(OP_APPLY_SET);
    pkt.writeUInt32(selectedCharacterInstanceId);
    pkt.writeUInt32(selectedKartInstanceId);
    // zero count picks the character plus kart shape not the room craft loop
    pkt.writeInt32(0);

    const auto charRec = characterBlob(characterRow);
    pkt.writeBytes(charRec.data(), charRec.size());
    const auto kartRec = kartBlob(kartRow);
    pkt.writeBytes(kartRec.data(), kartRec.size());

    checkSize(pkt, 12 + SIZE_CHARACTER_REC + SIZE_KART_REC, "applyEquipmentSet");
    return pkt;
}

Packet InventoryPackets::setRemainingUses(uint32_t itemInstanceId, int32_t remainingUses) {
    Packet pkt = Packet::fromCmdFull(OP_SET_REMAINING_USES);
    pkt.writeUInt32(itemInstanceId);
    pkt.writeInt32(remainingUses);

    checkSize(pkt, 8, "setRemainingUses");
    return pkt;
}

bool InventoryPackets::parseBuy(const Packet& pkt, BuyRequest& out) {
    const auto& payload = pkt.payload();
    Cursor cur(payload.data(), payload.size());

    uint32_t category = 0;
    uint32_t baseKey = 0;
    uint32_t priceKey = 0;
    if (!cur.readU32(category) || !cur.readU32(baseKey) || !cur.readU32(priceKey)) {
        LOG_WARN("PACKET", "parseBuy short payload " + std::to_string(payload.size()));
        return false;
    }

    std::u16string name;
    if (!cur.readWString(name, 64)) {
        LOG_WARN("PACKET", "parseBuy account name unterminated or too long");
        return false;
    }

    out.category = category;
    out.baseKey = baseKey;
    out.priceKey = static_cast<int32_t>(priceKey);
    out.accountName = name;
    return true;
}

bool InventoryPackets::parseDelete(const Packet& pkt, DeleteRequest& out) {
    const auto& payload = pkt.payload();
    Cursor cur(payload.data(), payload.size());

    uint32_t category = 0;
    uint32_t baseKey = 0;
    if (!cur.readU32(category) || !cur.readU32(baseKey)) {
        LOG_WARN("PACKET", "parseDelete short payload " + std::to_string(payload.size()));
        return false;
    }

    out.category = category;
    out.baseKey = baseKey;
    return true;
}

bool InventoryPackets::parseInstall(const Packet& pkt, InstallRequest& out) {
    const auto& payload = pkt.payload();
    Cursor cur(payload.data(), payload.size());

    uint32_t category = 0;
    uint32_t baseKey = 0;
    uint32_t aux = 0;
    if (!cur.readU32(category) || !cur.readU32(baseKey) || !cur.readU32(aux)) {
        LOG_WARN("PACKET", "parseInstall short payload " + std::to_string(payload.size()));
        return false;
    }

    out.category = category;
    out.baseKey = baseKey;
    out.aux = static_cast<int32_t>(aux);
    return true;
}

bool InventoryPackets::parseRemove(const Packet& pkt, RemoveRequest& out) {
    const auto& payload = pkt.payload();
    Cursor cur(payload.data(), payload.size());

    uint32_t category = 0;
    uint32_t baseKey = 0;
    if (!cur.readU32(category) || !cur.readU32(baseKey)) {
        LOG_WARN("PACKET", "parseRemove short payload " + std::to_string(payload.size()));
        return false;
    }

    out.category = category;
    out.baseKey = baseKey;
    return true;
}

}  // namespace knc
