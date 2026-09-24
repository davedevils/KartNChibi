/// the three garage kart rules the ability pair hiding value the durability and the skin keys of the owned blob

#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <vector>

#include "packets/PacketBuilder.h"
#include "packets/KartDefinitionWire.h"
#include "packets/gen/InventoryPackets.h"
#include "util/GarageSlotRules.h"
#include "util/KartDurability.h"

using namespace knc;

namespace {

uint32_t u32At(const std::vector<uint8_t>& p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p.data() + at, 4);
    return v;
}

int32_t i32At(const std::vector<uint8_t>& p, size_t at) {
    int32_t v = 0;
    std::memcpy(&v, p.data() + at, 4);
    return v;
}

uint32_t u32At(const std::array<uint8_t, 0x38>& p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p.data() + at, 4);
    return v;
}

// 29 fixed bytes the three strings 32 skin bytes and 68 stat bytes then the two pairs
size_t abilityOffset(const std::string& model, const std::string& disp, const std::string& desc) {
    return 29 + model.size() + 1 + disp.size() + 1 + desc.size() + 1 + 32 + 68;
}

// the pairs the burst sends on all 46 kart rows kart ability pairs draw 0x42B910 draws ids 0 to 25
const uint8_t kHiddenPairs[16] = {
    0xFF,0xFF,0xFF,0xFF, 0x00,0x00,0x00,0x00,
    0xFF,0xFF,0xFF,0xFF, 0x00,0x00,0x00,0x00,
};

}  // namespace

// ability pairs 0x00C0 record 0x130 and 0x138

TEST(KartAbilityWire, PlainKartHidesBothPairs) {
    std::array<int32_t, 8> parts{};
    std::array<float, 17> stats{};
    Packet pkt = PacketBuilder::vehicleCatalog(10010, "basic_1", parts, {1002}, false,
                                                "CAR_BASIC_1_TITLE", "CAR_BASIC_1_INFO", stats);
    const auto& body = pkt.payload();
    const size_t at = abilityOffset("basic_1", "CAR_BASIC_1_TITLE", "CAR_BASIC_1_INFO");
    ASSERT_GE(body.size(), at + 16 + 4);
    EXPECT_EQ(0, std::memcmp(body.data() + at, kHiddenPairs, 16));
    EXPECT_EQ(i32At(body, at), KART_ABILITY_NONE);
    EXPECT_EQ(i32At(body, at + 8), KART_ABILITY_NONE);
    // the option count follows the pairs so the offset above is the real one
    EXPECT_EQ(u32At(body, at + 16), 1u);
}

TEST(KartAbilityWire, KartWithAbilitiesShipsItsPairs) {
    std::array<int32_t, 8> parts{};
    std::array<float, 17> stats{};
    std::array<KartAbilityPair, 2> abilities;
    abilities[0].id = 3;  abilities[0].percent = 20;
    abilities[1].id = 25; abilities[1].percent = 5;
    Packet pkt = PacketBuilder::vehicleCatalog(12007, "thunder", parts, {100}, false,
                                                "CAR_THUNDER_TITLE", "CAR_THUNDER_INFO", stats,
                                                abilities);
    const auto& body = pkt.payload();
    const size_t at = abilityOffset("thunder", "CAR_THUNDER_TITLE", "CAR_THUNDER_INFO");
    ASSERT_GE(body.size(), at + 16 + 4);
    EXPECT_EQ(i32At(body, at), 3);
    EXPECT_EQ(u32At(body, at + 4), 20u);
    EXPECT_EQ(i32At(body, at + 8), 25);
    EXPECT_EQ(u32At(body, at + 12), 5u);
    EXPECT_EQ(u32At(body, at + 16), 1u);
}

TEST(KartAbilityWire, OnlyOneAbilityLeavesTheOtherHidden) {
    KartDefinitionFields f;
    f.modelName = "m";
    f.displayNameKey = "d";
    f.descriptionKey = "i";
    f.abilityPair0.id = 0;
    f.abilityPair0.percent = 15;
    Packet pkt(0x00C0);
    writeKartDefinitionBody(pkt, f);
    const auto& body = pkt.payload();
    ASSERT_EQ(body.size(), abilityOffset("m", "d", "i") + 16);
    const size_t at = body.size() - 16;
    // id 0 is a real ability the client draws it with its percent
    EXPECT_EQ(i32At(body, at), 0);
    EXPECT_EQ(u32At(body, at + 4), 15u);
    EXPECT_EQ(0, std::memcmp(body.data() + at + 8, kHiddenPairs + 8, 8));
}

// durability rules the wire never states

TEST(KartDurabilityRules, TheClientMaxIsTheOnlyCap) {
    EXPECT_EQ(KART_DURABILITY_MAX, 500);
    EXPECT_EQ(clampKartDurability(-3), 0);
    EXPECT_EQ(clampKartDurability(501), 500);
    EXPECT_EQ(clampKartDurability(77), 77);
}

TEST(KartDurabilityRules, GrantOnAFreshKartIsModeThreeCapped) {
    KartPeriod p = kartPeriodAfterDurabilityGrant(0, 0, 500);
    EXPECT_EQ(p.mode, KART_PERIOD_DURABILITY);
    EXPECT_EQ(p.value, 500);
    p = kartPeriodAfterDurabilityGrant(0, 0, 800);
    EXPECT_EQ(p.value, 500);
}

TEST(KartDurabilityRules, GrantOnADurabilityKartAddsCapped) {
    KartPeriod p = kartPeriodAfterDurabilityGrant(3, 120, 250);
    EXPECT_EQ(p.mode, 3u);
    EXPECT_EQ(p.value, 370);
    p = kartPeriodAfterDurabilityGrant(3, 400, 250);
    EXPECT_EQ(p.value, 500);
}

TEST(KartDurabilityRules, GrantOnADayKartSwitchesToDurability) {
    const KartPeriod p = kartPeriodAfterDurabilityGrant(1, 45883, 250);
    EXPECT_EQ(p.mode, 3u);
    EXPECT_EQ(p.value, 250);
}

TEST(KartDurabilityRules, ScrollGrantStacksOneUse) {
    KartPeriod p = scrollPeriodAfterGrant(0, 0);
    EXPECT_EQ(p.mode, KART_SCROLL_PERIOD_USES);
    EXPECT_EQ(p.value, 1);
    p = scrollPeriodAfterGrant(2, 3);
    EXPECT_EQ(p.value, 4);
}

TEST(KartDurabilityRules, AFinishedRaceTakesOneAndFloorsAtZero) {
    EXPECT_EQ(KART_DURABILITY_WEAR_PER_RACE, 1);
    int32_t out = -1;
    ASSERT_TRUE(kartDurabilityAfterWear(3, 500, KART_DURABILITY_WEAR_PER_RACE, out));
    EXPECT_EQ(out, 499);
    ASSERT_TRUE(kartDurabilityAfterWear(3, 0, KART_DURABILITY_WEAR_PER_RACE, out));
    EXPECT_EQ(out, 0);
    // a permanent or day kart never wears
    EXPECT_FALSE(kartDurabilityAfterWear(0, 0, 1, out));
    EXPECT_FALSE(kartDurabilityAfterWear(1, 45883, 1, out));
}

TEST(KartDurabilityRules, RepairAddsTheUnitAmountCapped) {
    int32_t out = -1;
    ASSERT_TRUE(kartDurabilityAfterRepair(3, 120, 250, out));
    EXPECT_EQ(out, 370);
    ASSERT_TRUE(kartDurabilityAfterRepair(3, 400, 500, out));
    EXPECT_EQ(out, 500);
    ASSERT_TRUE(kartDurabilityAfterRepair(3, 0, KART_REPAIR_DEFAULT_AMOUNT, out));
    EXPECT_EQ(out, 500);
    EXPECT_FALSE(kartDurabilityAfterRepair(1, 45883, 250, out));
}

// the owned kart blob 0x38

TEST(KartBlob, NeverShipsAZeroPaintOrPlateKey) {
    InventoryPackets::KartRow row;
    row.instanceId = 14;
    row.baseKey    = 10012;
    row.activeFlag = 1;
    const auto blob = InventoryPackets::kartBlob(row);
    // a zero at 0x08 or 0x0C makes sub 4510C0 return null and the stock car build gives up
    EXPECT_EQ(u32At(blob, 0x08), 9007u);
    EXPECT_EQ(u32At(blob, 0x0C), 9100u);
    EXPECT_EQ(u32At(blob, 0x00), 14u);
    EXPECT_EQ(u32At(blob, 0x04), 10012u);

    row.skinPrimary   = 9001;
    row.skinSecondary = 9101;
    const auto own = InventoryPackets::kartBlob(row);
    EXPECT_EQ(u32At(own, 0x08), 9001u);
    EXPECT_EQ(u32At(own, 0x0C), 9101u);
}

TEST(KartBlob, PeriodBlockLandsAtTheDurabilityOffsets) {
    InventoryPackets::KartRow row;
    row.instanceId  = 13;
    row.baseKey     = 12007;
    row.priceKey    = 4102;
    row.periodMode  = KART_PERIOD_DURABILITY;
    row.periodValue = 499;
    row.activeFlag  = 1;
    const auto blob = InventoryPackets::kartBlob(row);
    EXPECT_EQ(u32At(blob, 0x28), 4102u);
    EXPECT_EQ(u32At(blob, 0x2C), 3u);
    EXPECT_EQ(u32At(blob, 0x30), 499u);
    EXPECT_EQ(u32At(blob, 0x34), 1u);
}

TEST(KartBlob, RepairAckCarriesTheKartPeriodTail) {
    InventoryPackets::ItemRow scroll;
    scroll.instanceId  = 7;
    scroll.baseKey     = 3001;
    scroll.periodMode  = KART_SCROLL_PERIOD_USES;
    scroll.periodValue = 0;
    scroll.activeFlag  = 1;

    InventoryPackets::KartRow kart;
    kart.instanceId  = 13;
    kart.baseKey     = 12007;
    kart.priceKey    = 4102;
    kart.periodMode  = KART_PERIOD_DURABILITY;
    kart.periodValue = 500;
    kart.activeFlag  = 1;

    Packet pkt = InventoryPackets::useItemAckWithKartPeriod(scroll, kart);
    const auto& body = pkt.payload();
    ASSERT_EQ(body.size(), 4u + 0x1C + 16);
    EXPECT_EQ(u32At(body, 0), 2u);
    EXPECT_EQ(u32At(body, 4), 7u);
    // 0x484770 copies these sixteen bytes over the selected kart period block
    EXPECT_EQ(u32At(body, 0x20), 4102u);
    EXPECT_EQ(u32At(body, 0x24), 3u);
    EXPECT_EQ(u32At(body, 0x28), 500u);
    EXPECT_EQ(u32At(body, 0x2C), 1u);
}

// the garage paint Remove and the Delete of an expired worn paint

// sub 484B10 copies kart def 0x84 0x88 0x8C back on a Remove the row takes that key
TEST(GaragePaint, RemovePutsTheKartDefKeyBack) {
    EXPECT_EQ(kartSkinDefaultKey("skin_primary"), 9007u);
    EXPECT_EQ(kartSkinDefaultKey("skin_secondary"), 9100u);
    EXPECT_EQ(kartSkinDefaultKey("skin_tertiary"), 0u);

    // the Remove answer the stock reads cat 3 then the part row whose key picks the slot
    InventoryPackets::PartRow black;
    black.instanceId = 13;
    black.baseKey = 9001;
    black.periodMode = 1;
    black.periodValue = 45886;
    black.activeFlag = 1;
    Packet ack = InventoryPackets::unequipPartAck(black);
    ASSERT_EQ(ack.opcode(), 0x00BAu);
    ASSERT_EQ(ack.payload().size(), 4u + 0x1Cu);
    EXPECT_EQ(u32At(ack.payload(), 0), 3u);
    EXPECT_EQ(u32At(ack.payload(), 8), 9001u);

    // the set that follows carries the kart with the def paint the row now holds
    InventoryPackets::CharacterRow cr;
    cr.instanceId = 31;
    cr.baseKey = 9;
    InventoryPackets::KartRow kr;
    kr.instanceId = 32;
    kr.baseKey = 10010;
    kr.skinPrimary = kartSkinDefaultKey("skin_primary");
    kr.skinSecondary = kartSkinDefaultKey("skin_secondary");
    Packet set = InventoryPackets::applyEquipmentSet(cr.instanceId, kr.instanceId, cr, kr);
    ASSERT_EQ(set.payload().size(), 12u + 0x2Cu + 0x38u);
    EXPECT_EQ(u32At(set.payload(), 12 + 0x2C + 0x08), 9007u);
    EXPECT_EQ(u32At(set.payload(), 12 + 0x2C + 0x0C), 9100u);
}

// the user row of 2026-09-23 black paint day 45886 the client counted 45890 and showed Delete
TEST(GaragePaint, TheExpiredBlackPaintIsDeletable) {
    std::tm t{};
    t.tm_year = 126;
    t.tm_yday = 265;
    const int64_t today = clientDayNumber(t);
    EXPECT_EQ(today, 45890);
    // the database still said active the old handler answered MSG UNKNOWN ERROR
    EXPECT_TRUE(ownedRowExpired(1, 1, 45886, today));
    EXPECT_FALSE(ownedRowExpired(1, 1, 45890, today));
    EXPECT_FALSE(ownedRowExpired(1, 1, 45893, today));
    EXPECT_TRUE(ownedRowExpired(0, 0, 0, today));
    EXPECT_FALSE(ownedRowExpired(1, 0, 0, today));
    EXPECT_TRUE(ownedRowExpired(1, 2, 0, today));
    EXPECT_FALSE(ownedRowExpired(1, 2, 3, today));
    EXPECT_FALSE(ownedRowExpired(1, 3, 0, today));
}
