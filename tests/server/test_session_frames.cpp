/// two C2S frames in one TCP write the parser dispatches both the loss was the per opcode flood floor

#include <gtest/gtest.h>
#include <asio.hpp>
#include <chrono>
#include <memory>
#include <vector>
#include "net/Session.h"
#include "net/Packet.h"
#include "net/Protocol.h"
#include "security/RateLimiter.h"

using namespace knc;
using asio::ip::tcp;

namespace {

// one 0xB9 EquipUse frame category instance id then the minus one target
Packet equipUse(uint32_t category, uint32_t instanceId) {
    Packet p(CMD::C_GARAGE_INSTALL);
    p.writeUInt32(category);
    p.writeUInt32(instanceId);
    p.writeInt32(-1);
    return p;
}

} // namespace

TEST(SessionFrames, TwoLegacyFramesInOneWriteDispatchTwice) {
    asio::io_context io;
    tcp::acceptor acceptor(io, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    tcp::socket serverSide(io);
    tcp::socket clientSide(io);
    bool accepted = false;
    acceptor.async_accept(serverSide, [&accepted](std::error_code ec) { accepted = !ec; });
    clientSide.connect(acceptor.local_endpoint());
    while (!accepted) io.run_one();

    auto session = std::make_shared<Session>(std::move(serverSide));
    std::vector<uint32_t> seenInstances;
    session->setPacketHandler([&seenInstances](Session::Ptr, Packet& p) {
        EXPECT_EQ(p.opcode(), CMD::C_GARAGE_INSTALL);
        p.readUInt32();
        seenInstances.push_back(p.readUInt32());
    });
    session->start();

    // the character select and the kart select of one garage click land in one write
    std::vector<uint8_t> wire = equipUse(0, 11).serialize();
    const std::vector<uint8_t> second = equipUse(1, 12).serialize();
    wire.insert(wire.end(), second.begin(), second.end());
    asio::write(clientSide, asio::buffer(wire));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    io.restart();
    while (seenInstances.size() < 2 && std::chrono::steady_clock::now() < deadline) {
        io.run_for(std::chrono::milliseconds(20));
    }

    ASSERT_EQ(seenInstances.size(), 2u);
    EXPECT_EQ(seenInstances[0], 11u);
    EXPECT_EQ(seenInstances[1], 12u);

    // drain the aborted read so the session dies while the io context is still alive
    session->stop();
    io.restart();
    io.run();
    session.reset();
}

TEST(SessionFrames, DefaultFloorDropsTheSecondGarageInstall) {
    // the game server floor is 50 ms two frames of one write are microseconds apart
    RateLimiter::Config cfg;
    cfg.defaultMinIntervalMs = RateLimit::DEFAULT_MIN_INTERVAL;
    RateLimiter limiter(cfg);
    EXPECT_TRUE(limiter.check(CMD::C_GARAGE_INSTALL));
    EXPECT_FALSE(limiter.check(CMD::C_GARAGE_INSTALL));
    EXPECT_EQ(limiter.getDroppedCount(), 1);
}

TEST(SessionFrames, ZeroFloorKeepsBothGarageInstalls) {
    // what makeRateLimiterConfig sets for 0xB9 and 0xBA now
    RateLimiter::Config cfg;
    cfg.defaultMinIntervalMs = RateLimit::DEFAULT_MIN_INTERVAL;
    cfg.opcodeMinIntervalMs[CMD::C_GARAGE_INSTALL] = 0;
    cfg.opcodeMinIntervalMs[CMD::C_GARAGE_REMOVE] = 0;
    RateLimiter limiter(cfg);
    EXPECT_TRUE(limiter.check(CMD::C_GARAGE_INSTALL));
    EXPECT_TRUE(limiter.check(CMD::C_GARAGE_INSTALL));
    EXPECT_TRUE(limiter.check(CMD::C_GARAGE_REMOVE));
    EXPECT_TRUE(limiter.check(CMD::C_GARAGE_REMOVE));
    EXPECT_EQ(limiter.getDroppedCount(), 0);
}
