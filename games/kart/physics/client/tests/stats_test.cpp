// Test stats module decode 0xC0 slot 0xC3 record loadout parts 17 stats bonuses

#include "../stats.h"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace KnC::Kart::Client;

namespace {

void put_u32(std::vector<uint8_t>& buf, uint32_t v) {
    uint8_t bytes[4];
    std::memcpy(bytes, &v, 4);
    for (uint8_t b : bytes) buf.push_back(b);
}

void put_u8(std::vector<uint8_t>& buf, uint8_t v) {
    buf.push_back(v);
}

void put_f32(std::vector<uint8_t>& buf, float v) {
    uint8_t bytes[4];
    std::memcpy(bytes, &v, 4);
    for (uint8_t b : bytes) buf.push_back(b);
}

void put_cstr(std::vector<uint8_t>& buf, const std::string& s) {
    for (char c : s) buf.push_back(static_cast<uint8_t>(c));
    buf.push_back(0);
}

void put_bytes(std::vector<uint8_t>& buf, size_t count, uint8_t fill) {
    for (size_t i = 0; i < count; ++i) buf.push_back(fill);
}

// builds one 0xC0 kart record in the wire order kartDef in PartStatPackets writes
std::vector<uint8_t> build_kart_record_wire() {
    std::vector<uint8_t> buf;
    put_u32(buf, 1);          // visible flag unknown 04
    put_u32(buf, 0);
    put_u32(buf, 1001);       // kart def key flag 0c
    put_u8(buf, 1);
    put_u32(buf, 0);          // unknown 10 model scheme
    put_u32(buf, 1);
    put_u32(buf, 0);          // unknown 18 unknown 1c
    put_u32(buf, 0);
    put_cstr(buf, "kart_circler");
    put_cstr(buf, "name_key_circler");
    put_cstr(buf, "desc_key_circler");
    put_bytes(buf, 0x20, 0);  // skin key block

    const float stats[KART_STAT_COUNT] = {
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f,
        11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f
    };
    for (size_t i = 0; i < KART_STAT_COUNT; ++i) put_f32(buf, stats[i]);

    put_bytes(buf, 8, 0); // tail pair0 tail pair1
    put_bytes(buf, 8, 0);

    put_u32(buf, 1); // price option count price table key
    put_u32(buf, 500);
    put_u32(buf, 0);   // period mode period value
    put_u32(buf, 0);
    put_u32(buf, 1);   // active

    return buf;
}

// builds one 0xC3 track record in the wire order trackCatalogEntry in SpawnPackets writes
std::vector<uint8_t> build_track_record_wire() {
    std::vector<uint8_t> buf;
    put_u32(buf, 0);          // field0 unknown track id
    put_u32(buf, 7);
    put_u32(buf, 2);          // theme id
    put_cstr(buf, "Cookie_01");

    put_f32(buf, 0.4f);  // record 0x30 tuning 0 record 0x34 tuning 1
    put_f32(buf, 0.6f);
    put_f32(buf, 90.0f); // record 0x38 tuning 2 record 0x3c unknown
    put_u32(buf, 0);
    put_u32(buf, 500);   // record 0x40 fall off timeout ms record 0x44 unknown
    put_u32(buf, 0);
    put_u32(buf, 3);     // record 0x48 grade record 0x4c unknown
    put_u32(buf, 0);
    put_u32(buf, 3);     // record 0x50 lap count record 0x54 unknown
    put_u32(buf, 0);
    put_u32(buf, 0);     // record 0x58 unknown record 0x5c unknown
    put_u32(buf, 0);
    put_u32(buf, 0);     // record 0x60 unknown record 0x64 unknown
    put_u32(buf, 0);

    put_cstr(buf, "Cookie_01_INFO");
    return buf;
}

} // namespace

int main() {
    int failures = 0;

    const std::vector<uint8_t> kart_wire = build_kart_record_wire();
    KartRecord record;
    if (!kart_record_decode(kart_wire.data(), kart_wire.size(), record)) {
        std::printf("kart_record_decode failed\n");
        ++failures;
    }

    const std::vector<uint8_t> track_wire = build_track_record_wire();
    TrackRecord track;
    if (!track_record_decode(track_wire.data(), track_wire.size(), track)) {
        std::printf("track_record_decode failed\n");
        ++failures;
    }

    std::printf("kart_def_key %u model %s\n", record.kart_def_key, record.model_asset_name.c_str());
    std::printf("track_id %u folder %s display %s\n", track.track_id, track.folder_name.c_str(),
                track.display_name.c_str());

    KartStats stats;
    car_apply_kart_loadout(stats, record);

    const std::array<float, KART_STAT_COUNT> part_a = {
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
    };
    const std::array<float, KART_STAT_COUNT> part_b = {
        2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f,
        2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f
    };
    stat_bonus_add_part(stats, part_a, 25.0f);
    stat_bonus_add_part(stats, part_b, 0.0f);

    std::printf("base stats:\n");
    for (size_t i = 0; i < KART_STAT_COUNT; ++i) {
        std::printf("  %zu base %f bonus %f\n", i, static_cast<double>(stats.base[i]),
                    static_cast<double>(stats.bonus[i]));
    }

    TrackTuning tuning;
    world_track_init(tuning, track);
    std::printf("tuning %f %f %f\n", static_cast<double>(tuning.tuning_0),
                static_cast<double>(tuning.tuning_1), static_cast<double>(tuning.tuning_2));

    if (tuning.tuning_0 != 0.4f || tuning.tuning_1 != 0.6f || tuning.tuning_2 != 90.0f) {
        std::printf("tuning globals did not take 0.4 0.6 90\n");
        ++failures;
    }

    std::array<KartRecord, KART_CATALOG_CAPACITY> catalogue;
    size_t count = 0;
    if (!stat_catalog_store(catalogue, count, record) || count != 1) {
        std::printf("stat_catalog_store failed\n");
        ++failures;
    }

    if (failures == 0) {
        std::printf("stats_test PASS\n");
        return 0;
    }
    std::printf("stats_test FAIL %d\n", failures);
    return 1;
}
