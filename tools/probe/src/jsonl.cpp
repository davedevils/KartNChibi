#include "jsonl.h"

namespace probe {

static void append_hex(std::string& s, const uint8_t* data, uint32_t n) {
    static const char digits[] = "0123456789abcdef";
    for (uint32_t i = 0; i < n; i++) {
        s += digits[data[i] >> 4];
        s += digits[data[i] & 0x0F];
    }
}

std::string serialize_record(const ProbeRecord& rec, const ProbeConfig& cfg) {
    const HookDesc& hook = cfg.hooks[rec.hook_id];
    // names and labels come from our own probe cfg so they are not JSON escaped
    std::string s = "{";
    s += "\"seq\":" + std::to_string(rec.seq);
    s += ",\"t_us\":" + std::to_string(rec.t_us);
    s += ",\"thread\":" + std::to_string(rec.thread_id);
    s += ",\"hook\":\"" + hook.name + "\"";
    s += ",\"dir\":\"";
    s += (hook.dir == 2 ? "state" : hook.dir == 1 ? "out" : "in");
    s += "\"";
    if (rec.opcode != kNoOpcode) s += ",\"opcode\":" + std::to_string(rec.opcode);
    s += ",\"captures\":{";
    for (uint8_t i = 0; i < rec.blob_count; i++) {
        if (i) s += ",";
        s += "\"" + hook.captures[i].label + "\":\"";
        append_hex(s, rec.blobs[i].data, rec.blobs[i].size);
        s += "\"";
    }
    s += "}}";
    return s;
}

}  // namespace probe
