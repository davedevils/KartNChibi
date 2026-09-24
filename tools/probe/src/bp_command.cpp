#include "bp_command.h"
#include <sstream>
#include <vector>
namespace probe {
static bool parse_addr(const std::string& s, uint32_t& out) {
    try { size_t n = 0; unsigned long v = std::stoul(s, &n, 0); if (n != s.size()) return false; out = (uint32_t)v; return true; }
    catch (...) { return false; }
}
BpCommand parse_bp_command(const std::string& line) {
    std::istringstream ss(line);
    std::vector<std::string> tok; std::string t;
    while (ss >> t) tok.push_back(t);
    BpCommand c;
    if (tok.empty()) return c;
    if (tok[0] == "bplist") { c.kind = BpCmdKind::List; return c; }
    if (tok[0] == "bp" && tok.size() >= 2 && parse_addr(tok[1], c.addr)) {
        c.kind = BpCmdKind::Arm;
        for (size_t i = 2; i < tok.size(); i++) {
            if (tok[i] == "suspend") c.suspend = true;
            else if (tok[i] == "int3") c.int3 = true;
            // trace and hw are the defaults and are ignored
        }
        return c;
    }
    if (tok[0] == "rmbp" && tok.size() >= 2 && parse_addr(tok[1], c.addr)) { c.kind = BpCmdKind::Remove; return c; }
    if (tok[0] == "resume" && tok.size() >= 2) {
        if (tok[1] == "all") { c.kind = BpCmdKind::Resume; c.resume_all = true; return c; }
        if (parse_addr(tok[1], c.addr)) { c.kind = BpCmdKind::Resume; return c; }
    }
    return c;
}
}  // namespace probe
