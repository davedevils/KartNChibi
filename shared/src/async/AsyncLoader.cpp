
#include "shared/async/AsyncLoader.h"
#include "shared/debug/Logger.h"

namespace KnC {
namespace Async {

AsyncLoader::AsyncLoader()
    : m_running(false)
    , m_activeCount(0)
{
}

AsyncLoader::~AsyncLoader() {
    Stop();
}

void AsyncLoader::Start(int numThreads) {
    if (m_running) {
        LOG_WARNING(KnC::Debug::LogCategory::General, "AsyncLoader already running");
        return;
    }
    
    m_running = true;

    for (int i = 0; i < numThreads; i++) {
        m_workers.emplace_back(&AsyncLoader::WorkerThread, this);
    }
    
    LOG_INFO_F(KnC::Debug::LogCategory::General,
              "AsyncLoader started with %d worker threads", numThreads);
}

void AsyncLoader::Stop() {
    if (!m_running) return;
    
    m_running = false;

    m_condition.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    m_workers.clear();
    
    LOG_INFO(KnC::Debug::LogCategory::General, "AsyncLoader stopped");
}

void AsyncLoader::QueueTask(std::function<void()> task, LoadPriority priority) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.push({task, priority});
    }
    
    m_condition.notify_one();
}

void AsyncLoader::WorkerThread() {
    while (m_running) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this] {
                return !m_running || !m_tasks.empty();
            });
            
            if (!m_running && m_tasks.empty()) {
                break;
            }
            
            if (!m_tasks.empty()) {
                task = m_tasks.top();
                m_tasks.pop();
            } else {
                continue;
            }
        }

        m_activeCount++;
        
        try {
            task.func();
        } catch (const std::exception& e) {
            LOG_ERROR_F(KnC::Debug::LogCategory::General,
                       "AsyncLoader task failed: %s", e.what());
        }
        
        m_activeCount--;
    }
}

void AsyncLoader::WaitAll() {
    while (GetPendingCount() > 0 || GetActiveCount() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int AsyncLoader::GetPendingCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_tasks.size());
}

int AsyncLoader::GetActiveCount() const {
    return m_activeCount.load();
}

}}

