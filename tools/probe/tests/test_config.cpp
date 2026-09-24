#include "config.h"
#include <cassert>

using namespace probe;

int main() {
    const std::string text =
        "# comment line\n"
        "module=HBOnline.exe\n"
        "image_base=0x400000\n"
        "jsonl=probe_capture.jsonl\n"
        "tcp_port=8077\n"
        "ring_capacity=4096\n"
        "max_capture_bytes=512\n"
        "\n"
        "hook=dispatch dir=in addr=0x0050d8a0\n"
        "capture=opcode src=arg1 size=2 as=val\n"
        "capture=struct src=arg2 size=96\n"
        "\n"
        "hook=send dir=out addr=0x00420000\n"
        "capture=wire src=ecx deref=0,16 size=64\n";

    ProbeConfig cfg;
    std::string err;
    assert(parse_config(text, cfg, err));
    assert(err.empty());

    assert(cfg.module == "HBOnline.exe");
    assert(cfg.image_base == 0x400000);
    assert(cfg.jsonl_path == "probe_capture.jsonl");
    assert(cfg.tcp_port == 8077);
    assert(cfg.ring_capacity == 4096);
    assert(cfg.max_capture_bytes == 512);
    assert(cfg.hooks.size() == 2);

    const HookDesc& d = cfg.hooks[0];
    assert(d.name == "dispatch");
    assert(d.address == 0x0050d8a0);
    assert(d.dir == 0);
    assert(d.captures.size() == 2);
    assert(d.captures[0].label == "opcode");
    assert(d.captures[0].source == SourceKind::Arg);
    assert(d.captures[0].source_index == 1);
    assert(d.captures[0].mode == CaptureMode::Val);
    assert(d.captures[0].size == 2);
    assert(d.captures[1].source == SourceKind::Arg);
    assert(d.captures[1].source_index == 2);
    assert(d.captures[1].mode == CaptureMode::Ptr);
    assert(d.captures[1].deref.empty());

    const HookDesc& s = cfg.hooks[1];
    assert(s.name == "send");
    assert(s.dir == 1);
    assert(s.captures[0].source == SourceKind::Ecx);
    assert(s.captures[0].deref.size() == 2);
    assert(s.captures[0].deref[0] == 0);
    assert(s.captures[0].deref[1] == 16);

    // rebase is identity when bases match shifts by the delta otherwise
    assert(rebase_address(0x0050d8a0, 0x400000, 0x400000) == 0x0050d8a0);
    assert(rebase_address(0x0050d8a0, 0x400000, 0x500000) == 0x0060d8a0);

    // a capture before any hook is an error
    ProbeConfig bad; std::string berr;
    assert(!parse_config("capture=x src=eax size=4\n", bad, berr));
    assert(!berr.empty());

    // esp offset source parses to EspOff with the byte offset as the index
    ProbeConfig esp; std::string esperr;
    assert(parse_config(
        "module=m.exe\nhook=h dir=in addr=0x1000\ncapture=c src=esp+8 size=4\n", esp, esperr));
    assert(esp.hooks[0].captures[0].source == SourceKind::EspOff);
    assert(esp.hooks[0].captures[0].source_index == 8);

    // a decimal address parses correctly not just hex
    ProbeConfig dec; std::string decerr;
    assert(parse_config("module=m.exe\nhook=h dir=in addr=12345\n", dec, decerr));
    assert(dec.hooks[0].address == 12345);

    // CRLF line endings are tolerated
    ProbeConfig crlf; std::string crlferr;
    assert(parse_config(
        "module=m.exe\r\ntcp_port=9000\r\nhook=h dir=in addr=0x1000\r\n", crlf, crlferr));
    assert(crlf.tcp_port == 9000);

    // a malformed number fails gracefully no crash a non-empty error
    ProbeConfig num; std::string numerr;
    assert(!parse_config("module=m.exe\nimage_base=zzz\nhook=h dir=in addr=0x1000\n", num, numerr));
    assert(!numerr.empty());

    // a missing required module is rejected
    ProbeConfig nomod; std::string nomoderr;
    assert(!parse_config("hook=h dir=in addr=0x1000\n", nomod, nomoderr));
    assert(!nomoderr.empty());

    // a config with no hooks is rejected
    ProbeConfig nohooks; std::string nohookserr;
    assert(!parse_config("module=m.exe\n", nohooks, nohookserr));
    assert(!nohookserr.empty());

    // more than kMaxCapturesPerHook captures on one hook is rejected
    ProbeConfig toomanycaps; std::string toomanycapserr;
    assert(!parse_config(
        "module=m.exe\nhook=h dir=in addr=0x1000\n"
        "capture=a src=eax size=4\n"
        "capture=b src=eax size=4\n"
        "capture=c src=eax size=4\n"
        "capture=d src=eax size=4\n"
        "capture=e src=eax size=4\n",
        toomanycaps, toomanycapserr));
    assert(!toomanycapserr.empty());

    // exactly kMaxCapturesPerHook 4 captures on one hook is accepted
    ProbeConfig maxcaps; std::string maxcapserr;
    assert(parse_config(
        "module=m.exe\nhook=h dir=in addr=0x1000\n"
        "capture=a src=eax size=4\n"
        "capture=b src=eax size=4\n"
        "capture=c src=eax size=4\n"
        "capture=d src=eax size=4\n",
        maxcaps, maxcapserr));
    assert(maxcaps.hooks[0].captures.size() == 4);

    // an arg index above 32 is rejected
    ProbeConfig badarg; std::string badargerr;
    assert(!parse_config(
        "module=m.exe\nhook=h dir=in addr=0x1000\ncapture=c src=arg33 size=4\n",
        badarg, badargerr));
    assert(!badargerr.empty());

    // an esp offset above 4096 is rejected
    ProbeConfig badesp; std::string badesperr;
    assert(!parse_config(
        "module=m.exe\nhook=h dir=in addr=0x1000\ncapture=c src=esp+4097 size=4\n",
        badesp, badesperr));
    assert(!badesperr.empty());

    // abs source plus poll block
    const std::string ptext =
        "module=HBOnline.exe\n"
        "image_base=0x400000\n"
        "ring_capacity=4096\n"
        "max_capture_bytes=512\n"
        "hook=dispatch dir=in addr=0x004ecd00\n"
        "capture=opcode src=arg0 deref=4,4 size=2 as=ptr\n"
        "poll=player interval_ms=500\n"
        "capture=mgr src=abs:0x75b914 size=4\n"
        "capture=local src=abs:0x75b914 deref=0,0x78 size=512\n";
    ProbeConfig pc; std::string perr;
    assert(parse_config(ptext, pc, perr));
    assert(pc.hooks.size() == 2);
    assert(pc.hooks[0].is_poll == false);
    const HookDesc& pl = pc.hooks[1];
    assert(pl.name == "player");
    assert(pl.is_poll == true);
    assert(pl.interval_ms == 500);
    assert(pl.dir == 2);
    assert(pl.captures.size() == 2);
    assert(pl.captures[0].source == SourceKind::Abs);
    assert(pl.captures[0].source_index == 0x75b914);
    assert(pl.captures[0].mode == CaptureMode::Ptr);  // default
    assert(pl.captures[1].source == SourceKind::Abs);
    assert(pl.captures[1].deref.size() == 2 && pl.captures[1].deref[1] == 0x78);

    // a poll capture using a non-abs source is rejected
    ProbeConfig bad2; std::string berr2;
    assert(!parse_config("poll=p interval_ms=10\ncapture=x src=eax size=4\n", bad2, berr2));
    assert(!berr2.empty());

    // rebase shifts only abs captures by runtime base minus image base
    rebase_abs_captures(pc, 0x500000);
    assert(pc.hooks[1].captures[0].source_index == 0x85b914);
    assert(pc.hooks[0].captures[0].source == SourceKind::Arg);  // non-abs untouched
    assert(pc.hooks[0].captures[0].source_index == 0);

    // an explicit dir on a poll line is overridden to state 2
    ProbeConfig polldir; std::string polldirerr;
    assert(parse_config(
        "module=m.exe\npoll=p2 dir=in interval_ms=5\ncapture=x src=abs:0x1000 size=4\n",
        polldir, polldirerr));
    assert(polldir.hooks[0].dir == 2);

    return 0;
}
