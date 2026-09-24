/// unproven fields are named unkNN and default to zero so never synthesize them since callers only echo real values

#pragma once

#include "net/Packet.h"
#include "net/Protocol.h"
#include "packets/PacketBuilder.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

struct InventoryPackets {

    static constexpr uint16_t OP_OWNED_CHARACTER_LIST = 0x001B;  ///< CHAR LIST is S2C list i32 count then 0x2C per row KART LIST is same shape with 0x38 per row
    static constexpr uint16_t OP_OWNED_KART_LIST      = 0x001C;
    static constexpr uint16_t OP_OWNED_ITEM_LIST      = 0x001D;  ///< ITEM LIST is S2C list i32 count then 0x1C per row PART RECORD is single 0x1C record no count prefix
    static constexpr uint16_t OP_OWNED_PART_RECORD    = 0x001E;
    static constexpr uint16_t OP_OWNED_PET_LIST       = 0x0104;  ///< PET LIST is S2C list i32 count then 0x1C per row APPEND CHARACTER appends one 0x2C row no dedupe
    static constexpr uint16_t OP_APPEND_CHARACTER     = 0x009D;
    static constexpr uint16_t OP_APPEND_KART          = 0x009E;  ///< APPEND KART appends one 0x38 row no dedupe APPEND PART appends one 0x1C row no dedupe
    static constexpr uint16_t OP_APPEND_PART          = 0x009F;
    static constexpr uint16_t OP_BUY                  = 0x00B7;  ///< BUY and DELETE both work in both directions
    static constexpr uint16_t OP_DELETE               = 0x00B8;
    static constexpr uint16_t OP_INSTALL              = 0x00B9;  ///< INSTALL is equip use or select both directions REMOVE is unequip both directions
    static constexpr uint16_t OP_REMOVE               = 0x00BA;
    static constexpr uint16_t OP_APPLY_SET            = 0x00BC;  ///< APPLY SET is S2C apply equipment set SET REMAINING USES is S2C set uses on one 0x1D row
    static constexpr uint16_t OP_SET_REMAINING_USES   = 0x0115;

    // category enum proven three times in sub 4852E0 sub 484690 and sub 484F50
    static constexpr uint32_t CAT_CHARACTER  = 0;
    static constexpr uint32_t CAT_KART       = 1;
    static constexpr uint32_t CAT_ITEM       = 2;
    static constexpr uint32_t CAT_PART       = 3;
    static constexpr uint32_t CAT_PET        = 4;
    static constexpr uint32_t CAT_ROOM_CRAFT = 5;
    static constexpr uint32_t CAT_CAR_CRAFT  = 6;

    // client container caps overflow on 0x9D 0x9E or 0x9F raises a type 2 dialog and closes the socket
    static constexpr size_t CAP_CHARACTER = 64;
    static constexpr size_t CAP_KART      = 64;
    static constexpr size_t CAP_ITEM      = 64;
    static constexpr size_t CAP_PART      = 256;
    static constexpr size_t CAP_PET       = 64;

    static constexpr size_t SIZE_CHARACTER_REC = 0x2C;
    static constexpr size_t SIZE_KART_REC      = 0x38;
    static constexpr size_t SIZE_ITEM_REC      = 0x1C;
    static constexpr size_t SIZE_PART_REC      = 0x1C;
    static constexpr size_t SIZE_PET_REC       = 0x1C;

    static constexpr uint32_t PERIOD_PERMANENT  = 0;  ///< PERMANENT never expires since sub 451C90 falls through DAY period value is an absolute day number
    static constexpr uint32_t PERIOD_DAY        = 1;
    static constexpr uint32_t PERIOD_COUNT      = 2;  ///< COUNT expired when period value is 0 or less DURABILITY gauge value client never expires it since server owns it
    static constexpr uint32_t PERIOD_DURABILITY = 3;

    /// 0x1B owned character row 0x2C bytes
    struct CharacterRow {
        uint32_t instanceId = 0;   ///< instanceId 0x00 unique inside container baseKey 0x04 0xBF def catalog key
        uint32_t baseKey    = 0;
        int32_t  accBody    = 0;   ///< accBody 0x08 accessory slot 2 O BODY accFace 0x0C accessory slot 3 O FACE
        int32_t  accFace    = 0;
        int32_t  accHead    = 0;   ///< accHead 0x10 accessory slot 4 O HEAD accGlass 0x14 accessory slot 5 O GLASS
        int32_t  accGlass   = 0;
        int32_t  accBack    = 0;   ///< accBack 0x18 accessory slot 6 O BACK priceKey 0x1C 0xC6 price row key sub 45E3D0 draws the price
        uint32_t priceKey   = 0;
        uint32_t periodMode = 0;   ///< periodMode is rec offset 0x20 periodValue is rec offset 0x24
        uint32_t periodValue = 0;
        uint32_t activeFlag = 0;   ///< activeFlag rec offset 0x28 zero enables the garage Delete button
    };

    /// 0x1C owned kart row 0x38 bytes
    struct KartRow {
        uint32_t instanceId    = 0;  ///< instanceId is rec offset 0x00 baseKey is rec offset 0x04 0xC0 def catalog key
        uint32_t baseKey       = 0;
        uint32_t skinPrimary   = 0;  ///< skinPrimary 0x08 0xC2 part overlays def 0x84 sub 490A70 resolves skinSecondary 0x0C 0xC2 part overlays def 0x88 same loader
        uint32_t skinSecondary = 0;
        uint32_t skinTertiary  = 0;  ///< skinTertiary 0x10 0xC2 part overlays def 0x8C sub 482DB0 reports a use custom3 0x14 overlays def 0x90 no reader
        uint32_t custom3       = 0;
        uint32_t custom4       = 0;  ///< custom4 0x18 overlays def 0x94 no reader custom5 0x1C overlays def 0x98 no reader
        uint32_t custom5       = 0;
        uint32_t appliedItemA  = 0;  ///< appliedItemA 0x20 0xC1 item key def 0x9C is default appliedItemB 0x24 0xC1 item key def 0xA0 is default
        uint32_t appliedItemB  = 0;
        uint32_t priceKey      = 0;  ///< priceKey 0x28 0xC6 price row key garage sub 415FF0 shop sub 45E3D0 draw price periodMode is rec offset 0x2C
        uint32_t periodMode    = 0;
        uint32_t periodValue   = 0;  ///< periodValue rec offset 0x30 durability when period mode is 3 activeFlag rec offset 0x34
        uint32_t activeFlag    = 0;
    };

    /// 0x1D owned consumable row 0x1C bytes period block starts at offset 0x08
    struct ItemRow {
        uint32_t instanceId  = 0;  ///< instanceId rec offset 0x00 baseKey rec offset 0x04 0xC1 def catalog key
        uint32_t baseKey     = 0;
        uint32_t priceKey    = 0;  ///< priceKey 0x08 0xC6 price row key sub 415FF0 draws price periodMode 0x0C four bytes earlier than 0x1E and 0x104
        uint32_t periodMode  = 0;
        int32_t  periodValue = 0;  ///< periodValue 0x10 remaining uses for def types 4 to 7 activeFlag rec offset 0x14
        uint32_t activeFlag  = 0;
        int32_t  inUseFlag   = 0;  ///< inUseFlag rec offset 0x18 client tests equal to 1 only
    };

    /// 0x1E owned kart part row 0x1C bytes period block starts at offset 0x0C
    struct PartRow {
        uint32_t instanceId  = 0;  ///< instanceId 0x00 must be unique across whole 0x1E container baseKey 0x04 0xC2 def catalog key
        uint32_t baseKey     = 0;
        uint32_t unk08       = 0;  ///< unk08 0x08 no reader client only echoes it in C2S 0x00CC priceKey 0x0C 0xC6 price row key via sub 415FF0
        uint32_t priceKey    = 0;
        uint32_t periodMode  = 0;  ///< periodMode 0x10 C2S 0x00CC gated on this being 2 periodValue rec offset 0x14
        uint32_t periodValue = 0;
        uint32_t activeFlag  = 0;  ///< activeFlag rec offset 0x18
    };

    /// 0x0104 owned pet row 0x1C bytes period block starts at offset 0x0C
    struct PetRow {
        uint32_t instanceId   = 0;  ///< instanceId rec offset 0x00 baseKey rec offset 0x04 0x0103 def catalog key
        uint32_t baseKey      = 0;
        uint32_t equippedFlag = 0;  ///< equippedFlag 0x08 client tests equal to 1 priceKey 0x0C 0xC6 price row key sub 415FF0 draws price
        uint32_t priceKey     = 0;
        uint32_t periodMode   = 0;  ///< periodMode rec offset 0x10 periodValue rec offset 0x14
        uint32_t periodValue  = 0;
        uint32_t activeFlag   = 0;  ///< activeFlag rec offset 0x18
    };

    // record blobs the same bytes every list ack and append publishes

    /// 0x2C character blob built on PacketBuilder characterRecord
    static std::array<uint8_t, 0x2C> characterBlob(const CharacterRow& row);

    /// 0x38 kart blob built on PacketBuilder kartRecord
    static std::array<uint8_t, 0x38> kartBlob(const KartRow& row);

    /// 0x1C consumable blob
    static std::array<uint8_t, 0x1C> itemBlob(const ItemRow& row);

    /// 0x1C kart part blob
    static std::array<uint8_t, 0x1C> partBlob(const PartRow& row);

    /// 0x1C pet blob
    static std::array<uint8_t, 0x1C> petBlob(const PetRow& row);

    // S2C list packets each one fully replaces its container

    /// 0x001B owned character list 4 bytes then 0x2C per row cap 64
    static Packet ownedCharacterList(const std::vector<CharacterRow>& rows);

    /// 0x001C owned kart list 4 bytes then 0x38 per row cap 64
    static Packet ownedKartList(const std::vector<KartRow>& rows);

    /// 0x001D owned consumable list 4 bytes then 0x1C per row cap 64
    static Packet ownedItemList(const std::vector<ItemRow>& rows);

    /// 0x001E one owned kart part 0x1C bytes no count prefix client upserts by base key
    static Packet ownedPartRecord(const PartRow& row);

    /// 0x001E for a whole set one packet per row capped at 256
    static std::vector<Packet> ownedPartRecords(const std::vector<PartRow>& rows);

    /// 0x0104 owned pet list 4 bytes then 0x1C per row cap 64
    static Packet ownedPetList(const std::vector<PetRow>& rows);

    // S2C single record appends no dedupe overflow closes the socket

    /// 0x009D append one character row to the 0x1B container
    static Packet appendCharacter(const CharacterRow& row);

    /// 0x009E append one kart row to the 0x1C container
    static Packet appendKart(const KartRow& row);

    /// 0x009F append one part row to the 0x1E container
    static Packet appendPart(const PartRow& row);

    // S2C 0x00B7 buy ack wire order is category then currencyB then currencyA which land in client globals 0x01A20B14 and 0x01A20B10

    static Packet buyAckCharacter(uint32_t currencyB, uint32_t currencyA, const CharacterRow& row);
    static Packet buyAckKart(uint32_t currencyB, uint32_t currencyA, const KartRow& row);
    static Packet buyAckItem(uint32_t currencyB, uint32_t currencyA, const ItemRow& row);
    static Packet buyAckPart(uint32_t currencyB, uint32_t currencyA, const PartRow& row);
    static Packet buyAckPet(uint32_t currencyB, uint32_t currencyA, const PetRow& row);

    // S2C 0x00B8 delete ack always 8 bytes erases by base key categories 4 and 6 are client no ops

    static Packet deleteAck(uint32_t category, uint32_t baseKey);

    // S2C 0x00B9 equip use or select ack

    /// category 0 client keeps only rec offset 0x00 as the selected character
    static Packet selectCharacterAck(const CharacterRow& row);

    /// category 1 client keeps only rec offset 0x00 as the selected kart
    static Packet selectKartAck(const KartRow& row);

    /// category 2 without the tail for 0xC1 def types outside 4 to 7
    static Packet useItemAck(const ItemRow& row);

    /// category 2 with the 16 byte kart period tail only send when the item's def type is 4 to 7
    static Packet useItemAckWithKartPeriod(const ItemRow& row, const KartRow& selectedKart);

    /// category 3 the row must already be in the 0x1E container
    static Packet equipPartAck(const PartRow& row);

    /// category 4 first equip subOp must not be 2 client crashes if instanceId is missing from the 0x0104 container
    static Packet equipPetAck(uint32_t subOp, const PetRow& newPet);

    /// category 4 swap subOp 2 previous pet row clears first
    static Packet equipPetAckReplace(const PetRow& previousPet, const PetRow& newPet);

    // S2C 0x00BA unequip ack values restore from the def catalogs and the record only identifies the row

    /// category 2
    static Packet unequipItemAck(const ItemRow& row);

    /// category 3
    static Packet unequipPartAck(const PartRow& row);

    /// category 4 same null check crash risk as the equip ack
    static Packet unequipPetAck(const PetRow& row);

    /// categories 0 1 5 6 read nothing after the category 4 bytes total
    static Packet unequipNoopAck(uint32_t category);

    // S2C 0x00BC apply equipment set shape A only

    /// 112 bytes selected ids then a zero count then both records does not close MSG WAIT
    static Packet applyEquipmentSet(uint32_t selectedCharacterInstanceId,
                                    uint32_t selectedKartInstanceId,
                                    const CharacterRow& characterRow,
                                    const KartRow& kartRow);

    // S2C 0x0115 sets remaining uses on one 0x1D row

    /// 8 bytes writes rec offset 0x10 of the matching row no null check so only send a known instance id
    static Packet setRemainingUses(uint32_t itemInstanceId, int32_t remainingUses);

    // C2S parsers for the garage verbs

    /// C2S 0x00B7 buy
    struct BuyRequest {
        uint32_t category = 0;        ///< category offset 0x00 baseKey offset 0x04 catalog key not an instance id
        uint32_t baseKey  = 0;
        int32_t  priceKey = -1;       ///< priceKey offset 0x08 -1 means client could not resolve a price accountName offset 0x0C UTF-16LE NUL terminated
        std::u16string accountName;
    };

    /// C2S 0x00B8 delete only sent when the row's active flag is zero
    struct DeleteRequest {
        uint32_t category = 0;
        uint32_t baseKey  = 0;
    };

    /// C2S 0x00B9 equip use or select
    struct InstallRequest {
        uint32_t category = 0;   ///< category offset 0x00 baseKey offset 0x04 owned record rec offset 0x04 not an instance id
        uint32_t baseKey  = 0;
        int32_t  aux      = -1;  ///< offset 0x08 -1 normally owned kart instance id in the custom car branch
    };

    /// C2S 0x00BA unequip
    struct RemoveRequest {
        uint32_t category = 0;
        uint32_t baseKey  = 0;
    };

    /// parses C2S 0x00B7 false when the payload is short or unterminated
    static bool parseBuy(const Packet& pkt, BuyRequest& out);

    /// parses C2S 0x00B8 needs exactly 8 bytes
    static bool parseDelete(const Packet& pkt, DeleteRequest& out);

    /// parses C2S 0x00B9 needs exactly 12 bytes
    static bool parseInstall(const Packet& pkt, InstallRequest& out);

    /// parses C2S 0x00BA needs exactly 8 bytes
    static bool parseRemove(const Packet& pkt, RemoveRequest& out);
};

}  // namespace knc
