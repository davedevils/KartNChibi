#include "sink.h"
#include <cassert>
#include <vector>
#include <string>

using namespace probe;

int main() {
    ProbeConfig cfg;
    HookDesc h; h.name = "dispatch"; h.dir = 0;
    CaptureDesc c; c.label = "opcode"; h.captures.push_back(c);
    cfg.hooks.push_back(h);

    RingBuffer ring(8);
    for (uint64_t i = 0; i < 3; i++) {
        ProbeRecord r{};
        r.seq = i; r.hook_id = 0; r.opcode = kNoOpcode; r.blob_count = 1;
        r.blobs[0].size = 1; r.blobs[0].data[0] = static_cast<uint8_t>(i);
        assert(ring.try_push(r));
    }

    std::vector<std::string> lines;
    const int drained = drain_once(ring, cfg, [&](const std::string& l) { lines.push_back(l); }, 10);
    assert(drained == 3);
    assert(lines.size() == 3);
    assert(lines[0].find("\"seq\":0") != std::string::npos);
    assert(lines[1].find("\"seq\":1") != std::string::npos);
    assert(lines[2].find("\"seq\":2") != std::string::npos);

    // the budget caps how much the ring buffer drains
    for (uint64_t i = 0; i < 5; i++) { ProbeRecord r{}; r.hook_id = 0; r.opcode = kNoOpcode; ring.try_push(r); }
    const int capped = drain_once(ring, cfg, [](const std::string&) {}, 2);
    assert(capped == 2);
    return 0;
}
