#pragma once
#include <functional>
#include <string>
#include "config.h"
#include "ring_buffer.h"
#include "bp_queue.h"

namespace probe {

// drains up to budget records passing each serialized line to writer
int drain_once(RingBuffer& ring, const ProbeConfig& cfg,
               const std::function<void(const std::string&)>& writer, int budget);

// starts or stops the background drain thread ring and cfg must outlive stop sink
void start_sink(RingBuffer& ring, const ProbeConfig& cfg);
void stop_sink();

// wires the bp hit queue into the sink drain loop before or after start sink
void sink_set_bp_queue(BpQueue* queue);

}  // namespace probe
