/// one kart truth the garage the race and the car craft all read owned kart and its catalogue row

#include <gtest/gtest.h>
#include <map>
#include <string>

#include "util/KartDurability.h"
#include "util/OwnedKartRow.h"

using namespace knc;

namespace {

/// the row a shop buy writes it names a kart and nothing else no stat block at all
std::map<std::string, std::string> shopBoughtRow() {
    return {
        {"id", "31"},
        {"vehicle_type_id", "10015"},
        // the select composes these two from the catalogue row and the upgrade delta
        {"stat_speed", "60"}, {"stat_accel", "58"}, {"stat_handling", "56"},
        {"stat_drift", "46"}, {"stat_boost", "40"}, {"stat_weight", "50"},
        {"stat_special", "0"},
        {"durability", "500"}, {"max_durability", "500"},
        {"equipped", "0"},
    };
}

}  // namespace

// the legacy vehicles table is gone so no reader may name it again
TEST(OwnedKartTruth, TheSharedSelectNamesOwnedKartAndNeverTheLegacyTable) {
    const std::string sql = ownedKartSelect();
    EXPECT_NE(sql.find("FROM owned_kart k"), std::string::npos);
    EXPECT_EQ(sql.find(" vehicles "), std::string::npos);
    EXPECT_EQ(sql.find("vehicle_type_id FROM"), std::string::npos);
}

// the equipped kart is the selection column the profile blob reads not a second flag
TEST(OwnedKartTruth, TheEquippedFlagComesFromTheSelectionColumn) {
    const std::string sql = ownedKartSelect();
    EXPECT_NE(sql.find("selected_kart_instance_id = k.id"), std::string::npos);
    EXPECT_NE(sql.find("AS equipped"), std::string::npos);
}

// the stats are the catalogue value plus the upgrade so a grant needs no stat block
TEST(OwnedKartTruth, TheStatSelectAddsTheUpgradeToTheCatalogueValue) {
    const std::string sql = kartStatColumns();
    EXPECT_NE(sql.find("COALESCE(vt.stat_speed, 50) + k.up_speed AS stat_speed"),
              std::string::npos);
    EXPECT_NE(sql.find("COALESCE(vt.stat_special, 0) + k.up_special AS stat_special"),
              std::string::npos);
    EXPECT_NE(kartTemplateJoin().find("vehicle_templates vt ON vt.id = k.base_key"),
              std::string::npos);
}

// the durability still comes from the period pair only mode 3 carries one
TEST(OwnedKartTruth, TheDurabilitySelectReadsThePeriodPairOfTheOwnedRow) {
    const std::string sql = kartDurabilityColumns();
    EXPECT_NE(sql.find("k.period_mode = 3"), std::string::npos);
    EXPECT_NE(sql.find("k.period_value"), std::string::npos);
    EXPECT_NE(sql.find("500 AS max_durability"), std::string::npos);
}

// a kart bought in the shop has no upgrade yet and must still show its catalogue stats
TEST(OwnedKartStats, AShopKartShowsTheCatalogueValueWithNoUpgrade) {
    EXPECT_EQ(effectiveKartStat(60, 0), 60);
    EXPECT_EQ(effectiveKartStat(0, 0), 0);
}

// a garage upgrade is one step over the catalogue
TEST(OwnedKartStats, AnUpgradeAddsOverTheCatalogueValue) {
    EXPECT_EQ(effectiveKartStat(60, 3), 63);
    EXPECT_EQ(effectiveKartStat(40, 1), 41);
}

// an old seed could hold a stat below the catalogue and that is not a negative upgrade
TEST(OwnedKartStats, ANegativeDeltaIsNotAnUpgrade) {
    EXPECT_EQ(effectiveKartStat(50, -5), 50);
}

// there are seven stats in wire order and seven upgrade columns to match
TEST(OwnedKartStats, SevenStatsSevenUpgradeColumns) {
    EXPECT_EQ(KART_STAT_COUNT, 7u);
    const char* const* cols = kartUpgradeColumns();
    EXPECT_STREQ(cols[0], "up_speed");
    EXPECT_STREQ(cols[6], "up_special");
}

// the kart the shop grants lands in the garage with its stats its bar and its instance id
TEST(OwnedKartRowReader, AShopKartReachesTheGarageWithItsStats) {
    const VehicleInfo v = ownedKartVehicleInfo(shopBoughtRow());
    EXPECT_EQ(v.id, 31);
    EXPECT_EQ(v.templateId, 10015);
    EXPECT_EQ(v.stats[0], 60);
    EXPECT_EQ(v.stats[1], 58);
    EXPECT_EQ(v.stats[2], 56);
    EXPECT_EQ(v.stats[3], 46);
    EXPECT_EQ(v.stats[4], 40);
    EXPECT_EQ(v.stats[5], 50);
    EXPECT_EQ(v.stats[6], 0);
    EXPECT_EQ(v.durability, 500);
    EXPECT_EQ(v.maxDurability, KART_DURABILITY_MAX);
    EXPECT_FALSE(v.equipped);
}

// the starter kart of a new character is the selected one so it comes back equipped
TEST(OwnedKartRowReader, TheStarterKartComesBackEquipped) {
    std::map<std::string, std::string> row = shopBoughtRow();
    row["id"] = "2";
    row["vehicle_type_id"] = "10010";
    row["equipped"] = "1";
    const VehicleInfo v = ownedKartVehicleInfo(row);
    EXPECT_EQ(v.id, 2);
    EXPECT_EQ(v.templateId, 10010);
    EXPECT_TRUE(v.equipped);
}

// a column the select could not fill must never throw and must never publish a zero stat kart
TEST(OwnedKartRowReader, AMissingColumnFallsBackAndNeverThrows) {
    std::map<std::string, std::string> row;
    row["id"] = "7";
    row["vehicle_type_id"] = "10010";
    row["stat_speed"] = "";
    const VehicleInfo v = ownedKartVehicleInfo(row);
    EXPECT_EQ(v.id, 7);
    EXPECT_EQ(v.stats[0], 50);
    EXPECT_EQ(v.stats[3], 40);
    EXPECT_EQ(v.durability, KART_DURABILITY_MAX);
}
