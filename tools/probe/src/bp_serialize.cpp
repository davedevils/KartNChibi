#include "bp_serialize.h"
#include <cstdio>
namespace probe {
static void hex_bytes(std::string& s, const uint8_t* d, uint32_t n) {
    static const char* H = "0123456789abcdef";
    for (uint32_t i = 0; i < n; i++) { s += H[d[i] >> 4]; s += H[d[i] & 0xF]; }
}
static std::string hex32(uint32_t v) { char b[11]; std::snprintf(b, sizeof b, "0x%08x", v); return b; }
std::string serialize_bp_hit(const BpHit& h) {
    static const char* names[8] = {"eax","ecx","edx","ebx","esp","ebp","esi","edi"};
    std::string s = "{";
    s += "\"seq\":" + std::to_string(h.seq);
    s += ",\"t_us\":" + std::to_string(h.t_us);
    s += ",\"thread\":" + std::to_string(h.thread_id);
    s += ",\"dir\":\"bp\"";
    s += ",\"addr\":\"" + hex32(h.addr) + "\"";
    s += ",\"mode\":\"";
    s += (h.mode ? "suspend" : "trace");
    s += "\",\"regs\":{";
    for (int i = 0; i < 8; i++) { if (i) s += ","; s += "\"" + std::string(names[i]) + "\":\"" + hex32(h.regs[i]) + "\""; }
    s += ",\"eip\":\"" + hex32(h.eip) + "\"";
    s += ",\"eflags\":\"" + hex32(h.eflags) + "\"}";
    s += ",\"stack\":\""; hex_bytes(s, h.stack, h.stack_len); s += "\"";
    s += ",\"code\":\"";  hex_bytes(s, h.code, h.code_len);  s += "\"";
    s += "}";
    return s;
}
}  // namespace probe
