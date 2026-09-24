#include "poll.h"
#include "config.h"
#include "ring_buffer.h"
#include <cassert>
#include <cstdint>
#include <atomic>

using namespace probe;

int main() {
    // builds a poll hook with one abs capture pointing at a live uint32
    uint32_t target = 0xCAFEBABE;
    ProbeConfig cfg;
    cfg.max_capture_bytes = 512;
    HookDesc h;
    h.name = "player"; h.dir = 2; h.is_poll = true; h.interval_ms = 100;
    CaptureDesc cap;
    cap.label = "mgr"; cap.source = SourceKind::Abs;
    cap.source_index = reinterpret_cast<uint32_t>(&target);
    cap.mode = CaptureMode::Ptr; cap.size = 4;
    h.captures.push_back(cap);
    cfg.hooks.push_back(h);

    RingBuffer ring(16);
    std::atomic<uint64_t> seq{0};
    // next due 0 is due at now 0
    std::vector<uint64_t> next_due(1, 0);

    // due at now 0 pushes exactly one record
    const int pushed = evaluate_due_polls(cfg, ring, seq, next_due, 0, false);
    assert(pushed == 1);
    ProbeRecord rec{};
    assert(ring.try_pop(rec));
    assert(rec.hook_id == 0);
    assert(rec.seq == 0);
    assert(rec.blob_count == 1);
    assert(rec.blobs[0].size == 4);
    assert(*reinterpret_cast<uint32_t*>(rec.blobs[0].data) == 0xCAFEBABE);
    // next due advances by the interval of 100
    assert(next_due[0] == 100);

    // not due at now 50 since next due is 100 so nothing pushed
    const int skipped = evaluate_due_polls(cfg, ring, seq, next_due, 50, false);
    assert(skipped == 0);

    // force overrides the timer
    const int forced = evaluate_due_polls(cfg, ring, seq, next_due, 50, true);
    assert(forced == 1);
    ProbeRecord rec2{};
    assert(ring.try_pop(rec2));
    assert(rec2.hook_id == 0);
    assert(rec2.seq == 1);          // not due at 50 consumed no seq next due becomes 150
    assert(next_due[0] == 150);

    // snapshot only hook with interval 0 fires only when forced
    ProbeConfig snap_cfg;
    snap_cfg.max_capture_bytes = 512;
    HookDesc sh;
    sh.name = "world"; sh.dir = 2; sh.is_poll = true; sh.interval_ms = 0;
    sh.captures.push_back(cap);
    snap_cfg.hooks.push_back(sh);
    RingBuffer snap_ring(16);
    std::atomic<uint64_t> snap_seq{0};
    std::vector<uint64_t> snap_due(1, 0);
    assert(evaluate_due_polls(snap_cfg, snap_ring, snap_seq, snap_due, 0, false) == 0);
    assert(evaluate_due_polls(snap_cfg, snap_ring, snap_seq, snap_due, 0, true) == 1);
    assert(evaluate_due_polls(snap_cfg, snap_ring, snap_seq, snap_due, 999, false) == 0);

    return 0;
}
