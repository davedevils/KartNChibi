/// the Room Craft and Car Craft shop rows counted in the captured login burst and on our builders

#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "net/Packet.h"
#include "packets/gen/CustomCarPackets.h"
#include "packets/gen/RoomCraftPackets.h"

using namespace knc;

namespace {

struct BurstFrame {
    uint16_t opcode = 0;
    std::vector<uint8_t> payload;
};

uint32_t u32At(const std::vector<uint8_t>& p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p.data() + at, 4);
    return v;
}

// the recording is KNCB then the frame count then a spare dword then op size payload records
std::vector<BurstFrame> loadBurst() {
    std::vector<BurstFrame> out;
    const std::string path = std::string(KNC_REPO_ROOT) + "/server/data/reference_login_burst.bin";
    std::ifstream file(path, std::ios::binary);
    if (!file) return out;
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (raw.size() < 12 || std::memcmp(raw.data(), "KNCB", 4) != 0) return out;
    size_t off = 12;
    while (off + 4 <= raw.size()) {
        BurstFrame f;
        f.opcode = static_cast<uint16_t>(raw[off] | (raw[off + 1] << 8));
        const uint16_t size = static_cast<uint16_t>(raw[off + 2] | (raw[off + 3] << 8));
        if (off + 4 + size > raw.size()) break;
        f.payload.assign(raw.begin() + off + 4, raw.begin() + off + 4 + size);
        out.push_back(std::move(f));
        off += 4 + size;
    }
    return out;
}

// walks past the three ascii strings a definition carries
size_t skipStrings(const std::vector<uint8_t>& p, size_t at, int count) {
    for (int i = 0; i < count; ++i) {
        while (at < p.size() && p[at] != 0) ++at;
        ++at;
    }
    return at;
}

std::vector<uint32_t> carPartPriceKeys(const std::vector<uint8_t>& p) {
    std::vector<uint32_t> keys;
    size_t at = skipStrings(p, 0x14, 3) + 0x44 + 0x10 + 0x0C;
    if (at + 4 > p.size()) return keys;
    const uint32_t rows = u32At(p, at);
    at += 4;
    for (uint32_t i = 0; i < rows && at + 16 <= p.size(); ++i, at += 16) keys.push_back(u32At(p, at));
    return keys;
}

std::vector<uint32_t> roomObjectPriceKeys(const std::vector<uint8_t>& p) {
    std::vector<uint32_t> keys;
    size_t at = skipStrings(p, 0x18, 3);
    if (at + 4 > p.size()) return keys;
    const uint32_t rows = u32At(p, at);
    at += 4;
    for (uint32_t i = 0; i < rows && at + 16 <= p.size(); ++i, at += 16) keys.push_back(u32At(p, at));
    return keys;
}

} // namespace

// what the released server put on the wire

// the counts our server has to match one 0x0108 per part one 0x010C per object no second burst
TEST(CraftShopBurst, CategoryRowCounts) {
    const std::vector<BurstFrame> frames = loadBurst();
    ASSERT_FALSE(frames.empty()) << "reference_login_burst.bin did not parse";

    size_t carParts = 0, roomObjects = 0, priceRows = 0;
    for (const BurstFrame& f : frames) {
        if (f.opcode == 0x0108) ++carParts;
        if (f.opcode == 0x010C) ++roomObjects;
        if (f.opcode == 0x00C6) ++priceRows;
    }
    EXPECT_EQ(carParts, 35u);
    EXPECT_EQ(roomObjects, 71u);
    EXPECT_EQ(priceRows, 23u);
}

// 0 Sky 1 Floor 2 BgObj 3 Object 4 Effect the room craft strip and shop sub tabs read this
TEST(CraftShopBurst, RoomObjectCategories) {
    const std::vector<BurstFrame> frames = loadBurst();
    ASSERT_FALSE(frames.empty());

    std::map<uint32_t, size_t> perCategory;
    for (const BurstFrame& f : frames) {
        if (f.opcode != 0x010C) continue;
        ++perCategory[u32At(f.payload, 0x0C)];
    }
    EXPECT_EQ(perCategory[0], 11u);
    EXPECT_EQ(perCategory[1], 13u);
    EXPECT_EQ(perCategory[2], 4u);
    EXPECT_EQ(perCategory[3], 32u);
    EXPECT_EQ(perCategory[4], 11u);
}

// shop roomcraft grid draw 0x41A160 drops a tile whose first price option has no 0x00C6 row
TEST(CraftShopBurst, EveryCraftPriceKeyResolves) {
    const std::vector<BurstFrame> frames = loadBurst();
    ASSERT_FALSE(frames.empty());

    std::set<uint32_t> table;
    for (const BurstFrame& f : frames) {
        if (f.opcode == 0x00C6) table.insert(u32At(f.payload, 0));
    }
    // the car craft tier of the burst one day seven days permanent
    EXPECT_EQ(table.count(1007u), 1u);
    EXPECT_EQ(table.count(2007u), 1u);
    EXPECT_EQ(table.count(3007u), 1u);
    // the room craft tier the same three steps one number lower
    EXPECT_EQ(table.count(1006u), 1u);
    EXPECT_EQ(table.count(2006u), 1u);
    EXPECT_EQ(table.count(3006u), 1u);

    std::set<uint32_t> carKeys, roomKeys;
    for (const BurstFrame& f : frames) {
        if (f.opcode == 0x0108) {
            for (uint32_t k : carPartPriceKeys(f.payload)) {
                carKeys.insert(k);
                EXPECT_EQ(table.count(k), 1u) << "car craft price key " << k << " has no 0x00C6 row";
            }
        }
        if (f.opcode == 0x010C) {
            for (uint32_t k : roomObjectPriceKeys(f.payload)) {
                roomKeys.insert(k);
                // a free row ships one inert zero slot so the tile draw never derefs
                if (k != 0) EXPECT_EQ(table.count(k), 1u) << "room craft price key " << k << " has no 0x00C6 row";
            }
        }
    }
    EXPECT_EQ(carKeys, (std::set<uint32_t>{1007u, 2007u, 3007u}));
    EXPECT_EQ(roomKeys, (std::set<uint32_t>{0u, 1006u, 2006u, 3006u}));
}

// our builders on the same rows

// migration 061 points every part at the burst triple so the shop tile finds a price
TEST(CraftShopWire, PartDefCarriesTheBurstPriceTier) {
    CarPartDef def;
    def.enabled = 1;
    def.partKey = 1000;
    def.category = 0;
    def.modelDirName = "Firedragon";
    def.displayNameKey = "COVER_1000_TITLE";
    def.descriptionKey = "COVER_1000_INFO";
    def.priceRows.push_back({2007, 1, 1, 0});
    def.priceRows.push_back({3007, 1, 7, 0});
    def.priceRows.push_back({1007, 0, 0, 0});

    Packet pkt = CustomCarPackets::partDef(def);
    const std::vector<uint8_t>& p = pkt.payload();
    EXPECT_EQ(u32At(p, 0x08), 1000u);
    const std::vector<uint32_t> keys = carPartPriceKeys(p);
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], 2007u);
    EXPECT_EQ(keys[1], 3007u);
    EXPECT_EQ(keys[2], 1007u);
}

// the six room objects migration 061 adds ship the same three slots as every selling object
TEST(CraftShopWire, ObjectDefinitionCarriesThreePriceSlots) {
    RoomObjectDef def;
    def.enabled = 1;
    def.objectKey = 2013;
    def.category = 1;
    def.maxPlaceable = 1;
    def.assetFolder = "floor05";
    def.nameLocKey = "floor05";
    def.descLocKey = "floor05";
    def.prices.push_back({2006, 1, 1, 0});
    def.prices.push_back({3006, 1, 7, 0});
    def.prices.push_back({1006, 0, 0, 0});

    Packet pkt = RoomCraftPackets::objectDefinition(def);
    const std::vector<uint8_t>& p = pkt.payload();
    EXPECT_EQ(u32At(p, 0x00), 1u);
    EXPECT_EQ(u32At(p, 0x08), 2013u);
    EXPECT_EQ(u32At(p, 0x0C), 1u);
    const std::vector<uint32_t> keys = roomObjectPriceKeys(p);
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], 2006u);
    EXPECT_EQ(keys[1], 3006u);
    EXPECT_EQ(keys[2], 1006u);
}
