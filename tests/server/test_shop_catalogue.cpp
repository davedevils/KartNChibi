/// the shop audit against the captured burst the price rules the coin grant and migration 068

#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "net/Packet.h"
#include "packets/gen/GachaPetPackets.h"
#include "packets/gen/InventoryPackets.h"
#include "packets/gen/ShopPackets.h"
#include "util/ShopRules.h"

using namespace knc;

namespace {

struct Frame {
    uint16_t opcode = 0;
    std::vector<uint8_t> payload;
};

uint32_t u32At(const std::vector<uint8_t>& p, size_t at) {
    uint32_t v = 0;
    if (at + 4 <= p.size()) std::memcpy(&v, p.data() + at, 4);
    return v;
}

int32_t i32At(const std::vector<uint8_t>& p, size_t at) {
    return static_cast<int32_t>(u32At(p, at));
}

// KNCB then the frame count then op size payload records
std::vector<Frame> loadBurst() {
    std::vector<Frame> out;
    std::ifstream file(std::string(KNC_REPO_ROOT) + "/server/data/reference_login_burst.bin",
                       std::ios::binary);
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

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::string();
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

size_t skipCStr(const std::vector<uint8_t>& p, size_t at) {
    while (at < p.size() && p[at] != 0) ++at;
    return at + 1;
}

struct Option {
    uint32_t key = 0;
    uint32_t unit = 0;
    uint32_t amount = 0;
};

std::vector<Option> optionsAt(const std::vector<uint8_t>& p, size_t at) {
    std::vector<Option> out;
    const uint32_t n = u32At(p, at);
    at += 4;
    for (uint32_t i = 0; i < n && at + 16 <= p.size(); ++i, at += 16) {
        out.push_back({u32At(p, at), u32At(p, at + 4), u32At(p, at + 8)});
    }
    return out;
}

// the sold rows of one catalogue opcode with the equip slot for 0xC2 and their options
struct SoldRow {
    uint32_t key = 0;
    uint32_t slot = 0;
    std::vector<Option> options;
};

std::vector<SoldRow> soldRows(const std::vector<Frame>& frames, uint16_t op) {
    std::vector<SoldRow> out;
    for (const Frame& f : frames) {
        if (f.opcode != op) continue;
        const std::vector<uint8_t>& p = f.payload;
        SoldRow r;
        size_t at = 0;
        if (op == 0x00BF) {
            if (u32At(p, 0) == 0) continue;
            r.key = u32At(p, 0x0C);
            at = skipCStr(p, 0x14) + 0x14;
            at = skipCStr(p, at);
            at = skipCStr(p, at);
        } else if (op == 0x00C0) {
            if (u32At(p, 0) == 0) continue;
            r.key = u32At(p, 0x08);
            at = skipCStr(p, 0x1D);
            at = skipCStr(p, at);
            at = skipCStr(p, at) + 0x20 + 0x44 + 0x10;
        } else if (op == 0x00C2) {
            if (u32At(p, 0) == 0) continue;
            r.key = u32At(p, 0x08);
            at = skipCStr(p, 0x10);
            r.slot = u32At(p, at + 4);
            at = skipCStr(p, at + 12);
            at = skipCStr(p, at) + 0x10;
        } else if (op == 0x0103) {
            if (u32At(p, 0) == 0) continue;
            r.key = u32At(p, 0x08);
            at = skipCStr(p, 0x10);
            at = skipCStr(p, at);
            at = skipCStr(p, at);
        } else if (op == 0x010C) {
            r.key = u32At(p, 0x08);
            at = skipCStr(p, 0x18);
            at = skipCStr(p, at);
            at = skipCStr(p, at);
            // the burst ships its room tier on hidden rows too so the tier check takes every row with options
            if (u32At(p, at) == 0 || u32At(p, at + 4) == 0) continue;
        } else if (op == 0x0108) {
            r.key = u32At(p, 0x08);
            at = skipCStr(p, 0x14);
            at = skipCStr(p, at);
            at = skipCStr(p, at) + 0x44 + 0x10 + 0x0C;
        } else {
            continue;
        }
        r.options = optionsAt(p, at);
        out.push_back(r);
    }
    return out;
}

// price key to unit amount base and sale the tuple the client reads from one 0x00C6 row
using PriceTuple = std::tuple<uint32_t, uint32_t, int32_t, int32_t>;

std::map<uint32_t, PriceTuple> burstPrices(const std::vector<Frame>& frames) {
    std::map<uint32_t, PriceTuple> out;
    for (const Frame& f : frames) {
        if (f.opcode != 0x00C6 || f.payload.size() != 28) continue;
        out[u32At(f.payload, 0)] = PriceTuple(u32At(f.payload, 0x0C), u32At(f.payload, 0x10),
                                               i32At(f.payload, 0x14), i32At(f.payload, 0x18));
    }
    return out;
}

// the 23 rows of the captured burst key unit amount base sale
const std::map<uint32_t, PriceTuple>& expectedBurstPrices() {
    static const std::map<uint32_t, PriceTuple> rows = {
        {1001, PriceTuple(0, 0, 0, 3000)}, {1002, PriceTuple(0, 0, 0, 3000)},
        {1003, PriceTuple(0, 0, 0, 3000)}, {1004, PriceTuple(0, 0, 0, 3000)},
        {1005, PriceTuple(0, 0, 0, 3000)}, {1006, PriceTuple(0, 0, 0, 3000)},
        {1007, PriceTuple(0, 0, 0, 3000)},
        {2001, PriceTuple(1, 1, 1500, 0)}, {2002, PriceTuple(1, 1, 1800, 0)},
        {2003, PriceTuple(1, 1, 500, 0)},  {2004, PriceTuple(1, 1, 400, 0)},
        {2005, PriceTuple(1, 1, 2500, 0)}, {2006, PriceTuple(1, 1, 300, 0)},
        {2007, PriceTuple(1, 1, 1000, 0)},
        {3001, PriceTuple(1, 7, 7500, 0)}, {3002, PriceTuple(1, 7, 9000, 0)},
        {3003, PriceTuple(1, 7, 2500, 0)}, {3004, PriceTuple(1, 7, 2000, 0)},
        {3005, PriceTuple(1, 7, 12500, 0)}, {3006, PriceTuple(1, 7, 1500, 0)},
        {3007, PriceTuple(1, 7, 5000, 0)},
        {4001, PriceTuple(0, 0, 0, 1000)}, {4002, PriceTuple(0, 0, 2500, 0)},
    };
    return rows;
}

} // namespace

// the whole 0x00C6 table of the real server key by key
TEST(ShopBurstPrices, TheTwentyThreeRows) {
    const std::vector<Frame> frames = loadBurst();
    ASSERT_FALSE(frames.empty()) << "reference_login_burst.bin did not parse";
    EXPECT_EQ(burstPrices(frames), expectedBurstPrices());
}

// 0x45E36B the Astro icon goes beside a sale price so every permanent row and the Gacha Coin cost Astro
TEST(ShopBurstPrices, SaleRowsAreTheAstroRows) {
    const std::vector<Frame> frames = loadBurst();
    ASSERT_FALSE(frames.empty());
    std::set<uint32_t> astro;
    for (const auto& kv : burstPrices(frames)) {
        if (priceIsAstro(std::get<3>(kv.second))) astro.insert(kv.first);
    }
    EXPECT_EQ(astro, (std::set<uint32_t>{1001, 1002, 1003, 1004, 1005, 1006, 1007, 4001}));
    // the Gold Coin is 2500 gold permanent and the Gacha Coin 1000 Astro permanent
    const auto rows = burstPrices(frames);
    EXPECT_EQ(priceCharged(std::get<2>(rows.at(4002)), std::get<3>(rows.at(4002))), 2500);
    EXPECT_FALSE(priceIsAstro(std::get<3>(rows.at(4002))));
    EXPECT_EQ(priceCharged(std::get<2>(rows.at(4001)), std::get<3>(rows.at(4001))), 1000);
    EXPECT_TRUE(priceIsAstro(std::get<3>(rows.at(4001))));
}

// the item tab of the real server sells the two coins one permanent option each
TEST(ShopBurstPrices, TheTwoCoinsAndTheirOneOption) {
    const std::vector<Frame> frames = loadBurst();
    ASSERT_FALSE(frames.empty());
    std::map<uint32_t, std::vector<Option>> items;
    for (const Frame& f : frames) {
        if (f.opcode != 0x00C1) continue;
        size_t at = skipCStr(f.payload, 0x14);
        at = skipCStr(f.payload, at);
        at = skipCStr(f.payload, at);
        items[u32At(f.payload, 0x08)] = optionsAt(f.payload, at);
    }
    ASSERT_EQ(items.size(), 2u);
    ASSERT_EQ(items[kGachaCoinKey].size(), 1u);
    ASSERT_EQ(items[kGoldCoinKey].size(), 1u);
    EXPECT_EQ(items[kGachaCoinKey][0].key, burstCoinPriceKey(kGachaCoinKey));
    EXPECT_EQ(items[kGoldCoinKey][0].key, burstCoinPriceKey(kGoldCoinKey));
    EXPECT_EQ(items[kGoldCoinKey][0].unit, 0u);
}

// every sold row of every category sits on the tier burstPriceTier names in day week permanent order
TEST(ShopBurstPrices, EverySoldRowSitsOnItsCategoryTier) {
    const std::vector<Frame> frames = loadBurst();
    ASSERT_FALSE(frames.empty());
    const std::pair<uint16_t, uint32_t> cats[] = {
        {0x00BF, 0}, {0x00C0, 1}, {0x00C2, 3}, {0x0103, 4}, {0x010C, 5}, {0x0108, 6}};
    size_t checked = 0;
    for (const auto& c : cats) {
        for (const SoldRow& r : soldRows(frames, c.first)) {
            const uint32_t tier = burstPriceTier(c.second, r.slot);
            ASSERT_NE(tier, 0u);
            ASSERT_EQ(r.options.size(), 3u) << "opcode " << c.first << " key " << r.key;
            EXPECT_EQ(r.options[0].key, burstDayKey(tier)) << "opcode " << c.first << " key " << r.key;
            EXPECT_EQ(r.options[1].key, burstWeekKey(tier)) << "opcode " << c.first << " key " << r.key;
            EXPECT_EQ(r.options[2].key, burstPermanentKey(tier)) << "opcode " << c.first << " key " << r.key;
            EXPECT_EQ(r.options[0].unit, 1u);
            EXPECT_EQ(r.options[0].amount, 1u);
            EXPECT_EQ(r.options[1].amount, 7u);
            EXPECT_EQ(r.options[2].unit, 0u);
            ++checked;
        }
    }
    // 8 drivers 41 karts 367 parts 4 pets 31 room rows with a tier and 35 car craft parts
    EXPECT_EQ(checked, 8u + 41u + 367u + 4u + 31u + 35u) << "the sold row census moved";
}

// the server charges what the client prints the sale in Astro else the base in gold
TEST(ShopCoinGrant, TheChargeFollowsTheClientIcon) {
    ShopPackets::PriceRow gold;
    gold.priceBase = 2500;
    gold.priceSale = 0;
    EXPECT_EQ(ShopPackets::chargedAmount(gold), 2500);
    EXPECT_EQ(ShopPackets::currencyOf(gold.priceSale), ShopPackets::Currency::Gp);
    ShopPackets::PriceRow astro;
    astro.priceBase = 0;
    astro.priceSale = 3000;
    EXPECT_EQ(ShopPackets::chargedAmount(astro), 3000);
    EXPECT_EQ(ShopPackets::currencyOf(astro.priceSale), ShopPackets::Currency::Cash);
    ShopPackets::Wallet wallet;
    wallet.gp = 5000;
    wallet.cash = 0;
    EXPECT_TRUE(ShopPackets::canAfford(wallet, ShopPackets::currencyOf(gold.priceSale), 2500));
    EXPECT_FALSE(ShopPackets::canAfford(wallet, ShopPackets::currencyOf(astro.priceSale), 3000));
}

// sub 4830C0 sends 0x00ED only for a mode 2 row with a count so a unit 0 buy adds one
TEST(ShopCoinGrant, APermanentCoinBuyAddsOneCoin) {
    const CoinPeriod first = coinPeriodAfterPurchase(0, 0, 0, 0);
    EXPECT_EQ(first.mode, 2u);
    EXPECT_EQ(first.value, 1);
    const CoinPeriod second = coinPeriodAfterPurchase(first.mode, first.value, 0, 0);
    EXPECT_EQ(second.value, 2);
    // a unit 2 row gives its amount and a stray mode resets the count to the new coins
    EXPECT_EQ(coinPeriodAfterPurchase(2, 3, 2, 5).value, 8);
    EXPECT_EQ(coinPeriodAfterPurchase(1, 45884, 0, 0).value, 1);
    EXPECT_TRUE(isGachaCoinKey(2000));
    EXPECT_TRUE(isGachaCoinKey(2001));
    EXPECT_FALSE(isGachaCoinKey(1000));
}

// buy the Gold Coin then the tab one Play sends the row back and the server accepts and burns it
TEST(ShopCoinGrant, TheGoldCoinBoughtThenRolled) {
    // the buy the burst row 4002 unit 0 lands a mode 2 row with one coin
    const CoinPeriod bought = coinPeriodAfterPurchase(0, 0, 0, 0);
    InventoryPackets::ItemRow row;
    row.instanceId = 77;
    row.baseKey = kGoldCoinKey;
    row.priceKey = 4002;
    row.periodMode = bought.mode;
    row.periodValue = static_cast<int32_t>(bought.value);
    row.activeFlag = 1;
    const std::array<uint8_t, 0x1C> blob = InventoryPackets::itemBlob(row);
    const std::vector<uint8_t> bytes(blob.begin(), blob.end());

    // FUN 004830C0 the client gate rec 0x14 active and rec 0x10 above zero
    EXPECT_NE(u32At(bytes, 0x14), 0u);
    EXPECT_GT(i32At(bytes, 0x10), 0);
    EXPECT_EQ(u32At(bytes, 0x04), GachaPetPackets::GOLD_COIN_BASE_KEY);

    // C2S 0x00ED is the verbatim 0x1C row
    Packet roll = Packet::fromCmdFull(0x00ED);
    roll.writeBytes(bytes.data(), bytes.size());
    GachaPetPackets::GachaRollRequest req;
    ASSERT_TRUE(GachaPetPackets::parseRoll(roll, req));
    EXPECT_EQ(req.itemBaseKey, kGoldCoinKey);
    EXPECT_TRUE(GachaPetPackets::rollRequestMatchesTicket(req, row));

    // the roll burns the coin and the popup then reads zero
    const InventoryPackets::ItemRow after = GachaPetPackets::decrementedTicket(row);
    EXPECT_EQ(after.periodValue, 0);
    GachaPetPackets::GachaRollRequest again = req;
    again.remainingRolls = 0;
    EXPECT_FALSE(GachaPetPackets::rollRequestMatchesTicket(again, after));
}

// the old permanent grant of a coin gave a mode 0 row the gacha refuses
TEST(ShopCoinGrant, AModeZeroCoinIsRefused) {
    InventoryPackets::ItemRow row;
    row.instanceId = 5;
    row.baseKey = kGachaCoinKey;
    row.periodMode = 0;
    row.periodValue = 0;
    row.activeFlag = 1;
    GachaPetPackets::GachaRollRequest req;
    req.instanceId = 5;
    req.itemBaseKey = kGachaCoinKey;
    req.activeFlag = 1;
    EXPECT_FALSE(GachaPetPackets::rollRequestMatchesTicket(req, row));
}

// the price table of 068 is the burst table key by key
TEST(ShopSeed068, PriceRowsAreTheBurstRows) {
    const std::string sql =
        readFile(std::string(KNC_REPO_ROOT) + "/server/scripts/068_shop_burst_prices.sql");
    ASSERT_FALSE(sql.empty()) << "migration 068 is missing";
    const std::regex tuple(
        R"(\((\d{4}),\s*0,\s*0,\s*(\d),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d)\))");
    std::map<uint32_t, PriceTuple> seeded;
    for (auto it = std::sregex_iterator(sql.begin(), sql.end(), tuple); it != std::sregex_iterator(); ++it) {
        const std::smatch& m = *it;
        const uint32_t key = static_cast<uint32_t>(std::stoul(m[1]));
        seeded[key] = PriceTuple(static_cast<uint32_t>(std::stoul(m[2])), static_cast<uint32_t>(std::stoul(m[3])),
                                 std::stoi(m[4]), std::stoi(m[5]));
        // the currency column follows the sale price like the client icon
        EXPECT_EQ(std::stoi(m[6]), priceIsAstro(std::stoi(m[5])) ? 1 : 0) << "price key " << key;
    }
    EXPECT_EQ(seeded, expectedBurstPrices());
}

// the coins the tiers the hidden race items and the cleanups are all in 068 and nothing is dropped
TEST(ShopSeed068, OptionsAndCleanups) {
    const std::string sql =
        readFile(std::string(KNC_REPO_ROOT) + "/server/scripts/068_shop_burst_prices.sql");
    ASSERT_FALSE(sql.empty());
    EXPECT_NE(sql.find("(2, 2000, 0, 4001, 0, 0, 0)"), std::string::npos);
    EXPECT_NE(sql.find("(2, 2001, 0, 4002, 0, 0, 0)"), std::string::npos);
    EXPECT_NE(sql.find("SELECT 0, base_key, 0, 2001, 1, 1, 0"), std::string::npos);
    EXPECT_NE(sql.find("SELECT 1, id, 0, 2002, 1, 1, 0"), std::string::npos);
    EXPECT_NE(sql.find("CASE WHEN s.category = 2 THEN 2003 ELSE 2004 END"), std::string::npos);
    EXPECT_NE(sql.find("SELECT 4, base_key, 0, 2005, 1, 1, 0"), std::string::npos);
    EXPECT_NE(sql.find("UPDATE def_item_wire SET visible = 0 WHERE item_key BETWEEN 20001 AND 20903"),
              std::string::npos);
    EXPECT_NE(sql.find("UPDATE def_kart_skin SET category = 1 WHERE skin_key = 9100"), std::string::npos);
    EXPECT_NE(sql.find("reward_item_type = 0, reward_item_key = 0"), std::string::npos);
    EXPECT_EQ(sql.find("DROP TABLE"), std::string::npos);
    EXPECT_EQ(sql.find("TRUNCATE"), std::string::npos);
}
