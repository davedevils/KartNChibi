#include "db/DbWorkerPool.h"

#include "logging/Logger.h"

namespace knc {

DbWorkerPool::~DbWorkerPool() {
    stop();
}

void DbWorkerPool::start(size_t threads) {
    if (m_running.load()) return;
    if (threads == 0) threads = 1;
    m_running.store(true);
    m_completed.store(0);
    m_threads.reserve(threads);
    for (size_t i = 0; i < threads; ++i) {
        m_threads.emplace_back([this] { workerLoop(); });
    }
    LOG_INFO("DB", "worker pool started with " + std::to_string(threads) + " threads");
}

void DbWorkerPool::stop() {
    if (!m_running.exchange(false)) {
        // still join anything a failed start left behind
        for (auto& t : m_threads) if (t.joinable()) t.join();
        m_threads.clear();
        return;
    }
    m_cv.notify_all();
    for (auto& t : m_threads) if (t.joinable()) t.join();
    m_threads.clear();
    LOG_INFO("DB", "worker pool stopped after " + std::to_string(m_completed.load()) + " jobs");
}

bool DbWorkerPool::post(uint64_t orderKey, std::function<void()> job) {
    if (!job) return true;
    if (!m_running.load()) return false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queues[orderKey].push_back(std::move(job));
        ++m_pending;
    }
    m_cv.notify_one();
    return true;
}

size_t DbWorkerPool::pending() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pending;
}

bool DbWorkerPool::takeJob(uint64_t& keyOut, std::function<void()>& jobOut) {
    for (auto it = m_queues.begin(); it != m_queues.end(); ++it) {
        // a key already running must wait or two frames of one client could cross
        if (m_busy.count(it->first) != 0) continue;
        if (it->second.empty()) continue;
        keyOut = it->first;
        jobOut = std::move(it->second.front());
        it->second.pop_front();
        m_busy.insert(keyOut);
        return true;
    }
    return false;
}

void DbWorkerPool::workerLoop() {
    for (;;) {
        uint64_t key = 0;
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            // taking inside the predicate keeps a busy key from waking this thread in a loop
            m_cv.wait(lock, [this, &key, &job] { return takeJob(key, job) || !m_running.load(); });
            // stopped and nothing left to drain
            if (!job) return;
        }

        try {
            job();
        } catch (const std::exception& e) {
            LOG_ERROR("DB", std::string("worker job threw ") + e.what());
        } catch (...) {
            LOG_ERROR("DB", "worker job threw an unknown type");
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_busy.erase(key);
            if (m_pending > 0) --m_pending;
            auto it = m_queues.find(key);
            if (it != m_queues.end() && it->second.empty()) m_queues.erase(it);
        }
        m_completed.fetch_add(1);
        // the key is free now so a waiting job of the same client can move
        m_cv.notify_all();
    }
}

}
