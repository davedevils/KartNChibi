#!/usr/bin/env python3
# rewrites the c2s 0x19 game address to this vps public ip and callback port and logs chibikart's callback

import socket, struct, threading, sys, time, os

CFG = {
    "listen_port": 50600, "target_host": "203.0.113.10", "target_port": 50017,
    "public_ip": "", "callback_port": 50601,
    "user": "changeme", "pass": "changeme", "version": "1", "inject": 1,
}

# maps mitm local ini keys to cfg keys the file is shared with the proxy and the parser
INI_KEYS = {
    "target_host": "target_host", "target_port": "target_port",
    "user": "user", "pass": "pass", "version": "version",
    "relay_listen_port": "listen_port", "relay_public_ip": "public_ip",
    "relay_callback_port": "callback_port", "relay_inject": "inject",
}

def load_ini():
    # git ignored real values copy mitm example ini to start one
    p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "mitm.local.ini")
    if not os.path.exists(p): return
    for ln in open(p):
        ln = ln.strip()
        if not ln or ln[0] in "#;" or "=" not in ln: continue
        k, v = [x.strip() for x in ln.split("=", 1)]
        if k not in INI_KEYS: continue
        ck = INI_KEYS[k]
        if ck in ("listen_port", "target_port", "callback_port", "inject"):
            if v != "": CFG[ck] = int(v)
        else:
            CFG[ck] = v

HDR = 8
def frame_iter(buf):
    off = 0
    while len(buf) - off >= HDR:
        plen = buf[off] | (buf[off+1] << 8)
        total = HDR + plen
        if total > 0x2000 or len(buf) - off < total: break
        yield buf[off:off+total]
        off += total
    return off

def u16(b, o): return b[o] | (b[o+1] << 8)

def ts(): return time.strftime("%H:%M:%S")

LOG = None
def log(*a):
    s = "[%s] " % ts() + " ".join(str(x) for x in a)
    print(s, flush=True)
    if LOG: LOG.write(s + "\n"); LOG.flush()

def wstr(s):
    out = bytearray()
    for c in s: out += bytes([ord(c) & 0xFF, (ord(c) >> 8) & 0xFF])
    out += b"\x00\x00"
    return bytes(out)

def build_login():
    # sends C FULL STATE 0xFA empty then C CLIENT AUTH 0x07 version action 4 user and pass
    fs = struct.pack("<HHI", 0, 0x00FA, 0)
    pay = bytearray()
    pay += CFG["version"].encode() + b"\x00"
    pay += struct.pack("<i", 4)
    pay += wstr(CFG["user"]); pay += wstr(CFG["pass"])
    auth = struct.pack("<HHI", len(pay), 0x0007, 0) + bytes(pay)
    return fs + auth

def is_login_family(op): return op in (0x07, 0xFA, 0xD0, 0xFE)

def rewrite_0x19(frame):
    # payload is cstring host u32 port and u32 mode rewritten to announce the vps
    pay = frame[HDR:]
    nul = pay.find(b"\x00")
    if nul < 0: return frame, None
    old_host = pay[:nul].decode("latin1", "replace")
    rest = pay[nul+1:]
    if len(rest) < 8: return frame, None
    old_port = struct.unpack_from("<I", rest, 0)[0]
    mode = struct.unpack_from("<I", rest, 4)[0]
    newpay = CFG["public_ip"].encode() + b"\x00" + struct.pack("<II", CFG["callback_port"], mode)
    newframe = struct.pack("<HHI", len(newpay), 0x0019, 0) + newpay
    return newframe, (old_host, old_port, mode)

def answer_launcher(sock):
    pay = b"\x01" + b"mitm\x00" + b"ok\x00"
    sock.sendall(struct.pack("<HHI", len(pay), 0x00FE, 0) + pay)

def callback_listener():
    s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", CFG["callback_port"])); s.listen(8)
    log("CALLBACK listener up on 0.0.0.0:%d (waiting for chibikart to connect back)" % CFG["callback_port"])
    while True:
        c, a = s.accept()
        log("*** CALLBACK CONNECTION from %s:%d  <-- this is chibikart reaching the announced address ***" % a)
        threading.Thread(target=cb_dump, args=(c, a), daemon=True).start()

def cb_dump(c, a):
    buf = b""
    try:
        while True:
            d = c.recv(4096)
            if not d: break
            buf += d
            log("CALLBACK %s:%d +%d bytes  hex=%s" % (a[0], a[1], len(d), d[:48].hex()))
    except Exception as e:
        log("callback closed", a, e)
    finally:
        c.close()

def pump(src, dst, tag, is_c2s):
    login_phase = {"v": True}
    buf = b""
    injected = False
    first = True
    try:
        while True:
            d = src.recv(16384)
            if not d: break
            buf += d
            off = 0
            while len(buf) - off >= HDR:
                plen = buf[off] | (buf[off+1] << 8)
                total = HDR + plen
                if total > 0x2000: buf = b""; off = 0; break
                if len(buf) - off < total: break
                fr = buf[off:off+total]; off += total
                op = u16(fr, 2)
                fwd = fr
                if is_c2s and op == 0x0019 and CFG["public_ip"]:
                    fwd, info = rewrite_0x19(fr)
                    log("REWROTE C2S 0x19 announce %s -> %s:%d" % (info, CFG["public_ip"], CFG["callback_port"]))
                drop = is_c2s and CFG["inject"] and login_phase["v"] and is_login_family(op)
                log("%s 0x%04X len=%d%s" % (tag, op, plen, "  DROP" if drop else ""))
                if not drop: dst.sendall(fwd)
                if (not is_c2s) and op == 0x0007: login_phase["v"] = False
            if off: buf = buf[off:]
    except Exception as e:
        log(tag, "closed", e)

def handle(client):
    up = socket.socket()
    try:
        up.connect((CFG["target_host"], CFG["target_port"]))
    except Exception as e:
        log("upstream connect failed", e); client.close(); return
    # peek first frame for launcher 0xFE
    client.settimeout(5)
    try:
        head = client.recv(16384)
    except Exception:
        head = b""
    client.settimeout(None)
    if CFG["inject"] and len(head) >= 4 and u16(head, 2) == 0x00FE:
        log("launcher login faked locally")
        answer_launcher(client); time.sleep(0.2); client.close(); up.close(); return
    log("client up, upstream %s:%d, injecting login" % (CFG["target_host"], CFG["target_port"]))
    if CFG["inject"]: up.sendall(build_login())
    # feed the already read head into the c2s pump by prepending
    class Pre:
        def __init__(s, sock, pre): s.sock = sock; s.pre = pre
        def recv(s, n):
            if s.pre:
                d = s.pre; s.pre = b""; return d
            return s.sock.recv(n)
        def sendall(s, d): return s.sock.sendall(d)
    t = threading.Thread(target=pump, args=(Pre(client, head), up, "C2S", True), daemon=True)
    t.start()
    pump(up, client, "S2C", False)
    try: client.close()
    except: pass
    try: up.close()
    except: pass

def main():
    global LOG
    load_ini()
    LOG = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "relay.log"), "a")
    if not CFG["public_ip"]:
        log("WARNING public_ip not set, C2S 0x19 will NOT be rewritten, callback stays unreachable")
    threading.Thread(target=callback_listener, daemon=True).start()
    s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", CFG["listen_port"])); s.listen(16)
    log("RELAY up 0.0.0.0:%d -> %s:%d  public_ip=%s callback=%d" % (
        CFG["listen_port"], CFG["target_host"], CFG["target_port"], CFG["public_ip"], CFG["callback_port"]))
    while True:
        c, a = s.accept()
        threading.Thread(target=handle, args=(c,), daemon=True).start()

if __name__ == "__main__":
    main()
