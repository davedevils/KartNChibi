/// part and accessory catalogs sub 47F4F0 sub 47F6B0 sub 47F800 only 0x00C0 carries the 17 float stat block

#pragma once

#include "net/Packet.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

/// 0x10 price option row shared by 0x00C0 0x00C1 0x00C2
struct DefPriceOption {
    uint32_t priceTableKey = 0;  ///< priceTableKey 0x00 must exist in shop price periodType 0x04 unproven semantic
    uint32_t periodType    = 0;
    uint32_t periodValue   = 0;  ///< periodValue 0x08 unproven semantic active 0x0C unproven semantic
    uint32_t active        = 1;
};

/// S2C 0x00C1 item row verified against item catalog recv 0x47F6B0 carries no stats
struct ItemDefRow {
    uint32_t visible       = 1;   ///< visible record 0x00 zero hides row shop tile draw 0x419CC0 badge record 0x04 overlay 1 new 2 hot
    uint32_t badge         = 0;
    uint32_t itemKey       = 0;   ///< itemKey 0x08 key catalog lookup 0x4508C0 useType 0x0C 1 2 kart item slots 4 to 7 repair 8 9 pet
    uint32_t useType       = 0;
    uint32_t requiredLevel = 0;   ///< requiredLevel 0x10 level gate 0x419E2E vs byte 0x1A20B09 iconName 0x14 buffer 33 png stem icons load 0x4424F0
    std::string iconName;
    std::string displayNameKey;   ///< displayNameKey 0x35 buffer 33 title shop panel draw 0x45E3D0 descriptionKey 0x56 buffer 34 description
    std::string descriptionKey;
    std::vector<DefPriceOption> priceRows;
};

/// S2C 0x00C2 kart part row verified against part catalog recv 0x47F800 the model name sits between the word groups
struct KartPartDefRow {
    uint32_t visible        = 1;  ///< visible 0x00 zero hides row shop tile draw parts 0x4199D0 badge 0x04 overlay 1 new 2 hot
    uint32_t badge          = 0;
    uint32_t partKey        = 0;  ///< partKey 0x08 container key catalog lookup 0x4510C0 requiredLevel 0x0C level gate 0x419B9E vs byte 0x1A20B09
    uint32_t requiredLevel  = 0;
    std::string modelName;        ///< modelName 0x10 buffer 36 icons load 0x4423F0 restrictTarget 0x34 0 preview driver 1 preview kart restrict 0x415310
    uint32_t restrictTarget = 0;
    uint32_t equipSlot      = 0;  ///< equipSlot slot 0 colour 1 plate 2 body 3 face 4 head 5 glass 6 back 8 restrictKey 0xFFFFFFFF none
    uint32_t restrictKey    = 0;
    std::string displayNameKey;   ///< displayNameKey 0x40 buffer 33 title descriptionKey 0x61 buffer 35 description
    std::string descriptionKey;
    std::array<uint8_t, 8> abilityPair0{};   ///< abilityPair0 0x84 and abilityPair1 0x8C ability id then percent pairs draw 0x42B1B0
    std::array<uint8_t, 8> abilityPair1{};
    std::vector<DefPriceOption> priceRows;
};

/// one car craft part resolved into the shape the aggregator needs
struct EquippedPart {
    bool     present  = false;   ///< false writes nothing matches an id of zero
    uint32_t partKey  = 0;
    int32_t  grade    = 0;
    std::array<float, 17> stats{};       ///< stats offset 0x78 stat bonus add 0x48F710 tireExtra offset 0xCC tires slot only modelName offset 0x14 full set compare
    std::array<float, 3>  tireExtra{};
    std::string modelName;
};

/// output of the aggregation mirrors the client accumulator exactly
struct StatAggregate {
    bool factoryScheme = false;           ///< factoryScheme kart def offset 0x14 equals 1 base kart def offset 0xA4
    std::array<float, 17> base{};
    std::array<float, 17> bonus{};        ///< bonus is seven part sum zero off scheme total is base plus bonus what physics reads
    std::array<float, 17> total{};
    std::array<float, 3>  tireExtra{};    ///< tireExtra from tires slot only no grade fullSetMatched computed by client then NEVER read
    bool fullSetMatched = false;
};

/// garage bars sub 428AB0 Speed wire 0 and 1 Handling wire 2 Drift wire 8 10 11 Booster wire 3
struct GarageBars {
    std::array<float, 4> bar{};        ///< final value always inside 40 to 100
    std::array<float, 4> fraction{};
    std::array<bool, 4>  degenerate{}; ///< true when the catalog range was not positive
};

/// per wire stat physics values 13 has no reader 14 15 16 are the camera
struct DerivedPhysics {
    float accelScale         = 1.0f;  ///< accelScale wire 0 clamp 1 to 1 01 tick 0x49CDDA maxSpeedMul wire 1 clamp 1 to 2 times 320 kmh
    float maxSpeedMul        = 1.0f;
    float steeringGain       = 1.0f;  ///< steeringGain wire 2 clamp 1 to 4 miniTurboSpeedMul wire 3 clamp 1 to 1 2 times 120 kmh boost 0x496BE0
    float miniTurboSpeedMul  = 1.0f;
    float turnForceMul       = 1.0f;  ///< turnForceMul wire 5 clamp 1 to 2 snaps down above 2 bug kept wheelSpin wire 6 torque no clamp
    float wheelSpin          = 0.0f;
    float wheelSteerAngle    = 0.0f;  ///< wheelSteerAngle wire 7 front wheel angle no clamp driftChargeRate wire 8 clamp 0 3 to 0 8
    float driftChargeRate    = 0.3f;
    float driftSteer         = 1.2f;  ///< driftSteer wire 9 clamp 1 2 to 1 8 miniTurboThreshold wire 10 inverted clamp 0 2 to 1
    float miniTurboThreshold = 1.0f;
    float miniTurboHold      = 1.0f;  ///< miniTurboHold wire 11 inverted clamp 0 2 to 1 grip wire 12 no clamp per wheel 0x49C9D6
    float grip               = 0.0f;
};

/// parsed C2S 0x00CC kart part use notify
struct PartUseNotify {
    bool     valid       = false;
    uint32_t instanceId  = 0;   ///< instanceId echoed 0x1C offset 0x00 baseKey offset 0x04 0x00C2 definition key
    uint32_t baseKey     = 0;
    uint32_t word08      = 0;   ///< word08 offset 0x08 word0C offset 0x0C
    uint32_t word0C      = 0;
    uint32_t periodMode  = 0;   ///< periodMode offset 0x10 client only sends when this is 2 periodValue offset 0x14
    int32_t  periodValue = 0;
    uint32_t activeFlag  = 0;   ///< activeFlag offset 0x18 flag trailing byte 0 and 1 both observed unproven
    uint8_t  flag        = 0;
};

/// result of a server side grade change
struct UpgradeResult {
    bool        ok         = false;
    std::string reason;          ///< empty on success else a short cause
    uint32_t    instanceId = 0;
    uint32_t    partKey    = 0;
    int32_t     oldGrade   = 0;
    int32_t     newGrade   = 0;
};

/// result of a repair scroll use
struct RepairResult {
    bool        ok             = false;
    std::string reason;              ///< empty on success else a short cause
    uint32_t    itemKey        = 0;
    uint32_t    useType        = 0;  ///< the 0x00C1 definition offset 0x0C that gated it
    uint32_t    kartInstanceId = 0;
    int32_t     oldDurability  = 0;
    int32_t     newDurability  = 0;
    bool        wasLow         = false;  ///< client raised MSG DURABILITY LOW before
};

/// catalog builders parsers and the stat model
struct PartStatPackets {

    static constexpr uint16_t OP_S_KART_DEF        = 0x00C0;
    static constexpr uint16_t OP_S_ITEM_DEF        = 0x00C1;
    static constexpr uint16_t OP_S_KART_PART_DEF   = 0x00C2;
    /// 0x0109 owned part record the only way a grade change reaches the client
    static constexpr uint16_t OP_S_PART_INSTANCE   = 0x0109;
    /// C2S kart part use notify sender sub 482E60 and sub 482FE0
    static constexpr uint16_t OP_C_PART_USE_NOTIFY = 0x00CC;

    static constexpr size_t STAT_COUNT       = 17;
    static constexpr size_t SLOT_COUNT       = 7;
    static constexpr size_t TIRE_EXTRA_COUNT = 3;
    static constexpr size_t BAR_COUNT        = 4;

    /// client keeps only the first four price options sub 451CF0
    static constexpr size_t CAP_PRICE_ROWS = 4;
    /// 0x00C0 container cap stride 0x140
    static constexpr size_t CAP_KART_DEFS  = 64;
    /// 0x00C2 container cap unproven mirrors the other def tables
    static constexpr size_t CAP_PART_DEFS  = 256;

    /// grade scales stats 0 to 9 only this is where the flat half starts
    static constexpr size_t GRADE_SCALED_STATS = 10;
    /// the divisor in the grade multiplier sub 48F710 magic division
    static constexpr int32_t GRADE_BUCKET = 50;

    /// server side grade clamp client applies none 0 to 49 gives x1 50 to 99 x2 100 to 149 x3
    static constexpr int32_t GRADE_MIN = 0;
    static constexpr int32_t GRADE_MAX = 149;

    /// cosmetic rarity ladder deliberately not aligned with the 50 buckets
    static constexpr int32_t TIER_UNIQUE = 5;
    static constexpr int32_t TIER_EPIC   = 20;
    static constexpr int32_t TIER_LEGEND = 65;

    /// owned kart record offset 0x2C must equal this before durability applies
    static constexpr uint32_t DURABILITY_PERIOD_MODE = 3;
    /// client raises MSG DURABILITY LOW at or under this local player only
    static constexpr int32_t DURABILITY_LOW_WARN = 10;

    /// 0x00C1 definition offset 0x0C range that unlocks the repair verb
    static constexpr uint32_t REPAIR_USE_TYPE_MIN = 4;
    static constexpr uint32_t REPAIR_USE_TYPE_MAX = 7;

    /// handler stack buffers one byte of this is the NUL
    static constexpr size_t KART_BUF1 = 33;
    static constexpr size_t KART_BUF2 = 33;
    static constexpr size_t KART_BUF3 = 34;
    static constexpr size_t PART_BUF1 = 36;
    static constexpr size_t PART_BUF2 = 33;
    static constexpr size_t PART_BUF3 = 35;

    /// WIRE index k of the 0xC0 block at 0xA4 the client reads car 0x3448 plus 4 k bonus 0xA7940
    enum StatIndex : size_t {
        STAT_BODY_SETUP           = 0,   ///< idx0 0x49CDDA clamp 1 to 1 01 idx1 0x49CA95 clamp 1 to 2 times 320 kmh
        STAT_MAX_SPEED            = 1,
        STAT_STEERING_GAIN        = 2,   ///< idx2 drift update 0x49B1B2 times 3 plus 1 clamp 4 idx3 boost start 0x496D2B target speed duration
        STAT_MINI_TURBO_TARGET    = 3,
        STAT_BOOST_LEAN_LIFT      = 4,   ///< idx4 lean update 0x49B4C0 nose lift on boost visual only idx5 tick 0x49CB45 clamp 1 to 2
        STAT_TURN_FORCE           = 5,
        STAT_WHEEL_SPIN           = 6,   ///< idx6 tick 0x49D8D6 wheel spin torque idx7 tick 0x49D78F front wheel angle
        STAT_WHEEL_STEER_ANGLE    = 7,
        STAT_DRIFT_CHARGE_RATE    = 8,   ///< idx8 drift update 0x49AC11 idx9 tick 0x49D3C9 times 0 6 plus 1 2
        STAT_DRIFT_STEER          = 9,
        STAT_MINI_TURBO_THRESHOLD = 10,  ///< idx10 drift update 0x49AEF5 idx11 drift update 0x49AF47 times 800 ms
        STAT_MINI_TURBO_HOLD      = 11,
        STAT_GRIP                 = 12,  ///< idx12 tick 0x49C9D6 per wheel grip idx13 no read site in the exe
        STAT_NO_READ_13           = 13,
        STAT_CAMERA_DISTANCE      = 14,  ///< idx14 camera update 0x43F529 chase distance 9 idx15 camera update 0x43F50A chase pitch 37
        STAT_CAMERA_PITCH         = 15,
        STAT_CAMERA_HEIGHT        = 16   ///< camera update 0x43F510 look point height 3 5 shipped
    };

    /// loadout array order not the category enum order slot 1 is the only tire extras slot
    enum SlotIndex : size_t {
        SLOT_COVER   = 0,
        SLOT_TIRES   = 1,
        SLOT_BOOSTER = 2,
        SLOT_BUMPER  = 3,
        SLOT_FFENDER = 4,
        SLOT_RFENDER = 5,
        SLOT_WING    = 6
    };

    /// owned part record offset 0x08 category the sub 42F6C0 switch
    enum PartCategory : uint32_t {
        CATEGORY_COVER   = 0,
        CATEGORY_BOOSTER = 1,
        CATEGORY_TIRES   = 2,
        CATEGORY_FFENDER = 3,
        CATEGORY_RFENDER = 4,
        CATEGORY_BUMPER  = 5,
        CATEGORY_WING    = 6
    };

    /// category that a loadout slot must hold zero to six
    static uint32_t categoryForSlot(size_t slot);

    /// 0x00C1 one item or accessory definition one row per packet
    static Packet itemDef(const ItemDefRow& row);

    /// 0x00C2 one kart part definition one row per packet
    static Packet kartPartDef(const KartPartDefRow& row);

    /// whole 0x00C1 table
    static std::vector<Packet> itemDefTable(const std::vector<ItemDefRow>& rows);

    /// whole 0x00C2 table
    static std::vector<Packet> kartPartDefTable(const std::vector<KartPartDefRow>& rows);

    /// grade divided by 50 with C truncation the raw factor the client multiplies by
    static int32_t gradeScale(int32_t grade);

    /// 1 plus grade divided by 50 how many copies of a part stat land in indices 0 to 9
    static int32_t gradeMultiplier(int32_t grade);

    /// clamp a grade into the range the stat buckets stay sane in
    static int32_t clampGrade(int32_t grade);

    /// cosmetic tier 0 basic 1 unique 2 epic 3 legend thresholds 5 20 65
    static int32_t rarityTier(int32_t grade);

    /// mean grade over the resolved slots the chassis texture tier input
    static int32_t chassisTier(const std::array<EquippedPart, 7>& slots);

    /// folds one part into an accumulator exactly as sub 4286E0 does indices 0 to 9 scale others flat
    static void accumulatePart(std::array<float, 17>& acc,
                               const std::array<float, 17>& stats,
                               int32_t grade);

    /// full aggregation for one kart slots must be in loadout order bonus and tire extras stay zero off scheme
    static StatAggregate aggregate(const std::array<float, 17>& kartBase,
                                   bool factoryScheme,
                                   const std::array<EquippedPart, 7>& slots,
                                   const std::string& kartModelName = std::string());

    /// the four garage bars catalog must be every kart stat block sent client seeds min and max from index zero
    static GarageBars garageBars(const std::array<float, 17>& kartBase,
                                 const std::array<float, 17>& bonus,
                                 const std::vector<std::array<float, 17>>& catalog);

    /// per stat physics values with the client clamps applied
    static DerivedPhysics derive(const std::array<float, 17>& total,
                                 bool drifting = false);

    /// every reason this stat block misbehaves in the client empty means clean the grip wire 12 has no clamp
    static std::vector<std::string> statBlockIssues(const std::array<float, 17>& total);

    /// bootstrap block wire 12 grip 1 and the shipped camera tail 9 37 3 5
    static std::array<float, 17> unprovenPlayableStatBlock();

    /// parses C2S 0x00CC needs 0x1C plus one trailing byte
    static bool parsePartUseNotify(const Packet& pkt, PartUseNotify& out);

    /// whole 0x00C1 table from shop definition joined with item def ext
    static std::vector<ItemDefRow> loadItemDefs();

    /// whole 0x00C2 table from shop definition joined with kart part def ext
    static std::vector<KartPartDefRow> loadKartPartDefs();

    /// every kart stat block in 0x00C0 send order the bar normaliser input
    static std::vector<std::array<float, 17>> loadKartStatCatalog();

    /// base stats scheme and model name for one kart definition key
    static bool loadKartBase(uint32_t kartKey,
                             std::array<float, 17>& statsOut,
                             uint32_t& schemeOut,
                             std::string& modelNameOut);

    /// stats tire extras and model name for one car craft part key
    static bool loadPartBonus(uint32_t partKey, EquippedPart& out);

    /// kart instance to its 0x00C0 definition key zero when unknown
    static uint32_t kartKeyForInstance(uint32_t kartInstanceId);

    /// resolve the seven loadout slots of the preset bound to this kart
    static bool loadEquippedSlots(int32_t characterId,
                                  uint32_t kartInstanceId,
                                  std::array<EquippedPart, 7>& out);

    /// end to end aggregation for a character and one owned kart
    static bool aggregateForCharacter(int32_t characterId,
                                      uint32_t kartInstanceId,
                                      StatAggregate& out);

    /// end to end garage bars the same numbers the client will draw
    static bool barsForCharacter(int32_t characterId,
                                 uint32_t kartInstanceId,
                                 GarageBars& out);

    /// changes a grade and persists it there is no upgrade opcode so the caller must follow with upgradeAck
    static UpgradeResult applyUpgrade(int32_t characterId,
                                      uint32_t partInstanceId,
                                      int32_t gradeDelta);

    /// set an absolute grade instead of a delta
    static UpgradeResult setGrade(int32_t characterId,
                                  uint32_t partInstanceId,
                                  int32_t newGrade);

    /// 0x0109 re run of the owned part record so the client sees the grade built through CustomCarPackets
    static Packet upgradeAck(int32_t characterId, uint32_t partInstanceId);

    /// true when a 0x00C1 definition offset 0x0C unlocks the repair verb
    static bool isRepairItemType(uint32_t useType);

    /// 0x00C1 definition offset 0x0C for one item key false when unknown
    static bool loadItemUseType(uint32_t itemKey, uint32_t& useTypeOut);

    /// runs one repair scroll on one owned kart requires item type 4 to 7 and period mode 3
    static RepairResult applyRepair(int32_t characterId,
                                    uint32_t kartInstanceId,
                                    uint32_t itemKey,
                                    int32_t restoreAmount,
                                    int32_t durabilityCap);

    /// burns durability after a race floors at zero no op off mode 3
    static bool consumeDurability(int32_t characterId,
                                  uint32_t kartInstanceId,
                                  int32_t amount);

    /// current durability false when the kart is not on period mode 3
    static bool loadDurability(int32_t characterId,
                               uint32_t kartInstanceId,
                               int32_t& out);
};

}  // namespace knc
