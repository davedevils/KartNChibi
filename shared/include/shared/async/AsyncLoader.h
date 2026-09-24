
#pragma once

#include "shared/Shared.h"
#include <future>
#include <functional>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>

namespace KnC {
namespace Async {

// load priority low to critical critical loads now and blocks until done
enum class LoadPriority {
    Low,
    Normal,
    High,
    Critical
};

enum class LoadStatus {
    Pending,
    Loading,
    Completed,
    Failed
};

template<typename T>
struct LoadResult {
    std::atomic<LoadStatus> status{LoadStatus::Pending};
    T data;
    std::string error;

    bool IsReady() const { auto s = status.load(std::memory_order_acquire); return s == LoadStatus::Completed || s == LoadStatus::Failed; }
    bool IsSuccess() const { return status.load(std::memory_order_acquire) == LoadStatus::Completed; }
};

/// asynchronous resource loader with worker threads and a per request ready check
class AsyncLoader {
public:
    AsyncLoader();
    ~AsyncLoader();
    
    void Start(int numThreads = 4);
    
    /// stops the loader and blocks until all loads complete
    void Stop();
    
    template<typename T>
    std::shared_ptr<LoadResult<T>> LoadAsync(
        const std::string& path,
        std::function<T(const std::string&)> loader,
        LoadPriority priority = LoadPriority::Normal)
    {
        auto result = std::make_shared<LoadResult<T>>();
        result->status.store(LoadStatus::Pending, std::memory_order_release);

        // Create task
        auto task = [result, path, loader]() {
            result->status.store(LoadStatus::Loading, std::memory_order_release);

            try {
                result->data = loader(path);
                result->status.store(LoadStatus::Completed, std::memory_order_release);
            } catch (const std::exception& e) {
                result->error = e.what();
                result->status.store(LoadStatus::Failed, std::memory_order_release);
            }
        };
        
        // Queue task
        QueueTask(task, priority);
        
        return result;
    }
    
    void WaitAll();
    
    int GetPendingCount() const;
    
    int GetActiveCount() const;

private:
    struct Task {
        std::function<void()> func;
        LoadPriority priority;
        
        bool operator<(const Task& other) const {
            return static_cast<int>(priority) < static_cast<int>(other.priority);
        }
    };
    
    void QueueTask(std::function<void()> task, LoadPriority priority);
    void WorkerThread();
    
    std::vector<std::thread> m_workers;
    std::priority_queue<Task> m_tasks;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_running;
    std::atomic<int> m_activeCount;
};

}}

