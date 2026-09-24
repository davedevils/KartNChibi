/// room craft sub 47FF70 catalog sub 47D9A0 owned instance sub 47D9D0 stage push and 0x010F save

#pragma once
#include "net/Packet.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

/// one 16 byte price slot sub 451CF0 keeps at most 4
struct RoomObjectPrice {
    uint32_t currencyKey = 0;   ///< currencyKey price item key fed to sub 451E00 periodType 0 permanent 1 expiry day number 2 remaining count
    uint32_t periodType  = 0;
    uint32_t periodValue = 0;   ///< periodValue day number or count depending on periodType extra dword 3 never read by client always 0
    uint32_t extra       = 0;
};

/// S2C 0x010C object definition one frame per object container cap 256
struct RoomObjectDef {
    uint32_t enabled       = 1;  ///< enabled 0 hides row from shop and editor tabs badge 1 itembox hot 2 itembox new sub 41A160
    uint32_t badge         = 0;
    uint32_t objectKey     = 0;  ///< objectKey catalog key referenced by every instance record category 0 Sky 1 Floor 2 BgObj 3 Object 4 Effect
    uint32_t category      = 0;
    uint32_t maxPlaceable  = 1;  ///< maxPlaceable per key cap of simultaneously placed instances requiredLevel compared against player level global
    uint32_t requiredLevel = 0;
    std::string assetFolder;     ///< assetFolder ascii up to 32 chars feeds World Room category folder nameLocKey ascii up to 32 chars too
    std::string nameLocKey;
    std::string descLocKey;      ///< descLocKey ascii up to 33 chars prices only first 4 reach the client
    std::vector<RoomObjectPrice> prices;
};

/// 48 byte owned instance record shared by 0x010D 0x010F and the 0x0013 decor tail
struct RoomObjectInstance {
    uint32_t instanceId  = 0;    ///< instanceId 0x00 lookup key of 0x010F ack must be unique stable objectKey 0x04 must exist in 0x010C catalog
    uint32_t objectKey   = 0;
    uint32_t category    = 0;    ///< category 0x08 0 Sky 1 Floor 2 BgObj 3 Object 4 Effect pos0 0x0C do not reorder axis unresolved
    float    pos0        = 0.0f;
    float    pos1        = 0.0f; ///< pos1 0x10 pos2 0x14
    float    pos2        = 0.0f;
    float    yaw         = 0.0f; ///< yaw 0x18 placedFlag 0x1C 1 counts towards the placement caps
    uint32_t placedFlag  = 0;
    uint32_t priceKey    = 0;    ///< priceKey 0x20 tail convention no room craft path reads it periodType 0x24 0 permanent 1 expiry day 2 count
    uint32_t periodType  = 0;
    uint32_t periodValue = 0;    ///< periodValue 0x28 also stack quantity on merge path activeFlag 0x2C sub 488300 skips record unless this is 1
    uint32_t activeFlag  = 0;
};

/// room craft packet builders all static
struct RoomCraftPackets {
    static constexpr uint16_t OP_OBJECT_DEF    = 0x010C;  ///< objectDef S2C catalog placedObject S2C owned instance
    static constexpr uint16_t OP_PLACED_OBJECT = 0x010D;
    static constexpr uint16_t OP_STAGE_PUSH    = 0x010E;  ///< stagePush S2C force stage 19 save opcode both directions
    static constexpr uint16_t OP_SAVE          = 0x010F;

    static constexpr size_t RECORD_SIZE       = 0x30;   ///< recordSize 48 byte instance record catalogCap sub 4528E0 drops record 257 and later
    static constexpr size_t CATALOG_CAP       = 256;
    static constexpr size_t INSTANCE_CAP      = 256;    ///< instanceCap sub 4523A0 drops item 257 and later priceSlotCap sub 451CF0 refuses slot 5 and later
    static constexpr size_t PRICE_SLOT_CAP    = 4;
    static constexpr size_t ASSET_FOLDER_MAX  = 32;     ///< assetFolderMax and nameLocKeyMax both dest buffer 33 with the NUL
    static constexpr size_t NAME_LOC_KEY_MAX  = 32;
    static constexpr size_t DESC_LOC_KEY_MAX  = 33;     ///< descLocKeyMax dest buffer 34 with NUL framePayloadMax payload plus 8 byte header stays under 0x2000
    static constexpr size_t FRAME_PAYLOAD_MAX = 0x1FF7;
    static constexpr size_t SAVE_ACK_MAX_RECS = 170;    ///< saveAckMaxRecs 1FF7 minus 4 over 48 placedPerCategoryCap sub 4523F0 gate
    static constexpr size_t PLACED_PER_CATEGORY_CAP = 50;
    static constexpr uint32_t PROP_KEY_MIN = 4001;      ///< category 3 hardcoded range in sub 488300
    static constexpr uint32_t PROP_KEY_MAX = 4032;

    /// encodes one 48 byte instance record shared by 0x010D 0x010F and 0x0013
    static std::array<uint8_t, RECORD_SIZE> decorRecord(const RoomObjectInstance& inst);

    /// decode one 48 byte instance record from a wire buffer
    static RoomObjectInstance decodeRecord(const uint8_t* rec48);

    /// S2C 0x010C one object definition
    static Packet objectDefinition(const RoomObjectDef& def);

    /// S2C 0x010C one frame per definition trimmed to the catalog cap
    static std::vector<Packet> objectCatalog(const std::vector<RoomObjectDef>& defs);

    /// S2C 0x010D one owned instance
    static Packet placedObject(const RoomObjectInstance& inst);

    /// S2C 0x010D one frame per instance trimmed to the instance cap
    static std::vector<Packet> ownedInstances(const std::vector<RoomObjectInstance>& insts);

    /// S2C 0x010E empty frame that forces stage 19 send after catalog and inventory
    static Packet stagePush();

    /// S2C 0x010F ack built from records trimmed to the save ack cap
    static Packet saveAck(const std::vector<RoomObjectInstance>& records);

    /// S2C 0x010F ack echoing the exact bytes the client sent the safest form
    static Packet saveAckEcho(const std::vector<std::array<uint8_t, RECORD_SIZE>>& raw);

    /// S2C 0x010F ack with count 0 always safe still closes the save dialog
    static Packet saveAckEmpty();

    /// appends the 0x0013 decor tail returns records actually appended after the frame budget trim
    static size_t appendDecorTail(Packet& pkt, const std::vector<RoomObjectInstance>& decor);

    /// decodes a C2S 0x010F save request false when the payload is truncated or out of range
    static bool parseSaveRequest(const std::vector<uint8_t>& payload,
                                 std::vector<std::array<uint8_t, RECORD_SIZE>>& rawOut,
                                 std::vector<RoomObjectInstance>& out);

    /// forces activeFlag 0 on a record sub 488300 would crash on or ignore returns count deactivated
    static size_t sanitizeDecor(const std::vector<RoomObjectDef>& catalog,
                                std::vector<RoomObjectInstance>& decor);

    /// server side re check of the two placement caps the client skips on one replace branch
    static bool checkPlacementCaps(const std::vector<RoomObjectDef>& catalog,
                                   const std::vector<RoomObjectInstance>& insts,
                                   std::string& reason);

    /// true when every instance id of ack is present in held
    static bool ackIdsAreHeld(const std::vector<RoomObjectInstance>& held,
                              const std::vector<RoomObjectInstance>& ack);

    /// loads every room object def row plus its price slots ordered by slot index
    static std::vector<RoomObjectDef> loadObjectDefs();

    /// load the owned instances of one player for S2C 0x010D
    static std::vector<RoomObjectInstance> loadPlayerInstances(int32_t playerId);

    /// loads the decor of one room inner join drops keys absent from the catalog
    static std::vector<RoomObjectInstance> loadRoomDecor(int32_t roomId);

    /// persists the fields the client diffs position yaw and placed flag
    static bool persistSave(int32_t playerId, const std::vector<RoomObjectInstance>& records);
};

} // namespace knc
