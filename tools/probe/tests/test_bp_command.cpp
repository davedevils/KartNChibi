#include "bp_command.h"
#include <cassert>
using namespace probe;
int main() {
    BpCommand a = parse_bp_command("bp 0x428390");
    assert(a.kind == BpCmdKind::Arm && a.addr == 0x428390 && !a.suspend && !a.int3);
    BpCommand b = parse_bp_command("bp 0x428390 suspend int3");
    assert(b.kind == BpCmdKind::Arm && b.suspend && b.int3);
    BpCommand c = parse_bp_command("bp 0x428390 trace hw");
    assert(c.kind == BpCmdKind::Arm && !c.suspend && !c.int3);
    BpCommand r = parse_bp_command("rmbp 0x428390");
    assert(r.kind == BpCmdKind::Remove && r.addr == 0x428390);
    BpCommand s = parse_bp_command("resume 0x428390");
    assert(s.kind == BpCmdKind::Resume && s.addr == 0x428390 && !s.resume_all);
    BpCommand sa = parse_bp_command("resume all");
    assert(sa.kind == BpCmdKind::Resume && sa.resume_all);
    BpCommand l = parse_bp_command("bplist");
    assert(l.kind == BpCmdKind::List);
    assert(parse_bp_command("snap").kind == BpCmdKind::None);    // unknown command and missing addr both parse to None
    assert(parse_bp_command("bp").kind == BpCmdKind::None);
    assert(parse_bp_command("bp notanaddr").kind == BpCmdKind::None);
    return 0;
}
