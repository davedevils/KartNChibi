#pragma once
#include <atomic>
#include <cstdint>
#include <vector>
#include "config.h"
#include "ring_buffer.h"

namespace probe {

// evaluates all is poll hooks and pushes a record for each one due force bypasses the timer
int evaluate_due_polls(const ProbeConfig& cfg, RingBuffer& ring,
                       std::atomic<uint64_t>& seq, std::vector<uint64_t>& next_due_ms,
                       uint64_t now_ms, bool force);

// starts the background poll thread ring and cfg must outlive stop poll
void start_poll(RingBuffer& ring, const ProbeConfig& cfg);
void stop_poll();

// requests a one shot snapshot on the next poll iteration
void request_snap();

}  // namespace probe
