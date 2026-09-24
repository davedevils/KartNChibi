/// the accept loop and the session must outlive a kick a throw and a bad frame

#include <gtest/gtest.h>
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "net/ListenerHealth.h"
#include "net/Packet.h"
#include "net/Protocol.h"
#include "net/Session.h"

using namespace knc;
using asio::ip::tcp;

namespace {

/// the same accept loop shape the game server runs one session per socket and always arms again
class LoopHarness {
public:
    LoopHarness()
        : m_acceptor(m_io, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)) {
        startAccept();
    }

    uint16_t port() const { return m_acceptor.local_endpoint().port(); }
    asio::io_context& io() { return m_io; }
    ListenerHealth& health() { return m_health; }

    int accepted() const { return m_accepted.load(); }
    int disconnected() const { return m_disconnected.load(); }

    void onPacket(Session::PacketHandler h) { m_onPacket = std::move(h); }
    void onClosed(Session::DisconnectHandler h) { m_onClosed = std::move(h); }

    /// drives the loop until the predicate holds or the deadline passes
    template <typename Pred>
    bool pump(Pred done, int budgetMs = 4000) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budgetMs);
        while (!done() && std::chrono::steady_clock::now() < deadline) {
            m_io.restart();
            m_io.run_for(std::chrono::milliseconds(10));
        }
        return done();
    }

    /// a client socket already connected to this loop
    tcp::socket connectOne() {
        tcp::socket s(m_io);
        s.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port()));
        return s;
    }

private:
    void startAccept() {
        m_acceptor.async_accept([this](std::error_code ec, tcp::socket socket) {
            m_health.touch();
            if (ec) {
                if (acceptErrorIsFatal(ec)) return;
                startAccept();
                return;
            }
            try {
                greet(std::move(socket));
            } catch (...) {
                // a throw in the greeting must never end the loop
            }
            startAccept();
        });
    }

    void greet(tcp::socket socket) {
        auto session = std::make_shared<Session>(std::move(socket));
        m_accepted.fetch_add(1);

        session->setPacketHandler([this](Session::Ptr s, Packet& p) {
            if (m_onPacket) m_onPacket(s, p);
        });
        session->setDisconnectHandler([this](Session::Ptr s) {
            m_disconnected.fetch_add(1);
            if (m_onClosed) m_onClosed(s);
            std::lock_guard<std::mutex> lock(m_liveMutex);
            m_live.erase(s->id());
        });

        {
            std::lock_guard<std::mutex> lock(m_liveMutex);
            m_live[session->id()] = session;
        }
        session->start();
    }

    asio::io_context m_io;
    tcp::acceptor m_acceptor;
    ListenerHealth m_health;
    std::atomic<int> m_accepted{0};
    std::atomic<int> m_disconnected{0};
    Session::PacketHandler m_onPacket;
    Session::DisconnectHandler m_onClosed;
    std::mutex m_liveMutex;
    std::map<uint32_t, Session::Ptr> m_live;
};

Packet heartbeat() {
    Packet p(CMD::C_HEARTBEAT);
    p.writeInt32(1);
    return p;
}

std::error_code genericCode(std::errc e) {
    return std::make_error_code(e);
}

}  // namespace

TEST(AcceptResilience, TransientErrorsNeverEndTheLoop) {
    EXPECT_FALSE(acceptErrorIsFatal(genericCode(std::errc::interrupted)));
    EXPECT_FALSE(acceptErrorIsFatal(genericCode(std::errc::connection_aborted)));
    EXPECT_FALSE(acceptErrorIsFatal(genericCode(std::errc::connection_reset)));
    EXPECT_FALSE(acceptErrorIsFatal(genericCode(std::errc::too_many_files_open)));
    EXPECT_FALSE(acceptErrorIsFatal(genericCode(std::errc::too_many_files_open_in_system)));
}

TEST(AcceptResilience, DescriptorShortagePausesBeforeTheNextTry) {
    // EINTR and a reset peer arm again at once
    EXPECT_EQ(acceptRetryDelayMs(genericCode(std::errc::interrupted)), 0);
    EXPECT_EQ(acceptRetryDelayMs(genericCode(std::errc::connection_aborted)), 0);
    // EMFILE and ENFILE would spin hot without a pause
    EXPECT_GT(acceptRetryDelayMs(genericCode(std::errc::too_many_files_open)), 0);
    EXPECT_GT(acceptRetryDelayMs(genericCode(std::errc::too_many_files_open_in_system)), 0);
    EXPECT_GT(acceptRetryDelayMs(genericCode(std::errc::not_enough_memory)), 0);
}

TEST(AcceptResilience, AClosedAcceptorIsTheOnlyFatalCase) {
    EXPECT_TRUE(acceptErrorIsFatal(genericCode(std::errc::bad_file_descriptor)));
    EXPECT_TRUE(acceptErrorIsFatal(genericCode(std::errc::operation_canceled)));
    EXPECT_FALSE(acceptErrorIsFatal(std::error_code()));
}

TEST(ListenerHealthStamp, AFreshStampIsAliveAndAnOldOneIsStale) {
    ListenerHealth h;
    h.touch();
    EXPECT_FALSE(h.stale(1000));
    EXPECT_LT(h.ageMs(), 1000);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_TRUE(h.stale(10));
    h.touch();
    EXPECT_FALSE(h.stale(1000));
}

TEST(ListenerHealthStamp, TheFileIsWrittenOnlyWhenAPathIsSet) {
    ListenerHealth h;
    EXPECT_FALSE(h.flush());   // no path means no file

    const std::string path = std::string("knc_liveness_test.tmp");
    h.setFile(path);
    ASSERT_TRUE(h.flush());

    std::FILE* f = std::fopen(path.c_str(), "rb");
    ASSERT_NE(f, nullptr);
    char buf[64] = {0};
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    EXPECT_GT(n, 0u);
    std::remove(path.c_str());
}

TEST(LoopSurvives, ManyConnectAndDisconnectCycles) {
    LoopHarness h;
    const int kCycles = 40;

    for (int i = 0; i < kCycles; ++i) {
        tcp::socket client = h.connectOne();
        const int want = i + 1;
        ASSERT_TRUE(h.pump([&] { return h.accepted() >= want; })) << "loop stopped at cycle " << i;

        const auto frame = heartbeat().serialize();
        asio::write(client, asio::buffer(frame));
        std::error_code ec;
        client.shutdown(tcp::socket::shutdown_both, ec);
        client.close(ec);
        ASSERT_TRUE(h.pump([&] { return h.disconnected() >= want; })) << "no close at cycle " << i;
    }

    EXPECT_EQ(h.accepted(), kCycles);
    EXPECT_EQ(h.disconnected(), kCycles);
}

TEST(LoopSurvives, AClientThatDropsMidPacket) {
    LoopHarness h;
    std::atomic<int> dispatched{0};
    h.onPacket([&dispatched](Session::Ptr, Packet&) { dispatched.fetch_add(1); });

    tcp::socket client = h.connectOne();
    ASSERT_TRUE(h.pump([&] { return h.accepted() >= 1; }));

    // half a header then the socket goes away the parser must wait not crash
    const auto frame = heartbeat().serialize();
    asio::write(client, asio::buffer(frame.data(), 5));
    std::error_code ec;
    client.shutdown(tcp::socket::shutdown_both, ec);
    client.close(ec);

    ASSERT_TRUE(h.pump([&] { return h.disconnected() >= 1; }));
    EXPECT_EQ(dispatched.load(), 0);

    // the loop still takes the next client
    tcp::socket second = h.connectOne();
    ASSERT_TRUE(h.pump([&] { return h.accepted() >= 2; }));
    asio::write(second, asio::buffer(frame));
    ASSERT_TRUE(h.pump([&] { return dispatched.load() >= 1; }));
}

TEST(LoopSurvives, AMalformedFrameDropsOnlyThatClient) {
    LoopHarness h;
    std::atomic<int> dispatched{0};
    h.onPacket([&dispatched](Session::Ptr, Packet&) { dispatched.fetch_add(1); });

    tcp::socket bad = h.connectOne();
    ASSERT_TRUE(h.pump([&] { return h.accepted() >= 1; }));

    // a declared body of 0xFFFF puts the frame over the maximum so the session is dropped
    std::vector<uint8_t> junk(PACKET_HEADER_SIZE, 0);
    junk[0] = 0xFF;
    junk[1] = 0xFF;
    junk[2] = 0x01;
    asio::write(bad, asio::buffer(junk));

    ASSERT_TRUE(h.pump([&] { return h.disconnected() >= 1; }));
    EXPECT_EQ(dispatched.load(), 0);

    tcp::socket good = h.connectOne();
    ASSERT_TRUE(h.pump([&] { return h.accepted() >= 2; }));
    const auto frame = heartbeat().serialize();
    asio::write(good, asio::buffer(frame));
    ASSERT_TRUE(h.pump([&] { return dispatched.load() >= 1; }));
    EXPECT_EQ(h.disconnected(), 1);
}

TEST(LoopSurvives, AHandlerThatThrowsDropsOnlyThatClient) {
    LoopHarness h;
    std::atomic<int> seen{0};
    h.onPacket([&seen](Session::Ptr, Packet&) {
        if (seen.fetch_add(1) == 0) throw std::runtime_error("handler blew up");
    });

    tcp::socket first = h.connectOne();
    ASSERT_TRUE(h.pump([&] { return h.accepted() >= 1; }));
    const auto frame = heartbeat().serialize();
    asio::write(first, asio::buffer(frame));

    // the thrower is dropped the loop is untouched
    ASSERT_TRUE(h.pump([&] { return h.disconnected() >= 1; }));

    tcp::socket second = h.connectOne();
    ASSERT_TRUE(h.pump([&] { return h.accepted() >= 2; }));
    asio::write(second, asio::buffer(frame));
    ASSERT_TRUE(h.pump([&] { return seen.load() >= 2; }));
    EXPECT_EQ(h.disconnected(), 1);
}

/// the shape that froze the package server a kick under the race lock and a leave path taking it again
TEST(LoopSurvives, AKickTakenUnderAHandlerLockDoesNotFreezeTheLoop) {
    auto state = std::make_shared<std::mutex>();          // stands in for the race lock
    auto finished = std::make_shared<std::atomic<bool>>(false);
    auto kicked = std::make_shared<std::atomic<bool>>(false);

    LoopHarness h;

    h.onPacket([state, kicked](Session::Ptr s, Packet&) {
        // a motion handler holds the race lock for its whole body
        std::lock_guard<std::mutex> lock(*state);
        kicked->store(true);
        s->stop();   // the anti cheat kick used to call the leave path right here
    });

    h.onClosed([state, finished](Session::Ptr) {
        // the leave path takes the same lock the handler still held before the fix
        std::lock_guard<std::mutex> lock(*state);
        finished->store(true);
    });

    tcp::socket client = h.connectOne();
    ASSERT_TRUE(h.pump([&] { return h.accepted() >= 1; }));
    const auto frame = heartbeat().serialize();
    asio::write(client, asio::buffer(frame));

    ASSERT_TRUE(h.pump([&] { return kicked->load(); })) << "the handler never ran";
    ASSERT_TRUE(h.pump([&] { return finished->load(); }))
        << "the disconnect path never ran the loop is frozen";

    // the loop still accepts and still serves after the kick
    std::atomic<int> served{0};
    h.onPacket([&served](Session::Ptr, Packet&) { served.fetch_add(1); });
    tcp::socket after = h.connectOne();
    ASSERT_TRUE(h.pump([&] { return h.accepted() >= 2; })) << "the accept loop is deaf";
    asio::write(after, asio::buffer(frame));
    ASSERT_TRUE(h.pump([&] { return served.load() >= 1; }));
}

/// a ban kicks the account while a race runs the loop must keep taking clients
TEST(LoopSurvives, ABanWhileARaceRunsKeepsTheListenerAlive) {
    auto raceLock = std::make_shared<std::mutex>();
    auto bans = std::make_shared<std::atomic<int>>(0);
    auto leaves = std::make_shared<std::atomic<int>>(0);

    LoopHarness h;

    h.onPacket([raceLock, bans](Session::Ptr s, Packet& p) {
        std::lock_guard<std::mutex> lock(*raceLock);
        if (p.opcode() != CMD::C_HEARTBEAT) return;
        // count the strike write the ban row then drop the socket all under the race lock
        bans->fetch_add(1);
        s->send(Packet(CMD::C_HEARTBEAT));
        s->stop();
    });

    h.onClosed([raceLock, leaves](Session::Ptr) {
        std::lock_guard<std::mutex> lock(*raceLock);
        leaves->fetch_add(1);
    });

    const auto frame = heartbeat().serialize();
    for (int i = 0; i < 5; ++i) {
        tcp::socket racer = h.connectOne();
        const int want = i + 1;
        ASSERT_TRUE(h.pump([&] { return h.accepted() >= want; })) << "deaf at ban " << i;
        asio::write(racer, asio::buffer(frame));
        ASSERT_TRUE(h.pump([&] { return leaves->load() >= want; })) << "frozen at ban " << i;
        std::error_code ec;
        racer.close(ec);
    }

    EXPECT_EQ(bans->load(), 5);
    EXPECT_EQ(leaves->load(), 5);
    EXPECT_FALSE(h.health().stale(5000));
}

/// a session destroyed while still connected must not throw out of its destructor
TEST(SessionShutdown, DestroyingAConnectedSessionIsSilent) {
    asio::io_context io;
    tcp::acceptor acceptor(io, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    tcp::socket serverSide(io);
    bool accepted = false;
    acceptor.async_accept(serverSide, [&accepted](std::error_code ec) { accepted = !ec; });

    tcp::socket clientSide(io);
    clientSide.connect(acceptor.local_endpoint());
    while (!accepted) io.run_one();

    bool called = false;
    {
        auto session = std::make_shared<Session>(std::move(serverSide));
        session->setDisconnectHandler([&called](Session::Ptr) { called = true; });
        session->start();
        // the last owner goes away with the session still marked connected
    }
    EXPECT_FALSE(called);   // no callback and above all no terminate
}
