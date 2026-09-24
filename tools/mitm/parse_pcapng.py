# parses a direct chibikart pcapng into KnC frames splitting reassembled TCP on the 8 byte header port 50017

import sys, os, struct, collections

def default_target_host():
    # reads mitm local ini git ignored falls back to a placeholder when absent
    p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "mitm.local.ini")
    if os.path.exists(p):
        for ln in open(p):
            ln = ln.strip()
            if ln.startswith("target_host"):
                return ln.split("=", 1)[1].strip()
    return "203.0.113.10"

def read_pcapng(path):
    # minimal pcapng reader yields linktype and packet bytes for each block
    out = []
    with open(path, "rb") as f:
        blob = f.read()
    off = 0
    n = len(blob)
    linktype = 1
    while off + 12 <= n:
        btype, blen = struct.unpack_from("<II", blob, off)
        if blen < 12 or off + blen > n:
            break
        body = blob[off+8:off+blen-4]
        if btype == 0x00000001:            # Interface Description
            linktype = struct.unpack_from("<H", body, 0)[0]
        elif btype == 0x00000006:          # Enhanced Packet Block
            _iface, th, tl, caplen, origlen = struct.unpack_from("<IIIII", body, 0)
            pkt = body[20:20+caplen]
            out.append((linktype, pkt))
        elif btype == 0x00000003:          # Simple Packet Block
            origlen = struct.unpack_from("<I", body, 0)[0]
            out.append((linktype, body[4:4+origlen]))
        off += blen
    return out

def parse_eth_ip_tcp(linktype, pkt):
    # returns src ip src port dst ip dst port and payload or none
    p = pkt
    if linktype == 1:                       # Ethernet
        if len(p) < 14: return None
        eth = struct.unpack_from(">H", p, 12)[0]
        if eth != 0x0800: return None
        p = p[14:]
    # else assume raw IP
    if len(p) < 20 or (p[0] >> 4) != 4: return None
    ihl = (p[0] & 0xF) * 4
    proto = p[9]
    if proto != 6: return None
    src = ".".join(str(b) for b in p[12:16])
    dst = ".".join(str(b) for b in p[16:20])
    t = p[ihl:]
    if len(t) < 20: return None
    sport, dport = struct.unpack_from(">HH", t, 0)
    seq = struct.unpack_from(">I", t, 4)[0]
    doff = (t[12] >> 4) * 4
    payload = t[doff:]
    return (src, sport, dst, dport, seq, payload)

OPNAMES = {0x02:"DISPLAY_MSG",0x07:"AUTH/PLAYERINFO",0x0E:"CHANNEL_LIST",0x11:"MENU",
 0x12:"LOBBY",0x13:"ROOM_CONTEXT",0x19:"SERVER_REDIRECT",0x21:"ROOM_MEMBER",0x2D:"ROOM_ADD",
 0x30:"ROOM_STATE",0x32:"SLOT_ENABLED",0x34:"ALL_START",0x3A:"RACE_GO",0x3C:"FINISH",
 0x40:"MOTION",0x41:"CHECKPOINT",0x54:"GAME_REDIRECT",0xA7:"SESSION_CONFIRM"}

def frames(stream):
    off = 0
    while len(stream) - off >= 8:
        plen = stream[off] | (stream[off+1] << 8)
        total = 8 + plen
        if total > 0x2000 or len(stream) - off < total:
            break
        op = stream[off+2] | (stream[off+3] << 8)
        yield op, stream[off:off+total]
        off += total
    return

def main():
    path = sys.argv[1]
    sip = sys.argv[2] if len(sys.argv) > 2 else default_target_host()
    sport = int(sys.argv[3]) if len(sys.argv) > 3 else 50017
    pkts = read_pcapng(path)
    # reassemble per direction by seq
    segs = {"C2S": {}, "S2C": {}}
    for lt, pk in pkts:
        r = parse_eth_ip_tcp(lt, pk)
        if not r: continue
        src, sp, dst, dp, seq, pay = r
        if not pay: continue
        if dst == sip and dp == sport: d = "C2S"
        elif src == sip and sp == sport: d = "S2C"
        else: continue
        segs[d][seq] = pay
    for d in ("C2S", "S2C"):
        stream = b"".join(segs[d][s] for s in sorted(segs[d]))
        print("==== %s  %d bytes ===="%(d, len(stream)))
        for op, fr in frames(stream):
            nm = OPNAMES.get(op, "?")
            print("  %s 0x%04X %-16s len=%d  %s"%(d, op, nm, len(fr)-8, fr[8:8+24].hex()))

if __name__ == "__main__":
    main()
