#include "ring_buffer.h"

namespace probe {

static size_t round_up_pow2(size_t n) {
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

RingBuffer::RingBuffer(size_t capacity)
    : slots_(round_up_pow2(capacity < 2 ? 2 : capacity)),
      mask_(slots_.size() - 1) {}

bool RingBuffer::try_push(const ProbeRecord& record) {
    if (producer_lock_.test_and_set(std::memory_order_acquire)) {
        dropped_.fetch_add(1, std::memory_order_relaxed);  // another producer active
        return false;
    }
    const uint64_t head = head_.load(std::memory_order_relaxed);
    const uint64_t tail = tail_.load(std::memory_order_acquire);
    if (head - tail >= slots_.size()) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        producer_lock_.clear(std::memory_order_release);
        return false;
    }
    slots_[head & mask_] = record;
    head_.store(head + 1, std::memory_order_release);
    producer_lock_.clear(std::memory_order_release);
    return true;
}

bool RingBuffer::try_pop(ProbeRecord& out) {
    const uint64_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) return false;
    // x86 tso the acquire on head orders the producers slot write before this read revisit for a future x64 build
    out = slots_[tail & mask_];
    tail_.store(tail + 1, std::memory_order_release);
    return true;
}

}  // namespace probe
