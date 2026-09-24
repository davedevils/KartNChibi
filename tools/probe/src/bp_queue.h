#pragma once
#include <atomic>
#include <mutex>
#include <vector>
#include "bp_hit.h"
namespace probe {
// low frequency bp hit queue producer uses try lock and drops on contention so it never blocks the faulting thread
class BpQueue {
public:
    explicit BpQueue(size_t capacity);
    bool try_push(const BpHit& hit);
    bool try_pop(BpHit& out);
    uint64_t dropped() const { return dropped_.load(std::memory_order_relaxed); }
private:
    std::vector<BpHit> slots_;
    size_t head_ = 0, tail_ = 0, count_ = 0;
    std::mutex mutex_;
    std::atomic<uint64_t> dropped_{0};
};
}  // namespace probe
