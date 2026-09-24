#!/usr/bin/env python3
"""Logs in like KnCLauncher does and prints the session token the stock exe takes on its command line.

python tools/launcher_login.py <user> <password> [host] [port]
The wire is the 8 byte frame size u16 opcode u16 reserved u32 then the 0xFE payload user and password as C strings.
The answer is 0xFE with success u8 then the token then a message, both C strings.
"""
import socket
import struct
import sys


def frame(opcode, payload):
    return struct.pack('<HHI', len(payload), opcode, 0) + payload


def read_frame(sock):
    head = b''
    while len(head) < 8:
        chunk = sock.recv(8 - len(head))
        if not chunk:
            raise RuntimeError('closed')
        head += chunk
    size, opcode, _ = struct.unpack('<HHI', head)
    body = b''
    while len(body) < size:
        chunk = sock.recv(size - len(body))
        if not chunk:
            raise RuntimeError('closed')
        body += chunk
    return opcode, body


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    user, password = sys.argv[1], sys.argv[2]
    host = sys.argv[3] if len(sys.argv) > 3 else '127.0.0.1'
    port = int(sys.argv[4]) if len(sys.argv) > 4 else 50017
    sock = socket.create_connection((host, port), timeout=5)
    sock.sendall(frame(0xFE, user.encode() + b'\0' + password.encode() + b'\0'))
    while True:
        opcode, body = read_frame(sock)
        if opcode == 0xFE:
            break
    sock.close()
    success = body[0]
    rest = body[1:].split(b'\0')
    token = rest[0].decode(errors='replace')
    message = rest[1].decode(errors='replace') if len(rest) > 1 else ''
    if success != 1:
        print('refused', message, file=sys.stderr)
        return 1
    print(token)
    return 0


if __name__ == '__main__':
    sys.exit(main())
