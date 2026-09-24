#include "jsonl.h"
#include <cassert>

using namespace probe;

int main() {
    ProbeConfig cfg;
    HookDesc h; h.name = "dispatch"; h.dir = 0;
    CaptureDesc c0; c0.label = "opcode"; h.captures.push_back(c0);
    CaptureDesc c1; c1.label = "struct"; h.captures.push_back(c1);
    cfg.hooks.push_back(h);

    ProbeRecord r{};
    r.seq = 5; r.t_us = 1234; r.thread_id = 99; r.hook_id = 0; r.opcode = 118;
    r.blob_count = 2;
    r.blobs[0].size = 2; r.blobs[0].data[0] = 0x76; r.blobs[0].data[1] = 0x00;
    r.blobs[1].size = 3; r.blobs[1].data[0] = 0x01; r.blobs[1].data[1] = 0xA0; r.blobs[1].data[2] = 0xFF;

    const std::string line = serialize_record(r, cfg);
    const char* expected =
        "{\"seq\":5,\"t_us\":1234,\"thread\":99,\"hook\":\"dispatch\","
        "\"dir\":\"in\",\"opcode\":118,\"captures\":{\"opcode\":\"7600\","
        "\"struct\":\"01a0ff\"}}";
    assert(line == expected);

    // opcode is omitted when absent
    ProbeRecord r2{};
    r2.seq = 1; r2.t_us = 2; r2.thread_id = 3; r2.hook_id = 0; r2.opcode = kNoOpcode;
    r2.blob_count = 0;
    const std::string line2 = serialize_record(r2, cfg);
    assert(line2.find("opcode") == std::string::npos);
    assert(line2.find("\"captures\":{}") != std::string::npos);

    // dir 2 serializes as state
    ProbeConfig scfg;
    HookDesc sh; sh.name = "player"; sh.dir = 2;
    CaptureDesc sc; sc.label = "mgr"; sh.captures.push_back(sc);
    scfg.hooks.push_back(sh);
    ProbeRecord sr{};
    sr.seq = 1; sr.t_us = 9; sr.thread_id = 2; sr.hook_id = 0; sr.opcode = kNoOpcode;
    sr.blob_count = 1; sr.blobs[0].size = 2; sr.blobs[0].data[0] = 0xDE; sr.blobs[0].data[1] = 0xAD;
    const std::string sline = serialize_record(sr, scfg);
    assert(sline.find("\"dir\":\"state\"") != std::string::npos);
    assert(sline.find("\"mgr\":\"dead\"") != std::string::npos);
    return 0;
}
