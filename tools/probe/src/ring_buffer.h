#pragma once
#include <atomic>
#include <vector>
#include "probe_record.h"

namespace probe {

// non blocking ring buffer single consumer producer drops and counts on contention or full
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity);
    bool try_push(const ProbeRecord& record);
    bool try_pop(ProbeRecord& out);
    uint64_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

private:
    std::vector<ProbeRecord> slots_;
    size_t mask_;
    // head is the next write index tail is the next read index
    std::atomic<uint64_t> head_{0};
    std::atomic<uint64_t> tail_{0};
    std::atomic<uint64_t> dropped_{0};
    std::atomic_flag producer_lock_ = ATOMIC_FLAG_INIT;
};

}  // namespace probe
