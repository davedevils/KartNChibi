#pragma once

#include "net/Packet.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

struct ShopPackets {

    /// s2c shop stage ack with zero payload from sub 479550
    static constexpr uint16_t OP_S_SHOP_ACK       = 0x0010;
    /// s2c catalog reset with zero payload from sub 478B50
    static constexpr uint16_t OP_S_CATALOG_RESET  = 0x00BE;
    /// s2c price table row 28 bytes from sub 478CB0
    static constexpr uint16_t OP_S_PRICE_ROW      = 0x00C6;
    /// s2c pet definition catalog row from sub 4800D0
    static constexpr uint16_t OP_S_PET_DEF        = 0x0103;
    /// s2c car craft definition catalog row from sub 480210
    static constexpr uint16_t OP_S_CARCRAFT_DEF   = 0x0108;
    /// s2c room craft definition catalog row from sub 47FF70
    static constexpr uint16_t OP_S_ROOMCRAFT_DEF  = 0x010C;
    /// s2c buy ok from sub 484F50
    static constexpr uint16_t OP_S_BUY_OK         = 0x00B7;
    /// s2c sell ok 8 bytes from sub 484690
    static constexpr uint16_t OP_S_SELL_OK        = 0x00B8;
    /// s2c gift ok 220 bytes from sub 47BEF0
    static constexpr uint16_t OP_S_GIFT_OK        = 0x0098;
    /// s2c extend ok from sub 484EB0
    static constexpr uint16_t OP_S_EXTEND_OK      = 0x0112;
    /// s2c ascii dialog dispatcher case 1 from sub 478DA0
    static constexpr uint16_t OP_S_DIALOG_ASCII   = 0x0001;
    /// s2c utf16 dialog dispatcher case 2 from sub 478D40
    static constexpr uint16_t OP_S_DIALOG_WIDE    = 0x0002;

    /// c2s open shop with zero payload sender sub 4806B0
    static constexpr uint16_t OP_C_SHOP_ENTER     = 0x0010;
    /// c2s open shop from a channel with screen 7 and an arg sender sub 4806F0
    static constexpr uint16_t OP_C_SHOP_ENTER_ALT = 0x0018;
    /// c2s buy sender sub 4841D0
    static constexpr uint16_t OP_C_BUY            = 0x00B7;
    /// c2s sell or discard 8 bytes sender sub 4844A0
    static constexpr uint16_t OP_C_SELL           = 0x00B8;
    /// c2s gift sender sub 482880
    static constexpr uint16_t OP_C_GIFT           = 0x0098;
    /// c2s cash poll every 60 seconds while the shop stage is active sender sub 4832F0
    static constexpr uint16_t OP_C_CASH_POLL      = 0x00D0;
    /// c2s extend or renew 12 bytes sender sub 484570
    static constexpr uint16_t OP_C_EXTEND         = 0x0112;

    static constexpr size_t CAP_PRICE_ROWS   = 1536;  ///< priceRows cap enforced by sub 451DC0 petDefs cap enforced by sub 451750
    static constexpr size_t CAP_PET_DEFS     = 32;
    static constexpr size_t CAP_ROOMCRAFT    = 256;   ///< roomcraft cap enforced by sub 4528E0 carcraft cap enforced by sub 44F9F0
    static constexpr size_t CAP_CARCRAFT     = 256;
    static constexpr size_t CAP_OPTIONS      = 4;     ///< cap enforced by sub 451CF0

    /// handler stack buffers are 33 33 and 34 bytes longer strings smash the frame
    static constexpr size_t MAX_STR1 = 32;
    static constexpr size_t MAX_STR2 = 32;
    static constexpr size_t MAX_STR3 = 33;

    /// purchase category the switch value in sub 4852E0 and sub 484F50
    enum Category : uint32_t {
        CAT_CHARACTER = 0,  ///< CHARACTER defs at 00BF buy tail 0x2C KART defs at 00C0 buy tail 0x38
        CAT_KART      = 1,
        CAT_ITEM      = 2,  ///< ITEM defs at 00C1 buy tail 0x1C PART defs at 00C2 buy tail 0x1C
        CAT_PART      = 3,
        CAT_PET       = 4,  ///< PET defs at 0103 buy tail 0x1C ROOMCRAFT defs at 010C buy tail 0x30
        CAT_ROOMCRAFT = 5,
        CAT_CARCRAFT  = 6,  ///< defs at 0108 buy tail 1 plus 0x84 plus 0x34
        CAT_MAX       = 6
    };

    /// price row unit semantics from the sub 45D4A0 sub 454910 locale switch
    enum UnitType : uint32_t {
        UNIT_PERMANENT  = 0,  ///< PERMANENT unit amount ignored DAYS unit amount is days
        UNIT_DAYS       = 1,
        UNIT_USES       = 2,  ///< USES unit amount is uses DURABILITY unit amount ignored by the display
        UNIT_DURABILITY = 3
    };

    /// corner badge sprite selector definition wire offset 0x04
    enum Badge : uint32_t {
        BADGE_NONE = 0,
        BADGE_HOT  = 1,  ///< HOT Shop UI Shop itembox hot png NEW Shop UI Shop itembox new png
        BADGE_NEW  = 2
    };

    /// which wallet is charged server side only since the client never picks a currency
    enum class Currency : uint8_t { Gp = 0, Cash = 1 };

    /// one 16 byte purchase option only element 0 is ever read
    struct PurchaseOption {
        uint32_t priceKey = 0;  ///< must match a price row or the tile never draws
    };

    /// one s2c 00C6 row plus the server side currency selector
    struct PriceRow {
        uint32_t priceKey   = 0;               ///< priceKey 0x00 unique across all categories unitType 0x0C
        uint32_t unitType   = UNIT_PERMANENT;
        uint32_t unitAmount = 0;               ///< unitAmount 0x10 priceBase 0x14
        int32_t  priceBase  = 0;
        int32_t  priceSale  = 0;               ///< priceSale 0x18 greater than 0 replaces priceBase currency NOT on wire follows sale price like client icon
        Currency currency   = Currency::Gp;
    };

    /// s2c 0103 pet definition
    struct PetDef {
        uint32_t shopVisibleFlag    = 1;           ///< shopVisibleFlag 0x00 zero hides row badge 0x04
        uint32_t badge              = BADGE_NONE;
        uint32_t petBaseKey         = 0;           ///< petBaseKey 0x08 requiredPendantKey 0x0C pendant key sub 460A10 checks 0x11A owned pendant list
        uint32_t requiredPendantKey = 0;
        std::string name;   ///< name ascii truncated to MAX STR1 str2 ascii truncated to MAX STR2
        std::string str2;
        std::string desc;   ///< desc ascii truncated to MAX STR3 options truncated to CAP OPTIONS
        std::vector<PurchaseOption> options;
    };

    /// s2c 010C room craft definition
    struct RoomCraftDef {
        uint32_t shopVisibleFlag = 1;           ///< shopVisibleFlag 0x00 badge 0x04
        uint32_t badge           = BADGE_NONE;
        uint32_t baseKey         = 0;           ///< baseKey 0x08 subtype 0x0C 0 sky 1 terrain 2 back 3 obj 4 eff
        uint32_t subtype         = 0;
        uint32_t levelReq        = 0;           ///< wire 0x14
        std::string name;
        std::string str2;
        std::string desc;
        std::vector<PurchaseOption> options;
    };

    /// s2c 0108 car craft definition
    struct CarCraftDef {
        uint32_t shopVisibleFlag = 1;           ///< shopVisibleFlag 0x00 badge 0x04
        uint32_t badge           = BADGE_NONE;
        uint32_t baseKey         = 0;           ///< baseKey 0x08 subtype 0x0C
        uint32_t subtype         = 0;
        uint32_t levelReq        = 0;           ///< wire 0x10
        std::string name;
        std::string str2;
        std::string desc;
        std::array<uint8_t, 68> statBlock{};  ///< statBlock wire blob at 0x44 contents untraced pairBlock wire two 8 byte reads contents untraced
        std::array<uint8_t, 16> pairBlock{};
        std::array<uint8_t, 12> tailBlock{};  ///< wire blob at 0xC contents untraced
        std::vector<PurchaseOption> options;
    };

    /// the two client balances gp at 0x01A20B14 and cash at 0x01A20B10
    struct Wallet {
        int32_t gp   = 0;
        int32_t cash = 0;
    };

    /// c2s 00B7 priceKey is -1 when the client failed to resolve it
    struct BuyRequest {
        uint32_t category = 0;
        uint32_t baseKey  = 0;
        int32_t  priceKey = -1;
        std::u16string accountName;  ///< identity echo do not trust it key off the socket instead
    };

    /// c2s 00B8 key is a base key for cat 0 to 4 and 6 an instance uid for cat 5
    struct SellRequest {
        uint32_t category = 0;
        uint32_t key      = 0;
    };

    /// c2s 0098
    struct GiftRequest {
        uint32_t category = 0;
        std::u16string targetName;
        uint32_t baseKey  = 0;
        int32_t  priceKey = -1;
        std::u16string message;
    };

    /// c2s 0112 instanceUid must already exist client side or the ack crashes it
    struct ExtendRequest {
        uint32_t category    = 0;
        uint32_t instanceUid = 0;
        int32_t  priceKey    = -1;
    };

    /// c2s 00D0 second field is an ascii cstr not a fixed u32
    struct CashPollRequest {
        std::u16string accountName;
        std::string    extra;
    };

    /// c2s 0018 alternate shop entry reply with s2c 0010 and never echo 0018
    struct ShopEntryAltRequest {
        uint32_t targetScreen = 0;  ///< targetScreen 7 for the shop stage arg meaning unverified
        uint32_t arg          = 0;
    };

    /// 0x00BE catalog reset send exactly once before the shop stream
    static Packet catalogReset();

    /// 0x0010 shop stage ack send last so MSG WAIT closes after the data
    static Packet shopAck();

    /// 0x00C6 one price row 28 bytes offsets 0x04 and 0x08 are written zero
    static Packet priceRow(const PriceRow& row);

    /// 0x00C6 whole table one packet per row truncated to CAP PRICE ROWS
    static std::vector<Packet> priceTable(const std::vector<PriceRow>& rows);

    /// 0x0103 pet definition row
    static Packet petDefinition(const PetDef& def);

    /// 0x010C room craft definition row
    static Packet roomCraftDefinition(const RoomCraftDef& def);

    /// 0x0108 car craft definition row
    static Packet carCraftDefinition(const CarCraftDef& def);

    /// 0x00B7 buy ok generic form wire is category gp cash and record
    static Packet buyOk(uint32_t category, const Wallet& wallet,
                        const uint8_t* record, size_t recordLen);

    /// 0x00B7 category 0 0x2C record element 0 is uid and element 1 is base key
    static Packet buyOkCharacter(const Wallet& wallet, const std::array<uint8_t, 0x2C>& record);
    /// 0x00B7 category 1 0x38 record
    static Packet buyOkKart(const Wallet& wallet, const std::array<uint8_t, 0x38>& record);
    /// 0x00B7 category 2 3 or 4 0x1C record
    static Packet buyOkSmall(uint32_t category, const Wallet& wallet,
                             const std::array<uint8_t, 0x1C>& record);
    /// 0x00B7 category 5 0x30 record
    static Packet buyOkRoomCraft(const Wallet& wallet, const std::array<uint8_t, 0x30>& record);
    /// 0x00B7 category 6 a has extra byte plus 0x84 record plus an optional 0x34 record
    static Packet buyOkCarCraft(const Wallet& wallet, const std::array<uint8_t, 0x84>& record,
                                const std::array<uint8_t, 0x34>* extra52);

    /// 0x00B8 sell ok 8 bytes base key for cats 0-4 and 6 or an instance uid for cat 5
    static Packet sellOk(uint32_t category, uint32_t key);

    /// 0x0098 gift ok 220 bytes note the reversed order cash then gp
    static Packet giftOk(const Wallet& wallet);
    /// 0x0098 gift ok with an explicit 212 byte trailer whose contents are ignored
    static Packet giftOk(const Wallet& wallet, const uint8_t* blob212);

    /// 0x0112 extend ok 12 bytes for any category other than 5
    static Packet extendOk(uint32_t category, const Wallet& wallet);
    /// sub 484EB0 0x0112 extend ok category 5 60 bytes record 0 must be an owned instance uid
    static Packet extendOkRoomCraft(const Wallet& wallet, const std::array<uint8_t, 0x30>& record);

    /// 0x0002 utf16 dialog use dialog type 1 so the client is never wedged
    static Packet rejectWide(const std::u16string& messageKey, int32_t dialogType = 1);
    /// 0x0001 ascii dialog use dialog type 1 so the client is never wedged
    static Packet rejectAscii(const std::string& messageKey, int32_t dialogType = 1);

    static bool parseBuy(const Packet& pkt, BuyRequest& out);
    static bool parseSell(const Packet& pkt, SellRequest& out);
    static bool parseGift(const Packet& pkt, GiftRequest& out);
    static bool parseExtend(const Packet& pkt, ExtendRequest& out);
    static bool parseCashPoll(const Packet& pkt, CashPollRequest& out);
    static bool parseShopEntryAlt(const Packet& pkt, ShopEntryAltRequest& out);

    /// outcome of validatePurchase only Ok may debit and grant
    enum class PurchaseResult {
        Ok = 0,
        BadCategory,        ///< BadCategory category greater than 6 ClientResolveFail price key -1 client side sub 4852E0 failed
        ClientResolveFail,
        UnknownPrice,       ///< UnknownPrice no shop price row UnknownDefinition no shop definition row
        UnknownDefinition,
        OptionMismatch,     ///< OptionMismatch price key not an option of definition NotVisible shop visible flag is 0
        NotVisible,
        LevelTooLow,        ///< LevelTooLow level req greater than character level MissingCondition pet unlock condition not owned
        MissingCondition,
        NotEnoughMoney,
        DbError
    };

    /// outcome of validateSell
    enum class SellResult {
        Ok = 0,
        BadCategory,
        NotSellable  ///< categories 4 and 6 are no ops in sub 484690
    };

    /// amount actually charged priceSale when greater than 0 else priceBase
    static int32_t chargedAmount(const PriceRow& row);

    /// 0x45E36B sale above zero is an Astro price and a base price is gold
    static Currency currencyOf(int32_t priceSale);

    /// true when the wallet covers the cost in that currency
    static bool canAfford(const Wallet& wallet, Currency currency, int32_t cost);

    /// loads one shop price row false when absent or on a db error
    static bool loadPrice(uint32_t priceKey, PriceRow& out);

    /// true when category and baseKey really carry that price key in shop option
    static bool optionExists(uint32_t category, uint32_t baseKey, uint32_t priceKey);

    /// reads gp and cash from characters false when the row is missing
    static bool loadWallet(uint32_t characterId, Wallet& out);

    /// reads level from characters false when the row is missing
    static bool loadLevel(uint32_t characterId, int32_t& out);

    /// full server side gate for c2s 00B7 and the buy half of 0098 0112 read only
    static PurchaseResult validatePurchase(uint32_t characterId, uint32_t category,
                                           uint32_t baseKey, int32_t priceKey,
                                           PriceRow& priceOut, int32_t& costOut);

    /// category level gate for c2s 00B8 ownership is the caller's job
    static SellResult validateSell(const SellRequest& req);

    /// locale key for reject dialog only two keys proven cashCurrency picks gold or tek wording key reads as "Unknown Error"
    static const char* resultMessageKey(PurchaseResult result, bool cashCurrency = false);

    /// resolves a gift target name to a character id false when unknown
    static bool resolveCharacterIdByName(const std::u16string& name, uint32_t& out);
};

} // namespace knc
