#include "bp_engine.h"
#include "bp_queue.h"
#include <cassert>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <thread>

__declspec(noinline) int bp_sample_target(int a, int b) {
    volatile int x = a + b; return x;
}

__declspec(noinline) int bp_sample_target2(int a, int b) { volatile int y = a * 0 + b + a; return y; }
__declspec(noinline) int bp_sample_target3(int a, int b) { volatile int z = a - b + a + b; return z; }

int main() {
    using namespace probe;
    BpQueue q(16);
    bp_init(&q);

    const uint32_t addr = reinterpret_cast<uint32_t>(&bp_sample_target);
    assert(bp_arm(addr, /*suspend*/false, /*int3*/false));   // hw trace

    const int r = bp_sample_target(3, 4);                    // should still run normally
    assert(r == 7);

    BpHit hit{};
    assert(q.try_pop(hit));                                  // BP fired
    assert(hit.addr == addr);
    assert(hit.eip == addr);
    assert(hit.mode == 0);                                   // trace

    assert(bp_disarm(addr));
    const int r2 = bp_sample_target(5, 6);                   // no longer traps
    assert(r2 == 11);
    BpHit hit2{};
    assert(!q.try_pop(hit2));                                // nothing new

    // int3 trace 0xcc on a local function fires restores and target stays correct
    const uint32_t i2 = reinterpret_cast<uint32_t>(&bp_sample_target2);
    assert(bp_arm(i2, /*suspend*/false, /*int3*/true));
    const int ri = bp_sample_target2(10, 20);
    assert(ri == 30);                       // original behavior intact after step over and rearm
    BpHit ih{};
    assert(q.try_pop(ih) && ih.addr == i2);
    assert(ih.code[0] != 0xCC);             // dump shows the original opcode not the trap
    assert(bp_disarm(i2));

    // two int3 breakpoints hit repeatedly proves 0xcc is rearmed every time
    const uint32_t a2 = reinterpret_cast<uint32_t>(&bp_sample_target2);
    const uint32_t a3 = reinterpret_cast<uint32_t>(&bp_sample_target3);
    assert(bp_arm(a2, false, true));
    assert(bp_arm(a3, false, true));
    for (int k = 0; k < 5; ++k) {
        assert(bp_sample_target2(10, 20) == 30);
        assert(bp_sample_target3(7, 3) == 14);
        BpHit h2{}; assert(q.try_pop(h2) && h2.addr == a2);
        BpHit h3{}; assert(q.try_pop(h3) && h3.addr == a3);
    }

    // disarms one mid sequence the other keeps firing and the disarmed target runs clean
    assert(bp_disarm(a2));
    assert(bp_sample_target2(10, 20) == 30);
    BpHit none{}; assert(!q.try_pop(none));
    assert(bp_sample_target3(7, 3) == 14);
    BpHit h3b{}; assert(q.try_pop(h3b) && h3b.addr == a3);
    assert(bp_disarm(a3));

    // int3 suspend worker thread blocks in the wait main thread resumes it via duplicatehandle and a lock ordered resume
    const uint32_t sa = reinterpret_cast<uint32_t>(&bp_sample_target3);
    assert(bp_arm(sa, /*suspend*/true, /*int3*/true));
    std::atomic<int> worker_result{-1};
    std::atomic<bool> worker_done{false};
    std::thread worker([&] {
        worker_result = bp_sample_target3(7, 3);            // traps and suspends until resumed
        worker_done = true;
    });
    // gives the worker time to enter the suspend wait then resumes it
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    assert(!worker_done.load());                            // still suspended in the wait
    bp_resume(sa, /*all*/false);
    worker.join();
    assert(worker_result.load() == 14);                    // resumed and returned correctly suspend hit was captured
    BpHit sh{}; assert(q.try_pop(sh) && sh.addr == sa && sh.mode == 1);
    assert(bp_disarm(sa));
    assert(bp_sample_target3(7, 3) == 14);                 // clean after disarm no leftover 0xcc

    // bad int3 address safe read guard must reject it without crashing
    assert(bp_arm(0xdead0000u, false, true) == false);

    bp_shutdown();
    return 0;
}
