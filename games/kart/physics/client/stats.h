#pragma once
// kart stats track tuning 0xC0 0xC3 catalogue from INPUT AND STATS

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Kart::Client {

constexpr size_t KART_STAT_COUNT = 17; // car 0x3448 to car 0x3488 the 17 base stat floats

// WIRE index k of the 0xC0 block at 0xA4 base car 0x3448 plus 4 k bonus 0xA7940 plus 4 k
enum class KartStatIndex : size_t {
    BodySetupInput = 0,        // tick 0x49CDDA gain one plus x times 0 01 tick 0x49CA95 cap 320 kmh clamped 1 to 2
    MaxSpeed = 1,
    SteeringGain = 2,          // drift 0x49B1B2 times 3 plus 1 clamp 4 boost 0x496D2B 120 to 144 kmh 400 ms scale
    MiniTurboTargetSpeed = 3,
    BoostLeanLift = 4,         // lean 0x49B4C0 times 30 clamp 0 to 30 on boost tick 0x49CB45 plus one clamp 1 to 2
    TurnForce = 5,
    WheelSpin = 6,             // tick 0x49D8D6 kind 2 wheel spin torque tick 0x49D78F kind 2 front wheel angle
    WheelSteerAngle = 7,
    DriftChargeRate = 8,       // drift 0x49AC11 times half plus 0 3 tick 0x49D3C9 times 0 6 range 1 2 to 1 8
    DriftSteer = 9,
    MiniTurboThreshold = 10,   // drift 0x49AEF5 1 minus x times 0 8 gauge 10 drift 0x49AF47 same hold 800 ms
    MiniTurboHoldTime = 11,
    Grip = 12,                 // tick 0x49C9D6 times grip plus extra times 0 2 stat13 no read site in exe
    Stat13NoReader = 13,
    CameraDistance = 14,       // camera 0x43F529 chase distance 9 shipped camera 0x43F50A chase pitch degrees 37 shipped
    CameraPitch = 15,
    CameraHeight = 16          // camera 0x43F510 look point height 3 5 shipped
};

// the 17 base stats and the 17 part bonus floats car apply kart loadout 0x490A70 wire order
struct KartStats {
    std::array<float, KART_STAT_COUNT> base{};  // base copied from record 0xA4 plus 4k bonus 0xA7940 plus 4k summed by stat bonus add part
    std::array<float, KART_STAT_COUNT> bonus{};
};

// base plus bonus of one wire stat the pair every tick read site loads
inline float stat_total(const KartStats& stats, KartStatIndex k) {
    return stats.base[static_cast<size_t>(k)] + stats.bonus[static_cast<size_t>(k)];
}

// one price option row 16 bytes on the wire cap four kept per record
struct KartPriceOption {
    uint32_t price_table_key = 0; // price table key must exist in shop price table period mode value and active meaning not resolved
    uint32_t period_mode = 0;
    uint32_t period_value = 0;
    uint32_t active = 1;
};

constexpr size_t KART_PRICE_OPTION_CAP = 4; // client keeps the first four rows extra rows dropped

// the 320 byte 0xC0 catalogue slot as stat catalog store 0x44F510 copies it
struct KartRecord {
    uint32_t visible_flag = 0;       // record 0x00 zero hides row 0x04 no reader
    uint32_t unknown_04 = 0;
    uint32_t kart_def_key = 0;       // record 0x08 lookup key 0x0C one byte only
    uint8_t  flag_0c = 0;
    uint32_t unknown_10 = 0;         // record 0x10 no reader 0x14 zero or one gates part bonus and FactoryCar
    uint32_t model_scheme = 0;
    uint32_t unknown_18 = 0;         // record 0x18 0x1C no reader found
    uint32_t unknown_1c = 0;
    std::string model_asset_name;    // record 0x20 buffer 33 chassis asset path 0x41 buffer 33 meaning not proven
    std::string name_key;
    std::string description_key;     // record 0x62 buffer 34 meaning not proven 0x84 eight skin keys 6 and 7 land at car 0x3440 0x3444
    std::array<uint8_t, 0x20> skin_key_block{};
    std::array<float, KART_STAT_COUNT> stats{};       // record 0xA4 17 wire stat floats car 0x3448 0x130 meaning not proven
    std::array<uint8_t, 8> tail_pair0{};
    std::array<uint8_t, 8> tail_pair1{};              // record 0x138 meaning not proven wire order read before record keeps them
    std::vector<KartPriceOption> price_rows;
};

constexpr size_t KART_CATALOG_CAPACITY = 64; // stat catalog store 0x44F510 refuses past index 63

// stat catalog store 0x44F510 copies one parsed slot into the catalogue and bumps the count
bool stat_catalog_store(std::array<KartRecord, KART_CATALOG_CAPACITY>& catalogue,
                         size_t& count, const KartRecord& slot);

// the 0xC3 track record as world track init 0x4875C0 resolves it
struct TrackRecord {
    uint32_t field0_unknown = 0;      // record 0x00 no reader 0x04 lookup key matches S2C 0x14 track id
    uint32_t track_id = 0;
    uint32_t theme_id = 0;            // record 0x08 joins 0xC4 theme row 0x0C second folder under World for track path
    std::string folder_name;
    float tuning_0 = 0.0f;            // record 0x30 fills global 0x5EB6F0 0x34 fills global 0x5EB6F4
    float tuning_1 = 0.0f;
    float tuning_2 = 0.0f;            // record 0x38 fills global 0x5EB6F8 0x3C no reader found
    uint32_t unknown_3c = 0;
    uint32_t fall_off_timeout_ms = 500; // record 0x40 respawn window default 500 0x44 no reader found
    uint32_t unknown_44 = 0;
    uint32_t grade = 0;               // record 0x48 license grade required 0x4C no reader found
    uint32_t unknown_4c = 0;
    uint32_t lap_count = 3;           // record 0x50 lap count clamped 1 to 9 0x54 no reader found
    uint32_t unknown_54 = 0;
    uint32_t unknown_58 = 0;          // record 0x58 0x5C no reader found
    uint32_t unknown_5c = 0;
    uint32_t unknown_60 = 0;          // record 0x60 0x64 no reader found
    uint32_t unknown_64 = 0;
    std::string display_name;         // record 0x68 used for the info ui label
};

// the three per track globals world track init 0x4875C0 fills from a track record
struct TrackTuning {
    float tuning_0 = 0.0f; // 0x5EB6F0 engine or steering scale guess 0x5EB6F4 engine force durability penalty scale guess
    float tuning_1 = 0.0f;
    float tuning_2 = 0.0f; // 0x5EB6F8 turn force baseline additive term guess
};

// world track init 0x4875C0 copies record 0x30 0x34 0x38 into the tuning globals
void world_track_init(TrackTuning& tuning, const TrackRecord& record);

// decodes one 0xC0 kart record from a byte buffer in the wire order sub 47F4F0 reads
bool kart_record_decode(const uint8_t* data, size_t len, KartRecord& out);

// decodes one 0xC3 track record from a byte buffer in the wire order sub 47F990 reads
bool track_record_decode(const uint8_t* data, size_t len, TrackRecord& out);

// car apply kart loadout 0x490A70 copies the 17 wire stats to car 0x3448 and zeros car 0xA7940
void car_apply_kart_loadout(KartStats& stats, const KartRecord& record);

// item slot lookup 0x451D30 record index into the 16 byte slots null outside 0 to count minus 1
struct ItemInventory;
struct ItemSlot;
const ItemSlot* item_slot_lookup(const ItemInventory& inventory, int index);

// stat bonus add part 0x48F710 part float k adds to bonus wire k the first 10 scale by the grade
void stat_bonus_add_part(KartStats& stats, const std::array<float, KART_STAT_COUNT>& part_bonus,
                          float durability);

// one owned pet record 0x1C bytes the S2C pet list fills the container 0x1A69708 count 0x1A69E0C
struct OwnedPet {
    uint32_t instance_id = 0; // record 0x00 0x04 the pet base key maps via pet key to kind
    uint32_t base_key = 0;
    uint32_t equipped = 0;    // record 0x08 one means equipped
};

constexpr size_t PET_OWNED_CAP = 64; // container 0x1A69708 hard cap 64 records

// the owned pet container 0x1A69708 the in race pet effects read only this list
struct OwnedPetList {
    std::vector<OwnedPet> records;
};

// pet owned record lookup 0x4504E0 null outside 0 to count minus 1
const OwnedPet* pet_owned_record_lookup(const OwnedPetList& pets, int index);

// pet key to kind 0x451490 keys 10 20 30 40 give 0x13 0x14 0x15 0x16 else minus one
int32_t pet_key_to_kind(uint32_t base_key);

// the kind of the first equipped pet or minus one car boost start and the drift update loop this way
int32_t pet_equipped_kind(const OwnedPetList& pets);

} // namespace KnC Kart Client
