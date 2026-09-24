
#include <gtest/gtest.h>
#include "net/Protocol.h"

using namespace knc;

TEST(Protocol, HeaderSize) {
    EXPECT_EQ(sizeof(PacketHeader), PACKET_HEADER_SIZE);
    EXPECT_EQ(sizeof(PacketHeader), 8);
}

TEST(Protocol, StructureSizes) {
    EXPECT_EQ(sizeof(VehicleData), 44);
    EXPECT_EQ(sizeof(ItemData), 56);
    EXPECT_EQ(sizeof(AccessoryData), 28);
    EXPECT_EQ(sizeof(SmallItem), 32);
}

TEST(Protocol, CMDValues) {
    EXPECT_EQ(CMD::C_HEARTBEAT, 0xA6);
    EXPECT_EQ(CMD::S_CONNECTION_OK, 0x0A);
    EXPECT_EQ(CMD::S_SERVER_REDIRECT, 0x54);
    EXPECT_EQ(CMD::S_SHOW_MENU, 0x11);   // 0x11 pins the value over the old 0x0E guess 0x73 is shop poll no disconnect opcode
    EXPECT_EQ(CMD::C_SHOP_POLL, 0x73);
}

TEST(Protocol, RateLimits) {
    EXPECT_GT(RateLimit::CHAT_MIN_INTERVAL, 0);
    // the servers use this cap so it must clear 0x40 at 10 Hz plus 0x67 once per frame
    EXPECT_GE(RateLimit::GLOBAL_MAX_PACKETS_SEC, 400);
    EXPECT_GE(RateLimit::LOGIN_MIN_INTERVAL, 1000);
}

