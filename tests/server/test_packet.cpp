/// packet class tests cross validated against IDA

#include <gtest/gtest.h>
#include "net/Packet.h"
#include "net/Protocol.h"

using namespace knc;

TEST(Packet, HeaderConstant) {
    EXPECT_EQ(PACKET_HEADER_SIZE, 8);
}

TEST(Packet, MaxSize) {
    EXPECT_EQ(PACKET_MAX_SIZE, 65535);
}

TEST(Packet, DefaultConstruction) {
    Packet pkt(CMD::S_ACK);
    EXPECT_EQ(pkt.cmd(), CMD::S_ACK);
    EXPECT_EQ(pkt.flag(), 0);
    EXPECT_EQ(pkt.payloadSize(), 0);
}

TEST(Packet, ConstructionWithFlag) {
    Packet pkt(CMD::S_SHOW_LOBBY, 0x01);
    EXPECT_EQ(pkt.cmd(), CMD::S_SHOW_LOBBY);
    EXPECT_EQ(pkt.flag(), 0x01);
}

TEST(Packet, FromCmdFull) {
    // CMD 256 is 0x100 which splits into cmd 0 and flag 1
    auto pkt = Packet::fromCmdFull(0x100);
    EXPECT_EQ(pkt.cmdFull(), 0x100);
    EXPECT_EQ(pkt.cmd(), 0x00);
    EXPECT_EQ(pkt.flag(), 0x01);
}

TEST(Packet, WriteUInt8) {
    Packet pkt(CMD::S_ACK);
    pkt.writeUInt8(0xFF);
    EXPECT_EQ(pkt.payloadSize(), 1);
}

TEST(Packet, WriteUInt16) {
    Packet pkt(CMD::S_ACK);
    pkt.writeUInt16(0x1234);
    EXPECT_EQ(pkt.payloadSize(), 2);
}

TEST(Packet, WriteUInt32) {
    Packet pkt(CMD::S_ACK);
    pkt.writeUInt32(0x12345678);
    EXPECT_EQ(pkt.payloadSize(), 4);
}

TEST(Packet, WriteInt32) {
    Packet pkt(CMD::S_ACK);
    pkt.writeInt32(-1);
    EXPECT_EQ(pkt.payloadSize(), 4);
}

TEST(Packet, WriteFloat) {
    Packet pkt(CMD::S_ACK);
    pkt.writeFloat(3.14159f);
    EXPECT_EQ(pkt.payloadSize(), 4);
}

TEST(Packet, ReadUInt8) {
    Packet pkt(CMD::S_ACK);
    pkt.writeUInt8(0xAB);
    pkt.resetReadPos();
    EXPECT_EQ(pkt.readUInt8(), 0xAB);
}

TEST(Packet, ReadUInt16) {
    Packet pkt(CMD::S_ACK);
    pkt.writeUInt16(0xABCD);
    pkt.resetReadPos();
    EXPECT_EQ(pkt.readUInt16(), 0xABCD);
}

TEST(Packet, ReadUInt32) {
    Packet pkt(CMD::S_ACK);
    pkt.writeUInt32(0xDEADBEEF);
    pkt.resetReadPos();
    EXPECT_EQ(pkt.readUInt32(), 0xDEADBEEF);
}

TEST(Packet, ReadInt32Negative) {
    Packet pkt(CMD::S_ACK);
    pkt.writeInt32(-12345);
    pkt.resetReadPos();
    EXPECT_EQ(pkt.readInt32(), -12345);
}

TEST(Packet, ReadFloat) {
    Packet pkt(CMD::S_ACK);
    pkt.writeFloat(3.14159f);
    pkt.resetReadPos();
    EXPECT_NEAR(pkt.readFloat(), 3.14159f, 0.0001f);
}

TEST(Packet, WriteReadWString) {
    Packet pkt(CMD::S_ACK);
    std::u16string testStr = u"Hello";
    pkt.writeWString(testStr);
    pkt.resetReadPos();
    
    std::u16string result = pkt.readWString(10);
    EXPECT_EQ(result.substr(0, 5), testStr);
}

TEST(Packet, SerializeHeader) {
    Packet pkt(CMD::S_ACK);
    auto data = pkt.serialize();
    
    EXPECT_GE(data.size(), PACKET_HEADER_SIZE);
    
    // size is little-endian uint16 at offset 0
    uint16_t size = data[0] | (data[1] << 8);
    EXPECT_EQ(size, 0);   // wire size field is the payload length see Packet peekSize
}

TEST(Packet, SerializeWithPayload) {
    Packet pkt(CMD::S_ACK);
    pkt.writeInt32(0x12345678);
    auto data = pkt.serialize();
    
    uint16_t size = data[0] | (data[1] << 8);
    EXPECT_EQ(size, 4);   // payload length only the header is not counted
}

TEST(Protocol, CMDConstants) {
    EXPECT_EQ(CMD::S_LOGIN_RESPONSE, 0x01);
    EXPECT_EQ(CMD::S_DISPLAY_MESSAGE, 0x02);
    EXPECT_EQ(CMD::S_SET_GAME_VAR, 0x03);
    EXPECT_EQ(CMD::S_TRIGGER, 0x03);  // legacy alias
    EXPECT_EQ(CMD::S_ACK, 0x0B);
    EXPECT_EQ(CMD::S_SHOW_LOBBY, 0x12);
    EXPECT_EQ(CMD::S_UI_STATE_14, 0x16);
    
    EXPECT_EQ(CMD::C_CLIENT_AUTH, 0x07);
    EXPECT_EQ(CMD::C_HEARTBEAT, 0xA6);
    EXPECT_EQ(CMD::C_CHAT_MESSAGE, 0x2D);
}

// Protocol StructureSizes lives in test protocol cpp and already covers SmallItem

