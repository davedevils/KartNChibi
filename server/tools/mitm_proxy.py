#!/usr/bin/env python3
"""MITM proxy for game server 50018.
Client connects to proxy (50118), proxy connects to real server (50018).
Logs every byte both directions with timestamps.
"""
import socket
import threading
import struct
import time
import sys
from datetime import datetime

PROXY_PORT = 55018
REAL_HOST = "127.0.0.1"
REAL_PORT = 50018

def print_flush(*args, **kwargs):
    kwargs['flush'] = True
    print(*args, **kwargs)


CMD_NAMES = {
    0x02: "DISPLAY_MSG", 0x07: "SESSION_INFO", 0x0A: "CONNECTION_OK",
    0x0E: "CHANNEL_LIST", 0x12: "SHOW_LOBBY", 0x16: "UI_STATE_14",
    0x1B: "INV_VEHICLES", 0x1C: "INV_ITEMS", 0x1D: "INV_ACCESSORIES",
    0x20: "NOOP_PADDING", 0x54: "REDIRECT",
    0x8E: "CLIENT_OK", 0xA6: "HEARTBEAT", 0xA7: "SESSION_CONFIRM",
    0xCB: "LOBBY_ENTER_CB", 0xCC: "LOBBY_ENTER_CC",
    0xFA: "FULL_STATE",
}

def cmd_name(cmd):
    return CMD_NAMES.get(cmd, f"UNK_0x{cmd:02X}")

def ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]

def parse_packets(buf, direction):
    """Try to parse as many packets as possible from buf. Return remaining bytes."""
    off = 0
    while off + 8 <= len(buf):
        size = struct.unpack_from("<H", buf, off)[0]
        cmd = buf[off + 2]
        flag = buf[off + 3]
        total = 8 + size
        if off + total > len(buf):
            break
        payload = bytes(buf[off + 8 : off + total])
        hex_preview = " ".join(f"{b:02x}" for b in payload[:32])
        if len(payload) > 32:
            hex_preview += " ..."
        print_flush(f"  [{ts()}] {direction} size={size} cmd=0x{cmd:02x} ({cmd_name(cmd)}) flag=0x{flag:02x} payload={hex_preview}")
        off += total
    return buf[off:]

def forward(src, dst, direction, state):
    try:
        while True:
            chunk = src.recv(8192)
            if not chunk:
                print_flush(f"[{ts()}] {direction} connection closed")
                break
            hex_preview = " ".join(f"{b:02x}" for b in chunk[:24])
            if len(chunk) > 24:
                hex_preview += " ..."
            print_flush(f"[{ts()}] {direction} +{len(chunk)} bytes raw={hex_preview}")
            dst.sendall(chunk)
            state['buf'] += chunk
            state['buf'] = parse_packets(state['buf'], direction)
    except Exception as e:
        print_flush(f"[{ts()}] {direction} err: {e}")

def handle(client_sock, client_addr):
    print_flush(f"[{ts()}] CLIENT CONNECT from {client_addr}")
    try:
        server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_sock.connect((REAL_HOST, REAL_PORT))
        print_flush(f"[{ts()}] PROXY -> real server {REAL_HOST}:{REAL_PORT}")

        c2s_state = {'buf': bytearray()}
        s2c_state = {'buf': bytearray()}

        t1 = threading.Thread(target=forward, args=(client_sock, server_sock, "C->S", c2s_state))
        t2 = threading.Thread(target=forward, args=(server_sock, client_sock, "S->C", s2c_state))
        t1.start()
        t2.start()
        t1.join()
        t2.join()
    finally:
        client_sock.close()
        print_flush(f"[{ts()}] CLIENT {client_addr} DISCONNECTED")

def main():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", PROXY_PORT))
    srv.listen(5)
    print_flush(f"[{ts()}] MITM proxy listening on :{PROXY_PORT} -> {REAL_HOST}:{REAL_PORT}")
    while True:
        c, a = srv.accept()
        threading.Thread(target=handle, args=(c, a), daemon=True).start()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
