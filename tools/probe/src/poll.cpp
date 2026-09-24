#include "poll.h"
#include "capture.h"
#include "probe_record.h"
#include <thread>
#include <chrono>
#include <windows.h>

namespace probe {

static std::atomic<bool> g_running{false};
static std::atomic<bool> g_snap{false};
static std::thread g_thread;

// QPC microseconds matching hook engine's now us so poll and packet t us correlate
static uint64_t now_us_poll() {
    static const long long freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f.QuadPart; }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return static_cast<uint64_t>(c.QuadPart / freq) * 1000000ull +
           static_cast<uint64_t>(c.QuadPart % freq) * 1000000ull / freq;
}

int evaluate_due_polls(const ProbeConfig& cfg, RingBuffer& ring,
                       std::atomic<uint64_t>& seq, std::vector<uint64_t>& next_due_ms,
                       uint64_t now_ms, bool force) {
    int pushed = 0;
    for (size_t i = 0; i < cfg.hooks.size(); ++i) {
        const HookDesc& hook = cfg.hooks[i];
        if (!hook.is_poll) continue;
        if (hook.interval_ms == 0 && !force) continue;   // interval 0 means snapshot only
        if (!force && now_ms < next_due_ms[i]) continue;

        ProbeRecord rec{};
        rec.seq = seq.fetch_add(1, std::memory_order_relaxed);
        rec.t_us = now_us_poll();
        rec.thread_id = static_cast<uint32_t>(GetCurrentThreadId());
        rec.hook_id = static_cast<uint16_t>(i);
        rec.opcode = kNoOpcode;
        rec.blob_count = 0;

        const HookContext ctx{};
        for (const CaptureDesc& cap : hook.captures) {
            if (rec.blob_count >= kMaxCapturesPerHook) break;
            CaptureBlob blob{};
            resolve_capture(cap, ctx, cfg.max_capture_bytes, blob);
            rec.blobs[rec.blob_count++] = blob;
        }

        ring.try_push(rec);
        next_due_ms[i] = now_ms + hook.interval_ms;
        ++pushed;
    }
    return pushed;
}

static uint64_t now_ms() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

static void poll_loop(const ProbeConfig* cfg, RingBuffer* ring) {
    std::atomic<uint64_t> seq{0};
    std::vector<uint64_t> next_due(cfg->hooks.size(), 0);
    while (g_running.load(std::memory_order_relaxed)) {
        const bool force = g_snap.exchange(false, std::memory_order_relaxed);
        evaluate_due_polls(*cfg, *ring, seq, next_due, now_ms(), force);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
}

void start_poll(RingBuffer& ring, const ProbeConfig& cfg) {
    if (g_running.exchange(true)) return;  // already running skip and drop any stale snap request
    g_snap.store(false, std::memory_order_relaxed);
    g_thread = std::thread(poll_loop, &cfg, &ring);
}

void stop_poll() {
    g_running.store(false, std::memory_order_relaxed);
    if (g_thread.joinable()) g_thread.join();
}

void request_snap() {
    g_snap.store(true, std::memory_order_relaxed);
}

}  // namespace probe
