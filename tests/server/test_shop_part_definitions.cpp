/// what the released server sold on category 3 and the migration that lets our shop sell it too

#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace std;

namespace {

struct Frame {
    uint16_t opcode = 0;
    std::vector<uint8_t> payload;
};

uint32_t u32At(const std::vector<uint8_t>& p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p.data() + at, 4);
    return v;
}

// KNCB then the frame count then op size payload records the first record is the empty 0x00BE
std::vector<Frame> loadBurst() {
    std::vector<Frame> out;
    const std::string path = std::string(KNC_REPO_ROOT) + "/server/data/reference_login_burst.bin";
    std::ifstream file(path, std::ios::binary);
    if (!file) return out;
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (raw.size() < 8 || std::memcmp(raw.data(), "KNCB", 4) != 0) return out;
    size_t off = 8;
    while (off + 4 <= raw.size()) {
        Frame f;
        f.opcode = static_cast<uint16_t>(raw[off] | (raw[off + 1] << 8));
        const uint16_t size = static_cast<uint16_t>(raw[off + 2] | (raw[off + 3] << 8));
        if (off + 4 + size > raw.size()) break;
        f.payload.assign(raw.begin() + off + 4, raw.begin() + off + 4 + size);
        out.push_back(std::move(f));
        off += 4 + size;
    }
    return out;
}

size_t skipCStr(const std::vector<uint8_t>& p, size_t at) {
    while (at < p.size() && p[at] != 0) ++at;
    return at + 1;
}

/// one 0x00C2 kart part definition the wire order is the record order for this one
struct PartRow {
    uint32_t visible = 0;
    uint32_t key = 0;
    uint32_t level = 0;
    uint32_t slot = 0;
    std::vector<uint32_t> priceKeys;
    std::vector<uint32_t> periodType;
    std::vector<uint32_t> periodValue;
};

bool decodePart(const std::vector<uint8_t>& p, PartRow& out) {
    if (p.size() < 16) return false;
    out.visible = u32At(p, 0x00);
    out.key     = u32At(p, 0x08);
    out.level   = u32At(p, 0x0C);
    size_t at = skipCStr(p, 0x10);            // model name
    if (at + 12 > p.size()) return false;
    out.slot = u32At(p, at + 4);              // restrict target then equip slot then restrict key
    at += 12;
    at = skipCStr(p, at);                     // display name key then description key
    at = skipCStr(p, at);
    at += 16;                                 // two ability pairs
    if (at + 4 > p.size()) return false;
    const uint32_t rows = u32At(p, at);
    at += 4;
    for (uint32_t i = 0; i < rows && at + 16 <= p.size(); ++i, at += 16) {
        out.priceKeys.push_back(u32At(p, at));
        out.periodType.push_back(u32At(p, at + 4));
        out.periodValue.push_back(u32At(p, at + 8));
    }
    return true;
}

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::string();
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

} // namespace

// 473 part rows and every sold one carries the one day seven day permanent triple
TEST(ShopPartBurst, PartRowCountsAndVisibility) {
    const std::vector<Frame> frames = loadBurst();
    ASSERT_FALSE(frames.empty()) << "reference_login_burst.bin did not parse";

    size_t rows = 0, sold = 0, hidden = 0;
    for (const Frame& f : frames) {
        if (f.opcode != 0x00C2) continue;
        ++rows;
        PartRow r;
        ASSERT_TRUE(decodePart(f.payload, r)) << "0x00C2 row " << rows << " did not decode";
        if (r.visible != 0) ++sold; else ++hidden;
        // every part row of the recording is free of a level gate
        EXPECT_EQ(r.level, 0u) << "part " << r.key << " carries a level gate";
    }
    EXPECT_EQ(rows, 473u);
    EXPECT_EQ(sold, 367u);
    EXPECT_EQ(hidden, 106u);
}

// slot 2 to 6 are the driver accessories and they are the rows our shop could not sell
TEST(ShopPartBurst, DriverAccessoriesAreTheBulkOfTheSoldRows) {
    const std::vector<Frame> frames = loadBurst();
    ASSERT_FALSE(frames.empty());

    std::map<uint32_t, size_t> soldPerSlot;
    for (const Frame& f : frames) {
        if (f.opcode != 0x00C2) continue;
        PartRow r;
        if (!decodePart(f.payload, r) || r.visible == 0) continue;
        ++soldPerSlot[r.slot];
    }
    // 0 kart colour 1 name plate 8 kart item the rest are worn by the driver
    EXPECT_EQ(soldPerSlot[0], 8u);
    EXPECT_EQ(soldPerSlot[1], 13u);
    EXPECT_EQ(soldPerSlot[2], 109u);
    EXPECT_EQ(soldPerSlot[4], 152u);
    EXPECT_EQ(soldPerSlot[5], 30u);
    EXPECT_EQ(soldPerSlot[6], 35u);
    EXPECT_EQ(soldPerSlot[8], 20u);
    size_t driverWorn = 0;
    for (const auto& kv : soldPerSlot) {
        if (kv.first >= 2 && kv.first <= 6) driverWorn += kv.second;
    }
    EXPECT_EQ(driverWorn, 326u);
}

// the tier is by slot family and the order is one day then seven days then permanent
TEST(ShopPartBurst, EverySoldRowCarriesOneTierTriple) {
    const std::vector<Frame> frames = loadBurst();
    ASSERT_FALSE(frames.empty());

    std::set<uint32_t> table;
    for (const Frame& f : frames) {
        if (f.opcode == 0x00C6) table.insert(u32At(f.payload, 0));
    }

    for (const Frame& f : frames) {
        if (f.opcode != 0x00C2) continue;
        PartRow r;
        if (!decodePart(f.payload, r)) continue;
        if (r.visible == 0) {
            // an unsold row still ships one inert option so the tile draw never derefs
            ASSERT_EQ(r.priceKeys.size(), 1u) << "hidden part " << r.key;
            EXPECT_EQ(r.priceKeys[0], 0u);
            continue;
        }
        ASSERT_EQ(r.priceKeys.size(), 3u) << "sold part " << r.key;
        const bool kartSide = (r.slot == 0 || r.slot == 1 || r.slot == 8);
        const uint32_t day  = kartSide ? 2004u : 2003u;
        const uint32_t week = kartSide ? 3004u : 3003u;
        const uint32_t perm = kartSide ? 1004u : 1003u;
        EXPECT_EQ(r.priceKeys[0], day)  << "part " << r.key;
        EXPECT_EQ(r.priceKeys[1], week) << "part " << r.key;
        EXPECT_EQ(r.priceKeys[2], perm) << "part " << r.key;
        EXPECT_EQ(r.periodType[0], 1u);
        EXPECT_EQ(r.periodValue[0], 1u);
        EXPECT_EQ(r.periodType[1], 1u);
        EXPECT_EQ(r.periodValue[1], 7u);
        EXPECT_EQ(r.periodType[2], 0u);
        for (uint32_t k : r.priceKeys) {
            EXPECT_EQ(table.count(k), 1u) << "part price key " << k << " has no 0x00C6 row";
        }
    }
}

// the pets of the recording sell on their own tier and the item tab has only the two gacha coins
TEST(ShopPartBurst, PetAndItemRowsCarryTheirOwnTier) {
    const std::vector<Frame> frames = loadBurst();
    ASSERT_FALSE(frames.empty());

    size_t pets = 0, items = 0;
    for (const Frame& f : frames) {
        if (f.opcode == 0x0103) {
            ++pets;
            size_t at = skipCStr(f.payload, 0x10);
            at = skipCStr(f.payload, at);
            at = skipCStr(f.payload, at);
            ASSERT_LE(at + 4, f.payload.size());
            const uint32_t rows = u32At(f.payload, at);
            at += 4;
            ASSERT_EQ(rows, 3u);
            EXPECT_EQ(u32At(f.payload, at + 0x00), 2005u);
            EXPECT_EQ(u32At(f.payload, at + 0x10), 3005u);
            EXPECT_EQ(u32At(f.payload, at + 0x20), 1005u);
        }
        if (f.opcode == 0x00C1) ++items;
    }
    EXPECT_EQ(pets, 4u);
    EXPECT_EQ(items, 2u);
}

// ShopPackets validatePurchase reads shop definition so category 3 needs a row per accessory
TEST(ShopPartSeed, Migration062SeedsCategoryThreeFromTheSkinTable) {
    const std::string sql =
        readFile(std::string(KNC_REPO_ROOT) + "/server/scripts/062_accessory_shop_definitions.sql");
    ASSERT_FALSE(sql.empty()) << "migration 062 is missing";

    EXPECT_NE(sql.find("INSERT IGNORE INTO shop_definition"), std::string::npos);
    EXPECT_NE(sql.find("FROM def_kart_skin"), std::string::npos);
    EXPECT_NE(sql.find("FROM def_item_wire"), std::string::npos);
    // the burst gives every part row level zero so the seed must not invent a gate
    EXPECT_NE(sql.find("UPDATE shop_definition SET level_req = 0 WHERE category = 3"),
              std::string::npos);
    // an applied migration is never edited
    EXPECT_EQ(sql.find("DROP TABLE"), std::string::npos);
}
