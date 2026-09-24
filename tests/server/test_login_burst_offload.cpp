/// login burst writes session fields on io thread reads run on the pool

#include <gtest/gtest.h>
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "db/DbWorkerPool.h"
#include "net/Session.h"

using namespace knc;
using asio::ip::tcp;

namespace {

void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

/// mirrors GameServer LoginBurstFrames lead frames then the chunks
struct BurstFrames {
    std::vector<std::string> preBurst;
    std::vector<std::string> chunks;
};

/// mirrors GameServer sendPlayerData session writes here db reads on pool drip back on loop
struct BurstHarness {
    asio::io_context& io;
    DbWorkerPool& pool;
    int collectMs;

    std::mutex mutex;
    std::vector<std::string> wire;          ///< every frame in the order the session got it
    std::atomic<int> sessionPhases{0};
    std::atomic<int> dripped{0};
    std::thread::id loopThread;

    void login(const Session::Ptr& session) {
        // part A session fields written here only
        session->characterId = 100 + session->id();
        session->catalogsSent = true;
        sessionPhases.fetch_add(1);

        auto frames = std::make_shared<BurstFrames>();
        auto job = [this, session, frames] {
            // part B db read of burst never touches the session
            sleepMs(collectMs);
            frames->preBurst = {"0xBE", "0xC6", "0x10C", "0x10D"};
            frames->chunks = {"chunk0", "chunk1", "chunk2"};
            asio::post(io, [this, session, frames] {
                // part C only part back on the loop
                std::lock_guard<std::mutex> lock(mutex);
                for (const auto& f : frames->preBurst) wire.push_back(f);
                for (const auto& c : frames->chunks) wire.push_back(c);
                dripped.fetch_add(1);
            });
        };
        if (!pool.post(session->id(), job)) job();
    }
};

}  // namespace

// split lets accept loop keep taking clients while a burst builds
TEST(LoginBurstOffload, ABurstDoesNotStopTheAcceptLoop) {
    asio::io_context io;
    tcp::acceptor acceptor(io, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    const uint16_t port = acceptor.local_endpoint().port();

    DbWorkerPool pool;
    pool.start(2);

    BurstHarness burst{io, pool, 400};
    std::atomic<int> accepted{0};
    std::vector<Session::Ptr> kept;

    std::function<void()> arm = [&] {
        auto socket = std::make_shared<tcp::socket>(io);
        acceptor.async_accept(*socket, [&, socket](const std::error_code& ec) {
            if (!ec) {
                auto session = std::make_shared<Session>(std::move(*socket));
                kept.push_back(session);
                accepted.fetch_add(1);
                burst.login(session);
            }
            arm();
        });
    };
    arm();

    std::vector<tcp::socket> clients;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(4000);
    int opened = 0;
    while (accepted.load() < 5 && std::chrono::steady_clock::now() < deadline) {
        if (opened < 5) {
            tcp::socket c(io);
            c.connect(tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));
            clients.push_back(std::move(c));
            ++opened;
        }
        io.restart();
        io.run_for(std::chrono::milliseconds(10));
    }

    // five clients fit inside the 400ms burst session fields already written
    EXPECT_EQ(accepted.load(), 5);
    EXPECT_EQ(burst.sessionPhases.load(), 5);

    const auto drainBy = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (burst.dripped.load() < 5 && std::chrono::steady_clock::now() < drainBy) {
        io.restart();
        io.run_for(std::chrono::milliseconds(10));
    }
    EXPECT_EQ(burst.dripped.load(), 5);

    pool.stop();
    acceptor.close();
    for (auto& s : kept) s->stop();
    io.restart();
    io.run_for(std::chrono::milliseconds(50));
}

// wrong order breaks stock client lead frames must stay before chunks
TEST(LoginBurstOffload, TheFrameOrderSurvivesTheRoundTrip) {
    asio::io_context io;
    DbWorkerPool pool;
    pool.start(2);

    BurstHarness burst{io, pool, 5};
    auto session = std::make_shared<Session>(tcp::socket(io));
    burst.login(session);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(4000);
    while (burst.dripped.load() < 1 && std::chrono::steady_clock::now() < deadline) {
        io.restart();
        io.run_for(std::chrono::milliseconds(5));
    }
    ASSERT_EQ(burst.dripped.load(), 1);

    const std::vector<std::string> expected = {
        "0xBE", "0xC6", "0x10C", "0x10D", "chunk0", "chunk1", "chunk2"};
    EXPECT_EQ(burst.wire, expected);

    pool.stop();
}

// pool off burst still runs and still answers
TEST(LoginBurstOffload, WithNoPoolTheBurstStillRuns) {
    asio::io_context io;
    DbWorkerPool pool;   // never started KNC DB THREADS zero is the bisect

    BurstHarness burst{io, pool, 0};
    auto session = std::make_shared<Session>(tcp::socket(io));
    burst.login(session);

    io.restart();
    io.run_for(std::chrono::milliseconds(50));

    EXPECT_EQ(burst.sessionPhases.load(), 1);
    EXPECT_EQ(burst.dripped.load(), 1);
    EXPECT_EQ(burst.wire.size(), 7u);
}
