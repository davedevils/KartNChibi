#include "GameServerInternal.h"
#include "GameServer.h"
#include "handlers/GachaHandler.h"
#include "packets/gen/InventoryPackets.h"
#include "packets/gen/RoomCraftPackets.h"
#include "packets/gen/CustomCarPackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"
#include "util/ShopRules.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace knc {

namespace {

const char* const TAG = "GACHA";

using DbRow = std::map<std::string, std::string>;
using GP = GachaPetPackets;

constexpr size_t kFrameLimit = 0x2000;
constexpr size_t kFrameHeader = 8;

// 0x00B7 and 0x0135 use a different enum the gacha one has no pet value
constexpr uint32_t kShopCatCharacter = 0;
constexpr uint32_t kShopCatKart      = 1;
constexpr uint32_t kShopCatItem      = 2;
constexpr uint32_t kShopCatPart      = 3;
constexpr uint32_t kShopCatRoomCraft = 5;
constexpr uint32_t kShopCatCarCraft  = 6;

std::string colStr(const DbRow& row, const char* key) {
    return rowStrCore(row, key);
}

uint32_t colU32(const DbRow& row, const char* key) {
    return static_cast<uint32_t>(rowUInt64Throwing(row, key, 0));
}

int32_t colI32(const DbRow& row, const char* key) {
    return static_cast<int32_t>(rowInt64Throwing(row, key, 0));
}

uint32_t leadingCategory(const Packet& pkt) {
    const std::vector<uint8_t>& body = pkt.payload();
    if (body.size() < 4) return 0xFFFFFFFFu;
    return static_cast<uint32_t>(body[0]) |
           (static_cast<uint32_t>(body[1]) << 8) |
           (static_cast<uint32_t>(body[2]) << 16) |
           (static_cast<uint32_t>(body[3]) << 24);
}

// one oversize frame kills the client parser so refuse instead of sending it
bool sendChecked(const Session::Ptr& session, const Packet& pkt, const char* what) {
    const size_t total = pkt.payload().size() + kFrameHeader;
    if (total >= kFrameLimit) {
        LOG_ERROR(TAG, std::string(what) + " frame " + std::to_string(total) +
                       " over the client recv limit " + std::to_string(kFrameLimit) +
                       " not sent");
        return false;
    }
    session->send(pkt);
    return true;
}

// batches whole packets into one write so the client parse loop never splits a burst
class Burst {
public:
    void add(const Packet& pkt) {
        auto data = pkt.serialize();
        if (data.size() + kFrameHeader >= kFrameLimit) {
            LOG_ERROR(TAG, "burst frame over the client recv limit, dropped");
            return;
        }
        if (!m_buf.empty() && m_buf.size() + data.size() > kFrameLimit) {
            m_chunks.push_back(std::move(m_buf));
            m_buf.clear();
        }
        m_buf.insert(m_buf.end(), data.begin(), data.end());
        ++m_count;
    }

    void add(const std::vector<Packet>& packets) {
        for (const Packet& p : packets) add(p);
    }

    void flush(const Session::Ptr& session) {
        if (!m_buf.empty()) {
            m_chunks.push_back(std::move(m_buf));
            m_buf.clear();
        }
        for (auto& chunk : m_chunks) session->send(chunk);
        m_chunks.clear();
    }

    size_t count() const { return m_count; }

private:
    std::vector<std::vector<uint8_t>> m_chunks;
    std::vector<uint8_t> m_buf;
    size_t m_count = 0;
};

// forced roll slot consumed once so a test can pin the outcome without a rebuild
std::mutex g_forcedMutex;
std::unordered_map<uint32_t, uint64_t> g_forcedRolls;

bool takeForcedRoll(uint32_t characterId, uint64_t& valueOut) {
    std::lock_guard<std::mutex> lock(g_forcedMutex);
    auto it = g_forcedRolls.find(characterId);
    if (it == g_forcedRolls.end()) return false;
    valueOut = it->second;
    g_forcedRolls.erase(it);
    return true;
}

// every byte the client will read after the echoed ticket row
struct GrantedPrize {
    uint32_t category = GP::GC_NONE;
    // sub 47D5E0 case 5 reads blob 0x84 flag byte then blob 0x34 looked up by sub 4501D0
    bool hasSlotRecord = false;
    std::array<uint8_t, GP::SIZE_CHAR_REC> charRec{};
    std::array<uint8_t, GP::SIZE_KART_REC> kartRec{};
    std::array<uint8_t, GP::SIZE_SMALL_REC> smallRec{};
    std::array<uint8_t, GP::SIZE_ROOMCRAFT_REC> roomRec{};
    std::array<uint8_t, GP::SIZE_CARCRAFT_REC> carRec{};
    std::array<uint8_t, GP::SIZE_CARCRAFT_SLOT_REC> slotRec{};
};

// upserts one owned row and hands back its wire instance id
bool upsertOwned(Transaction& tx, const char* table, uint32_t characterId, uint32_t baseKey,
                 const char* insertSql, const DbParams& insertParams,
                 const char* updateSql, const DbParams& updateParams,
                 uint32_t& instanceIdOut) {
    auto existing = tx.query(std::string("SELECT id FROM ") + table +
                             " WHERE character_id = ? AND base_key = ? LIMIT 1",
                             { characterId, baseKey });
    if (!existing.empty()) {
        instanceIdOut = colU32(existing[0], "id");
        if (!tx.execute(updateSql, updateParams)) return false;
        return instanceIdOut != 0;
    }
    if (!tx.execute(insertSql, insertParams)) return false;
    instanceIdOut = static_cast<uint32_t>(tx.lastInsertId());
    return instanceIdOut != 0;
}

bool grantCharacter(Transaction& tx, uint32_t characterId, const GP::GachaPrizeEntry& entry,
                    GrantedPrize& out) {
    uint32_t instanceId = 0;
    if (!upsertOwned(tx, "owned_character", characterId, entry.prizeBaseKey,
                     "INSERT INTO owned_character (character_id, base_key, period_mode, "
                     "period_value, active_flag) VALUES (?, ?, ?, ?, 1)",
                     { characterId, entry.prizeBaseKey, entry.periodMode, entry.periodValue },
                     "UPDATE owned_character SET period_mode = ?, period_value = ?, "
                     "active_flag = 1 WHERE character_id = ? AND base_key = ?",
                     { entry.periodMode, entry.periodValue, characterId, entry.prizeBaseKey },
                     instanceId)) {
        return false;
    }

    InventoryPackets::CharacterRow row;
    row.instanceId = instanceId;
    row.baseKey = entry.prizeBaseKey;
    row.periodMode = entry.periodMode;
    row.periodValue = static_cast<uint32_t>(entry.periodValue < 0 ? 0 : entry.periodValue);
    row.activeFlag = 1;
    out.charRec = InventoryPackets::characterBlob(row);
    out.category = GP::GC_CHARACTER;
    return true;
}

bool grantKart(Transaction& tx, uint32_t characterId, const GP::GachaPrizeEntry& entry,
               GrantedPrize& out) {
    uint32_t instanceId = 0;
    if (!upsertOwned(tx, "owned_kart", characterId, entry.prizeBaseKey,
                     "INSERT INTO owned_kart (character_id, base_key, period_mode, "
                     "period_value, active_flag, skin_primary, skin_secondary) VALUES (?, ?, ?, ?, 1, ?, ?)",
                     { characterId, entry.prizeBaseKey, entry.periodMode, entry.periodValue,
                       kDefaultPaintKey, kDefaultPlateKey },
                     "UPDATE owned_kart SET period_mode = ?, period_value = ?, "
                     "active_flag = 1 WHERE character_id = ? AND base_key = ?",
                     { entry.periodMode, entry.periodValue, characterId, entry.prizeBaseKey },
                     instanceId)) {
        return false;
    }

    InventoryPackets::KartRow row;
    row.instanceId = instanceId;
    row.baseKey = entry.prizeBaseKey;
    row.periodMode = entry.periodMode;
    row.periodValue = static_cast<uint32_t>(entry.periodValue < 0 ? 0 : entry.periodValue);
    row.activeFlag = 1;
    // a factory chassis won runs on the durability bar like a bought one
    if (CustomCarPackets::applyFactoryDurability(tx, instanceId)) {
        row.periodMode = CustomCarPackets::FACTORY_PERIOD_MODE;
        row.periodValue = static_cast<uint32_t>(CustomCarPackets::FACTORY_START_DURABILITY);
    }
    out.kartRec = InventoryPackets::kartBlob(row);
    out.category = GP::GC_KART;

    // login burst ships 0x00C0 only for karts already owned so a fresh win has no def
    LOG_WARN(TAG, "kart prize " + std::to_string(entry.prizeBaseKey) + " for character " +
                  std::to_string(characterId) + " is granted but this session never got its "
                  "0x00C0 catalog row, sub_44F6F0 returns null so the reveal icon and the "
                  "garage entry stay blank until the player reconnects");
    return true;
}

bool grantItem(Transaction& tx, uint32_t characterId, const GP::GachaPrizeEntry& entry,
               GrantedPrize& out) {
    // count mode stacks so the win adds to what is already held
    const char* updateSql =
        entry.periodMode == InventoryPackets::PERIOD_COUNT
            ? "UPDATE owned_item SET period_mode = ?, period_value = period_value + ?, "
              "active_flag = 1 WHERE character_id = ? AND base_key = ?"
            : "UPDATE owned_item SET period_mode = ?, period_value = ?, "
              "active_flag = 1 WHERE character_id = ? AND base_key = ?";

    uint32_t instanceId = 0;
    if (!upsertOwned(tx, "owned_item", characterId, entry.prizeBaseKey,
                     "INSERT INTO owned_item (character_id, base_key, period_mode, "
                     "period_value, active_flag, in_use_flag) VALUES (?, ?, ?, ?, 1, 0)",
                     { characterId, entry.prizeBaseKey, entry.periodMode, entry.periodValue },
                     updateSql,
                     { entry.periodMode, entry.periodValue, characterId, entry.prizeBaseKey },
                     instanceId)) {
        return false;
    }

    auto rows = tx.query(
        "SELECT period_value FROM owned_item WHERE character_id = ? AND base_key = ? LIMIT 1",
        { characterId, entry.prizeBaseKey });
    if (rows.empty()) return false;

    InventoryPackets::ItemRow row;
    row.instanceId = instanceId;
    row.baseKey = entry.prizeBaseKey;
    row.periodMode = entry.periodMode;
    row.periodValue = colI32(rows[0], "period_value");
    row.activeFlag = 1;
    out.smallRec = InventoryPackets::itemBlob(row);
    out.category = GP::GC_ITEM;
    return true;
}

bool grantPart(Transaction& tx, uint32_t characterId, const GP::GachaPrizeEntry& entry,
               GrantedPrize& out) {
    uint32_t instanceId = 0;
    if (!upsertOwned(tx, "owned_part", characterId, entry.prizeBaseKey,
                     "INSERT INTO owned_part (character_id, base_key, period_mode, "
                     "period_value, active_flag) VALUES (?, ?, ?, ?, 1)",
                     { characterId, entry.prizeBaseKey, entry.periodMode, entry.periodValue },
                     "UPDATE owned_part SET period_mode = ?, period_value = ?, "
                     "active_flag = 1 WHERE character_id = ? AND base_key = ?",
                     { entry.periodMode, entry.periodValue, characterId, entry.prizeBaseKey },
                     instanceId)) {
        return false;
    }

    InventoryPackets::PartRow row;
    row.instanceId = instanceId;
    row.baseKey = entry.prizeBaseKey;
    row.periodMode = entry.periodMode;
    row.periodValue = static_cast<uint32_t>(entry.periodValue < 0 ? 0 : entry.periodValue);
    row.activeFlag = 1;
    out.smallRec = InventoryPackets::partBlob(row);
    out.category = GP::GC_PART;
    return true;
}

bool grantRoomCraft(Transaction& tx, uint32_t characterId, const GP::GachaPrizeEntry& entry,
                    GrantedPrize& out) {
    auto defs = tx.query("SELECT category FROM room_object_def WHERE object_key = ? LIMIT 1",
                         { entry.prizeBaseKey });
    if (defs.empty()) {
        LOG_ERROR(TAG, "room craft prize key " + std::to_string(entry.prizeBaseKey) +
                       " has no room_object_def row so the client would drop it");
        return false;
    }
    const uint32_t category = colU32(defs[0], "category");

    auto existing = tx.query(
        "SELECT instance_id, period_value FROM player_item_instance "
        "WHERE player_id = ? AND object_key = ? ORDER BY instance_id LIMIT 1",
        { characterId, entry.prizeBaseKey });

    uint32_t instanceId = 0;
    uint32_t periodValue = static_cast<uint32_t>(entry.periodValue < 0 ? 0 : entry.periodValue);
    if (existing.empty()) {
        if (!tx.execute(
                "INSERT INTO player_item_instance (player_id, object_key, category, placed, "
                "price_key, period_type, period_value, active) VALUES (?, ?, ?, 0, 0, ?, ?, 1)",
                { characterId, entry.prizeBaseKey, category, entry.periodMode, periodValue })) {
            return false;
        }
        instanceId = static_cast<uint32_t>(tx.lastInsertId());
    } else {
        instanceId = colU32(existing[0], "instance_id");
        // client merges on rec plus 0x00 and only updates the count at rec plus 0x28
        periodValue = colU32(existing[0], "period_value") + periodValue;
        if (!tx.execute(
                "UPDATE player_item_instance SET period_type = ?, period_value = ?, active = 1 "
                "WHERE instance_id = ?",
                { entry.periodMode, periodValue, instanceId })) {
            return false;
        }
    }
    if (instanceId == 0) return false;

    RoomObjectInstance inst;
    inst.instanceId = instanceId;
    inst.objectKey = entry.prizeBaseKey;
    inst.category = category;
    inst.placedFlag = 0;
    inst.priceKey = 0;
    inst.periodType = entry.periodMode;
    inst.periodValue = periodValue;
    inst.activeFlag = 1;
    out.roomRec = RoomCraftPackets::decorRecord(inst);
    out.category = GP::GC_ROOMCRAFT;
    return true;
}

bool grantCarCraft(Transaction& tx, uint32_t characterId, const GP::GachaPrizeEntry& entry,
                   GrantedPrize& out) {
    auto defs = tx.query("SELECT category FROM carcraft_part_def "
                         "WHERE part_key = ? AND enabled = 1 LIMIT 1",
                         { entry.prizeBaseKey });
    if (defs.empty()) {
        LOG_ERROR(TAG, "car craft prize key " + std::to_string(entry.prizeBaseKey) +
                       " has no enabled carcraft_part_def row so the client would drop it");
        return false;
    }
    const uint32_t category = colU32(defs[0], "category");

    auto existing = tx.query(
        "SELECT instance_id FROM custom_car_part_instance "
        "WHERE character_id = ? AND part_key = ? LIMIT 1",
        { characterId, entry.prizeBaseKey });

    uint32_t instanceId = 0;
    if (existing.empty()) {
        if (!tx.execute(
                "INSERT INTO custom_car_part_instance (character_id, part_key, category, "
                "equip_refcount, price_table_key, period_type, period_value, period_active, "
                "grade) VALUES (?, ?, ?, 0, 0, ?, ?, 1, 0)",
                { characterId, entry.prizeBaseKey, category, entry.periodMode,
                  entry.periodValue })) {
            return false;
        }
        instanceId = static_cast<uint32_t>(tx.lastInsertId());
    } else {
        instanceId = colU32(existing[0], "instance_id");
        if (!tx.execute(
                "UPDATE custom_car_part_instance SET period_type = ?, period_value = ?, "
                "period_active = 1 WHERE instance_id = ?",
                { entry.periodMode, entry.periodValue, instanceId })) {
            return false;
        }
    }
    if (instanceId == 0) return false;

    CarPartInstance inst;
    inst.instanceId = instanceId;
    inst.partKey = entry.prizeBaseKey;
    inst.category = category;
    inst.equipRefcount = 0;
    inst.priceTableKey = 0;
    inst.periodType = entry.periodMode;
    inst.periodValue = entry.periodValue;
    inst.periodActive = 1;
    inst.grade = 0;
    out.carRec = CustomCarPackets::partInstanceRecord(inst);
    // sub 4501D0 only overwrites an existing preset so a grant touching none must send zero
    out.hasSlotRecord = false;
    out.category = GP::GC_CARCRAFT;
    return true;
}

bool grantPrizeInTx(Transaction& tx, uint32_t characterId, const GP::GachaPrizeEntry& entry,
                    GrantedPrize& out) {
    switch (entry.prizeCategory) {
        case GP::GC_CHARACTER: return grantCharacter(tx, characterId, entry, out);
        case GP::GC_KART:      return grantKart(tx, characterId, entry, out);
        case GP::GC_ITEM:      return grantItem(tx, characterId, entry, out);
        case GP::GC_PART:      return grantPart(tx, characterId, entry, out);
        case GP::GC_ROOMCRAFT: return grantRoomCraft(tx, characterId, entry, out);
        case GP::GC_CARCRAFT:  return grantCarCraft(tx, characterId, entry, out);
        default:
            LOG_ERROR(TAG, "prize category " + std::to_string(entry.prizeCategory) +
                           " has no grant path");
            return false;
    }
}

// gacha reveal uses categories 4 and 5 the shop enum uses 4 5 and 6
bool shopCategoryToGacha(uint32_t shopCategory, uint32_t& gachaCategoryOut) {
    switch (shopCategory) {
        case kShopCatCharacter: gachaCategoryOut = GP::GC_CHARACTER; return true;
        case kShopCatKart:      gachaCategoryOut = GP::GC_KART;      return true;
        case kShopCatItem:      gachaCategoryOut = GP::GC_ITEM;      return true;
        case kShopCatPart:      gachaCategoryOut = GP::GC_PART;      return true;
        case kShopCatRoomCraft: gachaCategoryOut = GP::GC_ROOMCRAFT; return true;
        case kShopCatCarCraft:  gachaCategoryOut = GP::GC_CARCRAFT;  return true;
        default: return false;
    }
}

GP::ItemRow ticketFromRequest(const GP::GachaRollRequest& req) {
    GP::ItemRow row;
    row.instanceId = req.instanceId;
    row.baseKey = req.itemBaseKey;
    row.priceKey = req.purchaseOptionKey;
    row.periodMode = req.periodMode;
    row.periodValue = req.remainingRolls;
    row.activeFlag = req.activeFlag;
    row.inUseFlag = req.inUseFlag;
    return row;
}

std::string rawHex(const std::array<uint8_t, GP::SIZE_TICKET_REC>& raw) {
    static const char* d = "0123456789ABCDEF";
    std::string out;
    out.reserve(raw.size() * 3);
    for (size_t i = 0; i < raw.size(); ++i) {
        if (i) out.push_back(' ');
        out.push_back(d[(raw[i] >> 4) & 0xF]);
        out.push_back(d[raw[i] & 0xF]);
    }
    return out;
}

void sendNoPrize(const Session::Ptr& session, const GP::ItemRow& ticket, const char* why) {
    LOG_WARN(TAG, std::string("no prize for character ") +
                  std::to_string(session->characterId) + " reason " + why);
    sendChecked(session, GP::resultNoPrize(ticket), "gacha no prize");
}

void sendPrizeResult(const Session::Ptr& session, const GP::GachaHeader& header,
                     const GP::ItemRow& ticket, const GrantedPrize& prize) {
    const size_t tail = GP::prizeTailSize(prize.category, prize.hasSlotRecord);
    const size_t frame = GP::GACHA_HEADER_SIZE + GP::SIZE_TICKET_REC + tail + kFrameHeader;
    if (frame >= kFrameLimit) {
        LOG_ERROR(TAG, "gacha result frame " + std::to_string(frame) +
                       " over the client recv limit, refusing the prize frame");
        sendNoPrize(session, ticket, "frame over the recv limit");
        return;
    }

    switch (prize.category) {
        case GP::GC_CHARACTER:
            sendChecked(session, GP::resultCharacter(header, ticket, prize.charRec),
                        "gacha character");
            break;
        case GP::GC_KART:
            sendChecked(session, GP::resultKart(header, ticket, prize.kartRec), "gacha kart");
            break;
        case GP::GC_ITEM:
            sendChecked(session, GP::resultItem(header, ticket, prize.smallRec), "gacha item");
            break;
        case GP::GC_PART:
            sendChecked(session, GP::resultPart(header, ticket, prize.smallRec), "gacha part");
            break;
        case GP::GC_ROOMCRAFT:
            sendChecked(session, GP::resultRoomCraft(header, ticket, prize.roomRec),
                        "gacha room craft");
            break;
        case GP::GC_CARCRAFT:
            sendChecked(session,
                        GP::resultCarCraft(header, ticket, prize.carRec,
                                           prize.hasSlotRecord ? &prize.slotRec : nullptr),
                        "gacha car craft");
            break;
        default:
            LOG_ERROR(TAG, "granted prize category " + std::to_string(prize.category) +
                           " has no result builder");
            sendNoPrize(session, ticket, "granted category has no builder");
            break;
    }
}

bool findPetDef(uint32_t petBaseKey, GP::PetDef& out) {
    for (const GP::PetDef& d : GP::loadPetDefs()) {
        if (d.petBaseKey == petBaseKey) {
            out = d;
            return true;
        }
    }
    return false;
}

}  // namespace

void GachaHandler::forceNextRoll(uint32_t characterId, uint64_t rollValue) {
    std::lock_guard<std::mutex> lock(g_forcedMutex);
    g_forcedRolls[characterId] = rollValue;
    LOG_WARN(TAG, "next roll of character " + std::to_string(characterId) +
                  " is pinned to value " + std::to_string(rollValue));
}

void GachaHandler::handleRoll(Session::Ptr session, Packet& packet, GameServer* server) {
    const uint32_t characterId = session->characterId;
    if (characterId == 0) {
        LOG_ERROR(TAG, "gacha roll from " + session->remoteAddress() + " with no character");
        return;
    }

    GP::GachaRollRequest req;
    if (!GP::parseRoll(packet, req)) return;

    LOG_INFO(TAG, "C2S 0x00ED character " + std::to_string(characterId) + " ticket instance " +
                  std::to_string(req.instanceId) + " base key " +
                  std::to_string(req.itemBaseKey) + " client count " +
                  std::to_string(req.remainingRolls) + " raw [" + rawHex(req.raw) + "]");

    if (req.itemBaseKey != GP::TICKET_BASE_KEY && req.itemBaseKey != GP::GOLD_COIN_BASE_KEY) {
        LOG_ERROR(TAG, "gacha roll base key " + std::to_string(req.itemBaseKey) +
                       " is not a coin key, expected " +
                       std::to_string(GP::TICKET_BASE_KEY) + " or " +
                       std::to_string(GP::GOLD_COIN_BASE_KEY));
        sendNoPrize(session, ticketFromRequest(req), "base key is not a ticket");
        return;
    }

    GP::ItemRow ticket;
    if (!GP::loadTicket(characterId, req.itemBaseKey, ticket)) {
        sendNoPrize(session, ticketFromRequest(req), "server holds no ticket row");
        return;
    }
    if (!GP::rollRequestMatchesTicket(req, ticket)) {
        sendNoPrize(session, ticket, "request does not match the server ticket");
        return;
    }

    std::vector<GP::GachaPrizeEntry> pool = GP::loadPrizePool(req.itemBaseKey);
    const size_t dropped = GP::sanitizePrizePool(pool, req.itemBaseKey);
    if (dropped != 0) {
        LOG_ERROR(TAG, "prize pool for ticket " + std::to_string(req.itemBaseKey) +
                       " still held " + std::to_string(dropped) +
                       " unsafe rows after the loader sweep");
    }
    if (pool.empty()) {
        // never spend a ticket against a pool that cannot exist
        sendNoPrize(session, ticket, "prize pool is empty");
        return;
    }

    const uint64_t weight = GP::totalWeight(pool);
    if (weight == 0) {
        // a table with rows but no weight is a deliberate blank ticket so it still burns
        LOG_WARN(TAG, "prize pool for ticket " + std::to_string(req.itemBaseKey) +
                      " has zero total weight so the ticket burns with no prize");
        GP::GachaRollOutcome miss;
        GP::ItemRow updated;
        if (!GP::consumeTicketAndLog(characterId, req, miss, updated)) {
            sendNoPrize(session, ticket, "blank ticket consume failed");
            return;
        }
        const GP::ItemRow expected = GP::decrementedTicket(ticket);
        if (updated.periodValue != expected.periodValue) {
            LOG_WARN(TAG, "ticket count after the blank burn is " +
                          std::to_string(updated.periodValue) + " expected " +
                          std::to_string(expected.periodValue));
        }
        sendNoPrize(session, updated, "blank ticket");
        return;
    }

    GP::GachaRollOutcome outcome;
    uint64_t forced = 0;
    if (takeForcedRoll(characterId, forced)) {
        LOG_WARN(TAG, "roll of character " + std::to_string(characterId) +
                      " is forced to value " + std::to_string(forced) + " not random");
        outcome = GP::rollPrizeWithValue(pool, forced);
    } else {
        outcome = GP::rollPrize(pool);
    }
    if (!outcome.hasPrize) {
        sendNoPrize(session, ticket, "weighted pick produced nothing");
        return;
    }

    Transaction tx = Database::instance().beginTransaction();
    if (!tx.valid()) {
        LOG_ERROR(TAG, "gacha roll cannot open a transaction so nothing is spent");
        sendNoPrize(session, ticket, "no transaction");
        return;
    }

    GP::ItemRow updated;
    if (!GP::consumeTicketInTx(tx, characterId, req.itemBaseKey, updated)) {
        tx.rollback();
        sendNoPrize(session, ticket, "ticket decrement touched no row");
        return;
    }

    GrantedPrize prize;
    if (!grantPrizeInTx(tx, characterId, outcome.entry, prize)) {
        tx.rollback();
        sendNoPrize(session, ticket, "prize grant failed");
        return;
    }

    if (!GP::logRollInTx(tx, characterId, req, outcome, updated.periodValue)) {
        LOG_ERROR(TAG, "gacha log insert failed so the whole roll rolls back");
        tx.rollback();
        sendNoPrize(session, ticket, "roll log failed");
        return;
    }

    if (!tx.commit()) {
        LOG_ERROR(TAG, "gacha roll commit failed for character " + std::to_string(characterId));
        sendNoPrize(session, ticket, "commit failed");
        return;
    }

    const GP::ItemRow expected = GP::decrementedTicket(ticket);
    if (updated.periodValue != expected.periodValue) {
        LOG_WARN(TAG, "ticket count after the spend is " + std::to_string(updated.periodValue) +
                      " expected " + std::to_string(expected.periodValue));
    }

    const GP::GachaHeader header = GP::headerFor(outcome.entry);
    LOG_INFO(TAG, "character " + std::to_string(characterId) + " won category " +
                  std::to_string(header.prizeCategory) + " key " +
                  std::to_string(header.prizeBaseKey) + " rare " +
                  std::to_string(header.rareFlag) + " table index " +
                  std::to_string(outcome.index) + " tickets left " +
                  std::to_string(updated.periodValue));

    sendPrizeResult(session, header, updated, prize);
    // a chassis prize gets its slot and basic set after the reveal
    if (prize.category == GP::GC_KART && server) server->carCraft().pushFactoryLoadout(session);
}

void GachaHandler::sendRawPrize(Session::Ptr session, const GP::GachaHeader& header,
                                const GP::ItemRow& ticket, const std::vector<uint8_t>& tail) {
    const size_t expected = GP::prizeTailSize(header.prizeCategory, false);
    const size_t expectedWithSlot = GP::prizeTailSize(header.prizeCategory, true);
    if (tail.size() != expected && tail.size() != expectedWithSlot) {
        LOG_ERROR(TAG, "raw prize tail " + std::to_string(tail.size()) + " for category " +
                       std::to_string(header.prizeCategory) + " expected " +
                       std::to_string(expected) + " so the frame is refused");
        return;
    }
    sendChecked(session,
                GP::result(header, ticket, tail.empty() ? nullptr : tail.data(), tail.size()),
                "gacha raw prize");
}

void GachaHandler::sendRandomReward(Session::Ptr session, uint32_t category,
                                    uint32_t prizeBaseKey, int32_t resultCode) {
    const uint32_t characterId = session->characterId;
    if (characterId == 0) {
        LOG_ERROR(TAG, "random reward with no character on " + session->remoteAddress());
        return;
    }
    if (category == GP::CAT_PET) {
        // sub 47EEF0 reveal switch has no case for category 4 so its tail goes unread
        LOG_ERROR(TAG, "random reward category 4 is pet and the client has no case, refused");
        return;
    }

    uint32_t gachaCategory = GP::GC_NONE;
    if (!shopCategoryToGacha(category, gachaCategory)) {
        LOG_ERROR(TAG, "random reward category " + std::to_string(category) +
                       " is outside the shop enum, refused");
        return;
    }

    GP::GachaPrizeEntry entry;
    entry.prizeCategory = gachaCategory;
    entry.prizeBaseKey = prizeBaseKey;
    entry.periodMode = GP::GP_PERMANENT;
    entry.periodValue = 0;

    Transaction tx = Database::instance().beginTransaction();
    if (!tx.valid()) {
        LOG_ERROR(TAG, "random reward cannot open a transaction so nothing is granted");
        return;
    }

    GrantedPrize prize;
    if (!grantPrizeInTx(tx, characterId, entry, prize)) {
        tx.rollback();
        return;
    }
    if (!tx.commit()) {
        LOG_ERROR(TAG, "random reward commit failed for character " +
                       std::to_string(characterId));
        return;
    }

    GP::RandomRewardHeader header;
    header.resultCode = resultCode;
    header.category = category;

    if (category == kShopCatCarCraft) {
        sendChecked(session,
                    GP::randomRewardCarCraft(header, prize.carRec,
                                             prize.hasSlotRecord ? &prize.slotRec : nullptr),
                    "random reward car craft");
        return;
    }

    std::vector<uint8_t> tail;
    switch (gachaCategory) {
        case GP::GC_CHARACTER: tail.assign(prize.charRec.begin(), prize.charRec.end()); break;
        case GP::GC_KART:      tail.assign(prize.kartRec.begin(), prize.kartRec.end()); break;
        case GP::GC_ITEM:
        case GP::GC_PART:      tail.assign(prize.smallRec.begin(), prize.smallRec.end()); break;
        case GP::GC_ROOMCRAFT: tail.assign(prize.roomRec.begin(), prize.roomRec.end()); break;
        default: break;
    }

    const size_t expected = GP::randomRewardTailSize(category, false);
    if (tail.size() != expected) {
        LOG_ERROR(TAG, "random reward tail " + std::to_string(tail.size()) + " for category " +
                       std::to_string(category) + " expected " + std::to_string(expected));
        return;
    }

    sendChecked(session, GP::randomReward(header, tail.data(), tail.size()), "random reward");
    LOG_INFO(TAG, "random reward category " + std::to_string(category) + " key " +
                  std::to_string(prizeBaseKey) + " granted to character " +
                  std::to_string(characterId));
}

size_t GachaHandler::sendPetCatalog(Session::Ptr session) {
    std::vector<GP::PetDef> defs = GP::loadPetDefs();

    std::vector<GP::PetDef> safe;
    safe.reserve(defs.size());
    for (GP::PetDef& d : defs) {
        if (!GP::modelFolderIsShipped(d.modelFolder)) {
            // loader returns zero on a missing folder and the driver binds to slot 0
            LOG_ERROR(TAG, "pet " + std::to_string(d.petBaseKey) + " folder '" + d.modelFolder +
                           "' is not shipped so the row is dropped from the catalog");
            continue;
        }
        if (!GP::petKeyHasRaceEffect(d.petBaseKey)) {
            LOG_WARN(TAG, "pet " + std::to_string(d.petBaseKey) +
                          " is outside 10 20 30 40 so it will render but grant no effect");
        }
        if (d.options.size() > GP::CAP_PET_OPTIONS) {
            LOG_WARN(TAG, "pet " + std::to_string(d.petBaseKey) + " has " +
                          std::to_string(d.options.size()) + " options over the client cap " +
                          std::to_string(GP::CAP_PET_OPTIONS));
        }
        safe.push_back(std::move(d));
    }

    if (safe.size() > GP::CAP_PET_DEFS) {
        LOG_ERROR(TAG, "pet catalog " + std::to_string(safe.size()) + " over the client cap " +
                       std::to_string(GP::CAP_PET_DEFS));
    }

    Burst burst;
    burst.add(GP::petCatalog(safe));
    burst.flush(session);

    LOG_INFO(TAG, "pet catalog published " + std::to_string(burst.count()) + " rows");
    return burst.count();
}

void GachaHandler::sendPetDefinition(Session::Ptr session, uint32_t petBaseKey) {
    GP::PetDef def;
    if (!findPetDef(petBaseKey, def)) {
        LOG_ERROR(TAG, "pet definition " + std::to_string(petBaseKey) + " is not in the db");
        return;
    }
    sendChecked(session, GP::petDefinition(def), "pet definition");
}

void GachaHandler::sendOwnedPets(Session::Ptr session) {
    const uint32_t characterId = session->characterId;
    if (characterId == 0) return;

    std::vector<GP::PetRow> rows = GP::loadOwnedPets(characterId);
    const size_t cleared = GP::enforceSingleEquipped(rows);
    if (cleared != 0) {
        LOG_ERROR(TAG, "character " + std::to_string(characterId) + " had " +
                       std::to_string(cleared) +
                       " extra equipped pet rows, the db mirror is out of sync");
    }
    if (rows.size() > GP::CAP_OWNED_PETS) {
        LOG_ERROR(TAG, "character " + std::to_string(characterId) + " owns " +
                       std::to_string(rows.size()) + " pets over the client cap " +
                       std::to_string(GP::CAP_OWNED_PETS));
    }

    if (!sendChecked(session, GP::ownedPetList(rows), "owned pet list")) return;

    LOG_INFO(TAG, "owned pets sent " + std::to_string(rows.size()) + " rows equipped key " +
                  std::to_string(GP::equippedPetBaseKey(rows)));
}

uint32_t GachaHandler::equippedPetBaseKey(uint32_t characterId) {
    if (characterId == 0) return 0;

    const uint32_t fromRows = GP::equippedPetBaseKey(GP::loadOwnedPets(characterId));
    const uint32_t fromMirror = GP::loadEquippedPetBaseKey(characterId);
    if (fromRows != fromMirror) {
        // room and race read the mirror so a drift makes the pet vanish for others
        LOG_WARN(TAG, "character " + std::to_string(characterId) + " owned pet flag says " +
                      std::to_string(fromRows) + " but the mirror says " +
                      std::to_string(fromMirror) + " so the mirror wins");
    }
    return fromMirror;
}

void GachaHandler::onCharacterEnter(Session::Ptr session, GameServer* server) {
    (void)server;
    sendPetCatalog(session);
    sendOwnedPets(session);
}

bool GachaHandler::isPetBuy(const Packet& packet) {
    return packet.payload().size() >= 14 && leadingCategory(packet) == GP::CAT_PET;
}

bool GachaHandler::isPetEquip(const Packet& packet) {
    return packet.payload().size() == 12 && leadingCategory(packet) == GP::CAT_PET;
}

bool GachaHandler::isPetUnequip(const Packet& packet) {
    return packet.payload().size() == 8 && leadingCategory(packet) == GP::CAT_PET;
}

void GachaHandler::handlePetBuy(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;

    const uint32_t characterId = session->characterId;
    if (characterId == 0) {
        LOG_ERROR(TAG, "pet buy from " + session->remoteAddress() + " with no character");
        return;
    }

    GP::PetBuyRequest req;
    if (!GP::parseBuy(packet, req)) return;
    if (req.category != GP::CAT_PET) {
        LOG_ERROR(TAG, "pet buy routed with category " + std::to_string(req.category));
        return;
    }

    // trailing wstring is the launcher session token so it is never joined on
    LOG_INFO(TAG, "C2S 0x00B7 pet buy character " + std::to_string(characterId) + " key " +
                  std::to_string(req.petBaseKey) + " price key " + std::to_string(req.priceKey) +
                  " token chars " + std::to_string(req.sessionToken.size()));

    if (req.priceKey < 0) {
        LOG_WARN(TAG, "pet buy price key is minus one so the client failed to resolve it");
        return;
    }

    GP::PetDef def;
    if (!findPetDef(req.petBaseKey, def)) {
        LOG_ERROR(TAG, "pet buy for unknown key " + std::to_string(req.petBaseKey));
        return;
    }
    if (def.shopVisibleFlag == 0) {
        LOG_WARN(TAG, "pet " + std::to_string(req.petBaseKey) + " is hidden from the shop");
        return;
    }
    if (!GP::modelFolderIsShipped(def.modelFolder)) {
        LOG_ERROR(TAG, "pet " + std::to_string(req.petBaseKey) + " folder '" + def.modelFolder +
                       "' is not shipped so the buy is refused");
        return;
    }
    if (!GP::petKeyHasRaceEffect(req.petBaseKey)) {
        LOG_WARN(TAG, "pet " + std::to_string(req.petBaseKey) +
                      " has no race effect so the buyer gets a cosmetic only");
    }
    if (!GP::hasPetCondition(characterId, def.requiredPendantKey)) {
        LOG_WARN(TAG, "character " + std::to_string(characterId) +
                      " lacks pet condition key " + std::to_string(def.requiredPendantKey));
        return;
    }

    const uint32_t priceKey = static_cast<uint32_t>(req.priceKey);
    bool priceBelongsToPet = false;
    uint32_t optionPeriodMode = 0;
    int32_t optionPeriodValue = 0;
    for (const GP::PetOption& o : def.options) {
        if (o.priceOptionKey == priceKey) {
            priceBelongsToPet = true;
            optionPeriodMode = o.periodMode;
            optionPeriodValue = o.periodValue;
            break;
        }
    }
    if (!priceBelongsToPet) {
        LOG_ERROR(TAG, "price key " + std::to_string(priceKey) + " does not belong to pet " +
                       std::to_string(req.petBaseKey) + " so the buy is refused");
        return;
    }

    auto priceRows = Database::instance().queryPrepared(
        "SELECT price_base, price_sale, currency FROM shop_price WHERE price_key = ? LIMIT 1",
        { priceKey });
    if (priceRows.empty()) {
        LOG_ERROR(TAG, "price key " + std::to_string(priceKey) + " has no shop_price row");
        return;
    }
    const int32_t priceSale = colI32(priceRows[0], "price_sale");
    const int32_t priceBase = colI32(priceRows[0], "price_base");
    const int32_t amount = priceCharged(priceBase, priceSale);
    // the client prints a sale price with the Astro icon so the sale is charged in Astro
    const bool useCash = priceIsAstro(priceSale);

    Transaction tx = Database::instance().beginTransaction();
    if (!tx.valid()) {
        LOG_ERROR(TAG, "pet buy cannot open a transaction");
        return;
    }

    auto wallet = tx.query("SELECT gold, cash FROM characters WHERE id = ? FOR UPDATE",
                           { characterId });
    if (wallet.empty()) {
        tx.rollback();
        return;
    }
    int32_t gold = colI32(wallet[0], "gold");
    int32_t cash = colI32(wallet[0], "cash");

    if ((useCash && cash < amount) || (!useCash && gold < amount)) {
        LOG_INFO(TAG, "character " + std::to_string(characterId) +
                      " cannot afford pet " + std::to_string(req.petBaseKey));
        tx.rollback();
        return;
    }

    if (useCash) {
        cash -= amount;
        if (!tx.execute("UPDATE characters SET cash = ? WHERE id = ?", { cash, characterId })) {
            tx.rollback();
            return;
        }
    } else {
        gold -= amount;
        if (!tx.execute("UPDATE characters SET gold = ? WHERE id = ?", { gold, characterId })) {
            tx.rollback();
            return;
        }
    }

    auto owned = tx.query("SELECT id FROM owned_pet WHERE character_id = ? AND base_key = ? LIMIT 1",
                          { characterId, req.petBaseKey });
    uint32_t instanceId = 0;
    if (owned.empty()) {
        if (!tx.execute(
                "INSERT INTO owned_pet (character_id, base_key, equipped_flag, period_mode, "
                "period_value, active_flag) VALUES (?, ?, 0, ?, ?, 1)",
                { characterId, req.petBaseKey, optionPeriodMode, optionPeriodValue })) {
            tx.rollback();
            return;
        }
        instanceId = static_cast<uint32_t>(tx.lastInsertId());
    } else {
        instanceId = colU32(owned[0], "id");
        if (!tx.execute(
                "UPDATE owned_pet SET period_mode = ?, period_value = period_value + ?, "
                "active_flag = 1 WHERE id = ?",
                { optionPeriodMode, optionPeriodValue, instanceId })) {
            tx.rollback();
            return;
        }
    }

    if (!tx.execute(
            "INSERT INTO shop_transaction_log (character_id, verb, category, base_key, "
            "price_key, currency, amount, gold_before, gold_after, cash_before, cash_after, "
            "result) VALUES (?, 'buy', 4, ?, ?, ?, ?, ?, ?, ?, ?, 'ok')",
            { characterId, req.petBaseKey, priceKey, useCash ? 1 : 0, amount,
              useCash ? gold : gold + amount, gold,
              useCash ? cash + amount : cash, cash })) {
        tx.rollback();
        return;
    }

    if (instanceId == 0 || !tx.commit()) {
        LOG_ERROR(TAG, "pet buy commit failed for character " + std::to_string(characterId));
        return;
    }

    std::vector<GP::PetRow> rows = GP::loadOwnedPets(characterId);
    const GP::PetRow* bought = nullptr;
    for (const GP::PetRow& r : rows) {
        if (r.instanceId == instanceId) {
            bought = &r;
            break;
        }
    }
    if (bought == nullptr) {
        LOG_ERROR(TAG, "pet row " + std::to_string(instanceId) + " vanished after the buy");
        return;
    }

    sendChecked(session, GP::buyAck(static_cast<uint32_t>(gold), static_cast<uint32_t>(cash),
                                    *bought),
                "pet buy ack");
    LOG_INFO(TAG, "character " + std::to_string(characterId) + " bought pet " +
                  std::to_string(req.petBaseKey) + " for " + std::to_string(amount) +
                  (useCash ? " cash" : " gold"));
}

void GachaHandler::handlePetEquip(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;

    const uint32_t characterId = session->characterId;
    if (characterId == 0) return;

    GP::PetEquipRequest req;
    if (!GP::parseEquip(packet, req)) return;
    if (req.category != GP::CAT_PET) {
        LOG_ERROR(TAG, "pet equip routed with category " + std::to_string(req.category));
        return;
    }
    if (req.aux != -1) {
        LOG_WARN(TAG, "pet equip aux " + std::to_string(req.aux) +
                      " is not the expected minus one so the frame may be forged");
    }

    std::vector<GP::PetRow> rows = GP::loadOwnedPets(characterId);
    GP::enforceSingleEquipped(rows);

    GP::PetRow target;
    bool found = false;
    GP::PetRow previous;
    bool hasPrevious = false;
    for (const GP::PetRow& r : rows) {
        if (r.baseKey == req.petBaseKey) {
            target = r;
            found = true;
        } else if (r.equippedFlag == 1) {
            previous = r;
            hasPrevious = true;
        }
    }
    if (!found) {
        LOG_ERROR(TAG, "character " + std::to_string(characterId) + " does not own pet " +
                       std::to_string(req.petBaseKey));
        return;
    }
    if (target.activeFlag == 0) {
        LOG_WARN(TAG, "pet " + std::to_string(req.petBaseKey) + " is expired so it stays off");
        return;
    }
    // ack resolves by instance uid with no null check so an unknown uid writes to 0x8
    if (!GP::ackRowIsSafe(rows, target.instanceId)) return;
    if (hasPrevious && !GP::ackRowIsSafe(rows, previous.instanceId)) return;

    if (!GP::setEquippedPet(characterId, req.petBaseKey)) {
        LOG_ERROR(TAG, "pet equip persist failed for character " + std::to_string(characterId));
        return;
    }

    target.equippedFlag = 1;
    if (hasPrevious) {
        previous.equippedFlag = 0;
        sendChecked(session, GP::equipAckReplace(previous, target), "pet equip swap");
    } else {
        sendChecked(session, GP::equipAck(target), "pet equip");
    }

    LOG_INFO(TAG, "character " + std::to_string(characterId) + " equipped pet " +
                  std::to_string(req.petBaseKey) + " effect key " +
                  std::to_string(equippedPetBaseKey(characterId)));
}

void GachaHandler::handlePetUnequip(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;

    const uint32_t characterId = session->characterId;
    if (characterId == 0) return;

    GP::PetUnequipRequest req;
    if (!GP::parseUnequip(packet, req)) return;
    if (req.category != GP::CAT_PET) {
        LOG_ERROR(TAG, "pet unequip routed with category " + std::to_string(req.category));
        return;
    }

    std::vector<GP::PetRow> rows = GP::loadOwnedPets(characterId);
    GP::PetRow target;
    bool found = false;
    for (const GP::PetRow& r : rows) {
        if (r.baseKey == req.petBaseKey) {
            target = r;
            found = true;
            break;
        }
    }
    if (!found) {
        LOG_ERROR(TAG, "character " + std::to_string(characterId) + " does not own pet " +
                       std::to_string(req.petBaseKey));
        return;
    }
    if (!GP::ackRowIsSafe(rows, target.instanceId)) return;

    if (!GP::clearEquippedPet(characterId, req.petBaseKey)) {
        LOG_ERROR(TAG, "pet unequip persist failed for character " + std::to_string(characterId));
        return;
    }

    target.equippedFlag = 0;
    sendChecked(session, GP::unequipAck(target), "pet unequip");
    LOG_INFO(TAG, "character " + std::to_string(characterId) + " unequipped pet " +
                  std::to_string(req.petBaseKey));
}

} // namespace knc
