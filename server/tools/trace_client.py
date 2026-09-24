#!/usr/bin/env python3
"""Trace game server protocol by mimicking KnC.exe client.

Connects to game server with a pre-seeded session and logs every byte received.
Compares against expected packet structure to find mismatches.
"""
import socket
import struct
import sys
import time
import mysql.connector

def peek_packet(buf, off):
    """Return (size, cmd, flag) or None if not enough bytes."""
    if len(buf) - off < 8:
        return None
    size = struct.unpack_from("<H", buf, off)[0]
    cmd = buf[off + 2]
    flag = buf[off + 3]
    return (size, cmd, flag)


def dump_hex(data, max_bytes=80):
    return " ".join(f"{b:02x}" for b in data[:max_bytes])


CMD_NAMES = {
    0x02: "DISPLAY_MESSAGE",
    0x07: "SESSION_INFO",
    0x0A: "CONNECTION_OK",
    0x0E: "CHANNEL_LIST",
    0x12: "SHOW_LOBBY",
    0x1B: "INV_VEHICLES",
    0x1C: "INV_ITEMS",
    0x1D: "INV_ACCESSORIES",
    0x54: "REDIRECT",
    0xA6: "HEARTBEAT",
    0xA7: "SESSION_CONFIRM",
}


def cmd_name(cmd):
    return CMD_NAMES.get(cmd, f"UNKNOWN_0x{cmd:02X}")


def seed_active_session(account_id=1, char_id=0, token="TRACE_TOKEN", ip="127.0.0.1"):
    """Insert a fake active session so the game server accepts us."""
    db = mysql.connector.connect(
        host="127.0.0.1", port=3307, user="knc",
        password="knc_password", database="knc_emu",
        charset="utf8mb4", collation="utf8mb4_general_ci"
    )
    cur = db.cursor()
    cur.execute("DELETE FROM active_sessions WHERE client_ip = %s", (ip,))
    cur.execute(
        "INSERT INTO active_sessions (account_id, character_id, token, client_ip, created_at) "
        "VALUES (%s, %s, %s, %s, NOW())",
        (account_id, char_id, token, ip),
    )
    db.commit()
    cur.close()
    db.close()
    print(f"[DB] seeded active_sessions: acc={account_id} ip={ip} token={token}")


def main():
    seed_active_session()

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect(("127.0.0.1", 50018))
    print(f"[NET] connected to game server 50018")

    # sends 0xA7 session confirm like real client after a 1 second sleep header size 14 pad 0
    pkt_a7 = struct.pack("<HHI", 14, 0xA7, 0)
    pkt_a7 += b"178\x00"                         # version string
    pkt_a7 += struct.pack("<I", 8)               # state
    pkt_a7 += b"\x00\x00"                        # empty wstring null
    pkt_a7 += struct.pack("<I", 0)               # driverId
    s.sendall(pkt_a7)
    print(f"[SEND] 0xA7 session confirm ({len(pkt_a7)} bytes)")

    buf = bytearray()
    total_recv = 0
    deadline = time.time() + 3.0
    while time.time() < deadline:
        try:
            chunk = s.recv(8192)
        except socket.timeout:
            break
        if not chunk:
            print("[NET] server closed connection")
            break
        total_recv += len(chunk)
        buf.extend(chunk)
        print(f"[RECV] +{len(chunk)} bytes (total {total_recv})")

    print(f"\n[TOTAL] {total_recv} bytes received in buffer\n")
    print("=" * 80)
    print("PACKET ANALYSIS")
    print("=" * 80)

    off = 0
    pkt_num = 1
    while off < len(buf):
        hdr = peek_packet(buf, off)
        if hdr is None:
            print(f"[{pkt_num}] incomplete header at offset {off}: "
                  f"{dump_hex(buf[off:])}")
            break
        size, cmd, flag = hdr
        total = 8 + size
        if off + total > len(buf):
            print(f"[{pkt_num}] incomplete packet at offset {off}: "
                  f"header size={size} cmd=0x{cmd:02x} but only "
                  f"{len(buf) - off} bytes avail")
            break
        payload = bytes(buf[off + 8 : off + total])
        print(f"\n[{pkt_num}] offset {off}: "
              f"size={size} cmd=0x{cmd:02x} ({cmd_name(cmd)}) flag=0x{flag:02x}")
        print(f"    payload: {dump_hex(payload, 48)}"
              f"{' ...' if len(payload) > 48 else ''}")
        off += total
        pkt_num += 1

    print(f"\n[REMAINING] {len(buf) - off} bytes after last parseable packet")
    s.close()


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"[ERROR] {e}")
        sys.exit(1)
