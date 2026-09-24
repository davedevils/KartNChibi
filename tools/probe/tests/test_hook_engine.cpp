#include "hook_engine.h"
#include "ring_buffer.h"
#include "config.h"
#include <cassert>
#include <cstdint>

// a target with a real body so MinHook can patch and relocate its prologue
__declspec(noinline) int __stdcall sample_target(int a, int b) {
    volatile int x = a;
    for (int i = 0; i < 3; i++) x += b;
    return x;
}

int main() {
    using namespace probe;
    ProbeConfig cfg;
    cfg.image_base = 0;
    cfg.max_capture_bytes = 512;
    HookDesc h; h.name = "sample"; h.dir = 0;
    h.address = reinterpret_cast<uint32_t>(&sample_target);
    CaptureDesc a; a.label = "a"; a.source = SourceKind::Arg; a.source_index = 0; a.mode = CaptureMode::Val; a.size = 4;
    CaptureDesc b; b.label = "b"; b.source = SourceKind::Arg; b.source_index = 1; b.mode = CaptureMode::Val; b.size = 4;
    h.captures.push_back(a); h.captures.push_back(b);
    cfg.hooks.push_back(h);

    RingBuffer ring(64);
    assert(install_hooks(cfg, ring, 0));            // runtime base 0 gives an identity rebase

    const int result = sample_target(3, 4);
    assert(result == 3 + 4 * 3);                    // 15 still the original behavior

    uninstall_hooks();

    ProbeRecord rec{};
    assert(ring.try_pop(rec));
    assert(rec.hook_id == 0);
    assert(rec.blob_count == 2);
    assert(*reinterpret_cast<uint32_t*>(rec.blobs[0].data) == 3);
    assert(*reinterpret_cast<uint32_t*>(rec.blobs[1].data) == 4);

    // second cycle proves install uninstall install works idempotent MH Initialize
    assert(install_hooks(cfg, ring, 0));

    const int result2 = sample_target(5, 6);
    assert(result2 == 5 + 6 * 3);                   // 23 original behavior intact

    uninstall_hooks();

    ProbeRecord rec2{};
    assert(ring.try_pop(rec2));
    assert(rec2.hook_id == 0);
    assert(rec2.blob_count == 2);
    assert(*reinterpret_cast<uint32_t*>(rec2.blobs[0].data) == 5);
    assert(*reinterpret_cast<uint32_t*>(rec2.blobs[1].data) == 6);
    return 0;
}
