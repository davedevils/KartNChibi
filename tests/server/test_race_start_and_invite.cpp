/// the GO waits for every human scene loaded answer up to its cap and a lobby invite is refused aloud

#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "handlers/RaceHandler.h"
#include "handlers/SocialHandler.h"
#include "net/Packet.h"
#include "packets/gen/SocialPackets.h"

using namespace knc;

namespace {

// the tail of a packet read as the client parser 0x0126 of Session parseSystemLine reads it
struct SystemLine {
    uint32_t unused = 0;
    std::u16string name;
    std::string key;
    uint32_t type = 0;
};

SystemLine readSystemLine(Packet pkt) {
    SystemLine line;
    line.unused = pkt.readUInt32();
    line.name = pkt.readWString(13);
    line.key = pkt.readString(259);
    line.type = pkt.readUInt32();
    return line;
}

}  // namespace

// the grid arms every human and the GO waits till the last one answered
TEST(SceneLoadWait, TheGoWaitsForEveryHuman) {
    SceneLoadWait wait;
    wait.arm({9, 14}, 1000);
    EXPECT_TRUE(wait.owes(9));
    EXPECT_TRUE(wait.owes(14));
    EXPECT_FALSE(wait.clear(9));
    EXPECT_FALSE(wait.owes(9));
    EXPECT_TRUE(wait.clear(14));
}

// a second answer of the same racer the stock sends one per S2C 0x0D changes nothing
TEST(SceneLoadWait, ARepeatedAnswerIsHarmless) {
    SceneLoadWait wait;
    wait.arm({9, 14}, 0);
    EXPECT_FALSE(wait.clear(9));
    EXPECT_FALSE(wait.clear(9));
    EXPECT_TRUE(wait.clear(14));
    EXPECT_TRUE(wait.clear(14));
}

// the old 3 s fallback started the gate races without the client the cap covers the slow loads
TEST(SceneLoadWait, TheCapCoversEveryMeasuredLoad) {
    // the stock under wine answered 2870 ms after the grid our old client needed 27 s from the launch
    const uint64_t slowestStockAfterGrid = 16000;
    const uint64_t oldCloneFromLaunch = 28761;
    const uint64_t gridAfterLaunch = 5000;
    EXPECT_GT(SceneLoadWait::kWaitMs, slowestStockAfterGrid);
    EXPECT_GT(SceneLoadWait::kWaitMs, oldCloneFromLaunch - gridAfterLaunch);
    EXPECT_GT(SceneLoadWait::kWaitMs, 3000u);
    SceneLoadWait wait;
    wait.arm({9}, 5000);
    EXPECT_FALSE(wait.expired(5000 + 3000));
    EXPECT_FALSE(wait.expired(5000 + 28761 - 5000));
    EXPECT_TRUE(wait.expired(5000 + SceneLoadWait::kWaitMs));
}

// a racer who leaves before the GO owes nothing and the others go at once
TEST(SceneLoadWait, ALeaverOwesNothing) {
    SceneLoadWait wait;
    wait.arm({9, 14}, 0);
    EXPECT_FALSE(wait.clear(14));
    EXPECT_TRUE(wait.owes(9));
    EXPECT_TRUE(wait.clear(9));
    EXPECT_FALSE(wait.owes(9));
}

// a room of CPU cars alone arms nobody and goes at once
TEST(SceneLoadWait, NoHumanNoWait) {
    SceneLoadWait wait;
    wait.arm({}, 0);
    EXPECT_TRUE(wait.owed.empty());
}

// the stock Invite item sends 0x006C from the lobby the server used to drop it with no word
TEST(RoomInviteRefusal, FromTheLobbyTheInviterIsTold) {
    SocialHandler::RoomInviteCheck check;
    check.senderInRoom = false;
    check.targetOnline = true;
    const char* key = SocialHandler::roomInviteRefusal(check);
    ASSERT_NE(key, nullptr);
    EXPECT_STREQ(key, "MSG_UNSUPPORT");
}

// the other refusals keep their stock lines and a good invite goes out
TEST(RoomInviteRefusal, EveryOtherCase) {
    SocialHandler::RoomInviteCheck good;
    good.senderInRoom = true;
    good.roomAlive = true;
    good.targetOnline = true;
    EXPECT_EQ(SocialHandler::roomInviteRefusal(good), nullptr);

    SocialHandler::RoomInviteCheck gone = good;
    gone.roomAlive = false;
    EXPECT_STREQ(SocialHandler::roomInviteRefusal(gone), "MSG_UNSUPPORT");

    SocialHandler::RoomInviteCheck offline = good;
    offline.targetOnline = false;
    EXPECT_STREQ(SocialHandler::roomInviteRefusal(offline), "MSG_NOT_FIND_USER");

    SocialHandler::RoomInviteCheck blocked = good;
    blocked.blocked = true;
    EXPECT_STREQ(SocialHandler::roomInviteRefusal(blocked), "MSG_NOT_FIND_USER");

    SocialHandler::RoomInviteCheck same = good;
    same.sameRoom = true;
    EXPECT_STREQ(SocialHandler::roomInviteRefusal(same), "MSG_REJECT_SAME_ROOM");

    SocialHandler::RoomInviteCheck self = good;
    self.targetIsSender = true;
    EXPECT_STREQ(SocialHandler::roomInviteRefusal(self), "MSG_UNSUPPORT");
}

// the refusal rides S2C 0x0126 type 5 the only type sub 47EC00 draws and the client parser reads it back
TEST(RoomInviteRefusal, TheLineReachesTheClientParser) {
    const std::u16string name = u"HlTestTwo";
    Packet pkt = SocialPackets::systemChatLine(name, "MSG_UNSUPPORT");
    EXPECT_EQ(pkt.cmdFull(), 0x0126);
    const SystemLine line = readSystemLine(pkt);
    EXPECT_EQ(line.unused, 0u);
    EXPECT_EQ(line.name, name);
    EXPECT_EQ(line.key, "MSG_UNSUPPORT");
    EXPECT_EQ(line.type, 5u);
}
