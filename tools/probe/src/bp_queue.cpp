#include "bp_queue.h"
namespace probe {
BpQueue::BpQueue(size_t capacity) : slots_(capacity < 1 ? 1 : capacity) {}
bool BpQueue::try_push(const BpHit& hit) {
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock() || count_ == slots_.size()) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    slots_[head_] = hit;
    head_ = (head_ + 1) % slots_.size();
    ++count_;
    return true;
}
bool BpQueue::try_pop(BpHit& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (count_ == 0) return false;
    out = slots_[tail_];
    tail_ = (tail_ + 1) % slots_.size();
    --count_;
    return true;
}
}  // namespace probe
