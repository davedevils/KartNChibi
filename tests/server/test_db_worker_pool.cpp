/// database work leaves the io thread and a slow query never stops the accept loop

#include <gtest/gtest.h>
#include <asio.hpp>
#include <atomic>
#include <chrono>
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

/// waits for a predicate without blocking the whole suite when it never holds
template <typename Pred>
bool waitFor(Pred done, int budgetMs = 4000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budgetMs);
    while (!done() && std::chrono::steady_clock::now() < deadline) sleepMs(2);
    return done();
}

} // namespace

// the client needs its own frames in order so one key never runs two jobs at once
TEST(DbWorkerPool, OneKeyStaysInOrder) {
    DbWorkerPool pool;
    pool.start(4);

    std::mutex m;
    std::vector<int> seen;
    std::atomic<int> concurrent{0};
    std::atomic<int> maxConcurrent{0};

    for (int i = 0; i < 40; ++i) {
        ASSERT_TRUE(pool.post(7, [i, &m, &seen, &concurrent, &maxConcurrent] {
            const int now = concurrent.fetch_add(1) + 1;
            int prev = maxConcurrent.load();
            while (now > prev && !maxConcurrent.compare_exchange_weak(prev, now)) {}
            sleepMs(1);
            {
                std::lock_guard<std::mutex> lock(m);
                seen.push_back(i);
            }
            concurrent.fetch_sub(1);
        }));
    }

    EXPECT_TRUE(waitFor([&pool] { return pool.pending() == 0; }));
    pool.stop();

    ASSERT_EQ(seen.size(), 40u);
    for (int i = 0; i < 40; ++i) EXPECT_EQ(seen[static_cast<size_t>(i)], i);
    EXPECT_EQ(maxConcurrent.load(), 1);
}

// two clients must not wait on each other that is the whole reason for the pool
TEST(DbWorkerPool, TwoKeysRunSideBySide) {
    DbWorkerPool pool;
    pool.start(4);

    std::atomic<int> live{0};
    std::atomic<int> peak{0};
    for (int key = 0; key < 4; ++key) {
        ASSERT_TRUE(pool.post(static_cast<uint64_t>(key), [&live, &peak] {
            const int now = live.fetch_add(1) + 1;
            int prev = peak.load();
            while (now > prev && !peak.compare_exchange_weak(prev, now)) {}
            sleepMs(60);
            live.fetch_sub(1);
        }));
    }

    EXPECT_TRUE(waitFor([&pool] { return pool.pending() == 0; }));
    pool.stop();
    EXPECT_GT(peak.load(), 1);
    EXPECT_EQ(pool.completed(), 4u);
}

// a job that throws must not take the worker with it
TEST(DbWorkerPool, AThrowingJobLosesOnlyItself) {
    DbWorkerPool pool;
    pool.start(2);

    std::atomic<int> done{0};
    ASSERT_TRUE(pool.post(1, [] { throw std::runtime_error("db down"); }));
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(pool.post(1, [&done] { done.fetch_add(1); }));
    }
    EXPECT_TRUE(waitFor([&done] { return done.load() == 5; }));
    pool.stop();
    EXPECT_EQ(done.load(), 5);
}

// a caller must be able to fall back when the pool is off KNC DB THREADS 0 does that
TEST(DbWorkerPool, PostRefusesWhenTheToolIsStopped) {
    DbWorkerPool pool;
    EXPECT_FALSE(pool.running());
    EXPECT_FALSE(pool.post(1, [] {}));
    pool.start(1);
    EXPECT_TRUE(pool.running());
    pool.stop();
    EXPECT_FALSE(pool.post(1, [] {}));
}

// the point of the whole gap a slow query used to freeze the only io thread
TEST(DbWorkerPool, ASlowQueryDoesNotStopTheAcceptLoop) {
    asio::io_context io;
    tcp::acceptor acceptor(io, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    const uint16_t port = acceptor.local_endpoint().port();

    DbWorkerPool pool;
    pool.start(2);

    std::atomic<int> accepted{0};
    std::atomic<int> answered{0};
    std::vector<Session::Ptr> kept;

    std::function<void()> arm = [&] {
        auto socket = std::make_shared<tcp::socket>(io);
        acceptor.async_accept(*socket, [&, socket](const std::error_code& ec) {
            if (!ec) {
                auto session = std::make_shared<Session>(std::move(*socket));
                kept.push_back(session);
                accepted.fetch_add(1);
                // the first client hits a query that takes 400 ms on the pool not on the loop
                pool.post(session->id(), [&io, &answered] {
                    sleepMs(400);
                    asio::post(io, [&answered] { answered.fetch_add(1); });
                });
            }
            arm();
        });
    };
    arm();

    // the loop must keep taking clients while that job runs
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

    // five clients accepted well inside the single 400 ms query
    EXPECT_EQ(accepted.load(), 5);

    // and the answers come back on the loop once the queries finish
    const auto drainBy = std::chrono::steady_clock::now() + std::chrono::milliseconds(4000);
    while (answered.load() < 5 && std::chrono::steady_clock::now() < drainBy) {
        io.restart();
        io.run_for(std::chrono::milliseconds(10));
    }
    EXPECT_EQ(answered.load(), 5);

    pool.stop();
    acceptor.close();
    for (auto& s : kept) s->stop();
    io.restart();
    io.run_for(std::chrono::milliseconds(50));
}
