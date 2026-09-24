/// worker pool that keeps database calls off the io thread while a client frames stay in order

#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace knc {

/// different order keys run parallel same key stays in order never touch Session or Room reply via asio post
class DbWorkerPool {
public:
    DbWorkerPool() = default;
    ~DbWorkerPool();

    DbWorkerPool(const DbWorkerPool&) = delete;
    DbWorkerPool& operator=(const DbWorkerPool&) = delete;

    /// spawns the threads a second call is a no op zero threads means one
    void start(size_t threads);

    /// drains what is queued then joins every thread safe to call twice
    void stop();

    bool running() const { return m_running.load(); }

    /// job runs under an order key session id keeps one client serial false means caller must run it
    bool post(uint64_t orderKey, std::function<void()> job);

    size_t pending() const;

    size_t threads() const { return m_threads.size(); }

    /// jobs finished since the last start for the liveness log
    uint64_t completed() const { return m_completed.load(); }

private:
    void workerLoop();
    /// picks the next key nobody is running returns false when there is nothing to take
    bool takeJob(uint64_t& keyOut, std::function<void()>& jobOut);

    std::vector<std::thread> m_threads;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::map<uint64_t, std::deque<std::function<void()>>> m_queues;
    std::set<uint64_t> m_busy;
    size_t m_pending = 0;
    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_completed{0};
};

}
