/// reward popup is opcode 0x0135 not 0x0134 the dispatcher sub 4777C0 has no case for 308

#pragma once

#include "net/Packet.h"
#include "packets/gen/InventoryPackets.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

class Transaction;

struct GachaPetPackets {

    /// C2S and S2C gacha handled by sub 4830C0 out and sub 47D5E0 in
    static constexpr uint16_t OP_GACHA          = 0x00ED;
    /// S2C pet definition catalog row sub 4800D0
    static constexpr uint16_t OP_PET_DEF        = 0x0103;
    /// S2C owned pet list sub 47E3A0
    static constexpr uint16_t OP_OWNED_PET_LIST = 0x0104;
    /// C2S and S2C buy pet leg is category 4
    static constexpr uint16_t OP_BUY            = 0x00B7;
    /// C2S and S2C equip pet leg is category 4
    static constexpr uint16_t OP_EQUIP          = 0x00B9;
    /// C2S and S2C unequip pet leg is category 4
    static constexpr uint16_t OP_UNEQUIP        = 0x00BA;
    /// S2C level up random item popup dispatcher case 309
    static constexpr uint16_t OP_RANDOM_REWARD  = 0x0135;

    /// sub 4571D0 tab zero the Gacha Coin the popup rolls with at rest
    static constexpr uint32_t TICKET_BASE_KEY      = 2000;
    /// sub 4571D0 tab one the Gold Coin the tab click at 0x456904 switches to
    static constexpr uint32_t GOLD_COIN_BASE_KEY   = 2001;

    /// 20 byte S2C 0x00ED header five dwords
    static constexpr size_t GACHA_HEADER_SIZE = 20;
    /// 28 byte echoed 0x001D ticket row unconditional
    static constexpr size_t SIZE_TICKET_REC   = 0x1C;

    static constexpr size_t SIZE_CHAR_REC          = 0x2C;
    static constexpr size_t SIZE_KART_REC          = 0x38;
    static constexpr size_t SIZE_SMALL_REC         = 0x1C;  ///< item and part
    static constexpr size_t SIZE_ROOMCRAFT_REC     = 0x30;
    static constexpr size_t SIZE_CARCRAFT_REC      = 0x84;
    static constexpr size_t SIZE_CARCRAFT_SLOT_REC = 0x34;

    /// S2C 0x00ED prize category not the 0x00B7 enum 4 is roomcraft and 5 is carcraft no pet
    enum GachaCategory : uint32_t {
        GC_CHARACTER = 0,
        GC_KART      = 1,
        GC_ITEM      = 2,
        GC_PART      = 3,
        GC_ROOMCRAFT = 4,
        GC_CARCRAFT  = 5,
        GC_NONE      = 6   ///< anything 6 or above reads no tail and draws no icon
    };

    /// header period mode is the sprite selector of sub 456B90 modes 0 and 3 draw no digits
    enum GachaPeriodMode : uint32_t {
        GP_PERMANENT = 0,
        GP_DAYS      = 1,
        GP_TIMES     = 2,
        GP_SILENT    = 3
    };

    /// purchase category enum of 0x00B7 0x00B9 0x00BA and 0x0135
    static constexpr uint32_t CAT_PET = 4;

    static constexpr size_t CAP_PET_DEFS   = 32;  ///< PET DEFS sub 451750 returns -1 past 32 OWNED PETS sub 451530 returns -1 past 64
    static constexpr size_t CAP_OWNED_PETS = 64;
    static constexpr size_t CAP_PET_OPTIONS = 4;  ///< PET OPTIONS sub 451CF0 refuses slot 5

    static constexpr size_t MAX_STR1 = 32;  ///< handler stack buffers are 33 33 and 34 bytes longer smashes its frame MAX STR1 is model folder
    static constexpr size_t MAX_STR2 = 32;
    static constexpr size_t MAX_STR3 = 33;  ///< MAX STR2 is title loc key MAX STR3 is info loc key

    static constexpr uint32_t PET_KEY_ROSIE  = 10;  ///< PET KEY ROSIE is 5% skid gauge in speed mode PET KEY CHAI is 3% booster chance on mini turbo
    static constexpr uint32_t PET_KEY_CHAI   = 20;
    static constexpr uint32_t PET_KEY_PORKI  = 30;  ///< PORKI is 3 km per hour max speed during a booster DIMDIM is half a second blue booster duration
    static constexpr uint32_t PET_KEY_DIMDIM = 40;

    /// owned pet row and owned consumable row of the inventory domain
    using PetRow  = InventoryPackets::PetRow;
    using ItemRow = InventoryPackets::ItemRow;

    /// parsed C2S 0x00ED never trust remainingRolls since the client never decrements so a double click repeats
    struct GachaRollRequest {
        uint32_t instanceId        = 0;  ///< instanceId 0x00 owned ticket instance uid itemBaseKey 0x04 2000 from tab zero 2001 from tab one
        uint32_t itemBaseKey       = 0;
        uint32_t purchaseOptionKey = 0;  ///< purchaseOptionKey is 0x08 periodMode is 0x0C must be 2 for a ticket
        uint32_t periodMode        = 0;
        int32_t  remainingRolls    = 0;  ///< remainingRolls 0x10 client side count advisory only activeFlag 0x14 client gate non zero to send
        uint32_t activeFlag        = 0;
        int32_t  inUseFlag         = 0;  ///< inUseFlag 0x18 copied never read on this path raw is verbatim bytes for the log
        std::array<uint8_t, SIZE_TICKET_REC> raw{};
    };

    /// parse C2S 0x00ED needs exactly 28 bytes
    static bool parseRoll(const Packet& pkt, GachaRollRequest& out);

    /// cross checks a roll request against the ticket row and rejects a mismatch or a zero count
    static bool rollRequestMatchesTicket(const GachaRollRequest& req, const ItemRow& serverTicket);

    /// one weighted row of the prize table
    struct GachaPrizeEntry {
        uint32_t weight        = 0;  ///< weight 0 is never picked rareFlag is the animation selector only non zero plays the rare machine
        uint32_t rareFlag      = 0;
        uint32_t prizeCategory = GC_NONE;
        uint32_t prizeBaseKey  = 0;
        uint32_t periodMode    = GP_PERMANENT;
        int32_t  periodValue   = 0;
    };

    /// outcome of a roll hasPrize false means send resultNoPrize
    struct GachaRollOutcome {
        bool            hasPrize = false;
        size_t          index    = 0;  ///< index inside the table that was passed in
        GachaPrizeEntry entry{};
    };

    /// sum of every weight 0 when the table can never produce a prize
    static uint64_t totalWeight(const std::vector<GachaPrizeEntry>& table);

    /// deterministic weighted pick exposed so a test can pin the outcome using any 64 bit value
    static GachaRollOutcome rollPrizeWithValue(const std::vector<GachaPrizeEntry>& table,
                                               uint64_t rollValue);

    /// weighted pick with the process RNG the client never rolls this is the roll
    static GachaRollOutcome rollPrize(const std::vector<GachaPrizeEntry>& table);

    /// drops rows the client cannot render including any category 2 row matching the ticket key which would wipe it
    static size_t sanitizePrizePool(std::vector<GachaPrizeEntry>& table, uint32_t ticketBaseKey);

    /// the five header dwords stored at popup offset 0x1AAFC to 0x1AB0C
    struct GachaHeader {
        uint32_t rareFlag         = 0;        ///< rareFlag 0x00 non zero plays the rare machine prizeCategory 0x04 drives both the tail read and the icon
        uint32_t prizeCategory    = GC_NONE;
        uint32_t prizeBaseKey     = 0;        ///< prizeBaseKey 0x08 definition key of that category prizePeriodMode 0x0C sprite selector
        uint32_t prizePeriodMode  = GP_PERMANENT;
        int32_t  prizePeriodValue = 0;        ///< prizePeriodValue 0x10 three digits display only
    };

    /// header built straight from a rolled entry
    static GachaHeader headerFor(const GachaPrizeEntry& entry);

    /// byte count the client will read after the echoed ticket row
    static size_t prizeTailSize(uint32_t category, bool hasSlotRecord);

    /// S2C 0x00ED generic form is header then ticket echo then raw tail matching the category record
    static Packet result(const GachaHeader& header, const ItemRow& updatedTicket,
                         const uint8_t* tail, size_t tailLen);

    /// category 0 0x2C owned character record
    static Packet resultCharacter(const GachaHeader& header, const ItemRow& updatedTicket,
                                  const std::array<uint8_t, SIZE_CHAR_REC>& record);
    /// category 1 0x38 owned kart record
    static Packet resultKart(const GachaHeader& header, const ItemRow& updatedTicket,
                             const std::array<uint8_t, SIZE_KART_REC>& record);
    /// category 2 0x1C owned consumable record shares the ticket container so a key collision wipes it
    static Packet resultItem(const GachaHeader& header, const ItemRow& updatedTicket,
                             const std::array<uint8_t, SIZE_SMALL_REC>& record);
    /// category 3 0x1C owned kart part record
    static Packet resultPart(const GachaHeader& header, const ItemRow& updatedTicket,
                             const std::array<uint8_t, SIZE_SMALL_REC>& record);
    /// category 4 0x30 room craft record merges when rec offset 8 equals 3
    static Packet resultRoomCraft(const GachaHeader& header, const ItemRow& updatedTicket,
                                  const std::array<uint8_t, SIZE_ROOMCRAFT_REC>& record);
    /// category 5 0x84 car craft record then u8 then an optional 0x34 slot record
    static Packet resultCarCraft(const GachaHeader& header, const ItemRow& updatedTicket,
                                 const std::array<uint8_t, SIZE_CARCRAFT_REC>& record,
                                 const std::array<uint8_t, SIZE_CARCRAFT_SLOT_REC>* slotRecord);

    /// only clean refusal since there is no gacha reject opcode sends the unchanged ticket with category 6
    static Packet resultNoPrize(const ItemRow& unchangedTicket);

    /// post decrement ticket row to echo remaining is clamped at 0
    static ItemRow decrementedTicket(const ItemRow& before);

    /// 28 byte header of sub 47EEF0 three dwords have no reader anywhere
    struct RandomRewardHeader {
        uint32_t unknown00  = 0;  ///< unknown00 0x00 read into a local sub 47EEF0 never uses it resultCode 0x04 -900 -800 or -700 picks special branches
        int32_t  resultCode = 0;
        uint32_t popupArgA  = 0;  ///< popupArgA 0x08 stored at 0x11B44E8 no reader popupArgB 0x0C stored at 0x11B450C no reader
        uint32_t popupArgB  = 0;
        uint32_t category   = 6;  ///< category 0x10 the 0x00B7 enum case 4 pet does not exist unknown14 0x14 read into a local no reader
        uint32_t unknown14  = 0;
        uint32_t unknown18  = 0;  ///< unknown18 0x18 read into a local no reader
    };

    /// byte count the 0x0135 handler reads after its 28 byte header
    static size_t randomRewardTailSize(uint32_t category, bool hasSlotRecord);

    /// S2C 0x0135 reward popup category 4 has no case and is refused here
    static Packet randomReward(const RandomRewardHeader& header,
                               const uint8_t* tail, size_t tailLen);

    /// S2C 0x0135 category 6 builds the tail in the 0x84 then u8 order
    static Packet randomRewardCarCraft(const RandomRewardHeader& header,
                                       const std::array<uint8_t, SIZE_CARCRAFT_REC>& record,
                                       const std::array<uint8_t, SIZE_CARCRAFT_SLOT_REC>* slotRecord);

    /// one 16 byte purchase option of a pet definition
    struct PetOption {
        uint32_t priceOptionKey = 0;  ///< priceOptionKey is element 0 resolved through the 0x00C6 price table periodMode is element 1 mode 1 days 2 uses
        uint32_t periodMode     = 0;
        int32_t  periodValue    = 0;  ///< periodValue is element 2 unused is element 3 no reader
        uint32_t unused         = 0;
    };

    /// S2C 0x0103 pet definition one packet per pet container cap 32
    struct PetDef {
        uint32_t shopVisibleFlag    = 1;  ///< shopVisibleFlag 0x00 zero hides the shop row badge 0x04 0 none 1 hot 2 new
        uint32_t badge              = 0;
        uint32_t petBaseKey         = 0;  ///< petBaseKey 0x08 has a race effect only for four keys requiredPendantKey 0x0C sub 460A10 checks against owned pendant list
        uint32_t requiredPendantKey = 0;
        std::string modelFolder;  ///< modelFolder is str1 folder under Data Public Pet Body and Facial titleKey is str2 def trans index display name key
        std::string titleKey;
        std::string infoKey;      ///< infoKey is str3 def trans index description key options only the first 4 reach the client
        std::vector<PetOption> options;
    };

    /// true for 10 20 30 40 the literal switch of sub 451490
    static bool petKeyHasRaceEffect(uint32_t petBaseKey);

    /// true for the four folders shipped under Data Public Pet Body
    static bool modelFolderIsShipped(const std::string& folder);

    /// S2C 0x0103 one pet definition
    static Packet petDefinition(const PetDef& def);

    /// S2C 0x0103 one frame per pet trimmed to CAP PET DEFS
    static std::vector<Packet> petCatalog(const std::vector<PetDef>& defs);

    /// S2C 0x0104 full replace of the owned pet container delegates to the inventory domain
    static Packet ownedPetList(const std::vector<PetRow>& rows);

    /// clears equipped flags past the first since race effect scans take only the first index match
    static size_t enforceSingleEquipped(std::vector<PetRow>& rows);

    /// base key of the equipped row 0 when none feeds 0x0021 and 0x003E
    static uint32_t equippedPetBaseKey(const std::vector<PetRow>& rows);

    /// true when instanceId is present since the ack lookup has no null check
    static bool ackRowIsSafe(const std::vector<PetRow>& clientRows, uint32_t instanceId);

    /// S2C 0x00B7 category 4 buy ok 12 bytes then the 0x1C new pet row
    static Packet buyAck(uint32_t moneyGp, uint32_t moneyCash, const PetRow& newPet);

    /// S2C 0x00B9 category 4 first equip sub op 1 36 bytes
    static Packet equipAck(const PetRow& newPet);

    /// S2C 0x00B9 category 4 swap sub op 2 64 bytes previous row first
    static Packet equipAckReplace(const PetRow& previousPet, const PetRow& newPet);

    /// S2C 0x00BA category 4 32 bytes sends the row with rec offset 0x08 as 0
    static Packet unequipAck(const PetRow& row);

    /// C2S 0x00B7 category 4 the trailing wstring is the launcher session token
    struct PetBuyRequest {
        uint32_t category = 0;   ///< category 0x00 always 4 on this leg petBaseKey 0x04 pet def rec offset 0x08
        uint32_t petBaseKey = 0;
        int32_t  priceKey = -1;  ///< priceKey 0x08 -1 when client failed to resolve it reject it sessionToken 0x0C not a username never join on it
        std::u16string sessionToken;
    };

    /// C2S 0x00B9 category 4 12 bytes
    struct PetEquipRequest {
        uint32_t category   = 0;
        uint32_t petBaseKey = 0;  ///< petBaseKey is a BASE KEY not an instance uid aux is always -1 on the pet leg
        int32_t  aux        = -1;
    };

    /// C2S 0x00BA category 4 8 bytes
    struct PetUnequipRequest {
        uint32_t category   = 0;
        uint32_t petBaseKey = 0;
    };

    /// parse C2S 0x00B7 false on a short or unterminated payload
    static bool parseBuy(const Packet& pkt, PetBuyRequest& out);
    /// parse C2S 0x00B9 needs exactly 12 bytes
    static bool parseEquip(const Packet& pkt, PetEquipRequest& out);
    /// parse C2S 0x00BA needs exactly 8 bytes
    static bool parseUnequip(const Packet& pkt, PetUnequipRequest& out);

    /// pet definitions from shop definition and shop option category 4
    static std::vector<PetDef> loadPetDefs();

    /// owned pets of one character ordered by instance id
    static std::vector<PetRow> loadOwnedPets(uint32_t characterId);

    /// read one owned consumable row used for the gacha ticket
    static bool loadTicket(uint32_t characterId, uint32_t ticketBaseKey, ItemRow& out);

    /// enabled prize rows of one ticket key already sanitized
    static std::vector<GachaPrizeEntry> loadPrizePool(uint32_t ticketBaseKey);

    /// guarded ticket decrement runs an update that fails unless exactly one row moved so a double click can't spend twice
    static bool consumeTicketInTx(Transaction& tx, uint32_t characterId,
                                  uint32_t ticketBaseKey, ItemRow& updatedOut);

    /// consumeTicketInTx plus the gacha history row in a transaction it owns
    static bool consumeTicketAndLog(uint32_t characterId, const GachaRollRequest& req,
                                    const GachaRollOutcome& outcome, ItemRow& updatedOut);

    /// append one gacha history row inside a caller owned transaction
    static bool logRollInTx(Transaction& tx, uint32_t characterId,
                            const GachaRollRequest& req, const GachaRollOutcome& outcome,
                            int32_t remainingAfter);

    /// equips one pet keeping at most one row flagged and mirrors the base key into characters active pet id
    static bool setEquippedPet(uint32_t characterId, uint32_t petBaseKey);

    /// unequip clears the flag and nulls characters active pet id
    static bool clearEquippedPet(uint32_t characterId, uint32_t petBaseKey);

    /// equipped base key from owned pet 0 when none
    static uint32_t loadEquippedPetBaseKey(uint32_t characterId);

    /// true when the character owns the pendant the pet definition requires
    static bool hasPetCondition(uint32_t characterId, uint32_t conditionKey);
};

}  // namespace knc
