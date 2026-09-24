#include "bp_serialize.h"
#include <cassert>
#include <string>
using namespace probe;
int main() {
    BpHit h{};
    h.seq = 7; h.t_us = 42; h.thread_id = 9; h.addr = 0x00428390; h.mode = 1;
    h.regs[0] = 0x11; h.regs[4] = 0x22;   // eax and esp
    h.eip = 0x00428390; h.eflags = 0x206;
    h.stack_len = 2; h.stack[0] = 0xDE; h.stack[1] = 0xAD;
    h.code_len = 1; h.code[0] = 0x55;
    const std::string s = serialize_bp_hit(h);
    assert(s.find("\"dir\":\"bp\"") != std::string::npos);
    assert(s.find("\"addr\":\"0x00428390\"") != std::string::npos);
    assert(s.find("\"mode\":\"suspend\"") != std::string::npos);
    assert(s.find("\"eax\":\"0x00000011\"") != std::string::npos);
    assert(s.find("\"eip\":\"0x00428390\"") != std::string::npos);
    assert(s.find("\"stack\":\"dead\"") != std::string::npos);
    assert(s.find("\"code\":\"55\"") != std::string::npos);
    return 0;
}
