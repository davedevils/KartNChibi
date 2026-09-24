/// the owned kart record a reward carries and the two frames of the channel return

#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "net/Packet.h"
#include "packets/gen/CharCreatePackets.h"
#include "packets/gen/InventoryPackets.h"
#include "packets/gen/MissionPackets.h"
#include "packets/PacketBuilder.h"

using namespace knc;

namespace {

uint32_t u32At(const std::vector<uint8_t>& p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p.data() + at, 4);
    return v;
}

template <size_t N>
uint32_t u32At(const std::array<uint8_t, N>& p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p.data() + at, 4);
    return v;
}

} // namespace

// 0x08 and 0x0C are the paint and the plate the durability lives at 0x30 under mode 3
TEST(KartRewardRecord, DurabilityIsNotThePaintSlot) {
    const MissionPackets::RewardBlob r =
        MissionPackets::kartReward(4242, 10010, 9007, 9100, 0, 3, 250);

    EXPECT_EQ(r.type, 1u);
    ASSERT_EQ(r.bytes.size(), 0x38u);
    EXPECT_TRUE(MissionPackets::validateRewardBlob(r));

    EXPECT_EQ(u32At(r.bytes, 0x00), 4242u);   // owned instance id 0x04 is the 0xC0 catalogue key
    EXPECT_EQ(u32At(r.bytes, 0x04), 10010u);
    EXPECT_EQ(u32At(r.bytes, 0x08), 9007u);   // paint key sub 4510C0 resolves it 0x0C is name plate key
    EXPECT_EQ(u32At(r.bytes, 0x0C), 9100u);
    EXPECT_EQ(u32At(r.bytes, 0x10), 0u);      // kart item slot 8 0x2C is period mode 3 durability
    EXPECT_EQ(u32At(r.bytes, 0x2C), 3u);
    EXPECT_EQ(u32At(r.bytes, 0x30), 250u);    // durability itself 0x34 zero means a reward lands unequipped
    EXPECT_EQ(u32At(r.bytes, 0x34), 0u);
}

// every writer of the record must agree with the one the inventory ships
TEST(KartRewardRecord, TheRewardMatchesTheInventoryBlobSlotForSlot) {
    InventoryPackets::KartRow row;
    row.instanceId    = 4242;
    row.baseKey       = 10010;
    row.skinPrimary   = 9007;
    row.skinSecondary = 9100;
    row.periodMode    = 3;
    row.periodValue   = 250;
    const std::array<uint8_t, 0x38> blob = InventoryPackets::kartBlob(row);

    const MissionPackets::RewardBlob r =
        MissionPackets::kartReward(4242, 10010, 9007, 9100, 0, 3, 250);

    for (size_t off : {size_t(0x00), size_t(0x04), size_t(0x08), size_t(0x0C),
                       size_t(0x10), size_t(0x2C), size_t(0x30), size_t(0x34)}) {
        EXPECT_EQ(u32At(r.bytes, off), u32At(blob, off)) << "offset " << off;
    }
}

// the helper the inventory builds on must not put a stat block where the skins go
TEST(KartRewardRecord, TheBuilderHelperCarriesTheSkinKeys) {
    VehicleInfo v;
    v.id = 77;
    v.templateId = 10010;
    v.durability = 500;
    v.equipped = true;
    const std::array<uint8_t, 0x38> rec = PacketBuilder::kartRecord(v);

    EXPECT_EQ(u32At(rec, 0x00), 77u);
    EXPECT_EQ(u32At(rec, 0x04), 10010u);
    EXPECT_NE(u32At(rec, 0x08), 0u);        // a zero paint or plate key hides the kart
    EXPECT_NE(u32At(rec, 0x0C), 0u);
    EXPECT_EQ(u32At(rec, 0x2C), 3u);
    EXPECT_EQ(u32At(rec, 0x30), 500u);
    EXPECT_EQ(u32At(rec, 0x34), 1u);
}

// S2C 0x0019 ascii host then port then mode the client reads the host first
TEST(ChannelReturnWire, RedirectIsAsciiHostThenPortThenMode) {
    const Packet pkt = CharCreatePackets::serverRedirect("127.0.0.1", 50017, 4);
    EXPECT_EQ(pkt.opcode(), 0x0019u);

    const std::vector<uint8_t>& p = pkt.payload();
    ASSERT_EQ(p.size(), std::string("127.0.0.1").size() + 1 + 8);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(p.data())), "127.0.0.1");
    EXPECT_EQ(u32At(p, 10), 50017u);
    EXPECT_EQ(u32At(p, 14), 4u);
}

// C2S 0x00A7 sub 480500 writes the version the stage then the ticket token and the account
TEST(ChannelReturnWire, ReauthCarriesTheTicketNotTheCredentials) {
    Packet pkt = Packet::fromCmdFull(0x00A7);
    pkt.writeString("178");
    pkt.writeUInt32(4);
    pkt.writeWString(u"abcdef0123456789abcdef0123456789");
    pkt.writeUInt32(9001);

    const CharCreatePackets::ReauthRequest req = CharCreatePackets::parseReauth(pkt);
    ASSERT_TRUE(req.ok);
    EXPECT_EQ(req.clientVersion, "178");
    // stage 4 is the one the channel return forwards from the S2C 0x0019 mode
    EXPECT_EQ(req.targetStage, CharCreatePackets::TARGET_STAGE_LOGIN);
    EXPECT_EQ(req.reauthToken, u"abcdef0123456789abcdef0123456789");
    EXPECT_EQ(req.reauthSessionId, 9001u);
}

// S2C 0x12F a password room sets the flag and ships the password the popup accept joins with
TEST(RandomInviteWire, ThePopupCarriesTheRoomAndItsPassword) {
    const Packet open = PacketBuilder::invitePopupShort(1, u"Host", 12, u"");
    ASSERT_EQ(open.payload().size(), 4u + 2 * (4 + 1) + 4 + 4);
    EXPECT_EQ(u32At(open.payload(), 0), 1u);
    EXPECT_EQ(u32At(open.payload(), 4 + 2 * 5), 12u);
    EXPECT_EQ(u32At(open.payload(), 4 + 2 * 5 + 4), 0u);   // empty password keeps the flag 0

    const Packet locked = PacketBuilder::invitePopupShort(1, u"Host", 12, u"1234");
    ASSERT_EQ(locked.payload().size(), 4u + 2 * (4 + 1) + 4 + 4 + 2 * (4 + 1));
    EXPECT_EQ(u32At(locked.payload(), 4 + 2 * 5 + 4), 1u);
}
