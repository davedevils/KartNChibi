#!/usr/bin/env python3
"""KnC packet injector + MITM proxy.

Sits between the game client and a KnC server and lets you INJECT raw packets on
the fly to test which packet drives which client screen (login -> char-create ->
channel -> DMV -> lobby). Reverse-engineering aid: you type an opcode, the client
reacts, you learn the flow.

Wire header (from Protocol.h, confirmed live): 8 bytes =
    size:u16(LE)  cmd:u8  flag:u8  pad:4 bytes    then `size` payload bytes.
The full opcode is the u16 cmd|flag<<8 (e.g. 0x132 -> cmd=0x32 flag=0x01).

Usage:
    # proxy the LOGIN server (where char-create + channel select happen)
    python packet_injector.py --listen 50117 --real 127.0.0.1:50017
    # proxy the GAME server (after redirect)
    python packet_injector.py --listen 55018 --real 127.0.0.1:50018

Point the client at --listen (via Network2.ini for login, or the login server's
redirect port for game). Then drive it from the control console:

    ncat 127.0.0.1 9999        (or telnet)
      s2c <op> [payload_hex]   inject a server->client packet   e.g.  s2c 03
      c2s <op> [payload_hex]   inject a client->server packet
      raw s2c <hex>            send fully pre-framed bytes to the client
      raw c2s <hex>            send fully pre-framed bytes to the server
      list                     show the active connection
    <op> accepts 03, 0x03, 0e, 132, 0x132 ...
"""
import argparse
import socket
import struct
import threading
import sys
from datetime import datetime

CMD_NAMES = {
    0x02: "DISPLAY_MSG", 0x03: "SET_GAME_VAR(charcreate)", 0x04: "REGISTER_NICK_RES",
    0x07: "SESSION_INFO", 0x0A: "CONNECTION_OK", 0x0E: "CHANNEL_LIST",
    0x0F: "SHOW_GARAGE", 0x10: "SHOW_SHOP", 0x11: "SHOW_MENU", 0x12: "SHOW_LOBBY",
    0x13: "ROOM_DATA", 0x16: "UI_STATE_14(DMV)", 0x1B: "INV_VEHICLES",
    0x1C: "INV_ITEMS", 0x1D: "INV_ACCESSORIES", 0x20: "NOOP_PADDING",
    0x54: "REDIRECT", 0x8E: "CLIENT_OK", 0xA6: "HEARTBEAT", 0xA7: "SESSION_CONFIRM",
}

# most recent live connection shared with the control console
ACTIVE = {"client": None, "server": None, "addr": None}
CLIENT_LOCK = threading.Lock()
SERVER_LOCK = threading.Lock()


def ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


def log(*a):
    print(*a, flush=True)


def cmd_name(cmd):
    return CMD_NAMES.get(cmd, f"UNK_0x{cmd:02X}")


def build_packet(opcode, payload=b""):
    cmd = opcode & 0xFF
    flag = (opcode >> 8) & 0xFF
    return struct.pack("<H", len(payload)) + bytes([cmd, flag]) + b"\x00\x00\x00\x00" + payload


def parse_packets(buf, direction):
    off = 0
    while off + 8 <= len(buf):
        size = struct.unpack_from("<H", buf, off)[0]
        cmd = buf[off + 2]
        flag = buf[off + 3]
        total = 8 + size
        if off + total > len(buf):
            break
        payload = bytes(buf[off + 8: off + total])
        preview = " ".join(f"{b:02x}" for b in payload[:32]) + (" ..." if len(payload) > 32 else "")
        log(f"  [{ts()}] {direction} size={size} cmd=0x{cmd:02x} ({cmd_name(cmd)}) flag=0x{flag:02x} payload={preview}")
        off += total
    return buf[off:]


def send_locked(sock, data, to_client):
    lock = CLIENT_LOCK if to_client else SERVER_LOCK
    with lock:
        sock.sendall(data)


def forward(src, dst, direction, to_client):
    buf = bytearray()
    try:
        while True:
            chunk = src.recv(8192)
            if not chunk:
                log(f"[{ts()}] {direction} connection closed")
                break
            send_locked(dst, chunk, to_client)
            buf += chunk
            buf = parse_packets(buf, direction)
    except OSError as e:
        log(f"[{ts()}] {direction} err: {e}")


def handle(client_sock, client_addr, real_host, real_port):
    log(f"[{ts()}] CLIENT CONNECT from {client_addr}")
    try:
        server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_sock.connect((real_host, real_port))
        log(f"[{ts()}] PROXY -> real server {real_host}:{real_port}")
        ACTIVE["client"], ACTIVE["server"], ACTIVE["addr"] = client_sock, server_sock, client_addr

        t1 = threading.Thread(target=forward, args=(client_sock, server_sock, "C->S", False), daemon=True)
        t2 = threading.Thread(target=forward, args=(server_sock, client_sock, "S->C", True), daemon=True)
        t1.start(); t2.start(); t1.join(); t2.join()
    finally:
        client_sock.close()
        log(f"[{ts()}] CLIENT {client_addr} DISCONNECTED")


def do_inject(line):
    """Parse and execute one control command. Returns a reply string."""
    parts = line.split()
    if not parts:
        return ""
    op = parts[0].lower()
    if op == "list":
        return f"active client={ACTIVE['addr']} connected={ACTIVE['client'] is not None}"
    if ACTIVE["client"] is None:
        return "no active connection yet (connect the client first)"

    try:
        if op == "raw" and len(parts) >= 3:
            to_client = parts[1].lower() == "s2c"
            data = bytes.fromhex(parts[2])
            send_locked(ACTIVE["client"] if to_client else ACTIVE["server"], data, to_client)
            return f"sent {len(data)} raw bytes {'->client' if to_client else '->server'}"
        if op in ("s2c", "c2s") and len(parts) >= 2:
            opcode = int(parts[1], 16)
            payload = bytes.fromhex(parts[2]) if len(parts) >= 3 else b""
            pkt = build_packet(opcode, payload)
            to_client = op == "s2c"
            send_locked(ACTIVE["client"] if to_client else ACTIVE["server"], pkt, to_client)
            log(f"[{ts()}] INJECT {op} opcode=0x{opcode:02x} ({cmd_name(opcode & 0xFF)}) {len(payload)}B payload")
            return f"injected 0x{opcode:02x} ({len(pkt)} bytes) {'->client' if to_client else '->server'}"
    except (ValueError, OSError) as e:
        return f"error: {e}"
    return "usage: s2c <op> [hex] | c2s <op> [hex] | raw s2c <hex> | list"


def control_client(conn, addr):
    conn.sendall(b"KnC injector. cmds: s2c <op> [hex] | c2s <op> [hex] | raw s2c <hex> | list\n> ")
    buf = b""
    try:
        while True:
            data = conn.recv(1024)
            if not data:
                break
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                reply = do_inject(line.decode(errors="replace").strip())
                conn.sendall((reply + "\n> ").encode())
    except OSError:
        pass
    finally:
        conn.close()


def control_server(port):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", port))
    srv.listen(5)
    log(f"[{ts()}] CONTROL console on 127.0.0.1:{port} (ncat 127.0.0.1 {port})")
    while True:
        c, a = srv.accept()
        threading.Thread(target=control_client, args=(c, a), daemon=True).start()


def main():
    ap = argparse.ArgumentParser(description="KnC packet injector + MITM proxy")
    ap.add_argument("--listen", type=int, default=50117, help="port the client connects to")
    ap.add_argument("--real", default="127.0.0.1:50017", help="host:port of the real server")
    ap.add_argument("--control", type=int, default=9999, help="control console port")
    args = ap.parse_args()
    real_host, real_port = args.real.split(":")
    real_port = int(real_port)

    threading.Thread(target=control_server, args=(args.control,), daemon=True).start()

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", args.listen))
    srv.listen(5)
    log(f"[{ts()}] INJECTOR proxy :{args.listen} -> {real_host}:{real_port}")
    while True:
        c, a = srv.accept()
        threading.Thread(target=handle, args=(c, a, real_host, real_port), daemon=True).start()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
