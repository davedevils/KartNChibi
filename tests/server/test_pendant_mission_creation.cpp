/// the pendant rows and grants the mission lock fee reward and refusal and the nickname taboo fold

#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "net/Packet.h"
#include "packets/gen/CharCreatePackets.h"
#include "packets/gen/MissionPackets.h"
#include "packets/gen/SocialPackets.h"
#include "packets/PacketBuilder.h"
#include "util/PendantRules.h"

using namespace knc;

namespace {

uint32_t u32At(const std::vector<uint8_t>& p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p.data() + at, 4);
    return v;
}

std::vector<MissionPackets::MissionProgressEntry> rows(std::initializer_list<uint32_t> cleared) {
    std::vector<MissionPackets::MissionProgressEntry> out;
    uint32_t id = 0;
    for (uint32_t c : cleared) out.push_back({id++, c});
    return out;
}

}  // namespace

// sub 451250 finds the row by key then erases the first row with its instance so ids must differ
TEST(PendantRows, EachOwnedRowCarriesItsOwnInstance) {
    EXPECT_EQ(pendantInstanceId(1), 1u);
    EXPECT_EQ(pendantInstanceId(12), 12u);
    EXPECT_NE(pendantInstanceId(3), pendantInstanceId(4));
    const auto row = pendantRow(9);
    std::vector<uint8_t> bytes(row.begin(), row.end());
    ASSERT_EQ(bytes.size(), 8u);
    EXPECT_EQ(u32At(bytes, 0), 9u);
    EXPECT_EQ(u32At(bytes, 4), 9u);
}

// the login rows and the live grant both ride sub 47E880 on 0x011A and 0x011B
TEST(PendantRows, LoginAndLiveGrantFrames) {
    const Packet login = PacketBuilder::entitySimple(false, 5, 5);
    const Packet live = PacketBuilder::entitySimple(true, 8, 8);
    EXPECT_EQ(login.opcode(), 0x011A);
    EXPECT_EQ(live.opcode(), 0x011B);
    ASSERT_EQ(login.payload().size(), 8u);
    EXPECT_EQ(u32At(live.payload(), 0), 8u);
    EXPECT_EQ(u32At(live.payload(), 4), 8u);
}

// minus one takes it off an owned key goes on anything else keeps the worn one
TEST(PendantEquip, TheAnswerAlwaysCarriesTheWornKey) {
    EXPECT_EQ(pendantEquipApplied(-1, false, 4), 0);
    EXPECT_EQ(pendantEquipApplied(0, false, 4), 0);
    EXPECT_EQ(pendantEquipApplied(7, true, 4), 7);
    EXPECT_EQ(pendantEquipApplied(7, false, 4), 4);
    EXPECT_EQ(pendantEquipApplied(7, false, 0), 0);
}

// PENDANT 06 to 12 of the text table the race counts and the level tiers
TEST(PendantGrants, LevelAndRaceCountTiers) {
    EXPECT_EQ(levelPendantFor(9), 0u);
    EXPECT_EQ(levelPendantFor(10), 8u);
    EXPECT_EQ(levelPendantFor(29), 9u);
    EXPECT_EQ(levelPendantFor(50), 12u);
    EXPECT_EQ(levelPendantsUpTo(35), (std::vector<uint32_t>{8, 9, 10}));
    EXPECT_TRUE(levelPendantsUpTo(1).empty());
    EXPECT_TRUE(racePendantsFor(99).empty());
    EXPECT_EQ(racePendantsFor(100), (std::vector<uint32_t>{6}));
    EXPECT_EQ(racePendantsFor(1500), (std::vector<uint32_t>{6, 7}));
    EXPECT_EQ(kTutorialPendant, 1u);
}

// PENDANT 02 to 05 quest levels 5 10 15 20 for Rosie Chai Porki Dim Dim sub 429D30 entry 0x24
TEST(PendantGrants, QuestLevelsEarnThePetPendants) {
    EXPECT_EQ(questPendantFor(5), 2u);
    EXPECT_EQ(questPendantFor(10), 3u);
    EXPECT_EQ(questPendantFor(15), 4u);
    EXPECT_EQ(questPendantFor(20), 5u);
    EXPECT_EQ(questPendantFor(1), 0u);
    EXPECT_EQ(questPendantFor(4), 0u);
    EXPECT_EQ(questPendantFor(25), 0u);
}

// PENDANT 13 the hidden row is mission chapter 1 the five missions 0 to 4 all cleared
TEST(PendantGrants, MissionChapterOneEarnsTheHiddenPendant) {
    EXPECT_EQ(kMissionChapterOnePendant, 13u);
    EXPECT_TRUE(missionChapterOneCleared({0, 1, 2, 3, 4}));
    EXPECT_TRUE(missionChapterOneCleared({4, 3, 2, 1, 0, 7}));
    EXPECT_FALSE(missionChapterOneCleared({0, 1, 2, 3}));
    EXPECT_FALSE(missionChapterOneCleared({}));
}

// PENDANT 01 comes from the rookie licence only migration 068 drops the mission 2 grant and the 037 blanket rows
TEST(PendantGrants, Migration068KeepsTheTutorialPendantToTheTutorial) {
    std::ifstream f(std::string(KNC_REPO_ROOT) + "/server/scripts/068_shop_burst_prices.sql", std::ios::binary);
    ASSERT_TRUE(f.good()) << "migration 068 is missing";
    const std::string sql((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_NE(sql.find("WHERE mission_id = 2 AND reward_item_type = 7 AND reward_item_key = 1"), std::string::npos);
    EXPECT_NE(sql.find("WHERE o.pendant_key = 1 AND COALESCE(c.license_class, 0) = 0"), std::string::npos);
    EXPECT_EQ(kTutorialPendant, 1u);
}

TEST(PendantWire, TheUserListCarriesTheWornKey) {
    SocialPackets::UserListEntry e;
    e.playerId = 12;
    e.name = u"AgPenOne";
    e.level = 3;
    e.pendantSlot = 8;
    const Packet page = SocialPackets::userListPage(0, 1, {e});
    ASSERT_EQ(page.payload().size(), 296u);
    EXPECT_EQ(u32At(page.payload(), 0x0C), 12u);
    EXPECT_EQ(u32At(page.payload(), 0x0C + 0x24), 8u);
    EXPECT_EQ(u32At(page.payload(), 0x124), 1u);
}

// sub 499180 reads the u32 after the pccafe byte as the pendant of the room name plate
TEST(PendantWire, TheRoomMemberCarriesTheWornKey) {
    RoomMemberWire m;
    m.slot = 1;
    m.playerId = 12;
    m.displayName = u"Abcd";
    m.titleKey = 8;
    const Packet pkt = PacketBuilder::roomMember(m);
    const size_t at = 12 + 2 * (m.displayName.size() + 1) + 3;
    EXPECT_EQ(u32At(pkt.payload(), at), 8u);
}

// the profile blob 0x4C4 is the worn pendant 0x01A20B2C not the rank points
TEST(PendantWire, TheProfileBlobCarriesTheWornKey) {
    PlayerData p;
    p.id = 12;
    p.name = "AgPenOne";
    p.rankPoints = 1000;
    p.pendantKey = 8;
    const Packet pkt = PacketBuilder::sessionConfirm(12, p);
    ASSERT_GE(pkt.payload().size(), 4u + 0x4C8u);
    EXPECT_EQ(u32At(pkt.payload(), 4 + 0x4C4), 8u);
    const Packet refresh = PacketBuilder::connectionOkWithPlayer(p);
    EXPECT_EQ(u32At(refresh.payload(), 0x22), 8u);
}

// type 7 of 0x008C and 0x00A3 is the pendant instance then key never a key and a count
TEST(MissionReward, ThePendantBlobIsInstanceThenKey) {
    const MissionPackets::RewardBlob r = MissionPackets::pendantReward(1);
    EXPECT_EQ(r.type, 7u);
    ASSERT_EQ(r.bytes.size(), 8u);
    EXPECT_TRUE(MissionPackets::validateRewardBlob(r));
    EXPECT_EQ(u32At(r.bytes, 0), 1u);
    EXPECT_EQ(u32At(r.bytes, 4), 1u);
    const Packet pkt = MissionPackets::missionCompleteWithReward(2, 900, 400, r);
    ASSERT_EQ(pkt.payload().size(), 28u);
    EXPECT_EQ(u32At(pkt.payload(), 0), 1u);
    EXPECT_EQ(u32At(pkt.payload(), 4), 2u);
    EXPECT_EQ(u32At(pkt.payload(), 0x0C), 900u);
    EXPECT_EQ(u32At(pkt.payload(), 0x10), 400u);
    EXPECT_EQ(u32At(pkt.payload(), 0x18), 1u);
}

// has reward 2 makes sub 47B9E0 read twelve bytes and write nothing
TEST(MissionReward, ARefusalIsTwelveBytesWithFlagTwo) {
    const Packet pkt = MissionPackets::missionRefused(3);
    EXPECT_EQ(pkt.opcode(), 0x008C);
    ASSERT_EQ(pkt.payload().size(), 12u);
    EXPECT_EQ(u32At(pkt.payload(), 0), 2u);
    EXPECT_EQ(u32At(pkt.payload(), 4), 3u);
}

// sub 43B9A0 the first row the cleared rows and the row right after a cleared one play
TEST(MissionLock, TheStockUnlockOrder) {
    const auto fresh = rows({0, 0, 0, 0, 0});
    EXPECT_TRUE(MissionPackets::missionPlayable(fresh, 0));
    EXPECT_FALSE(MissionPackets::missionPlayable(fresh, 1));
    const auto two = rows({1, 1, 0, 0, 0});
    EXPECT_TRUE(MissionPackets::missionPlayable(two, 1));
    EXPECT_TRUE(MissionPackets::missionPlayable(two, 2));
    EXPECT_FALSE(MissionPackets::missionPlayable(two, 3));
    // a later row cleared out of order stays playable a missing id never is
    const auto gap = rows({1, 0, 1, 0, 0});
    EXPECT_TRUE(MissionPackets::missionPlayable(gap, 2));
    EXPECT_TRUE(MissionPackets::missionPlayable(gap, 3));
    EXPECT_FALSE(MissionPackets::missionPlayable(gap, 9));
}

// the menu shows the fee until the row is cleared and the start charges the same number
TEST(MissionFee, TheFeeStopsOnceCleared) {
    EXPECT_EQ(MissionPackets::entryFeeFor(20, false), 20u);
    EXPECT_EQ(MissionPackets::entryFeeFor(20, true), 0u);
    EXPECT_EQ(MissionPackets::entryFeeFor(0, false), 0u);
}

// sub 4E15E0 drops the marks lowers A to Z and runs a substring search on the folded name
TEST(NicknameTaboo, TheClientFoldAndSearch) {
    EXPECT_EQ(CharCreatePackets::tabooFold(u"F.u_c-K 1"), u"fuck1");
    const std::vector<std::u16string> words = {u"fuck", u"Ku_Klux_Klan", u"KKK", u"²"};
    EXPECT_TRUE(CharCreatePackets::tabooHit(u"xF.u.c.kx", words));
    EXPECT_TRUE(CharCreatePackets::tabooHit(u"KuKluxKlan", words));
    EXPECT_TRUE(CharCreatePackets::tabooHit(u"kkkRacer", words));
    EXPECT_TRUE(CharCreatePackets::tabooHit(u"Ab²cd", words));
    EXPECT_FALSE(CharCreatePackets::tabooHit(u"HlTester", words));
    EXPECT_FALSE(CharCreatePackets::tabooHit(u"Racer", {u"", u"-"}));
}

// the stock edit box refuses the percent sign and the length stays four to eleven
TEST(NicknameTaboo, LengthAndThePercentSign) {
    EXPECT_EQ(CharCreatePackets::validateNickname(u"abc"), CharCreatePackets::CREATE_INVALID_NICK);
    EXPECT_EQ(CharCreatePackets::validateNickname(u"abcd"), CharCreatePackets::CREATE_OK);
    EXPECT_EQ(CharCreatePackets::validateNickname(u"abcdefghijk"), CharCreatePackets::CREATE_OK);
    EXPECT_EQ(CharCreatePackets::validateNickname(u"abcdefghijkl"), CharCreatePackets::CREATE_INVALID_NICK);
    EXPECT_EQ(CharCreatePackets::validateNickname(u"ab%sx"), CharCreatePackets::CREATE_INVALID_NICK);
}
