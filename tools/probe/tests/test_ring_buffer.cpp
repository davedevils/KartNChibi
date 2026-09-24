#include "ring_buffer.h"
#include <cassert>

using probe::ProbeRecord;
using probe::RingBuffer;

static ProbeRecord rec(uint64_t seq) {
    ProbeRecord r{};
    r.seq = seq;
    return r;
}

int main() {
    RingBuffer ring(4);                 // FIFO order across a wrap capacity rounds to 4
    for (uint64_t i = 0; i < 3; i++) assert(ring.try_push(rec(i)));
    ProbeRecord out{};
    for (uint64_t i = 0; i < 3; i++) { assert(ring.try_pop(out)); assert(out.seq == i); }
    assert(!ring.try_pop(out));         // empty

    RingBuffer small(2);                // full then drop and count usable slots equal 2
    assert(small.try_push(rec(10)));
    assert(small.try_push(rec(11)));
    assert(!small.try_push(rec(12)));   // full so the push is dropped
    assert(small.dropped() == 1);
    assert(small.try_pop(out) && out.seq == 10);
    assert(small.try_push(rec(13)));    // slot freed
    assert(small.try_pop(out) && out.seq == 11);
    assert(small.try_pop(out) && out.seq == 13);
    return 0;
}
