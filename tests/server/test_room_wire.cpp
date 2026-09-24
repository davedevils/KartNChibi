/// the room wire 0x13 carries the track id in the unread dword and 0x21 speaks 0 red 1 blue

#include <gtest/gtest.h>
#include <asio.hpp>
#include <cstring>
#include <memory>
#include "GameServerInternal.h"
#include "game/Room.h"
#include "net/Session.h"
#include "packets/PacketBuilder.h"

using namespace knc;

namespace {

uint32_t dwordAt(const std::vector<uint8_t>& p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p.data() + at, 4);
    return v;
}

// a session that never connects the room only reads its id
std::shared_ptr<Session> fakeSession(asio::io_context& io, uint32_t characterId) {
    auto s = std::make_shared<Session>(asio::ip::tcp::socket(io));
    s->characterId = characterId;
    return s;
}

} // namespace

TEST(RoomWire, RoomContextCarriesTrackIdAndMaster) {
    RoomData rd;
    rd.id = 7;
    rd.name = "ab";
    rd.maxPlayers = 8;
    rd.mode = 1;
    rd.mapId = 80;
    rd.hostId = 4242;
    rd.trackId = 90;

    const Packet pkt = PacketBuilder::playerRoomData(rd, {});
    const std::vector<uint8_t>& p = pkt.payload();
    // room id then the wide name with its terminator then eight dwords
    const size_t base = 4 + 2 * (rd.name.size() + 1);
    ASSERT_EQ(p.size(), base + 8 * 4);
    EXPECT_EQ(dwordAt(p, 0), 7u);
    EXPECT_EQ(dwordAt(p, base + 0), 8u);      // BCE22C seats BCE210 mode
    EXPECT_EQ(dwordAt(p, base + 4), 1u);
    EXPECT_EQ(dwordAt(p, base + 12), 90u);    // BCE214 track id never the map id BCE220 master
    EXPECT_EQ(dwordAt(p, base + 24), 4242u);
}

TEST(RoomWire, MemberTeamIsTheWireSpace) {
    EXPECT_EQ(wireTeam(0), 0u);
    EXPECT_EQ(wireTeam(1), 0u);
    EXPECT_EQ(wireTeam(2), 1u);

    RoomMemberWire m;
    m.slot = 3;
    m.team = wireTeam(2);
    m.playerId = 99;
    m.displayName = u"x";
    const Packet pkt = PacketBuilder::roomMember(m);
    const std::vector<uint8_t>& p = pkt.payload();
    ASSERT_EQ(p.size(), 187u + 2 * 2);
    EXPECT_EQ(dwordAt(p, 0), 3u);
    EXPECT_EQ(dwordAt(p, 4), 1u);   // blue on the wire
    EXPECT_EQ(dwordAt(p, 8), 99u);
}

TEST(RoomWire, TeamModeSeatsJoinersOnBothSides) {
    asio::io_context io;
    RoomSettings st;
    st.mode = GameMode::ItemTeam;
    st.maxPlayers = 8;
    Room room(1, st);
    ASSERT_TRUE(room.isTeamMode());

    auto a = fakeSession(io, 1);
    auto b = fakeSession(io, 2);
    auto c = fakeSession(io, 3);
    ASSERT_TRUE(room.addPlayer(a, 1, u"a", 0));
    ASSERT_TRUE(room.addPlayer(b, 2, u"b", 0));
    ASSERT_TRUE(room.addPlayer(c, 3, u"c", 0));

    EXPECT_EQ(static_cast<int>(room.getPlayer(a->id())->team), 1);   // red then blue then red again the tie goes red
    EXPECT_EQ(static_cast<int>(room.getPlayer(b->id())->team), 2);
    EXPECT_EQ(static_cast<int>(room.getPlayer(c->id())->team), 1);
    EXPECT_EQ(room.addBots(1), 1);
    EXPECT_EQ(static_cast<int>(room.bots().front().team), 2);

    RoomSettings solo;
    solo.mode = GameMode::ItemSingle;
    Room single(2, solo);
    auto d = fakeSession(io, 4);
    ASSERT_TRUE(single.addPlayer(d, 4, u"d", 0));
    EXPECT_EQ(static_cast<int>(single.getPlayer(d->id())->team), 0);
}
