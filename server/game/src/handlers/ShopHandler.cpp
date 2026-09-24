// shop stage verbs on ShopPackets

#include "GameServerInternal.h"
#include "handlers/ShopHandler.h"

#include "GameServer.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"
#include "util/KartDurability.h"
#include "util/ShopRules.h"
#include "packets/gen/CustomCarPackets.h"
#include "packets/gen/InventoryPackets.h"
#include "packets/gen/ProgressionPackets.h"
#include "packets/gen/RoomCraftPackets.h"

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace knc {

namespace {

using DbRow = std::map<std::string, std::string>;

// sub 452830 keyed container so a prop copy is a new row not an overwrite
constexpr uint32_t ROOM_CATEGORY_PROP = 3;

constexpr size_t GIFT_NAME_MAX_BYTES = 32;
constexpr size_t GIFT_MESSAGE_MAX_BYTES = 512;

// sub 478D40 reads the wide cstr into a bare wchar 160 stack cell
constexpr size_t DIALOG_WIDE_MAX_CHARS = 128;

// sub 4806F0 screen id that lands on the same stage as plain C2S 0x0010
constexpr uint32_t STAGE_SHOP = 7;

int64_t colI64(const DbRow& row, const char* key, int64_t def = 0) {
    return rowInt64NoThrow(row, key, def);
}

uint32_t colU32(const DbRow& row, const char* key) {
    const int64_t v = colI64(row, key, 0);
    if (v < 0) return 0;
    return static_cast<uint32_t>(v);
}

int32_t colI32(const DbRow& row, const char* key) {
    return static_cast<int32_t>(colI64(row, key, 0));
}

float colF32(const DbRow& row, const char* key) {
    return rowFloatNoThrow(row, key, 0.0f);
}

std::string toUtf8(const std::u16string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (size_t i = 0; i < s.size(); ++i) {
        uint32_t cp = static_cast<uint32_t>(s[i]);
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size()) {
            const uint32_t lo = static_cast<uint32_t>(s[i + 1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                ++i;
            }
        }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

// cut on a codepoint edge else the insert dies on broken utf eight
std::string clampUtf8(std::string s, size_t maxBytes) {
    if (s.size() <= maxBytes) return s;
    s.resize(maxBytes);
    while (!s.empty() && (static_cast<uint8_t>(s.back()) & 0xC0) == 0x80) s.pop_back();
    if (!s.empty() && (static_cast<uint8_t>(s.back()) & 0x80) != 0) s.pop_back();
    return s;
}

// client compares against sub 451C30 which is 365 times tm year plus tm yday minus 365
uint32_t dayNumberToday() {
    const std::time_t now = std::time(nullptr);
    std::tm parts{};
#if defined(_WIN32)
    localtime_s(&parts, &now);
#else
    localtime_r(&now, &parts);
#endif
    const int64_t v = 365LL * parts.tm_year + parts.tm_yday - 365LL;
    if (v < 0) return 0;
    return static_cast<uint32_t>(v);
}

const char* purchaseResultName(ShopPackets::PurchaseResult result) {
    switch (result) {
        case ShopPackets::PurchaseResult::Ok:                return "Ok";
        case ShopPackets::PurchaseResult::BadCategory:       return "BadCategory";
        case ShopPackets::PurchaseResult::ClientResolveFail: return "ClientResolveFail";
        case ShopPackets::PurchaseResult::UnknownPrice:      return "UnknownPrice";
        case ShopPackets::PurchaseResult::UnknownDefinition: return "UnknownDefinition";
        case ShopPackets::PurchaseResult::OptionMismatch:    return "OptionMismatch";
        case ShopPackets::PurchaseResult::NotVisible:        return "NotVisible";
        case ShopPackets::PurchaseResult::LevelTooLow:       return "LevelTooLow";
        case ShopPackets::PurchaseResult::MissingCondition:  return "MissingCondition";
        case ShopPackets::PurchaseResult::NotEnoughMoney:    return "NotEnoughMoney";
        case ShopPackets::PurchaseResult::DbError:           return "DbError";
    }
    return "DbError";
}

// only refusal channel that releases the wait modal
void rejectPurchase(const Session::Ptr& session, ShopPackets::PurchaseResult result,
                    bool cashCurrency = false) {
    session->send(ShopPackets::rejectAscii(
        ShopPackets::resultMessageKey(result, cashCurrency), 1));
}

void rejectGeneric(const Session::Ptr& session) {
    session->send(ShopPackets::rejectAscii("MSG_UNKNOWN_ERROR", 1));
}

// 0x0002 shows the utf sixteen literally so a name or a number can appear
void rejectLiteral(const Session::Ptr& session, std::u16string text) {
    if (text.size() > DIALOG_WIDE_MAX_CHARS) text.resize(DIALOG_WIDE_MAX_CHARS);
    session->send(ShopPackets::rejectWide(text, 1));
}

std::u16string u16FromAscii(const char* s) {
    std::u16string out;
    for (; s != nullptr && *s != '\0'; ++s) out.push_back(static_cast<char16_t>(*s));
    return out;
}

std::u16string u16FromInt(int64_t v) {
    return u16FromAscii(std::to_string(v).c_str());
}

// per session catalog claim every client append is blind so twice duplicates

std::mutex g_catalogMutex;
std::set<std::weak_ptr<Session>, std::owner_less<std::weak_ptr<Session>>> g_priceSent;

bool claimPriceTable(const Session::Ptr& session) {
    std::lock_guard<std::mutex> lock(g_catalogMutex);
    for (auto it = g_priceSent.begin(); it != g_priceSent.end();) {
        if (it->expired()) it = g_priceSent.erase(it);
        else ++it;
    }
    return g_priceSent.insert(std::weak_ptr<Session>(session)).second;
}

std::vector<ShopPackets::PriceRow> loadPriceTable() {
    std::vector<ShopPackets::PriceRow> out;
    auto rows = Database::instance().queryPrepared(
        "SELECT price_key, unit_type, unit_amount, price_base, price_sale, currency "
        "FROM shop_price ORDER BY price_key",
        {});
    out.reserve(rows.size());
    for (const auto& row : rows) {
        ShopPackets::PriceRow p;
        p.priceKey   = colU32(row, "price_key");
        p.unitType   = colU32(row, "unit_type");
        p.unitAmount = colU32(row, "unit_amount");
        p.priceBase  = colI32(row, "price_base");
        p.priceSale  = colI32(row, "price_sale");
        p.currency   = ShopPackets::currencyOf(p.priceSale);
        out.push_back(p);
    }
    return out;
}

// sub 451DC0 caps at 0x600 and drops silently so stop loudly out collects for a worker thread
size_t sendPriceTableOnce(const Session::Ptr& session, std::vector<Packet>* out = nullptr) {
    if (!claimPriceTable(session)) return 0;

    const std::vector<ShopPackets::PriceRow> rows = loadPriceTable();
    size_t sent = 0;
    for (const auto& row : rows) {
        if (sent >= ShopPackets::CAP_PRICE_ROWS) {
            LOG_ERROR("SHOP", "shop_price has " + std::to_string(rows.size()) +
                              " rows client keeps " +
                              std::to_string(ShopPackets::CAP_PRICE_ROWS) +
                              " every tile past that is unbuyable");
            break;
        }
        if (out) out->push_back(ShopPackets::priceRow(row));
        else session->send(ShopPackets::priceRow(row));
        ++sent;
    }
    return sent;
}

// price quote the only place a charge amount comes from

struct Quote {
    ShopPackets::PriceRow row;
    int32_t cost = 0;
    bool ok = false;
};

// re read at charge time so an edited or pulled option cannot bill a stale price
Quote quoteOption(uint32_t category, uint32_t baseKey, int32_t priceKey) {
    Quote q;
    if (priceKey < 0) return q;

    const uint32_t key = static_cast<uint32_t>(priceKey);
    if (!ShopPackets::loadPrice(key, q.row)) return q;
    if (!ShopPackets::optionExists(category, baseKey, key)) return q;

    q.cost = ShopPackets::chargedAmount(q.row);
    q.ok = q.cost >= 0;
    return q;
}

bool loadDefinitionLevelReq(uint32_t category, uint32_t baseKey, int32_t& out) {
    auto rows = Database::instance().queryPrepared(
        "SELECT level_req FROM shop_definition WHERE category = ? AND base_key = ? LIMIT 1",
        {category, baseKey});
    if (rows.empty()) return false;
    out = colI32(rows[0], "level_req");
    return true;
}

uint32_t currencyCode(ShopPackets::Currency currency) {
    return currency == ShopPackets::Currency::Cash ? 1u : 0u;
}

void logShopTx(Transaction& tx, uint32_t characterId, const char* verb, uint32_t category,
               uint32_t baseKey, int32_t priceKey, ShopPackets::Currency currency, int32_t amount,
               const ShopPackets::Wallet& before, const ShopPackets::Wallet& after,
               const char* result) {
    tx.execute(
        "INSERT INTO shop_transaction_log (character_id, verb, category, base_key, price_key, "
        "currency, amount, gold_before, gold_after, cash_before, cash_after, result) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        {characterId, verb, category, baseKey, priceKey, currencyCode(currency), amount,
         before.gp, after.gp, before.cash, after.cash, result});
}

void logShopRefusal(uint32_t characterId, const char* verb, uint32_t category, uint32_t baseKey,
                    int32_t priceKey, const char* result) {
    if (characterId == 0) return;
    Database::instance().executePrepared(
        "INSERT INTO shop_transaction_log (character_id, verb, category, base_key, price_key, "
        "result) VALUES (?, ?, ?, ?, ?, ?)",
        {characterId, verb, category, baseKey, priceKey, result});
}

// period price unit type drives the owned record period pair

struct Period {
    uint32_t mode  = InventoryPackets::PERIOD_PERMANENT;
    int64_t  value = 0;
};

Period periodAfterPurchase(uint32_t category, const ShopPackets::PriceRow& price, uint32_t oldMode,
                           int64_t oldValue, uint32_t baseKey = 0) {
    Period p;
    // a coin is a count the gacha popup spends whatever unit its price row prints
    if (category == ShopPackets::CAT_ITEM && isGachaCoinKey(baseKey)) {
        const CoinPeriod c = coinPeriodAfterPurchase(oldMode, oldValue, price.unitType, price.unitAmount);
        p.mode  = c.mode;
        p.value = c.value;
        return p;
    }
    switch (price.unitType) {
        case ShopPackets::UNIT_DAYS: {
            const int64_t today = static_cast<int64_t>(dayNumberToday());
            const int64_t from = (oldMode == InventoryPackets::PERIOD_DAY && oldValue > today)
                                     ? oldValue
                                     : today;
            p.mode  = InventoryPackets::PERIOD_DAY;
            p.value = from + static_cast<int64_t>(price.unitAmount);
            break;
        }
        case ShopPackets::UNIT_USES: {
            const int64_t held = (oldMode == InventoryPackets::PERIOD_COUNT && oldValue > 0)
                                     ? oldValue
                                     : 0;
            p.mode  = InventoryPackets::PERIOD_COUNT;
            p.value = held + static_cast<int64_t>(price.unitAmount);
            break;
        }
        case ShopPackets::UNIT_DURABILITY: {
            // a kart takes the durability capped an item is a repair scroll and takes one use
            const KartPeriod k = category == ShopPackets::CAT_KART
                                     ? kartPeriodAfterDurabilityGrant(oldMode, oldValue, price.unitAmount)
                                     : scrollPeriodAfterGrant(oldMode, oldValue);
            p.mode  = k.mode;
            p.value = k.value;
            break;
        }
        default:
            p.mode  = InventoryPackets::PERIOD_PERMANENT;
            p.value = 0;
            break;
    }
    return p;
}

// grants one row per owned container

const char* ownedTable(uint32_t category) {
    switch (category) {
        case ShopPackets::CAT_CHARACTER: return "owned_character";
        case ShopPackets::CAT_KART:      return "owned_kart";
        case ShopPackets::CAT_ITEM:      return "owned_item";
        case ShopPackets::CAT_PART:      return "owned_part";
        case ShopPackets::CAT_PET:       return "owned_pet";
        default:                         return nullptr;
    }
}

// client append helpers drop or smash past these so never hand out row cap plus one
int64_t containerCap(uint32_t category) {
    switch (category) {
        case ShopPackets::CAT_CHARACTER: return 64;
        case ShopPackets::CAT_KART:      return 64;
        case ShopPackets::CAT_ITEM:      return 64;
        case ShopPackets::CAT_PART:      return 256;
        case ShopPackets::CAT_PET:       return 64;
        case ShopPackets::CAT_ROOMCRAFT: return 256;
        case ShopPackets::CAT_CARCRAFT:  return 256;
        default:                         return 0;
    }
}

bool containerHasRoom(Transaction& tx, const std::string& sql, const DbParams& params,
                      uint32_t category) {
    auto rows = tx.query(sql, params);
    if (rows.empty()) return true;
    return colI64(rows[0], "n") < containerCap(category);
}

// table name comes from the switch above never from the wire
bool grantSimple(Transaction& tx, uint32_t category, uint32_t characterId, uint32_t baseKey,
                 uint32_t instanceUid, const ShopPackets::PriceRow& price, uint32_t& idOut) {
    const char* table = ownedTable(category);
    if (table == nullptr) return false;
    const std::string t(table);

    std::vector<DbRow> rows;
    if (instanceUid != 0) {
        rows = tx.query("SELECT id, period_mode, period_value FROM " + t +
                        " WHERE id = ? AND character_id = ? FOR UPDATE",
                        {instanceUid, characterId});
    } else {
        rows = tx.query("SELECT id, period_mode, period_value FROM " + t +
                        " WHERE character_id = ? AND base_key = ? FOR UPDATE",
                        {characterId, baseKey});
    }

    if (!rows.empty()) {
        const uint32_t id = colU32(rows[0], "id");
        const Period p = periodAfterPurchase(category, price, colU32(rows[0], "period_mode"),
                                             colI64(rows[0], "period_value"), baseKey);
        if (!tx.execute("UPDATE " + t +
                        " SET period_mode = ?, period_value = ?, active_flag = 1 WHERE id = ?",
                        {p.mode, p.value, id})) {
            return false;
        }
        // a factory chassis stays on the durability bar a renewal fills it again
        if (category == ShopPackets::CAT_KART) CustomCarPackets::applyFactoryDurability(tx, id);
        idOut = id;
        return id != 0;
    }

    // renew needs the row so never create one here
    if (instanceUid != 0) return false;

    if (!containerHasRoom(tx, "SELECT COUNT(*) AS n FROM " + t + " WHERE character_id = ?",
                          {characterId}, category)) {
        LOG_WARN("SHOP", "Container full cat=" + std::to_string(category) +
                         " char=" + std::to_string(characterId));
        return false;
    }

    const Period p = periodAfterPurchase(category, price, 0, 0, baseKey);
    bool inserted = false;
    if (category == ShopPackets::CAT_KART) {
        // a kart row carries the red paint and the standard plate or the stock car build returns null
        inserted = tx.execute("INSERT INTO owned_kart (character_id, base_key, period_mode, period_value, "
                              "active_flag, skin_primary, skin_secondary) VALUES (?, ?, ?, ?, 1, ?, ?)",
                              {characterId, baseKey, p.mode, p.value, kDefaultPaintKey, kDefaultPlateKey});
    } else {
        inserted = tx.execute("INSERT INTO " + t +
                              " (character_id, base_key, period_mode, period_value, active_flag) "
                              "VALUES (?, ?, ?, ?, 1)",
                              {characterId, baseKey, p.mode, p.value});
    }
    if (!inserted) {
        return false;
    }
    idOut = static_cast<uint32_t>(tx.lastInsertId());
    if (idOut == 0) return false;
    if (category == ShopPackets::CAT_KART) CustomCarPackets::applyFactoryDurability(tx, idOut);

    // a bought driver needs its three BODYSET keys or the bone is bare and the chibi invisible
    if (category == ShopPackets::CAT_CHARACTER) {
        if (!tx.execute(
                "UPDATE owned_character oc JOIN drivers d ON d.id = oc.base_key SET "
                "oc.acc_body = COALESCE((SELECT MIN(s.skin_key) FROM def_kart_skin s WHERE s.category = 2 "
                "AND s.name LIKE CONCAT(d.name, '\\_char\\_body\\_%')), 0), "
                "oc.acc_face = COALESCE((SELECT MIN(s.skin_key) FROM def_kart_skin s WHERE s.category = 2 "
                "AND s.name LIKE CONCAT(d.name, '\\_char\\_face\\_%')), 0), "
                "oc.acc_head = COALESCE((SELECT MIN(s.skin_key) FROM def_kart_skin s WHERE s.category = 2 "
                "AND s.name LIKE CONCAT(d.name, '\\_char\\_head\\_%')), 0) "
                "WHERE oc.id = ? AND oc.character_id = ?",
                {idOut, characterId})) {
            return false;
        }
    }
    return true;
}

bool grantRoomCraft(Transaction& tx, uint32_t characterId, uint32_t objectKey, uint32_t instanceUid,
                    const ShopPackets::PriceRow& price, uint32_t& idOut) {
    if (instanceUid != 0) {
        auto rows = tx.query("SELECT instance_id, period_type, period_value FROM player_item_instance "
                             "WHERE instance_id = ? AND player_id = ? FOR UPDATE",
                             {instanceUid, characterId});
        if (rows.empty()) return false;
        const Period p = periodAfterPurchase(ShopPackets::CAT_ROOMCRAFT, price, colU32(rows[0], "period_type"),
                                             colI64(rows[0], "period_value"));
        if (!tx.execute("UPDATE player_item_instance SET price_key = ?, period_type = ?, "
                        "period_value = ?, active = 1 WHERE instance_id = ?",
                        {price.priceKey, p.mode, p.value, instanceUid})) {
            return false;
        }
        idOut = instanceUid;
        return true;
    }

    auto defs = tx.query("SELECT category FROM room_object_def "
                         "WHERE object_key = ? AND enabled = 1 LIMIT 1",
                         {objectKey});
    if (defs.empty()) return false;
    const uint32_t roomCategory = colU32(defs[0], "category");

    // client erases by base key first for every subtype but props
    if (roomCategory != ROOM_CATEGORY_PROP) {
        auto own = tx.query("SELECT instance_id, period_type, period_value FROM player_item_instance "
                            "WHERE player_id = ? AND object_key = ? "
                            "ORDER BY instance_id LIMIT 1 FOR UPDATE",
                            {characterId, objectKey});
        if (!own.empty()) {
            const uint32_t id = colU32(own[0], "instance_id");
            const Period p = periodAfterPurchase(ShopPackets::CAT_ROOMCRAFT, price, colU32(own[0], "period_type"),
                                                 colI64(own[0], "period_value"));
            if (!tx.execute("UPDATE player_item_instance SET category = ?, price_key = ?, "
                            "period_type = ?, period_value = ?, active = 1 WHERE instance_id = ?",
                            {roomCategory, price.priceKey, p.mode, p.value, id})) {
                return false;
            }
            idOut = id;
            return true;
        }
    }

    if (!containerHasRoom(tx, "SELECT COUNT(*) AS n FROM player_item_instance WHERE player_id = ?",
                          {characterId}, ShopPackets::CAT_ROOMCRAFT)) {
        LOG_WARN("SHOP", "Room craft container full char=" + std::to_string(characterId));
        return false;
    }

    const Period p = periodAfterPurchase(ShopPackets::CAT_ROOMCRAFT, price, 0, 0);
    if (!tx.execute("INSERT INTO player_item_instance (player_id, object_key, category, placed, "
                    "price_key, period_type, period_value, active) VALUES (?, ?, ?, 0, ?, ?, ?, 1)",
                    {characterId, objectKey, roomCategory, price.priceKey, p.mode, p.value})) {
        return false;
    }
    idOut = static_cast<uint32_t>(tx.lastInsertId());
    return idOut != 0;
}

bool grantCarCraft(Transaction& tx, uint32_t characterId, uint32_t partKey, uint32_t instanceUid,
                   const ShopPackets::PriceRow& price, uint32_t& idOut) {
    if (instanceUid != 0) {
        auto rows = tx.query("SELECT instance_id, period_type, period_value FROM custom_car_part_instance "
                             "WHERE instance_id = ? AND character_id = ? FOR UPDATE",
                             {instanceUid, characterId});
        if (rows.empty()) return false;
        const Period p = periodAfterPurchase(ShopPackets::CAT_CARCRAFT, price, colU32(rows[0], "period_type"),
                                             colI64(rows[0], "period_value"));
        if (!tx.execute("UPDATE custom_car_part_instance SET price_table_key = ?, period_type = ?, "
                        "period_value = ?, period_active = 1 WHERE instance_id = ?",
                        {price.priceKey, p.mode, p.value, instanceUid})) {
            return false;
        }
        idOut = instanceUid;
        return true;
    }

    auto defs = tx.query("SELECT category FROM carcraft_part_def "
                         "WHERE part_key = ? AND enabled = 1 LIMIT 1",
                         {partKey});
    if (defs.empty()) return false;
    const uint32_t partCategory = colU32(defs[0], "category");

    // client keeps one instance per part key so mirror that here
    auto own = tx.query("SELECT instance_id, period_type, period_value FROM custom_car_part_instance "
                        "WHERE character_id = ? AND part_key = ? "
                        "ORDER BY instance_id LIMIT 1 FOR UPDATE",
                        {characterId, partKey});
    if (!own.empty()) {
        const uint32_t id = colU32(own[0], "instance_id");
        const Period p = periodAfterPurchase(ShopPackets::CAT_CARCRAFT, price, colU32(own[0], "period_type"),
                                             colI64(own[0], "period_value"));
        if (!tx.execute("UPDATE custom_car_part_instance SET category = ?, price_table_key = ?, "
                        "period_type = ?, period_value = ?, period_active = 1 WHERE instance_id = ?",
                        {partCategory, price.priceKey, p.mode, p.value, id})) {
            return false;
        }
        idOut = id;
        return true;
    }

    if (!containerHasRoom(tx, "SELECT COUNT(*) AS n FROM custom_car_part_instance "
                              "WHERE character_id = ?",
                          {characterId}, ShopPackets::CAT_CARCRAFT)) {
        LOG_WARN("SHOP", "Car craft container full char=" + std::to_string(characterId));
        return false;
    }

    const Period p = periodAfterPurchase(ShopPackets::CAT_CARCRAFT, price, 0, 0);
    if (!tx.execute("INSERT INTO custom_car_part_instance (character_id, part_key, category, "
                    "equip_refcount, price_table_key, period_type, period_value, period_active, grade) "
                    "VALUES (?, ?, ?, 0, ?, ?, ?, 1, 0)",
                    {characterId, partKey, partCategory, price.priceKey, p.mode, p.value})) {
        return false;
    }
    idOut = static_cast<uint32_t>(tx.lastInsertId());
    return idOut != 0;
}

bool grantAny(Transaction& tx, uint32_t ownerId, uint32_t category, uint32_t baseKey,
              uint32_t instanceUid, const ShopPackets::PriceRow& price, uint32_t& idOut) {
    if (category <= ShopPackets::CAT_PET) {
        return grantSimple(tx, category, ownerId, baseKey, instanceUid, price, idOut);
    }
    if (category == ShopPackets::CAT_ROOMCRAFT) {
        return grantRoomCraft(tx, ownerId, baseKey, instanceUid, price, idOut);
    }
    if (category == ShopPackets::CAT_CARCRAFT) {
        return grantCarCraft(tx, ownerId, baseKey, instanceUid, price, idOut);
    }
    return false;
}

// charge plus grant one transaction

struct GiftInfo {
    std::string targetName;
    uint32_t    targetId = 0;
    std::string message;
};

struct ChargeOutcome {
    bool ok = false;
    bool insufficient = false;
    ShopPackets::Wallet before;
    ShopPackets::Wallet after;
    uint32_t instanceId = 0;
};

ChargeOutcome chargeAndGrant(uint32_t payerId, uint32_t ownerId, const char* verb,
                             uint32_t category, uint32_t baseKey, int32_t priceKey,
                             uint32_t instanceUid, const ShopPackets::PriceRow& price,
                             int32_t cost, const GiftInfo* gift) {
    ChargeOutcome out;

    auto tx = Database::instance().beginTransaction();
    if (!tx.valid()) {
        LOG_ERROR("SHOP", "beginTransaction failed for " + std::string(verb));
        return out;
    }

    // same row lock else double spend
    auto walletRows = tx.query("SELECT gold, cash FROM characters WHERE id = ? FOR UPDATE",
                               {payerId});
    if (walletRows.empty()) return out;
    out.before.gp   = colI32(walletRows[0], "gold");
    out.before.cash = colI32(walletRows[0], "cash");
    out.after = out.before;

    if (!ShopPackets::canAfford(out.before, price.currency, cost)) {
        out.insufficient = true;
        return out;
    }

    if (price.currency == ShopPackets::Currency::Cash) {
        out.after.cash = out.before.cash - cost;
        if (!tx.execute("UPDATE characters SET cash = ? WHERE id = ?", {out.after.cash, payerId})) {
            return out;
        }
    } else {
        out.after.gp = out.before.gp - cost;
        if (!tx.execute("UPDATE characters SET gold = ? WHERE id = ?", {out.after.gp, payerId})) {
            return out;
        }
    }

    if (!grantAny(tx, ownerId, category, baseKey, instanceUid, price, out.instanceId)) {
        LOG_WARN("SHOP", std::string(verb) + " grant failed cat=" + std::to_string(category) +
                         " base=" + std::to_string(baseKey) +
                         " owner=" + std::to_string(ownerId));
        return out;
    }

    if (gift != nullptr) {
        if (!tx.execute("INSERT INTO gift_log (sender_id, target_name, target_id, category, "
                        "base_key, price_key, message) VALUES (?, ?, ?, ?, ?, ?, ?)",
                        {payerId, gift->targetName, gift->targetId, category, baseKey,
                         priceKey, gift->message})) {
            return out;
        }
    }

    logShopTx(tx, payerId, verb, category, baseKey, priceKey, price.currency, cost,
              out.before, out.after, "Ok");

    if (!tx.commit()) {
        LOG_ERROR("SHOP", std::string(verb) + " commit failed char=" + std::to_string(payerId));
        return out;
    }

    out.ok = true;
    return out;
}

// owned record loaders the bytes the ack tail carries

bool loadCharacterRow(uint32_t id, InventoryPackets::CharacterRow& out) {
    auto rows = Database::instance().queryPrepared(
        "SELECT id, base_key, acc_body, acc_face, acc_head, acc_glass, acc_back, price_key, "
        "period_mode, period_value, active_flag FROM owned_character WHERE id = ? LIMIT 1",
        {id});
    if (rows.empty()) return false;
    out.instanceId  = colU32(rows[0], "id");
    out.baseKey     = colU32(rows[0], "base_key");
    out.accBody     = colI32(rows[0], "acc_body");
    out.accFace     = colI32(rows[0], "acc_face");
    out.accHead     = colI32(rows[0], "acc_head");
    out.accGlass    = colI32(rows[0], "acc_glass");
    out.accBack     = colI32(rows[0], "acc_back");
    out.priceKey    = colU32(rows[0], "price_key");
    out.periodMode  = colU32(rows[0], "period_mode");
    out.periodValue = colU32(rows[0], "period_value");
    out.activeFlag  = colU32(rows[0], "active_flag");
    return true;
}

bool loadKartRow(uint32_t id, InventoryPackets::KartRow& out) {
    auto rows = Database::instance().queryPrepared(
        "SELECT id, base_key, skin_primary, skin_secondary, skin_tertiary, custom3, custom4, "
        "custom5, applied_item_a, applied_item_b, price_key, period_mode, period_value, active_flag "
        "FROM owned_kart WHERE id = ? LIMIT 1",
        {id});
    if (rows.empty()) return false;
    out.instanceId    = colU32(rows[0], "id");
    out.baseKey       = colU32(rows[0], "base_key");
    out.skinPrimary   = colU32(rows[0], "skin_primary");
    out.skinSecondary = colU32(rows[0], "skin_secondary");
    out.skinTertiary  = colU32(rows[0], "skin_tertiary");
    out.custom3       = colU32(rows[0], "custom3");
    out.custom4       = colU32(rows[0], "custom4");
    out.custom5       = colU32(rows[0], "custom5");
    out.appliedItemA  = colU32(rows[0], "applied_item_a");
    out.appliedItemB  = colU32(rows[0], "applied_item_b");
    out.priceKey      = colU32(rows[0], "price_key");
    out.periodMode    = colU32(rows[0], "period_mode");
    out.periodValue   = colU32(rows[0], "period_value");
    out.activeFlag    = colU32(rows[0], "active_flag");
    return true;
}

bool loadItemRow(uint32_t id, InventoryPackets::ItemRow& out) {
    auto rows = Database::instance().queryPrepared(
        "SELECT id, base_key, price_key, period_mode, period_value, active_flag, in_use_flag "
        "FROM owned_item WHERE id = ? LIMIT 1",
        {id});
    if (rows.empty()) return false;
    out.instanceId  = colU32(rows[0], "id");
    out.baseKey     = colU32(rows[0], "base_key");
    out.priceKey    = colU32(rows[0], "price_key");
    out.periodMode  = colU32(rows[0], "period_mode");
    out.periodValue = colI32(rows[0], "period_value");
    out.activeFlag  = colU32(rows[0], "active_flag");
    out.inUseFlag   = colI32(rows[0], "in_use_flag");
    return true;
}

bool loadPartRow(uint32_t id, InventoryPackets::PartRow& out) {
    auto rows = Database::instance().queryPrepared(
        "SELECT id, base_key, unk_08, price_key, period_mode, period_value, active_flag "
        "FROM owned_part WHERE id = ? LIMIT 1",
        {id});
    if (rows.empty()) return false;
    out.instanceId  = colU32(rows[0], "id");
    out.baseKey     = colU32(rows[0], "base_key");
    out.unk08       = colU32(rows[0], "unk_08");
    out.priceKey    = colU32(rows[0], "price_key");
    out.periodMode  = colU32(rows[0], "period_mode");
    out.periodValue = colU32(rows[0], "period_value");
    out.activeFlag  = colU32(rows[0], "active_flag");
    return true;
}

bool loadPetRow(uint32_t id, InventoryPackets::PetRow& out) {
    auto rows = Database::instance().queryPrepared(
        "SELECT id, base_key, equipped_flag, price_key, period_mode, period_value, active_flag "
        "FROM owned_pet WHERE id = ? LIMIT 1",
        {id});
    if (rows.empty()) return false;
    out.instanceId   = colU32(rows[0], "id");
    out.baseKey      = colU32(rows[0], "base_key");
    out.equippedFlag = colU32(rows[0], "equipped_flag");
    out.priceKey     = colU32(rows[0], "price_key");
    out.periodMode   = colU32(rows[0], "period_mode");
    out.periodValue  = colU32(rows[0], "period_value");
    out.activeFlag   = colU32(rows[0], "active_flag");
    return true;
}

bool loadRoomInstance(uint32_t id, RoomObjectInstance& out) {
    auto rows = Database::instance().queryPrepared(
        "SELECT instance_id, object_key, category, pos_x, pos_y, pos_z, yaw, placed, price_key, "
        "period_type, period_value, active FROM player_item_instance WHERE instance_id = ? LIMIT 1",
        {id});
    if (rows.empty()) return false;
    out.instanceId  = colU32(rows[0], "instance_id");
    out.objectKey   = colU32(rows[0], "object_key");
    out.category    = colU32(rows[0], "category");
    out.pos0        = colF32(rows[0], "pos_x");
    out.pos1        = colF32(rows[0], "pos_y");
    out.pos2        = colF32(rows[0], "pos_z");
    out.yaw         = colF32(rows[0], "yaw");
    out.placedFlag  = colU32(rows[0], "placed");
    out.priceKey    = colU32(rows[0], "price_key");
    out.periodType  = colU32(rows[0], "period_type");
    out.periodValue = colU32(rows[0], "period_value");
    out.activeFlag  = colU32(rows[0], "active");
    return true;
}

bool loadCarPartInstance(uint32_t id, CarPartInstance& out) {
    auto rows = Database::instance().queryPrepared(
        "SELECT instance_id, part_key, category, equip_refcount, price_table_key, period_type, "
        "period_value, period_active, grade FROM custom_car_part_instance WHERE instance_id = ? LIMIT 1",
        {id});
    if (rows.empty()) return false;
    out.instanceId    = colU32(rows[0], "instance_id");
    out.partKey       = colU32(rows[0], "part_key");
    out.category      = colU32(rows[0], "category");
    out.equipRefcount = colI32(rows[0], "equip_refcount");
    out.priceTableKey = colU32(rows[0], "price_table_key");
    out.periodType    = colU32(rows[0], "period_type");
    out.periodValue   = colI32(rows[0], "period_value");
    out.periodActive  = colU32(rows[0], "period_active");
    out.grade         = colI32(rows[0], "grade");
    return true;
}

// tail size follows the category so a wrong pair desyncs the client parse
bool sendBuyAck(const Session::Ptr& session, uint32_t category,
                const ShopPackets::Wallet& wallet, uint32_t instanceId) {
    switch (category) {
        case ShopPackets::CAT_CHARACTER: {
            InventoryPackets::CharacterRow row;
            if (!loadCharacterRow(instanceId, row)) return false;
            session->send(ShopPackets::buyOkCharacter(wallet, InventoryPackets::characterBlob(row)));
            return true;
        }
        case ShopPackets::CAT_KART: {
            InventoryPackets::KartRow row;
            if (!loadKartRow(instanceId, row)) return false;
            session->send(ShopPackets::buyOkKart(wallet, InventoryPackets::kartBlob(row)));
            return true;
        }
        case ShopPackets::CAT_ITEM: {
            InventoryPackets::ItemRow row;
            if (!loadItemRow(instanceId, row)) return false;
            session->send(ShopPackets::buyOkSmall(ShopPackets::CAT_ITEM, wallet,
                                                  InventoryPackets::itemBlob(row)));
            return true;
        }
        case ShopPackets::CAT_PART: {
            InventoryPackets::PartRow row;
            if (!loadPartRow(instanceId, row)) return false;
            session->send(ShopPackets::buyOkSmall(ShopPackets::CAT_PART, wallet,
                                                  InventoryPackets::partBlob(row)));
            return true;
        }
        case ShopPackets::CAT_PET: {
            InventoryPackets::PetRow row;
            if (!loadPetRow(instanceId, row)) return false;
            session->send(ShopPackets::buyOkSmall(ShopPackets::CAT_PET, wallet,
                                                  InventoryPackets::petBlob(row)));
            return true;
        }
        case ShopPackets::CAT_ROOMCRAFT: {
            RoomObjectInstance inst;
            if (!loadRoomInstance(instanceId, inst)) return false;
            session->send(ShopPackets::buyOkRoomCraft(wallet, RoomCraftPackets::decorRecord(inst)));
            return true;
        }
        case ShopPackets::CAT_CARCRAFT: {
            CarPartInstance inst;
            if (!loadCarPartInstance(instanceId, inst)) return false;
            // no preset touched on a plain buy so no extra record
            session->send(ShopPackets::buyOkCarCraft(wallet,
                                                     CustomCarPackets::partInstanceRecord(inst),
                                                     nullptr));
            return true;
        }
        default:
            return false;
    }
}

// the owned record bytes of one instance the reward blob of 0x008C carries them
bool ownedRecordBytes(uint32_t category, uint32_t instanceId, std::vector<uint8_t>& out) {
    out.clear();
    switch (category) {
        case ShopPackets::CAT_CHARACTER: {
            InventoryPackets::CharacterRow row;
            if (!loadCharacterRow(instanceId, row)) return false;
            const auto blob = InventoryPackets::characterBlob(row);
            out.assign(blob.begin(), blob.end());
            return true;
        }
        case ShopPackets::CAT_KART: {
            InventoryPackets::KartRow row;
            if (!loadKartRow(instanceId, row)) return false;
            const auto blob = InventoryPackets::kartBlob(row);
            out.assign(blob.begin(), blob.end());
            return true;
        }
        case ShopPackets::CAT_ITEM: {
            InventoryPackets::ItemRow row;
            if (!loadItemRow(instanceId, row)) return false;
            const auto blob = InventoryPackets::itemBlob(row);
            out.assign(blob.begin(), blob.end());
            return true;
        }
        case ShopPackets::CAT_PART: {
            InventoryPackets::PartRow row;
            if (!loadPartRow(instanceId, row)) return false;
            const auto blob = InventoryPackets::partBlob(row);
            out.assign(blob.begin(), blob.end());
            return true;
        }
        case ShopPackets::CAT_PET: {
            InventoryPackets::PetRow row;
            if (!loadPetRow(instanceId, row)) return false;
            const auto blob = InventoryPackets::petBlob(row);
            out.assign(blob.begin(), blob.end());
            return true;
        }
        case ShopPackets::CAT_ROOMCRAFT: {
            RoomObjectInstance inst;
            if (!loadRoomInstance(instanceId, inst)) return false;
            const auto blob = RoomCraftPackets::decorRecord(inst);
            out.assign(blob.begin(), blob.end());
            return true;
        }
        case ShopPackets::CAT_CARCRAFT: {
            CarPartInstance inst;
            if (!loadCarPartInstance(instanceId, inst)) return false;
            const auto blob = CustomCarPackets::partInstanceRecord(inst);
            out.assign(blob.begin(), blob.end());
            // the flag byte after the part zero means no preset record follows
            out.push_back(0);
            return true;
        }
        default:
            return false;
    }
}

// base key of one owned instance the sender holds
bool loadBaseKeyByInstance(uint32_t category, uint32_t characterId, uint32_t instanceUid,
                           uint32_t& baseKeyOut) {
    if (category <= ShopPackets::CAT_PET) {
        const char* table = ownedTable(category);
        if (table == nullptr) return false;
        auto rows = Database::instance().queryPrepared(
            std::string("SELECT base_key FROM ") + table +
            " WHERE id = ? AND character_id = ? LIMIT 1",
            {instanceUid, characterId});
        if (rows.empty()) return false;
        baseKeyOut = colU32(rows[0], "base_key");
        return true;
    }
    if (category == ShopPackets::CAT_ROOMCRAFT) {
        auto rows = Database::instance().queryPrepared(
            "SELECT object_key FROM player_item_instance "
            "WHERE instance_id = ? AND player_id = ? LIMIT 1",
            {instanceUid, characterId});
        if (rows.empty()) return false;
        baseKeyOut = colU32(rows[0], "object_key");
        return true;
    }
    if (category == ShopPackets::CAT_CARCRAFT) {
        auto rows = Database::instance().queryPrepared(
            "SELECT part_key FROM custom_car_part_instance "
            "WHERE instance_id = ? AND character_id = ? LIMIT 1",
            {instanceUid, characterId});
        if (rows.empty()) return false;
        baseKeyOut = colU32(rows[0], "part_key");
        return true;
    }
    return false;
}

} // namespace

// login definition stream head the only legal home for S2C 0x00BE

void ShopHandler::sendLoginCatalogs(Session::Ptr session, GameServer* server,
                                    std::vector<Packet>* out) {
    (void)server;

    // wipe leads or a reconnect in the same process doubles every catalog
    if (out) out->push_back(ShopPackets::catalogReset());
    else session->send(ShopPackets::catalogReset());

    const size_t sent = sendPriceTableOnce(session, out);

    LOG_INFO("SHOP", "Catalog reset sent char=" + std::to_string(session->characterId) +
                     " priceRows=" + std::to_string(sent) +
                     " every other catalog must follow before any screen ack");
}

// C2S 0x0010 shop stage entry

void ShopHandler::handleEnterShop(Session::Ptr session, GameServer* server) {
    (void)server;

    // append is blind so a second table would duplicate up to the 1536 cap
    const size_t sent = sendPriceTableOnce(session);

    // data first the ack closes the wait modal and snapshots the containers
    session->send(ShopPackets::shopAck());

    LOG_INFO("SHOP", "Shop stage char=" + std::to_string(session->characterId) +
                     " priceRows=" + std::to_string(sent));
}

// C2S 0x0018 stage entry from a channel

void ShopHandler::handleShopEntryAlt(Session::Ptr session, Packet& packet, GameServer* server) {
    ShopPackets::ShopEntryAltRequest req;
    if (!ShopPackets::parseShopEntryAlt(packet, req)) {
        LOG_WARN("SHOP", "Stage entry alt short payload from " + session->remoteAddress());
        return;
    }

    if (req.targetScreen != STAGE_SHOP) {
        LOG_DEBUG("SHOP", "Stage entry alt screen=" + std::to_string(req.targetScreen) +
                          " arg=" + std::to_string(req.arg) + " not the shop");
        return;
    }

    LOG_INFO("SHOP", "Shop stage from channel char=" + std::to_string(session->characterId));
    handleEnterShop(session, server);
}

// C2S 0x00D0 astro poll

void ShopHandler::handleCashPoll(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;

    ShopPackets::CashPollRequest req;
    if (!ShopPackets::parseCashPoll(packet, req)) {
        LOG_WARN("SHOP", "Astro poll short payload from " + session->remoteAddress());
        return;
    }

    // sub 406740 field is the launcher token not a name
    const std::string token = toUtf8(req.accountName);
    // in client relogin rewrites the login copy but never 1ADEE30 so drift is legal
    if (!session->sessionToken.empty() && !token.empty() && token != session->sessionToken) {
        LOG_DEBUG("SHOP", "Astro poll carries the boot launcher token not the login token char=" +
                          std::to_string(session->characterId) +
                          " from " + session->remoteAddress() +
                          " expected after an in client relogin so it never gates");
    }

    const uint32_t characterId = session->characterId;
    if (characterId == 0) {
        LOG_DEBUG("SHOP", "Astro poll before character select from " + session->remoteAddress());
        return;
    }

    ShopPackets::Wallet wallet;
    if (!ShopPackets::loadWallet(characterId, wallet)) {
        LOG_WARN("SHOP", "Astro poll wallet missing char=" + std::to_string(characterId));
        return;
    }

    session->send(ProgressionPackets::astroBalance(wallet.cash));

    LOG_DEBUG("SHOP", "Astro poll char=" + std::to_string(characterId) +
                      " astro=" + std::to_string(wallet.cash) +
                      " host=" + req.extra);
}

// C2S 0x00B7 buy

void ShopHandler::handleBuy(Session::Ptr session, const ShopPackets::BuyRequest& req,
                            GameServer* server) {
    const uint32_t characterId = session->characterId;
    if (characterId == 0) {
        LOG_WARN("SHOP", "Buy without a character from " + session->remoteAddress());
        rejectGeneric(session);
        return;
    }

    ShopPackets::PriceRow price;
    int32_t cost = 0;
    const auto verdict = ShopPackets::validatePurchase(characterId, req.category, req.baseKey,
                                                       req.priceKey, price, cost);
    if (verdict != ShopPackets::PurchaseResult::Ok) {
        LOG_WARN("SHOP", "Buy refused char=" + std::to_string(characterId) +
                         " cat=" + std::to_string(req.category) +
                         " base=" + std::to_string(req.baseKey) +
                         " price=" + std::to_string(req.priceKey) +
                         " why=" + purchaseResultName(verdict));
        logShopRefusal(characterId, "buy", req.category, req.baseKey, req.priceKey,
                       purchaseResultName(verdict));
        rejectPurchase(session, verdict);
        return;
    }

    // quote wins over the gate copy so an edit between the two never bills wrong
    const Quote quote = quoteOption(req.category, req.baseKey, req.priceKey);
    if (!quote.ok) {
        LOG_WARN("SHOP", "Buy option vanished char=" + std::to_string(characterId) +
                         " cat=" + std::to_string(req.category) +
                         " price=" + std::to_string(req.priceKey));
        logShopRefusal(characterId, "buy", req.category, req.baseKey, req.priceKey,
                       "UnknownPrice");
        rejectPurchase(session, ShopPackets::PurchaseResult::UnknownPrice);
        return;
    }
    if (quote.cost != cost || quote.row.currency != price.currency) {
        LOG_WARN("SHOP", "Buy price moved char=" + std::to_string(characterId) +
                         " was=" + std::to_string(cost) +
                         " now=" + std::to_string(quote.cost));
        logShopRefusal(characterId, "buy", req.category, req.baseKey, req.priceKey, "PriceMoved");
        rejectLiteral(session, u16FromAscii("Price changed to ") + u16FromInt(quote.cost) +
                               u16FromAscii(" please try again"));
        return;
    }
    price = quote.row;
    cost  = quote.cost;

    const auto charged = chargeAndGrant(characterId, characterId, "buy", req.category, req.baseKey,
                                        req.priceKey, 0, price, cost, nullptr);
    if (!charged.ok) {
        const auto why = charged.insufficient ? ShopPackets::PurchaseResult::NotEnoughMoney
                                              : ShopPackets::PurchaseResult::DbError;
        logShopRefusal(characterId, "buy", req.category, req.baseKey, req.priceKey,
                       purchaseResultName(why));
        rejectPurchase(session, why, price.currency == ShopPackets::Currency::Cash);
        return;
    }

    if (!sendBuyAck(session, req.category, charged.after, charged.instanceId)) {
        LOG_ERROR("SHOP", "Buy granted but record load failed char=" + std::to_string(characterId) +
                          " cat=" + std::to_string(req.category) +
                          " inst=" + std::to_string(charged.instanceId));
        rejectGeneric(session);
        return;
    }

    // a bought chassis shows in the garage and the factory only once its slot and basic set land
    if (req.category == ShopPackets::CAT_KART && server) server->carCraft().pushFactoryLoadout(session);

    LOG_INFO("SHOP", "Buy ok char=" + std::to_string(characterId) +
                     " cat=" + std::to_string(req.category) +
                     " base=" + std::to_string(req.baseKey) +
                     " cost=" + std::to_string(cost) +
                     (price.currency == ShopPackets::Currency::Cash ? " astro" : " gold") +
                     " inst=" + std::to_string(charged.instanceId));
}

// C2S 0x00B8 sell or discard

void ShopHandler::handleSell(Session::Ptr session, const ShopPackets::SellRequest& req,
                             GameServer* server) {
    (void)server;

    const uint32_t characterId = session->characterId;
    if (characterId == 0) {
        LOG_WARN("SHOP", "Sell without a character from " + session->remoteAddress());
        rejectGeneric(session);
        return;
    }

    const auto gate = ShopPackets::validateSell(req);
    if (gate != ShopPackets::SellResult::Ok) {
        LOG_WARN("SHOP", "Sell refused char=" + std::to_string(characterId) +
                         " cat=" + std::to_string(req.category));
        logShopRefusal(characterId, "sell", req.category, req.key, -1,
                       gate == ShopPackets::SellResult::BadCategory ? "BadCategory" : "NotSellable");
        rejectGeneric(session);
        return;
    }

    auto& db = Database::instance();

    // active driver row would leave the client without a character
    if (req.category == ShopPackets::CAT_CHARACTER) {
        auto active = db.queryPrepared(
            "SELECT equipped_driver_id FROM characters WHERE id = ? LIMIT 1", {characterId});
        if (!active.empty() && colU32(active[0], "equipped_driver_id") == req.key) {
            logShopRefusal(characterId, "sell", req.category, req.key, -1, "InUse");
            rejectGeneric(session);
            return;
        }
    }

    auto tx = db.beginTransaction();
    if (!tx.valid()) {
        LOG_ERROR("SHOP", "beginTransaction failed for sell");
        rejectGeneric(session);
        return;
    }

    bool removed = false;
    if (req.category <= ShopPackets::CAT_PART) {
        const char* table = ownedTable(req.category);
        if (table == nullptr) {
            rejectGeneric(session);
            return;
        }
        // key is the base key here so every matching row goes like the client does
        removed = tx.execute(std::string("DELETE FROM ") + table +
                             " WHERE character_id = ? AND base_key = ?",
                             {characterId, req.key}) && tx.affectedRows() > 0;
    } else if (req.category == ShopPackets::CAT_ROOMCRAFT) {
        // key is the instance uid here
        removed = tx.execute("DELETE FROM player_item_instance "
                             "WHERE instance_id = ? AND player_id = ?",
                             {req.key, characterId}) && tx.affectedRows() > 0;
        if (removed) {
            // stale decor would keep drawing a row nobody owns
            tx.execute("DELETE FROM room_decor WHERE instance_id = ?", {req.key});
        }
    }

    if (!removed) {
        LOG_WARN("SHOP", "Sell found nothing char=" + std::to_string(characterId) +
                         " cat=" + std::to_string(req.category) +
                         " key=" + std::to_string(req.key));
        logShopRefusal(characterId, "sell", req.category, req.key, -1, "UnknownDefinition");
        rejectGeneric(session);
        return;
    }

    const ShopPackets::Wallet zero;
    logShopTx(tx, characterId, "sell", req.category, req.key, -1,
              ShopPackets::Currency::Gp, 0, zero, zero, "Ok");

    if (!tx.commit()) {
        LOG_ERROR("SHOP", "Sell commit failed char=" + std::to_string(characterId));
        rejectGeneric(session);
        return;
    }

    session->send(ShopPackets::sellOk(req.category, req.key));

    LOG_INFO("SHOP", "Sell ok char=" + std::to_string(characterId) +
                     " cat=" + std::to_string(req.category) +
                     " key=" + std::to_string(req.key));
}

// C2S 0x0098 gift

void ShopHandler::handleGift(Session::Ptr session, const ShopPackets::GiftRequest& req,
                             GameServer* server) {
    (void)server;

    const uint32_t characterId = session->characterId;
    if (characterId == 0) {
        LOG_WARN("SHOP", "Gift without a character from " + session->remoteAddress());
        rejectGeneric(session);
        return;
    }

    uint32_t targetId = 0;
    if (!ShopPackets::resolveCharacterIdByName(req.targetName, targetId) ||
        targetId == characterId) {
        LOG_WARN("SHOP", "Gift target refused char=" + std::to_string(characterId));
        logShopRefusal(characterId, "gift", req.category, req.baseKey, req.priceKey,
                       "UnknownDefinition");
        // wide dialog so the name the player typed comes back in the text
        rejectLiteral(session, req.targetName + u16FromAscii(" cannot receive this gift"));
        return;
    }

    ShopPackets::PriceRow price;
    int32_t cost = 0;
    const auto verdict = ShopPackets::validatePurchase(characterId, req.category, req.baseKey,
                                                       req.priceKey, price, cost);
    if (verdict != ShopPackets::PurchaseResult::Ok) {
        LOG_WARN("SHOP", "Gift refused char=" + std::to_string(characterId) +
                         " cat=" + std::to_string(req.category) +
                         " base=" + std::to_string(req.baseKey) +
                         " why=" + purchaseResultName(verdict));
        logShopRefusal(characterId, "gift", req.category, req.baseKey, req.priceKey,
                       purchaseResultName(verdict));
        rejectPurchase(session, verdict);
        return;
    }

    // row lands in the target bag so the target level is what gates it
    int32_t needLevel = 0;
    if (loadDefinitionLevelReq(req.category, req.baseKey, needLevel) && needLevel > 0) {
        int32_t targetLevel = 0;
        if (!ShopPackets::loadLevel(targetId, targetLevel) || targetLevel < needLevel) {
            LOG_WARN("SHOP", "Gift target level too low char=" + std::to_string(characterId) +
                             " to=" + std::to_string(targetId) +
                             " need=" + std::to_string(needLevel) +
                             " has=" + std::to_string(targetLevel));
            logShopRefusal(characterId, "gift", req.category, req.baseKey, req.priceKey,
                           "TargetLevel");
            rejectLiteral(session, req.targetName + u16FromAscii(" needs level ") +
                                   u16FromInt(needLevel));
            return;
        }
    }

    const Quote quote = quoteOption(req.category, req.baseKey, req.priceKey);
    if (!quote.ok || quote.cost != cost || quote.row.currency != price.currency) {
        LOG_WARN("SHOP", "Gift price moved char=" + std::to_string(characterId) +
                         " was=" + std::to_string(cost) +
                         " now=" + std::to_string(quote.cost));
        logShopRefusal(characterId, "gift", req.category, req.baseKey, req.priceKey, "PriceMoved");
        rejectPurchase(session, ShopPackets::PurchaseResult::UnknownPrice);
        return;
    }
    price = quote.row;
    cost  = quote.cost;

    GiftInfo gift;
    gift.targetName = clampUtf8(toUtf8(req.targetName), GIFT_NAME_MAX_BYTES);
    gift.targetId   = targetId;
    gift.message    = clampUtf8(toUtf8(req.message), GIFT_MESSAGE_MAX_BYTES);

    const auto charged = chargeAndGrant(characterId, targetId, "gift", req.category, req.baseKey,
                                        req.priceKey, 0, price, cost, &gift);
    if (!charged.ok) {
        const auto why = charged.insufficient ? ShopPackets::PurchaseResult::NotEnoughMoney
                                              : ShopPackets::PurchaseResult::DbError;
        logShopRefusal(characterId, "gift", req.category, req.baseKey, req.priceKey,
                       purchaseResultName(why));
        rejectPurchase(session, why);
        return;
    }

    // builder writes astro first here unlike buy and extend
    session->send(ShopPackets::giftOk(charged.after));

    // 0x99 appends the mail to an open mailbox or a gift to someone online only appears after they relog
    if (server && targetId != 0) {
        if (auto to = server->findSessionByCharacterId(static_cast<int32_t>(targetId))) {
            auto row = Database::instance().queryPrepared(
                "SELECT g.id, g.category, g.sender_id, g.message, "
                "DATE_FORMAT(g.sent_at, '%Y-%m-%d') AS d, "
                "DATE_FORMAT(g.sent_at, '%H:%i:%s') AS t, "
                "COALESCE(sc.name, 'System') AS sender_name "
                "FROM gift_log g LEFT JOIN characters sc ON sc.id = g.sender_id "
                "WHERE g.target_id = ? ORDER BY g.id DESC LIMIT 1", {targetId});
            if (!row.empty()) {
                to->send(PacketBuilder::giftArrived(PacketBuilder::giftRecord(
                    static_cast<int32_t>(std::stoll(row[0].at("id")) & 0x7fffffff),
                    std::stoi(row[0].at("category")), std::stoi(row[0].at("sender_id")),
                    row[0].at("sender_name"), row[0].at("d"), row[0].at("t"),
                    row[0].at("message"))));
                LOG_INFO("SHOP", "gift pushed live to char " + std::to_string(targetId));
            }
        }
    }

    LOG_INFO("SHOP", "Gift ok from=" + std::to_string(characterId) +
                     " to=" + std::to_string(targetId) +
                     " cat=" + std::to_string(req.category) +
                     " base=" + std::to_string(req.baseKey) +
                     " cost=" + std::to_string(cost));
}

// C2S 0x0112 extend

void ShopHandler::handleExtend(Session::Ptr session, const ShopPackets::ExtendRequest& req,
                               GameServer* server) {
    (void)server;

    const uint32_t characterId = session->characterId;
    if (characterId == 0) {
        LOG_WARN("SHOP", "Extend without a character from " + session->remoteAddress());
        rejectGeneric(session);
        return;
    }

    if (req.category > ShopPackets::CAT_MAX) {
        logShopRefusal(characterId, "extend", req.category, 0, req.priceKey, "BadCategory");
        rejectPurchase(session, ShopPackets::PurchaseResult::BadCategory);
        return;
    }

    uint32_t baseKey = 0;
    if (!loadBaseKeyByInstance(req.category, characterId, req.instanceUid, baseKey)) {
        LOG_WARN("SHOP", "Extend unknown instance char=" + std::to_string(characterId) +
                         " cat=" + std::to_string(req.category) +
                         " inst=" + std::to_string(req.instanceUid));
        logShopRefusal(characterId, "extend", req.category, 0, req.priceKey, "UnknownDefinition");
        rejectPurchase(session, ShopPackets::PurchaseResult::UnknownDefinition);
        return;
    }

    ShopPackets::PriceRow price;
    int32_t cost = 0;
    const auto verdict = ShopPackets::validatePurchase(characterId, req.category, baseKey,
                                                       req.priceKey, price, cost);
    if (verdict != ShopPackets::PurchaseResult::Ok) {
        LOG_WARN("SHOP", "Extend refused char=" + std::to_string(characterId) +
                         " cat=" + std::to_string(req.category) +
                         " base=" + std::to_string(baseKey) +
                         " why=" + purchaseResultName(verdict));
        logShopRefusal(characterId, "extend", req.category, baseKey, req.priceKey,
                       purchaseResultName(verdict));
        rejectPurchase(session, verdict);
        return;
    }

    const Quote quote = quoteOption(req.category, baseKey, req.priceKey);
    if (!quote.ok || quote.cost != cost || quote.row.currency != price.currency) {
        LOG_WARN("SHOP", "Extend price moved char=" + std::to_string(characterId) +
                         " was=" + std::to_string(cost) +
                         " now=" + std::to_string(quote.cost));
        logShopRefusal(characterId, "extend", req.category, baseKey, req.priceKey, "PriceMoved");
        rejectPurchase(session, ShopPackets::PurchaseResult::UnknownPrice);
        return;
    }

    // permanent option adds no period so the player would pay for nothing
    if (quote.row.unitType == ShopPackets::UNIT_PERMANENT || quote.row.unitAmount == 0) {
        LOG_WARN("SHOP", "Extend on a non period option char=" + std::to_string(characterId) +
                         " price=" + std::to_string(req.priceKey) +
                         " unit=" + std::to_string(quote.row.unitType));
        logShopRefusal(characterId, "extend", req.category, baseKey, req.priceKey, "NotRenewable");
        rejectPurchase(session, ShopPackets::PurchaseResult::OptionMismatch);
        return;
    }
    price = quote.row;
    cost  = quote.cost;

    const auto charged = chargeAndGrant(characterId, characterId, "extend", req.category, baseKey,
                                        req.priceKey, req.instanceUid, price, cost, nullptr);
    if (!charged.ok) {
        const auto why = charged.insufficient ? ShopPackets::PurchaseResult::NotEnoughMoney
                                              : ShopPackets::PurchaseResult::DbError;
        logShopRefusal(characterId, "extend", req.category, baseKey, req.priceKey,
                       purchaseResultName(why));
        rejectPurchase(session, why);
        return;
    }

    if (req.category == ShopPackets::CAT_ROOMCRAFT) {
        RoomObjectInstance inst;
        if (!loadRoomInstance(charged.instanceId, inst)) {
            LOG_ERROR("SHOP", "Extend charged but room record load failed inst=" +
                              std::to_string(charged.instanceId));
            rejectGeneric(session);
            return;
        }
        // uid must be one the client already holds else it writes through a null
        session->send(ShopPackets::extendOkRoomCraft(charged.after,
                                                     RoomCraftPackets::decorRecord(inst)));
    } else {
        session->send(ShopPackets::extendOk(req.category, charged.after));
    }

    LOG_INFO("SHOP", "Extend ok char=" + std::to_string(characterId) +
                     " cat=" + std::to_string(req.category) +
                     " inst=" + std::to_string(charged.instanceId) +
                     " cost=" + std::to_string(cost));
}

bool ShopHandler::grantReward(uint32_t characterId, uint32_t category, uint32_t baseKey,
                              std::vector<uint8_t>& recordOut) {
    recordOut.clear();
    if (characterId == 0 || baseKey == 0 || category > ShopPackets::CAT_CARCRAFT) return false;
    // a reward is kept for good a consumable comes as one use
    ShopPackets::PriceRow price;
    price.unitType = category == ShopPackets::CAT_ITEM ? ShopPackets::UNIT_USES : ShopPackets::UNIT_PERMANENT;
    price.unitAmount = 1;

    auto tx = Database::instance().beginTransaction();
    if (!tx.valid()) return false;
    uint32_t instanceId = 0;
    if (!grantAny(tx, characterId, category, baseKey, 0, price, instanceId)) {
        tx.rollback();
        LOG_WARN("SHOP", "reward grant failed char=" + std::to_string(characterId) +
                         " cat=" + std::to_string(category) + " base=" + std::to_string(baseKey));
        return false;
    }
    if (!tx.commit()) return false;
    if (!ownedRecordBytes(category, instanceId, recordOut)) {
        LOG_ERROR("SHOP", "reward granted but its record did not load char=" + std::to_string(characterId) +
                          " cat=" + std::to_string(category) + " inst=" + std::to_string(instanceId));
        return false;
    }
    LOG_INFO("SHOP", "reward granted char=" + std::to_string(characterId) + " cat=" +
                     std::to_string(category) + " base=" + std::to_string(baseKey) +
                     " inst=" + std::to_string(instanceId));
    return true;
}

} // namespace knc
