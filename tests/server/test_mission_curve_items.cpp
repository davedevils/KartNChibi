/// covers mission chain migration 071 level curve item tab and slot exchange count

#include <gtest/gtest.h>
#include <cstdint>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "packets/gen/MissionPackets.h"
#include "packets/gen/ProgressionPackets.h"
#include "util/KartDurability.h"
#include "util/LevelCurve.h"
#include "util/ShopRules.h"
#include "util/SlotExchange.h"

using namespace knc;

namespace {

std::string readRepoFile(const std::string& rel) {
    std::ifstream f(std::string(KNC_REPO_ROOT) + "/" + rel, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::vector<MissionPackets::MissionProgressEntry> missionRows(std::initializer_list<uint32_t> cleared) {
    std::vector<MissionPackets::MissionProgressEntry> out;
    uint32_t id = 0;
    for (uint32_t c : cleared) out.push_back({id++, c});
    return out;
}

struct CurveRow {
    int32_t level = 0;
    int32_t cumExp = 0;
};

std::vector<CurveRow> curve071() {
    const std::string sql = readRepoFile("server/scripts/071_level_curve_and_item_tab.sql");
    std::vector<CurveRow> rows;
    const std::regex tuple(R"(\((\d+),(\d+)\))");
    for (auto it = std::sregex_iterator(sql.begin(), sql.end(), tuple); it != std::sregex_iterator(); ++it)
        rows.push_back({std::stoi((*it)[1]), std::stoi((*it)[2])});
    return rows;
}

}  // namespace

// ids 0 and 2 cleared not 1 list still stops at first uncleared row
TEST(MissionChain, TheListStopsAtTheFirstRowNotCleared) {
    const auto all = missionRows({1, 0, 1, 0, 0});
    const auto chain = MissionPackets::chainProgress(all);
    ASSERT_EQ(chain.size(), 2u);
    EXPECT_EQ(chain[0].missionId, 0u);
    EXPECT_EQ(chain[0].cleared, 1u);
    EXPECT_EQ(chain[1].missionId, 1u);
    EXPECT_EQ(chain[1].cleared, 0u);
    EXPECT_FALSE(MissionPackets::missionPlayable(chain, 2));
    EXPECT_TRUE(MissionPackets::missionPlayable(chain, 1));

    // fresh character sees one open row slots past the list draw locked
    const auto fresh = MissionPackets::chainProgress(missionRows({0, 0, 0, 0, 0}));
    ASSERT_EQ(fresh.size(), 1u);
    EXPECT_TRUE(MissionPackets::missionPlayable(fresh, 0));
    EXPECT_FALSE(MissionPackets::missionPlayable(fresh, 1));

    // all cleared shows all five rows
    EXPECT_EQ(MissionPackets::chainProgress(missionRows({1, 1, 1, 1, 1})).size(), 5u);
}

// clearing row 1 opens rows 2 and 3 via 0x8A a hidden earlier clear on row 2 stays cleared
TEST(MissionChain, AFirstClearOpensTheNextRows) {
    const auto all = missionRows({1, 0, 1, 0, 0});
    const auto opened = MissionPackets::rowsOpenedByClear(all, 1);
    ASSERT_EQ(opened.size(), 2u);
    EXPECT_EQ(opened[0].missionId, 2u);
    EXPECT_EQ(opened[0].cleared, 1u);
    EXPECT_EQ(opened[1].missionId, 3u);
    EXPECT_EQ(opened[1].cleared, 0u);

    const Packet unlock = MissionPackets::missionUnlocked(opened[1].missionId, opened[1].cleared);
    EXPECT_EQ(unlock.opcode(), 0x008A);
    EXPECT_EQ(unlock.payload().size(), 8u);

    // last row opens nothing replay also opens nothing
    EXPECT_TRUE(MissionPackets::rowsOpenedByClear(missionRows({1, 1, 1, 1, 0}), 4).empty());
    EXPECT_TRUE(MissionPackets::rowsOpenedByClear(missionRows({1, 1, 0, 0, 0}), 0).empty());
}

// levels 1 to 17 are the GOA totals and level 46 meets the video point of 1035000
TEST(LevelCurve071, GoaHeadAndTheVideoPoint) {
    const auto rows = curve071();
    ASSERT_EQ(rows.size(), 55u);
    const std::vector<int32_t> goa = {0, 100, 300, 600, 1000, 1500, 2150, 2950, 3900,
                                      5000, 6300, 7900, 9800, 12000, 14500, 17350, 20800};
    for (size_t i = 0; i < goa.size(); ++i) {
        EXPECT_EQ(rows[i].level, static_cast<int32_t>(i + 1));
        EXPECT_EQ(rows[i].cumExp, goa[i]) << "level " << i + 1;
    }
    for (size_t i = 1; i < rows.size(); ++i) {
        EXPECT_EQ(rows[i].level, rows[i - 1].level + 1);
        EXPECT_GT(rows[i].cumExp, rows[i - 1].cumExp);
        EXPECT_EQ(rows[i].cumExp % 50, 0);
    }
    EXPECT_EQ(rows[45].level, 46);
    EXPECT_EQ(rows[45].cumExp, 1035000);
    EXPECT_EQ(rows.back().level, 55);
}

// the stored level and the 0x000A floor and next come from the same rows so the bar never overflows
TEST(LevelCurve071, TheLevelUpAndTheDisplayAgree) {
    const auto rows = curve071();
    ASSERT_FALSE(rows.empty());

    // the GM of the video EXP 1000000 of 1035000 at level 45
    int32_t floorVal = 0;
    int32_t nextVal = 0;
    const int32_t gm = levelOnCurve(rows, 1000000, 1);
    EXPECT_EQ(gm, 45);
    EXPECT_TRUE(boundsOnCurve(rows, gm, floorVal, nextVal, ProgressionPackets::EXP_MAX));
    EXPECT_EQ(nextVal, 1035000);
    EXPECT_EQ(gm - ProgressionPackets::DB_LEVEL_MIN, 44);

    for (size_t i = 0; i < rows.size(); ++i) {
        const int32_t level = rows[i].level;
        EXPECT_EQ(levelOnCurve(rows, rows[i].cumExp, 1), level);
        ASSERT_TRUE(boundsOnCurve(rows, level, floorVal, nextVal, ProgressionPackets::EXP_MAX));
        EXPECT_EQ(floorVal, rows[i].cumExp);
        EXPECT_GT(nextVal, floorVal);
        const int32_t lastOfLevel = nextVal - 1;
        EXPECT_EQ(levelOnCurve(rows, lastOfLevel, 1), level);
        // sub 429990 fills exp minus floor over next minus floor times 256 pixels
        const double bar = 256.0 * (lastOfLevel - floorVal) / (nextVal - floorVal);
        EXPECT_GE(bar, 0.0);
        EXPECT_LT(bar, 256.0);
    }
    EXPECT_LE(rows.back().level - ProgressionPackets::DB_LEVEL_MIN, ProgressionPackets::WIRE_LEVEL_MAX_SEND);
}

// the video Item tab sells Slot Exchange in gold by uses and the 100% kit fills the bar
TEST(ItemTab071, SlotExchangeRowsAndTheFullKit) {
    const std::string sql = readRepoFile("server/scripts/071_level_curve_and_item_tab.sql");
    ASSERT_FALSE(sql.empty());
    const std::regex price(R"(\((42\d\d), 0, 0, (\d), +(\d+), +(\d+), (\d+), (\d)\))");
    std::map<int, std::vector<int>> rows;
    for (auto it = std::sregex_iterator(sql.begin(), sql.end(), price); it != std::sregex_iterator(); ++it)
        rows[std::stoi((*it)[1])] = {std::stoi((*it)[2]), std::stoi((*it)[3]), std::stoi((*it)[4]),
                                     std::stoi((*it)[5]), std::stoi((*it)[6])};
    ASSERT_EQ(rows.size(), 3u);
    const std::map<int, std::pair<int, int>> want = {{4201, {50, 50}}, {4202, {105, 100}}, {4203, {220, 200}}};
    for (const auto& w : want) {
        const auto& r = rows.at(w.first);
        EXPECT_EQ(r[0], 2) << "unit type 2 prints UNIT TIMES";
        EXPECT_EQ(r[1], w.second.first);
        EXPECT_EQ(r[2], w.second.second);
        EXPECT_FALSE(priceIsAstro(r[3])) << "a zero sale is a gold price";
        EXPECT_EQ(priceCharged(r[2], r[3]), w.second.second);
    }
    EXPECT_NE(sql.find("(2, 1000, 0, 4201, 2,  50, 0)"), std::string::npos);
    EXPECT_NE(sql.find("UPDATE def_item_wire SET visible = 0 WHERE item_key IN (2000, 3000)"), std::string::npos);
    EXPECT_NE(sql.find("UPDATE shop_price SET unit_type = 3, unit_amount = 500 WHERE price_key = 4102"),
              std::string::npos);

    int32_t after = 0;
    ASSERT_TRUE(kartDurabilityAfterRepair(KART_PERIOD_DURABILITY, 1, 500, after));
    EXPECT_EQ(after, KART_DURABILITY_MAX);
}

// sub 4AEDD0 takes the use and swaps before C2S 0x00CB so the server follows the count and never swaps
TEST(SlotExchange, OneUsePerSwapAndResyncOnADifference) {
    const auto used = slotExchangeStep(SLOT_EXCHANGE_FLAG_USED, 49, 1, 50);
    EXPECT_TRUE(used.decrement);
    EXPECT_EQ(used.countAfter, 49);
    EXPECT_FALSE(used.resync);

    // the client spent a use the server does not hold so 0x0115 puts zero back
    const auto empty = slotExchangeStep(SLOT_EXCHANGE_FLAG_USED, 3, 1, 0);
    EXPECT_FALSE(empty.decrement);
    EXPECT_EQ(empty.countAfter, 0);
    EXPECT_TRUE(empty.resync);

    // an inactive row never swaps in sub 4AEDD0 so nothing is paid
    EXPECT_FALSE(slotExchangeStep(SLOT_EXCHANGE_FLAG_USED, 10, 0, 10).decrement);

    // the lobby and room sync only repairs a drift
    EXPECT_FALSE(slotExchangeStep(SLOT_EXCHANGE_FLAG_SYNC, 50, 1, 50).resync);
    const auto drift = slotExchangeStep(SLOT_EXCHANGE_FLAG_SYNC, 10, 1, 50);
    EXPECT_FALSE(drift.decrement);
    EXPECT_TRUE(drift.resync);
    EXPECT_EQ(drift.countAfter, 50);
}
