/// the dispatch gates the login token rule the profile blob wallet and the sub 40C950 start rule

#include <gtest/gtest.h>
#include <asio.hpp>

#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "game/Room.h"
#include "net/Packet.h"
#include "net/ProfileBlob.h"
#include "net/Protocol.h"
#include "net/Session.h"
#include "packets/PacketBuilder.h"
#include "security/LoginToken.h"
#include "security/PacketValidator.h"
#include "security/RateLimiter.h"
#include "util/DevCommands.h"
#include "util/RetiredRoutes.h"

using namespace knc;
using asio::ip::tcp;

namespace {

uint32_t u32At(const std::vector<uint8_t>& p, size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, p.data() + at, 4);
    return v;
}

std::shared_ptr<Session> fakeSession(asio::io_context& io, uint32_t characterId) {
    auto s = std::make_shared<Session>(tcp::socket(io));
    s->characterId = characterId;
    return s;
}

// a connected pair the server side wrapped in a Session the test drives the client side
struct Loopback {
    asio::io_context io;
    tcp::acceptor acceptor{io, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)};
    tcp::socket serverSide{io};
    tcp::socket clientSide{io};

    Loopback() {
        bool accepted = false;
        acceptor.async_accept(serverSide, [&accepted](std::error_code ec) { accepted = !ec; });
        clientSide.connect(acceptor.local_endpoint());
        while (!accepted) io.run_one();
    }

    template <typename Done>
    void runUntil(Done done, std::chrono::milliseconds limit) {
        const auto deadline = std::chrono::steady_clock::now() + limit;
        io.restart();
        while (!done() && std::chrono::steady_clock::now() < deadline) {
            io.run_for(std::chrono::milliseconds(20));
        }
    }
};

void drain(Loopback& lb, std::shared_ptr<Session>& session) {
    session->stop();
    lb.io.restart();
    lb.io.run_for(std::chrono::milliseconds(50));
    session.reset();
}

} // namespace

// the low byte check dropped every 0x01xx frame whose low byte is zero the quest discard was one
TEST(PacketValidatorGate, TheQuestDiscardPassesAndOpcodeZeroDoesNot) {
    EXPECT_EQ(PacketValidator::validate(Packet::fromCmdFull(0x0100)), ValidationResult::OK);
    EXPECT_EQ(PacketValidator::validate(Packet::fromCmdFull(0x0000)), ValidationResult::INVALID_CMD);
}

TEST(PacketValidatorGate, APayloadPastTheClientFrameCapIsRefused) {
    Packet chunk = Packet::fromCmdFull(0x00AF);
    std::vector<uint8_t> body(4 + 136 * 28, 0);
    chunk.writeBytes(body.data(), body.size());
    EXPECT_EQ(PacketValidator::validate(chunk), ValidationResult::OK);

    Packet huge = Packet::fromCmdFull(0x00B4);
    std::vector<uint8_t> big(PACKET_MAX_C2S_PAYLOAD + 1, 0x41);
    huge.writeBytes(big.data(), big.size());
    EXPECT_EQ(PacketValidator::validate(huge), ValidationResult::INVALID_SIZE);
}

// a socket with no account used to reach room create chat and the shop
TEST(PacketValidatorGate, OnlyTheLoginFramesPassBeforeAnAccountIsBound) {
    for (uint16_t op : {0x00A6, 0x0007, 0x00FA, 0x00A7, 0x00D0, 0x000B, 0x0130}) {
        EXPECT_TRUE(PacketValidator::allowedBeforeAuth(op)) << std::hex << op;
    }
    for (uint16_t op : {0x002D, 0x002F, 0x00B4, 0x00B5, 0x00B7, 0x0010, 0x0040, 0x0004, 0x0019}) {
        EXPECT_FALSE(PacketValidator::allowedBeforeAuth(op)) << std::hex << op;
    }
}

TEST(PacketValidatorGate, PasswordTokenAndKeyFramesAreNeverDumped) {
    for (uint16_t op : {0x0007, 0x00A7, 0x00FE, 0x00D0, 0x00F0}) {
        EXPECT_TRUE(PacketValidator::payloadIsSecret(op)) << std::hex << op;
    }
    EXPECT_FALSE(PacketValidator::payloadIsSecret(0x0040));
    EXPECT_FALSE(PacketValidator::payloadIsSecret(0x00B4));
}

// keyed on the low byte 0x0121 and 0x21 shared one interval and dropped each other
TEST(RateLimiterWide, SixteenBitOpcodesKeepTheirOwnInterval) {
    RateLimiter::Config cfg;
    cfg.defaultMinIntervalMs = 50;
    RateLimiter limiter(cfg);
    EXPECT_TRUE(limiter.check(0x0121));
    EXPECT_TRUE(limiter.check(0x0021));
    EXPECT_FALSE(limiter.check(0x0121));
    EXPECT_EQ(limiter.getDroppedCount(), 1);
}

// a first frame of 75 bytes starts with 0x4B the old 12 byte codec took it for its magic
TEST(SessionGate, AFirstFrameWhoseSizeLowByteIsKStaysAnEightByteFrame) {
    Loopback lb;
    auto session = std::make_shared<Session>(std::move(lb.serverSide));
    std::vector<std::pair<uint16_t, size_t>> seen;
    session->setPacketHandler([&seen](Session::Ptr, Packet& p) {
        seen.emplace_back(p.opcode(), p.payload().size());
    });
    session->start();

    Packet reauth = Packet::fromCmdFull(0x00A7);
    std::vector<uint8_t> body(75, 0x31);
    reauth.writeBytes(body.data(), body.size());
    const std::vector<uint8_t> wire = reauth.serialize();
    ASSERT_EQ(wire[0], 0x4B);
    asio::write(lb.clientSide, asio::buffer(wire));

    lb.runUntil([&seen] { return !seen.empty(); }, std::chrono::seconds(3));
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0].first, 0x00A7);
    EXPECT_EQ(seen[0].second, 75u);
    drain(lb, session);
}

// a header past the client frame cap is a forged frame the socket closes before the body arrives
TEST(SessionGate, AnOversizedHeaderClosesTheSocket) {
    Loopback lb;
    auto session = std::make_shared<Session>(std::move(lb.serverSide));
    bool dispatched = false;
    bool closed = false;
    session->setPacketHandler([&dispatched](Session::Ptr, Packet&) { dispatched = true; });
    session->setDisconnectHandler([&closed](Session::Ptr) { closed = true; });
    session->start();

    const uint16_t claimed = static_cast<uint16_t>(PACKET_MAX_C2S_PAYLOAD + 1);
    const uint8_t header[8] = { static_cast<uint8_t>(claimed & 0xFF), static_cast<uint8_t>(claimed >> 8),
                                0xB4, 0x00, 0, 0, 0, 0 };
    asio::write(lb.clientSide, asio::buffer(header, sizeof(header)));

    lb.runUntil([&closed] { return closed; }, std::chrono::seconds(3));
    EXPECT_TRUE(closed);
    EXPECT_FALSE(dispatched);
    drain(lb, session);
}

TEST(SessionGate, ASilentSocketIsClosedAfterTheIdleLimit) {
    Loopback lb;
    auto session = std::make_shared<Session>(std::move(lb.serverSide));
    bool closed = false;
    session->setDisconnectHandler([&closed](Session::Ptr) { closed = true; });
    session->setIdleTimeout(std::chrono::milliseconds(150));
    session->start();

    lb.runUntil([&closed] { return closed; }, std::chrono::seconds(3));
    EXPECT_TRUE(closed);
    drain(lb, session);
}

TEST(SessionGate, AKeepaliveEverySecondPushesTheIdleDeadlineAway) {
    Loopback lb;
    auto session = std::make_shared<Session>(std::move(lb.serverSide));
    bool closed = false;
    int beats = 0;
    session->setPacketHandler([&beats](Session::Ptr, Packet&) { ++beats; });
    session->setDisconnectHandler([&closed](Session::Ptr) { closed = true; });
    session->setIdleTimeout(std::chrono::milliseconds(600));
    session->start();

    Packet beat(CMD::C_HEARTBEAT);
    beat.writeInt32(0);
    const std::vector<uint8_t> wire = beat.serialize();
    for (int i = 0; i < 8; ++i) {
        asio::write(lb.clientSide, asio::buffer(wire));
        lb.runUntil([] { return false; }, std::chrono::milliseconds(150));
    }
    EXPECT_FALSE(closed);
    EXPECT_EQ(beats, 8);
    drain(lb, session);
}

// any 32 letter password logged into any account the launcher store must hold that token now
TEST(LauncherToken, AnyThirtyTwoCharPasswordNoLongerLogsIn) {
    const std::string token(32, 'a');
    EXPECT_TRUE(looksLikeLauncherToken(token));
    EXPECT_FALSE(launcherTokenAccepted("admin", token, nullptr));

    const LauncherTokenCheck refuse = [](const std::string&, const std::string&) { return false; };
    EXPECT_FALSE(launcherTokenAccepted("admin", token, refuse));

    const LauncherTokenCheck store = [](const std::string& user, const std::string& t) {
        return user == "admin" && t == "Zx81Zx81Zx81Zx81Zx81Zx81Zx81Zx81";
    };
    EXPECT_TRUE(launcherTokenAccepted("admin", "Zx81Zx81Zx81Zx81Zx81Zx81Zx81Zx81", store));
    EXPECT_FALSE(launcherTokenAccepted("other", "Zx81Zx81Zx81Zx81Zx81Zx81Zx81Zx81", store));
    EXPECT_FALSE(launcherTokenAccepted("admin", std::string(31, 'a'), store));
    EXPECT_FALSE(looksLikeLauncherToken(std::string(31, 'a') + "!"));
}

// the login server wrote gold where the client reads exp and astro where it reads gold
TEST(ProfileBlobWallet, LoginAndGameAgreeOnExpAstroAndGold) {
    std::vector<uint8_t> login(ProfileBlob::kSize, 0);
    ProfileBlob::writeWallet(login, 7, 1234, 55, 9999);
    EXPECT_EQ(login[ProfileBlob::kLevel], 7u);
    EXPECT_EQ(u32At(login, 0x4A4), 1234u);
    EXPECT_EQ(u32At(login, 0x4A8), 55u);
    EXPECT_EQ(u32At(login, 0x4AC), 9999u);

    PlayerData p;
    p.id = 12;
    p.xp = 1234;
    p.cash = 55;
    p.gold = 9999;
    const Packet game = PacketBuilder::sessionConfirm(12, p);
    ASSERT_EQ(game.payload().size(), 4u + ProfileBlob::kSize);
    for (size_t off : {ProfileBlob::kExpCurrent, ProfileBlob::kAstro, ProfileBlob::kGold}) {
        EXPECT_EQ(u32At(game.payload(), 4 + off), u32At(login, off)) << "offset " << off;
    }
}

// sub 480500 echoes the blob head on the channel return the game 0xA7 wrote the character there
TEST(ProfileBlobWallet, TheGameBlobHeadKeepsTheLoginTicket) {
    PlayerData p;
    p.id = 12;
    p.accountId = 77;
    p.name = "Racer";
    p.ticketToken = "Tk01Tk01Tk01Tk01Tk01Tk01Tk01Tk01";
    const Packet game = PacketBuilder::sessionConfirm(12, p);
    const std::vector<uint8_t>& b = game.payload();
    EXPECT_EQ(u32At(b, 4 + ProfileBlob::kTicketId), 77u);
    for (size_t i = 0; i < p.ticketToken.size(); ++i) {
        EXPECT_EQ(b[4 + ProfileBlob::kTicketText + i * 2], static_cast<uint8_t>(p.ticketToken[i]));
        EXPECT_EQ(b[4 + ProfileBlob::kTicketText + i * 2 + 1], 0u);
    }
    EXPECT_EQ(b[4 + ProfileBlob::kNickname], static_cast<uint8_t>('R'));
}

// the 12 byte 0xAA 0xAC and the 16 byte 0xCB paid gold on every send no stock client sends them
TEST(RetiredRoutes, TheOldGoldPayingShapesAreDropped) {
    EXPECT_TRUE(isRetiredC2S(0x00AA, 12));
    EXPECT_TRUE(isRetiredC2S(0x00AC, 12));
    EXPECT_TRUE(isRetiredC2S(0x00CB, 16));
    for (uint16_t op : {0x001B, 0x001C, 0x001D, 0x0030, 0x0036, 0x00A9, 0x00AB, 0x00C7, 0x00C8, 0x00C9, 0x00CA}) {
        EXPECT_TRUE(isRetiredC2S(op, 4)) << std::hex << op;
    }
}

TEST(RetiredRoutes, TheStockShapesStillPass) {
    EXPECT_FALSE(isRetiredC2S(0x00AA, 4));    // GhostEnter
    EXPECT_FALSE(isRetiredC2S(0x00CB, 29));   // ConsumableUseNotify
    EXPECT_FALSE(isRetiredC2S(0x00CC, 29));   // KartPartUseNotify
    EXPECT_FALSE(isRetiredC2S(0x00CD, 4));    // ability ack
    EXPECT_FALSE(isRetiredC2S(0x00A3, 12));   // licence test pass
    EXPECT_FALSE(isRetiredC2S(0x0130, 4));    // option 11
    EXPECT_FALSE(isRetiredC2S(0x002D, 30));
}

// the chat money commands were on unless the env said 0
TEST(DevCommandsGate, OffUnlessTheOperatorTurnsThemOn) {
    EXPECT_FALSE(devCommandsEnabled(nullptr, 0));
    EXPECT_FALSE(devCommandsEnabled("0", 0));
    EXPECT_FALSE(devCommandsEnabled("", 0));
    EXPECT_FALSE(devCommandsEnabled("yes", 0));
    EXPECT_FALSE(devCommandsEnabled("10", 0));
    EXPECT_TRUE(devCommandsEnabled("1", 0));
    EXPECT_TRUE(devCommandsEnabled(nullptr, 1));
}

// sub 40C950 team rooms need red equal blue and four members bots count as members
TEST(RoomStartRule, TeamRoomsNeedFourBalancedMembers) {
    asio::io_context io;
    RoomSettings st;
    st.mode = GameMode::ItemTeam;
    st.maxPlayers = 8;
    Room room(1, st);
    auto host = fakeSession(io, 101);
    ASSERT_TRUE(room.addPlayer(host, 101, u"Host", 10010));

    room.addBots(1);
    EXPECT_STREQ(room.startRefusal(), "MSG_SMALL_MEMBER_ERROR");

    room.addBots(2);
    ASSERT_EQ(room.participants().size(), 4u);
    EXPECT_EQ(room.startRefusal(), nullptr);
    EXPECT_TRUE(room.canStart());

    const uint8_t mine = room.getPlayer(host->id())->team;
    room.setPlayerTeam(host->id(), mine == 1 ? 2 : 1);
    EXPECT_STREQ(room.startRefusal(), "MSG_NOT_BALLENCE_START");
}

TEST(RoomStartRule, ASoloRoomStillLetsOneTesterRace) {
    asio::io_context io;
    RoomSettings st;
    st.mode = GameMode::SpeedSingle;
    st.maxPlayers = 16;
    Room room(2, st);
    auto host = fakeSession(io, 201);
    ASSERT_TRUE(room.addPlayer(host, 201, u"Solo", 10010));
    EXPECT_EQ(room.startRefusal(), nullptr);
}

TEST(RoomStartRule, AWaitingGuestAndARunningRaceRefuseTheStart) {
    asio::io_context io;
    RoomSettings st;
    st.mode = GameMode::ItemSingle;
    Room room(3, st);
    auto host = fakeSession(io, 301);
    auto guest = fakeSession(io, 302);
    ASSERT_TRUE(room.addPlayer(host, 301, u"Host", 10010));
    ASSERT_TRUE(room.addPlayer(guest, 302, u"Guest", 10010));
    EXPECT_STREQ(room.startRefusal(), "MSG_NOT_AVAILABLE_START");

    room.setPlayerReady(guest->id(), true);
    EXPECT_EQ(room.startRefusal(), nullptr);

    room.setState(RoomState::Racing);
    EXPECT_STREQ(room.startRefusal(), "MSG_NOT_AVAILABLE_START");
}
