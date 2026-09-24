/// 0107 0108 0109 must all be buffered before 010A sub 430EF0 sub 42FBD0 and sub 430420 crash otherwise

#pragma once
#include "net/Packet.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace knc {

/// car part category matches sub 42F6C0 switch and CarFactory tab indices
enum class CarPartCategory : uint32_t {
    Cover   = 0,
    Booster = 1,
    Tire    = 2,
    FFender = 3,
    RFender = 4,
    Bumper  = 5,
    Wing    = 6
};

/// price row key resolves via sub 4852E0 and sub 451E00 the 00C6 table or fails MSG UNKNOWN ERROR
struct CarPartPriceRow {
    uint32_t priceTableKey = 0;
    /// INFERRED 1 day count 2 usage count
    uint32_t periodType    = 0;
    uint32_t periodValue   = 0;
    uint32_t active        = 1;
};

/// 0108 part definition wire is 0x74 head plus 3 cstr plus 4 plus 0x10 times rows
struct CarPartDef {
    /// zero hides the row in the shop
    uint32_t enabled  = 1;
    /// record offset 0x08 the key every instance points at
    uint32_t partKey  = 0;
    /// must match the instance category
    uint32_t category = 0;
    /// ascii capped 32 feeds Car FactoryCar CAT percent s path
    std::string modelDirName;
    /// ascii capped 32 string table key
    std::string displayNameKey;
    /// ascii capped 33 string table key
    std::string descriptionKey;
    /// 0x44 stat block sub 48F710 accumulator
    float    stats[17]      = {0.0f};
    /// rec 0xBC and 0xC4 sub 42B7A0 draws the pair only for 0 to 25
    int32_t  abilityId[2]      = {-1, -1};
    /// rec 0xC0 and 0xC8 the percent printed beside the icon
    uint32_t abilityPercent[2] = {0, 0};
    /// only read for the tire slot sub 490A70
    uint32_t wheelAttach[3] = {0, 0, 0};
    /// client keeps the first four only
    std::vector<CarPartPriceRow> priceRows;
};

/// 0109 owned part instance exactly 0x84 bytes on the wire
struct CarPartInstance {
    /// what a preset slot and car config store
    uint32_t instanceId    = 0;
    /// must resolve in the 0x0108 catalog
    uint32_t partKey       = 0;
    /// must match the def
    uint32_t category      = 0;
    /// built slots that hold it sub 42FE20 draws the in use art above zero
    int32_t  equipRefcount = 0;
    /// inferred no client read any value is safe
    uint32_t priceTableKey = 0;
    /// 0 permanent 1 day number 2 usage count
    uint32_t periodType    = 0;
    int32_t  periodValue   = 0;
    /// zero blocks equip and draws UNIT EXPIRED
    uint32_t periodActive  = 1;
    /// tier thresholds 5 20 65
    int32_t  grade         = 0;
};

/// 0107 preset row exactly 0x34 bytes on the wire
struct CarPreset {
    uint32_t presetId       = 0;
    /// 0 an empty slot the stock default 1 a built car
    uint32_t slotState      = 1;
    /// ascii 9 chars max sub 432B20 widens it into a 10 WCHAR stack cell
    std::string name;
    /// zero on an empty slot else an owned factory chassis
    uint32_t kartInstanceId = 0;
    uint32_t partCover      = 0;
    uint32_t partTire       = 0;  ///< zero can never be saved MSG UNSUPPORT
    uint32_t partBooster    = 0;
    uint32_t partBumper     = 0;
    uint32_t partFFender    = 0;
    uint32_t partRFender    = 0;
    uint32_t partWing       = 0;
};

/// parsed c2s 010B save request everything here is attacker controlled
struct CarSaveRequest {
    bool     valid          = false;
    uint32_t presetId       = 0;
    uint32_t kartInstanceId = 0;
    uint32_t partCover      = 0;
    uint32_t partTire       = 0;
    uint32_t partBooster    = 0;
    uint32_t partBumper     = 0;
    uint32_t partFFender    = 0;
    uint32_t partRFender    = 0;
    uint32_t partWing       = 0;
    /// wire dword 0 of each 0x84 record
    std::vector<uint32_t> clientInstanceIds;
    /// wire dword 3 never trust it
    std::vector<int32_t>  clientRefcounts;
};

/// parsed c2s 0114 preset rename request everything here is attacker controlled
struct CarRenameRequest {
    bool        valid    = false;
    uint32_t    presetId = 0;
    std::string name;      ///< ascii no length prefix on the wire read bounded then clamped
};

class Transaction;

/// one owned factory chassis sub 42EFE0 draws one top row slot per built preset
struct FactoryChassis {
    uint32_t kartInstanceId = 0;
    /// vehicle template name the part family folder under Car FactoryCar
    std::string model;
};

/// a chassis with no built slot it takes the row presetId or a new row when zero
struct FactoryFit {
    FactoryChassis chassis;
    uint32_t presetId = 0;
};

/// what the sync does to the preset rows of one character
struct FactoryPresetPlan {
    /// a duplicate slot a lost chassis or a spare empty slot
    std::vector<uint32_t> dropPresetIds;
    /// rows turned back into the clean empty slot
    std::vector<uint32_t> resetPresetIds;
    /// chassis that get a basic set and a built slot
    std::vector<FactoryFit> fit;
    /// no chassis and no row so one empty slot is inserted
    bool seedEmpty = false;
};

/// builders for the CarFactory stage 18 opcode family
struct CustomCarPackets {

    /// client container caps of sub 4500E0 records past these are consumed then dropped
    static constexpr std::size_t MAX_PRESETS_PER_CHAR = 5;
    /// sub 44F760
    static constexpr std::size_t MAX_PARTS_PER_CHAR   = 256;
    /// sub 44F9F0
    static constexpr std::size_t MAX_PART_DEFS        = 256;
    /// sub 451CF0
    static constexpr std::size_t MAX_PRICE_ROWS       = 4;

    /// preset slot states 0x0107 row 0x04 sub 433140 draws the name plate only on a built car
    static constexpr uint32_t SLOT_EMPTY = 0;
    static constexpr uint32_t SLOT_BUILT = 1;

    /// sub 432B20 widens the name into WCHAR 10 with cch 20 and sub 455C20 wcscpy into 10 WCHAR
    static constexpr std::size_t PRESET_NAME_MAX = 9;

    /// owned kart 0x2C mode 3 makes 0x30 the durability kart durability bar draw 0x42AD20
    static constexpr uint32_t FACTORY_PERIOD_MODE = 3;
    /// a new chassis starts on the full bar 0x42AD20 clamps at 500
    static constexpr int32_t FACTORY_START_DURABILITY = 500;

    /// one preset per owned chassis kept built slots first then the chassis in kart id order
    static FactoryPresetPlan planFactoryPresets(const std::vector<CarPreset>& presets,
                                                const std::vector<FactoryChassis>& chassis);

    /// the BASIC part of each category 0 to 6 whose model folder is the chassis zero when none
    static std::array<uint32_t, 7> basicSetKeys(const std::string& model,
                                                const std::vector<CarPartDef>& defs);

    /// built slot of a chassis with one part instance per category 0 to 6
    static CarPreset chassisPreset(uint32_t presetId, uint32_t kartInstanceId,
                                   const std::array<uint32_t, 7>& instanceByCategory);

    /// equip count of each part the number of built slots that hold it sub 42F6C0 counts the same way
    static std::vector<std::pair<uint32_t, int32_t>> refcountsFromPresets(const std::vector<CarPreset>& presets);

    /// a live tire for a built slot the family basic tire first then any live tire else zero
    static uint32_t pickTire(const std::string& model, const std::vector<CarPartDef>& defs,
                             const std::vector<CarPartInstance>& owned);

    /// every owned chassis on mode 3 with a built slot and a basic set true when a row changed
    static bool syncFactoryLoadouts(int32_t characterId);

    /// a factory chassis just granted goes on mode 3 with a full bar true when the kart is one
    static bool applyFactoryDurability(Transaction& tx, uint32_t kartInstanceId);

    /// 0x34 preset row bytes
    static std::array<uint8_t, 0x34> presetRecord(const CarPreset& preset);

    /// 0x20 car config blob the tail of a preset row from offset 0x14
    static std::array<uint8_t, 0x20> carConfigBlob(const CarPreset& preset);

    /// 0x84 part instance bytes
    static std::array<uint8_t, 0x84> partInstanceRecord(const CarPartInstance& inst);

    /// resolved custom car 0x3C block for the 0021 and 003E tail
    static std::array<uint8_t, 0x3C> customCarBlock(uint32_t chassisKartKey,
                                                    const CarPreset& preset,
                                                    const std::vector<CarPartInstance>& owned);

    /// all zero custom car block for a kart that is not a factory car
    static std::array<uint8_t, 0x3C> customCarBlockEmpty();

    /// the empty slot the real server shows first no kart no part no name
    static CarPreset emptyPreset(uint32_t presetId);

    /// 0107 full preset snapshot one empty slot when the set is empty count 0 crashes stage 18
    static Packet presetList(const std::vector<CarPreset>& presets);

    /// 0x0108 one part definition
    static Packet partDef(const CarPartDef& def);

    /// 0x0109 one owned part instance
    static Packet partInstance(const CarPartInstance& inst);

    /// 010A open car craft ack no payload
    static Packet openCarCraftAck();

    /// 010B save result client patches the preset and every refcount
    static Packet saveResult(const CarPreset& preset,
                             uint32_t selectedKartInstanceId,
                             const std::vector<CarPartInstance>& parts);

    /// 0114 rename ack the name is clamped to 9 chars the rename click and dialog cells hold 10 WCHAR
    static Packet presetRenameAck(uint32_t presetId, const std::string& name);

    /// 0124 single preset row update same 0x34 shape as 0107 avoids a full resync
    static Packet presetRowUpdate(const CarPreset& preset);

    /// parses c2s 010B and resets the read cursor first
    static CarSaveRequest parseSaveRequest(Packet& pkt);

    /// keep only slots the player owns with the matching category
    static CarPreset validateSave(const CarSaveRequest& req,
                                  const CarPreset& current,
                                  const std::vector<CarPartInstance>& owned);

    /// a built car needs a factory chassis and a tire else it goes back to the empty slot
    static bool presetIsBuildable(const CarPreset& preset,
                                  const std::vector<uint32_t>& factoryKarts,
                                  const std::vector<CarPartInstance>& owned);

    /// parses c2s 0114 and resets the read cursor first
    static CarRenameRequest parseRenameRequest(Packet& pkt);

    /// clamps an ascii preset name to 9 chars dropping at the first null
    static std::string clampPresetName(const std::string& name);

    static std::vector<CarPreset>       loadPresets(int32_t characterId);
    static std::vector<CarPartInstance> loadPartInstances(int32_t characterId);
    static std::vector<CarPartDef>      loadPartDefs();

    /// persists a validated preset no op when the row is missing
    static bool savePreset(int32_t characterId, const CarPreset& preset);

    /// write back the equip count of every part from the stored built slots
    static bool syncRefcounts(int32_t characterId);

    /// insert the empty slot when the character has no preset
    static bool ensureDefaultPreset(int32_t characterId);

    /// renames one preset returns false when not owned by the character
    static bool renamePreset(int32_t characterId, uint32_t presetId, const std::string& name);

    /// kart instance id to 0x00C0 catalog key zero when unknown
    static uint32_t chassisKartKey(uint32_t kartInstanceId);

    /// true when the kart catalog row is flagged as a factory car
    static bool isFactoryKart(uint32_t kartInstanceId);

    /// end to end custom car block for a room or race member
    static std::array<uint8_t, 0x3C> customCarBlockFor(int32_t characterId,
                                                       uint32_t kartInstanceId);
};

} // namespace knc
