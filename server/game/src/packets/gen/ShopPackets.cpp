#include "packets/gen/ShopPackets.h"

#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"
#include "util/ShopRules.h"

#include <cstdlib>
#include <map>
#include <string>

namespace knc {

namespace {

// opcode above one byte loses its high byte through the plain ctor
Packet make(uint16_t op) {
    if (op > 0xFF) return Packet::fromCmdFull(op);
    return Packet::fromCmdFull(op);   // never narrow an opcode
}

void checkSize(const Packet& pkt, size_t expected, const char* what) {
    // wrong size here silently desyncs the whole stream loud is better
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", std::string(what) + " size " +
                            std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
}

// client reads with strlen into a small stack buffer so long string smashes its frame
std::string clampAscii(const std::string& in, size_t maxChars, const char* what) {
    size_t nulProcessedLen = 0;
    std::string out = clampAsciiCore(in, maxChars, AsciiNulMode::kStripNul, nulProcessedLen);
    if (nulProcessedLen > maxChars) {
        LOG_WARN("PACKET", std::string("ShopPackets clamp ") + what + " from " +
                           std::to_string(nulProcessedLen) + " to " + std::to_string(maxChars));
    }
    return out;
}

// option vector caps at four client side the fifth append is rejected
size_t writeOptions(Packet& pkt, const std::vector<ShopPackets::PurchaseOption>& opts,
                    const char* what) {
    size_t n = opts.size();
    if (n > ShopPackets::CAP_OPTIONS) {
        LOG_WARN("PACKET", std::string("ShopPackets clamp options on ") + what + " from " +
                           std::to_string(n) + " to " + std::to_string(ShopPackets::CAP_OPTIONS));
        n = ShopPackets::CAP_OPTIONS;
    }
    // never ship an empty list sub 451D30 derefs with no null test and the screen stops ending frames
    if (n == 0) {
        pkt.writeInt32(1);
        for (int i = 0; i < 4; ++i) pkt.writeUInt32(0);
        return 1;
    }
    pkt.writeInt32(static_cast<int32_t>(n));
    for (size_t i = 0; i < n; ++i) {
        pkt.writeUInt32(opts[i].priceKey);
        pkt.writeUInt32(0);  // rest of the record has no reader anywhere
        pkt.writeUInt32(0);
        pkt.writeUInt32(0);
    }
    return n;
}

size_t buyTailSize(uint32_t category) {
    switch (category) {
        case ShopPackets::CAT_CHARACTER: return 0x2C;
        case ShopPackets::CAT_KART:      return 0x38;
        case ShopPackets::CAT_ITEM:      return 0x1C;
        case ShopPackets::CAT_PART:      return 0x1C;
        case ShopPackets::CAT_PET:       return 0x1C;
        case ShopPackets::CAT_ROOMCRAFT: return 0x30;
        default:                         return 0;  // car craft is variable caller checks
    }
}

// bounded reader so a short or crafted payload can never walk off the buffer
struct Cursor {
    const uint8_t* p = nullptr;
    size_t n = 0;
    size_t i = 0;
    bool bad = false;

    explicit Cursor(const std::vector<uint8_t>& v) : p(v.data()), n(v.size()) {}

    uint32_t u32() {
        if (bad || i + 4 > n) { bad = true; return 0; }
        uint32_t v = static_cast<uint32_t>(p[i]) |
                     (static_cast<uint32_t>(p[i + 1]) << 8) |
                     (static_cast<uint32_t>(p[i + 2]) << 16) |
                     (static_cast<uint32_t>(p[i + 3]) << 24);
        i += 4;
        return v;
    }

    std::u16string wstr(size_t maxChars) {
        std::u16string out;
        while (true) {
            if (bad || i + 2 > n) { bad = true; return std::u16string(); }
            char16_t c = static_cast<char16_t>(static_cast<uint16_t>(p[i]) |
                                               (static_cast<uint16_t>(p[i + 1]) << 8));
            i += 2;
            if (c == 0) break;
            if (out.size() >= maxChars) { bad = true; return std::u16string(); }
            out.push_back(c);
        }
        return out;
    }

    std::string restAsCStr(size_t maxLen) {
        std::string out;
        while (i < n && out.size() < maxLen) {
            char c = static_cast<char>(p[i++]);
            if (c == '\0') break;
            out.push_back(c);
        }
        return out;
    }
};

int64_t colI64(const std::map<std::string, std::string>& row, const char* key, int64_t def) {
    return rowInt64NoThrow(row, key, def);
}

// db is utf eight so encode properly instead of dropping the high bytes
std::string toUtf8(const std::u16string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (size_t i = 0; i < s.size(); ++i) {
        uint32_t cp = static_cast<uint32_t>(s[i]);
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size()) {
            uint32_t lo = static_cast<uint32_t>(s[i + 1]);
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

struct DefRow {
    uint32_t visible = 0;
    uint32_t levelReq = 0;
    uint32_t conditionKey = 0;
};

bool loadDefinition(uint32_t category, uint32_t baseKey, DefRow& out) {
    auto rows = Database::instance().queryPrepared(
        "SELECT shop_visible_flag, level_req, required_pendant_key "
        "FROM shop_definition WHERE category = ? AND base_key = ? LIMIT 1",
        {category, baseKey});
    if (rows.empty()) return false;
    out.visible      = static_cast<uint32_t>(colI64(rows[0], "shop_visible_flag", 0));
    out.levelReq     = static_cast<uint32_t>(colI64(rows[0], "level_req", 0));
    out.conditionKey = static_cast<uint32_t>(colI64(rows[0], "required_pendant_key", 0));
    return true;
}

bool hasPetCondition(uint32_t characterId, uint32_t conditionKey) {
    auto rows = Database::instance().queryPrepared(
        "SELECT condition_key FROM pet_condition "
        "WHERE character_id = ? AND condition_key = ? LIMIT 1",
        {characterId, conditionKey});
    return !rows.empty();
}

} // namespace

Packet ShopPackets::catalogReset() {
    // must lead the stream else reconnect refills every catalog past its cap
    Packet pkt = make(OP_S_CATALOG_RESET);
    checkSize(pkt, 0, "catalogReset");
    return pkt;
}

Packet ShopPackets::shopAck() {
    // handler closes the wait modal last so send this after the catalogs
    Packet pkt = make(OP_S_SHOP_ACK);
    checkSize(pkt, 0, "shopAck");
    return pkt;
}

Packet ShopPackets::priceRow(const PriceRow& row) {
    Packet pkt = make(OP_S_PRICE_ROW);

    pkt.writeUInt32(row.priceKey);
    pkt.writeUInt32(0);  // no reader in the binary keep zero
    pkt.writeUInt32(0);
    pkt.writeUInt32(row.unitType);
    pkt.writeUInt32(row.unitAmount);
    pkt.writeInt32(row.priceBase);
    pkt.writeInt32(row.priceSale);

    checkSize(pkt, 28, "priceRow");
    return pkt;
}

std::vector<Packet> ShopPackets::priceTable(const std::vector<PriceRow>& rows) {
    std::vector<Packet> out;
    size_t n = rows.size();
    if (n > CAP_PRICE_ROWS) {
        LOG_WARN("PACKET", "ShopPackets priceTable clamp from " + std::to_string(n) +
                           " to " + std::to_string(CAP_PRICE_ROWS));
        n = CAP_PRICE_ROWS;
    }
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) out.push_back(priceRow(rows[i]));
    return out;
}

Packet ShopPackets::petDefinition(const PetDef& def) {
    Packet pkt = make(OP_S_PET_DEF);

    const std::string s1 = clampAscii(def.name, MAX_STR1, "petDef str1");
    const std::string s2 = clampAscii(def.str2, MAX_STR2, "petDef str2");
    const std::string s3 = clampAscii(def.desc, MAX_STR3, "petDef str3");

    pkt.writeUInt32(def.shopVisibleFlag);
    pkt.writeUInt32(def.badge);
    pkt.writeUInt32(def.petBaseKey);
    pkt.writeUInt32(def.requiredPendantKey);
    pkt.writeString(s1);
    pkt.writeString(s2);
    pkt.writeString(s3);
    const size_t opts = writeOptions(pkt, def.options, "petDef");

    checkSize(pkt, 16 + s1.size() + 1 + s2.size() + 1 + s3.size() + 1 + 4 + 16 * opts,
              "petDefinition");
    return pkt;
}

Packet ShopPackets::roomCraftDefinition(const RoomCraftDef& def) {
    Packet pkt = make(OP_S_ROOMCRAFT_DEF);

    const std::string s1 = clampAscii(def.name, MAX_STR1, "roomCraftDef str1");
    const std::string s2 = clampAscii(def.str2, MAX_STR2, "roomCraftDef str2");
    const std::string s3 = clampAscii(def.desc, MAX_STR3, "roomCraftDef str3");

    pkt.writeUInt32(def.shopVisibleFlag);
    pkt.writeUInt32(def.badge);
    pkt.writeUInt32(def.baseKey);
    pkt.writeUInt32(def.subtype);
    pkt.writeUInt32(0);  // no reader in the binary keep zero
    pkt.writeUInt32(def.levelReq);
    pkt.writeString(s1);
    pkt.writeString(s2);
    pkt.writeString(s3);
    const size_t opts = writeOptions(pkt, def.options, "roomCraftDef");

    checkSize(pkt, 24 + s1.size() + 1 + s2.size() + 1 + s3.size() + 1 + 4 + 16 * opts,
              "roomCraftDefinition");
    return pkt;
}

Packet ShopPackets::carCraftDefinition(const CarCraftDef& def) {
    Packet pkt = make(OP_S_CARCRAFT_DEF);

    const std::string s1 = clampAscii(def.name, MAX_STR1, "carCraftDef str1");
    const std::string s2 = clampAscii(def.str2, MAX_STR2, "carCraftDef str2");
    const std::string s3 = clampAscii(def.desc, MAX_STR3, "carCraftDef str3");

    pkt.writeUInt32(def.shopVisibleFlag);
    pkt.writeUInt32(def.badge);
    pkt.writeUInt32(def.baseKey);
    pkt.writeUInt32(def.subtype);
    pkt.writeUInt32(def.levelReq);
    pkt.writeString(s1);
    pkt.writeString(s2);
    pkt.writeString(s3);
    pkt.writeBytes(def.statBlock.data(), def.statBlock.size());
    pkt.writeBytes(def.pairBlock.data(), def.pairBlock.size());
    pkt.writeBytes(def.tailBlock.data(), def.tailBlock.size());
    const size_t opts = writeOptions(pkt, def.options, "carCraftDef");

    checkSize(pkt, 120 + s1.size() + 1 + s2.size() + 1 + s3.size() + 1 + 16 * opts,
              "carCraftDefinition");
    return pkt;
}

Packet ShopPackets::buyOk(uint32_t category, const Wallet& wallet,
                          const uint8_t* record, size_t recordLen) {
    Packet pkt = make(OP_S_BUY_OK);

    pkt.writeUInt32(category);
    pkt.writeInt32(wallet.gp);    // gp first here but cash first on gift
    pkt.writeInt32(wallet.cash);
    if (record != nullptr && recordLen > 0) pkt.writeBytes(record, recordLen);

    // short tail makes the handler underrun its own read
    bool ok;
    if (category == CAT_CARCRAFT) {
        ok = (recordLen == 0x85 || recordLen == 0xB9);
    } else if (category > CAT_MAX) {
        ok = (recordLen == 0);
    } else {
        ok = (recordLen == buyTailSize(category));
    }
    if (!ok) {
        LOG_ERROR("PACKET", "buyOk bad tail for category " + std::to_string(category) +
                            " len " + std::to_string(recordLen));
    }

    checkSize(pkt, 12 + recordLen, "buyOk");
    return pkt;
}

Packet ShopPackets::buyOkCharacter(const Wallet& wallet, const std::array<uint8_t, 0x2C>& record) {
    return buyOk(CAT_CHARACTER, wallet, record.data(), record.size());
}

Packet ShopPackets::buyOkKart(const Wallet& wallet, const std::array<uint8_t, 0x38>& record) {
    return buyOk(CAT_KART, wallet, record.data(), record.size());
}

Packet ShopPackets::buyOkSmall(uint32_t category, const Wallet& wallet,
                               const std::array<uint8_t, 0x1C>& record) {
    if (category != CAT_ITEM && category != CAT_PART && category != CAT_PET) {
        LOG_ERROR("PACKET", "buyOkSmall wrong category " + std::to_string(category));
    }
    return buyOk(category, wallet, record.data(), record.size());
}

Packet ShopPackets::buyOkRoomCraft(const Wallet& wallet, const std::array<uint8_t, 0x30>& record) {
    return buyOk(CAT_ROOMCRAFT, wallet, record.data(), record.size());
}

Packet ShopPackets::buyOkCarCraft(const Wallet& wallet, const std::array<uint8_t, 0x84>& record,
                                  const std::array<uint8_t, 0x34>* extra52) {
    Packet pkt = make(OP_S_BUY_OK);

    pkt.writeUInt32(CAT_CARCRAFT);
    pkt.writeInt32(wallet.gp);
    pkt.writeInt32(wallet.cash);
    pkt.writeUInt8(static_cast<uint8_t>(extra52 != nullptr ? 1 : 0));
    pkt.writeBytes(record.data(), record.size());
    if (extra52 != nullptr) pkt.writeBytes(extra52->data(), extra52->size());

    const size_t expected = 12 + 1 + 0x84 + (extra52 != nullptr ? 0x34u : 0u);
    checkSize(pkt, expected, "buyOkCarCraft");
    return pkt;
}

Packet ShopPackets::sellOk(uint32_t category, uint32_t key) {
    Packet pkt = make(OP_S_SELL_OK);

    pkt.writeUInt32(category);
    pkt.writeUInt32(key);  // base key everywhere except room craft which sends its uid

    if (category == CAT_PET || category == CAT_CARCRAFT) {
        LOG_WARN("PACKET", "sellOk category " + std::to_string(category) +
                           " has no client handler so nothing is removed locally");
    }

    checkSize(pkt, 8, "sellOk");
    return pkt;
}

Packet ShopPackets::giftOk(const Wallet& wallet) {
    return giftOk(wallet, nullptr);
}

Packet ShopPackets::giftOk(const Wallet& wallet, const uint8_t* blob212) {
    Packet pkt = make(OP_S_GIFT_OK);

    pkt.writeInt32(wallet.cash);  // reversed versus buy and extend
    pkt.writeInt32(wallet.gp);
    if (blob212 != nullptr) {
        pkt.writeBytes(blob212, 212);
    } else {
        for (int i = 0; i < 212; ++i) pkt.writeUInt8(0);  // handler underruns without them
    }

    checkSize(pkt, 220, "giftOk");
    return pkt;
}

Packet ShopPackets::extendOk(uint32_t category, const Wallet& wallet) {
    if (category == CAT_ROOMCRAFT) {
        LOG_ERROR("PACKET", "extendOk category five needs the record use extendOkRoomCraft");
    }

    Packet pkt = make(OP_S_EXTEND_OK);
    pkt.writeUInt32(category);
    pkt.writeInt32(wallet.gp);
    pkt.writeInt32(wallet.cash);

    checkSize(pkt, 12, "extendOk");
    return pkt;
}

Packet ShopPackets::extendOkRoomCraft(const Wallet& wallet,
                                      const std::array<uint8_t, 0x30>& record) {
    Packet pkt = make(OP_S_EXTEND_OK);

    pkt.writeUInt32(CAT_ROOMCRAFT);
    pkt.writeInt32(wallet.gp);
    pkt.writeInt32(wallet.cash);
    pkt.writeBytes(record.data(), record.size());  // uid must already be owned else client dies

    checkSize(pkt, 60, "extendOkRoomCraft");
    return pkt;
}

Packet ShopPackets::rejectWide(const std::u16string& messageKey, int32_t dialogType) {
    // type two would latch the client into the wedged dialog state
    Packet pkt = make(OP_S_DIALOG_WIDE);
    pkt.writeWString(messageKey);
    pkt.writeInt32(dialogType);
    checkSize(pkt, 2 * (messageKey.size() + 1) + 4, "rejectWide");
    return pkt;
}

Packet ShopPackets::rejectAscii(const std::string& messageKey, int32_t dialogType) {
    Packet pkt = make(OP_S_DIALOG_ASCII);
    pkt.writeString(messageKey);
    pkt.writeInt32(dialogType);
    checkSize(pkt, messageKey.size() + 1 + 4, "rejectAscii");
    return pkt;
}

bool ShopPackets::parseBuy(const Packet& pkt, BuyRequest& out) {
    Cursor c(pkt.payload());
    out.category = c.u32();
    out.baseKey  = c.u32();
    out.priceKey = static_cast<int32_t>(c.u32());
    out.accountName = c.wstr(64);
    return !c.bad;
}

bool ShopPackets::parseSell(const Packet& pkt, SellRequest& out) {
    Cursor c(pkt.payload());
    out.category = c.u32();
    out.key      = c.u32();
    return !c.bad;
}

bool ShopPackets::parseGift(const Packet& pkt, GiftRequest& out) {
    Cursor c(pkt.payload());
    out.category   = c.u32();
    out.targetName = c.wstr(64);
    out.baseKey    = c.u32();
    out.priceKey   = static_cast<int32_t>(c.u32());
    out.message    = c.wstr(512);
    return !c.bad;
}

bool ShopPackets::parseExtend(const Packet& pkt, ExtendRequest& out) {
    Cursor c(pkt.payload());
    out.category    = c.u32();
    out.instanceUid = c.u32();
    out.priceKey    = static_cast<int32_t>(c.u32());
    return !c.bad;
}

bool ShopPackets::parseCashPoll(const Packet& pkt, CashPollRequest& out) {
    Cursor c(pkt.payload());
    out.accountName = c.wstr(64);
    if (c.bad) return false;
    // trailing writer never decompiled so take what is left instead of guessing a width
    out.extra = c.restAsCStr(256);
    return true;
}

bool ShopPackets::parseShopEntryAlt(const Packet& pkt, ShopEntryAltRequest& out) {
    Cursor c(pkt.payload());
    out.targetScreen = c.u32();
    out.arg          = c.u32();
    return !c.bad;
}

int32_t ShopPackets::chargedAmount(const PriceRow& row) {
    // sale price is what the tile shows so it is what the player must pay
    return priceCharged(row.priceBase, row.priceSale);
}

ShopPackets::Currency ShopPackets::currencyOf(int32_t priceSale) {
    // the detail panel draws the Astro icon on a sale price and the Gold icon on a base price
    return priceIsAstro(priceSale) ? Currency::Cash : Currency::Gp;
}

bool ShopPackets::canAfford(const Wallet& wallet, Currency currency, int32_t cost) {
    if (cost < 0) return false;
    return currency == Currency::Cash ? wallet.cash >= cost : wallet.gp >= cost;
}

bool ShopPackets::loadPrice(uint32_t priceKey, PriceRow& out) {
    auto rows = Database::instance().queryPrepared(
        "SELECT price_key, unit_type, unit_amount, price_base, price_sale, currency "
        "FROM shop_price WHERE price_key = ? LIMIT 1",
        {priceKey});
    if (rows.empty()) return false;

    out.priceKey   = static_cast<uint32_t>(colI64(rows[0], "price_key", 0));
    out.unitType   = static_cast<uint32_t>(colI64(rows[0], "unit_type", 0));
    out.unitAmount = static_cast<uint32_t>(colI64(rows[0], "unit_amount", 0));
    out.priceBase  = static_cast<int32_t>(colI64(rows[0], "price_base", 0));
    out.priceSale  = static_cast<int32_t>(colI64(rows[0], "price_sale", 0));
    // the currency column is not on the wire so the client icon rule wins over it
    out.currency   = currencyOf(out.priceSale);
    return true;
}

bool ShopPackets::optionExists(uint32_t category, uint32_t baseKey, uint32_t priceKey) {
    auto rows = Database::instance().queryPrepared(
        "SELECT slot FROM shop_option "
        "WHERE category = ? AND base_key = ? AND price_key = ? LIMIT 1",
        {category, baseKey, priceKey});
    return !rows.empty();
}

bool ShopPackets::loadWallet(uint32_t characterId, Wallet& out) {
    auto rows = Database::instance().queryPrepared(
        "SELECT gold, cash FROM characters WHERE id = ? LIMIT 1", {characterId});
    if (rows.empty()) return false;
    out.gp   = static_cast<int32_t>(colI64(rows[0], "gold", 0));
    out.cash = static_cast<int32_t>(colI64(rows[0], "cash", 0));
    return true;
}

bool ShopPackets::loadLevel(uint32_t characterId, int32_t& out) {
    auto rows = Database::instance().queryPrepared(
        "SELECT level FROM characters WHERE id = ? LIMIT 1", {characterId});
    if (rows.empty()) return false;
    out = static_cast<int32_t>(colI64(rows[0], "level", 1));
    return true;
}

ShopPackets::PurchaseResult ShopPackets::validatePurchase(uint32_t characterId, uint32_t category,
                                                          uint32_t baseKey, int32_t priceKey,
                                                          PriceRow& priceOut, int32_t& costOut) {
    costOut = 0;

    if (category > CAT_MAX) return PurchaseResult::BadCategory;

    // client always sends even when its own resolve failed so this is the real gate
    if (priceKey < 0) return PurchaseResult::ClientResolveFail;

    const uint32_t key = static_cast<uint32_t>(priceKey);
    if (!loadPrice(key, priceOut)) return PurchaseResult::UnknownPrice;
    if (!optionExists(category, baseKey, key)) return PurchaseResult::OptionMismatch;

    DefRow def;
    if (!loadDefinition(category, baseKey, def)) return PurchaseResult::UnknownDefinition;
    if (def.visible == 0) return PurchaseResult::NotVisible;

    int32_t level = 0;
    if (!loadLevel(characterId, level)) return PurchaseResult::DbError;
    if (def.levelReq > static_cast<uint32_t>(level < 0 ? 0 : level)) {
        return PurchaseResult::LevelTooLow;
    }

    if (category == CAT_PET && def.conditionKey > 0 &&
        !hasPetCondition(characterId, def.conditionKey)) {
        return PurchaseResult::MissingCondition;
    }

    Wallet wallet;
    if (!loadWallet(characterId, wallet)) return PurchaseResult::DbError;

    costOut = chargedAmount(priceOut);
    if (!canAfford(wallet, priceOut.currency, costOut)) return PurchaseResult::NotEnoughMoney;

    return PurchaseResult::Ok;
}

ShopPackets::SellResult ShopPackets::validateSell(const SellRequest& req) {
    if (req.category > CAT_MAX) return SellResult::BadCategory;
    // no dispatcher case client side so the row would come back on the next login
    if (req.category == CAT_PET || req.category == CAT_CARCRAFT) return SellResult::NotSellable;
    return SellResult::Ok;
}

const char* ShopPackets::resultMessageKey(PurchaseResult result, bool cashCurrency) {
    // every key here exists in def trans index txt anything missing falls back which read as unknown error before
    switch (result) {
        case PurchaseResult::LevelTooLow:      return "MSG_LEVEL_TOO_LOW";
        case PurchaseResult::MissingCondition: return "MSG_NOT_CONDITION";
        case PurchaseResult::NotEnoughMoney:   return cashCurrency ? "MSG_NO_CASH" : "MSG_NO_MONEY";
        default: break;
    }
    return "MSG_UNKNOWN_ERROR";
}

bool ShopPackets::resolveCharacterIdByName(const std::u16string& name, uint32_t& out) {
    if (name.empty()) return false;
    auto rows = Database::instance().queryPrepared(
        "SELECT id FROM characters WHERE name = ? LIMIT 1", {toUtf8(name)});
    if (rows.empty()) return false;
    out = static_cast<uint32_t>(colI64(rows[0], "id", 0));
    return out != 0;
}

} // namespace knc
