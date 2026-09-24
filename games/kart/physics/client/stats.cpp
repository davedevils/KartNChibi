#include "stats.h"

#include "car_state.h"

#include <cstring>

namespace KnC::Kart::Client {

namespace {

// small cursor over a wire byte buffer every read bounds checked native little endian
struct WireCursor {
    const uint8_t* data = nullptr;
    size_t len = 0;
    size_t pos = 0;
    bool ok = true;

    bool require(size_t n) {
        if (!ok || pos + n > len) {
            ok = false;
            return false;
        }
        return true;
    }

    uint32_t read_u32() {
        if (!require(4)) return 0;
        uint32_t v = 0;
        std::memcpy(&v, data + pos, 4);
        pos += 4;
        return v;
    }

    uint8_t read_u8() {
        if (!require(1)) return 0;
        uint8_t v = data[pos];
        pos += 1;
        return v;
    }

    float read_f32() {
        if (!require(4)) return 0.0f;
        float v = 0.0f;
        std::memcpy(&v, data + pos, 4);
        pos += 4;
        return v;
    }

    void read_bytes(uint8_t* dst, size_t n) {
        if (!require(n)) {
            if (dst) std::memset(dst, 0, n);
            return;
        }
        std::memcpy(dst, data + pos, n);
        pos += n;
    }

    template <size_t N>
    void read_bytes(std::array<uint8_t, N>& dst) {
        read_bytes(dst.data(), N);
    }

    // reads a nul terminated ascii string stops at the buffer end if no nul is found
    std::string read_cstr() {
        std::string out;
        while (ok && pos < len) {
            char c = static_cast<char>(data[pos]);
            ++pos;
            if (c == '\0') break;
            out.push_back(c);
        }
        return out;
    }
};

} // namespace

void world_track_init(TrackTuning& tuning, const TrackRecord& record) {
    // world track init 0x4875C0 lines 0x48762C to 0x48764D copy record 0x30 0x34 0x38
    tuning.tuning_0 = record.tuning_0;
    tuning.tuning_1 = record.tuning_1;
    tuning.tuning_2 = record.tuning_2;
}

bool stat_catalog_store(std::array<KartRecord, KART_CATALOG_CAPACITY>& catalogue,
                         size_t& count, const KartRecord& slot) {
    // stat catalog store 0x44F510 refuses past index 63 then increments the counter
    if (count >= KART_CATALOG_CAPACITY) {
        return false;
    }
    catalogue[count] = slot;
    ++count;
    return true;
}

bool kart_record_decode(const uint8_t* data, size_t len, KartRecord& out) {
    WireCursor cur{data, len, 0, true};

    out = KartRecord{};
    out.visible_flag = cur.read_u32();   // record 0x00 to 0x1C four bytes each except 0x0C one byte
    out.unknown_04 = cur.read_u32();
    out.kart_def_key = cur.read_u32();
    out.flag_0c = cur.read_u8();
    out.unknown_10 = cur.read_u32();
    out.model_scheme = cur.read_u32();
    out.unknown_18 = cur.read_u32();
    out.unknown_1c = cur.read_u32();

    out.model_asset_name = cur.read_cstr(); // record 0x20 0x41 0x62 name buffers 33 33 34 bytes
    out.name_key = cur.read_cstr();
    out.description_key = cur.read_cstr();

    cur.read_bytes(out.skin_key_block); // record 0x84 the 0x20 byte skin block

    for (size_t i = 0; i < KART_STAT_COUNT; ++i) {
        out.stats[i] = cur.read_f32(); // record 0xA4 the 17 base stat floats
    }

    cur.read_bytes(out.tail_pair0); // record 0x130 0x138 tail pairs
    cur.read_bytes(out.tail_pair1);

    uint32_t price_option_count = cur.read_u32();
    out.price_rows.clear();
    for (uint32_t i = 0; i < price_option_count && cur.ok; ++i) {
        KartPriceOption row;
        row.price_table_key = cur.read_u32();
        row.period_mode = cur.read_u32();
        row.period_value = cur.read_u32();
        row.active = cur.read_u32();
        if (out.price_rows.size() < KART_PRICE_OPTION_CAP) {
            out.price_rows.push_back(row); // client keeps the first four extra rows dropped
        }
    }

    return cur.ok;
}

bool track_record_decode(const uint8_t* data, size_t len, TrackRecord& out) {
    WireCursor cur{data, len, 0, true};

    out = TrackRecord{};
    out.field0_unknown = cur.read_u32(); // record 0x00 to 0x0C four fields
    out.track_id = cur.read_u32();
    out.theme_id = cur.read_u32();
    out.folder_name = cur.read_cstr();

    uint32_t bits0 = cur.read_u32();     // record 0x30 0x34 0x38 tuning floats as raw bits
    uint32_t bits1 = cur.read_u32();
    uint32_t bits2 = cur.read_u32();
    std::memcpy(&out.tuning_0, &bits0, 4);
    std::memcpy(&out.tuning_1, &bits1, 4);
    std::memcpy(&out.tuning_2, &bits2, 4);

    out.unknown_3c = cur.read_u32();           // record 0x3C to 0x64 four bytes each
    out.fall_off_timeout_ms = cur.read_u32();
    out.unknown_44 = cur.read_u32();
    out.grade = cur.read_u32();
    out.unknown_4c = cur.read_u32();
    out.lap_count = cur.read_u32();
    out.unknown_54 = cur.read_u32();
    out.unknown_58 = cur.read_u32();
    out.unknown_5c = cur.read_u32();
    out.unknown_60 = cur.read_u32();
    out.unknown_64 = cur.read_u32();

    out.display_name = cur.read_cstr();  // record 0x68

    return cur.ok;
}

void car_apply_kart_loadout(KartStats& stats, const KartRecord& record) {
    // car apply kart loadout 0x490A70 copies the 320 byte slot record 0xA4 lands at car 0x3448
    stats.base = record.stats;
    stats.bonus.fill(0.0f); // car 0xA7940 plus 4 k zeroed before parts are applied
}

void stat_bonus_add_part(KartStats& stats, const std::array<float, KART_STAT_COUNT>& part_bonus,
                          float durability) {
    // stat bonus add part 0x48F710 part 0x78 plus 4k into car 0xA7940 plus 4k durability is part grade
    constexpr size_t DURABILITY_SCALED_COUNT = 10;
    const float scale = 1.0f + durability / 50.0f;
    for (size_t i = 0; i < DURABILITY_SCALED_COUNT; ++i) {
        stats.bonus[i] += part_bonus[i] * scale;
    }
    // wire 10 to 16 add flat with no grade term
    for (size_t i = DURABILITY_SCALED_COUNT; i < KART_STAT_COUNT; ++i) {
        stats.bonus[i] += part_bonus[i];
    }
}

const ItemSlot* item_slot_lookup(const ItemInventory& inventory, int index) {
    // item slot lookup 0x451D30 this plus 4 plus index times 0x10 when index is under this 0x44
    if (index < 0 || index >= inventory.count || index >= kItemSlotCount) return nullptr;
    return &inventory.slots[static_cast<size_t>(index)];
}

const OwnedPet* pet_owned_record_lookup(const OwnedPetList& pets, int index) {
    // pet owned record lookup 0x4504E0 this plus 4 plus index times 0x1C under the count at this 0x704
    if (index < 0 || index >= static_cast<int>(pets.records.size())) return nullptr;
    return &pets.records[static_cast<size_t>(index)];
}

int32_t pet_key_to_kind(uint32_t base_key) {
    // pet key to kind 0x451490 four keys map the rest give minus one
    switch (base_key) {
        case 10: return 0x13;
        case 20: return 0x14;
        case 30: return 0x15;
        case 40: return 0x16;
        default: return -1;
    }
}

int32_t pet_equipped_kind(const OwnedPetList& pets) {
    // car boost start 0x496CD0 walks the records and stops at the first one with record 0x08 equal to 1
    for (int i = 0; i < static_cast<int>(pets.records.size()) && i < static_cast<int>(PET_OWNED_CAP); ++i) {
        const OwnedPet* rec = pet_owned_record_lookup(pets, i);
        if (rec != nullptr && rec->equipped == 1) return pet_key_to_kind(rec->base_key);
    }
    return -1;
}

} // namespace KnC Kart Client
