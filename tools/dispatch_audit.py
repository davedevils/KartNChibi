#!/usr/bin/env python3
"""Dispatch audit, every opcode constant that cites a client address.

Many constants in Protocol.h and the generated packet headers carry a comment
naming the client function their layout was reversed from, `// sub_47CC20`. That
comment is checkable. The client dispatcher is

    movzx eax, ax
    dec   eax
    cmp   eax, 0x134
    ja    default
    movzx eax, byte ptr [eax + 0x478638]
    jmp   dword ptr [eax*4 + 0x4782D8]

so opcode minus one indexes a 309 byte table, which indexes 216 jump slots, and
slot 215 is a bare `pop edi; pop esi; ret 4`. Resolve each slot to its handler
body and you get body address -> opcode. If a constant says `sub_X` and the table
says X belongs to a different opcode, the constant is wrong.

That `dec eax` is the trap. Reading the index as the opcode is off by one, and it
found five real bugs, the friend and gacha blocks were shipping correct payloads
under wrong numbers so the client dropped every one of them on the floor.

C_ constants cite their sender function instead, which is not in the dispatch
table, so they are reported separately and are not errors.

Run it with opcode_audit py and opcode_truncation py.
"""
import re
import io
import sys
import glob
import struct
import os

import capstone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, 'DevClient', 'KnC.exe')
IDX_TABLE = 0x478638
JMP_TABLE = 0x4782D8
N_OPCODES = 0x135
N_SLOTS = 216
DEFAULT_SLOT = 215


def load_pe(path):
    d = io.open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    ns = struct.unpack_from('<H', d, pe + 6)[0]
    osz = struct.unpack_from('<H', d, pe + 20)[0]
    base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
    secs = []
    for i in range(ns):
        o = pe + 24 + osz + 40 * i
        vsz, va, rsz, ro = struct.unpack_from('<IIII', d, o + 8)
        secs.append((base + va, vsz, ro))
    return d, secs


def reader(d, secs):
    def rd(a, n):
        for va, vsz, ro in secs:
            if va <= a < va + vsz:
                o = ro + (a - va)
                return d[o:o + n]
        return b''
    return rd


def build_map(rd):
    """body address -> the opcodes that reach it, plus the stub addresses."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    idx = [rd(IDX_TABLE + i, 1)[0] for i in range(N_OPCODES)]
    jt = [struct.unpack('<I', rd(JMP_TABLE + 4 * i, 4))[0] for i in range(N_SLOTS)]

    def resolve(a):
        for ins in md.disasm(rd(a, 24), a):
            if ins.mnemonic in ('jmp', 'call') and ins.op_str.startswith('0x'):
                return int(ins.op_str, 16)
            if ins.mnemonic in ('ret', 'pop'):
                return a
        return a

    addr2op = {}
    for op in range(1, N_OPCODES + 1):
        slot = idx[op - 1]
        if slot == DEFAULT_SLOT:
            continue
        addr2op.setdefault(jt[slot], []).append(op)
        addr2op.setdefault(resolve(jt[slot]), []).append(op)
    return addr2op


DECL = re.compile(r'constexpr\s+\w+\s+([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;\s*//\s*(.*)')
SUB = re.compile(r'sub_([0-9A-Fa-f]{6})|\b0x([0-9A-Fa-f]{6})\b')


def main():
    if not os.path.exists(EXE):
        print('client binary not found at %s' % EXE)
        return 0
    d, secs = load_pe(EXE)
    addr2op = build_map(reader(d, secs))

    sources = [os.path.join(ROOT, 'shared/src/include/net/Protocol.h')]
    for pat in ('server/**/*.h', 'server/**/*.cpp'):
        sources += glob.glob(os.path.join(ROOT, pat), recursive=True)

    agree = wrong = senders = 0
    problems = []
    for f in sources:
        for line in io.open(f, encoding='utf-8', errors='ignore'):
            m = DECL.search(line)
            if not m:
                continue
            s = SUB.search(m.group(3))
            if not s:
                continue
            name, val = m.group(1), int(m.group(2), 16)
            addr = int(s.group(1) or s.group(2), 16)
            real = addr2op.get(addr)
            if real is None:
                senders += 1  # a C constant naming its sender not a handler
                continue
            if val in real:
                agree += 1
            else:
                wrong += 1
                problems.append((name, val, addr, sorted(set(real))))

    print('constants citing a handler address : %d' % (agree + wrong))
    print('  agree with the dispatch table    : %d' % agree)
    print('  disagree                         : %d' % wrong)
    print('constants citing a sender address  : %d  (not checkable here)' % senders)
    for name, val, addr, real in problems:
        print('  FAIL %s = 0x%03X but 0x%X is opcode %s' % (
            name, val, addr, ' '.join('0x%03X' % r for r in real)))
    return 1 if wrong else 0


if __name__ == '__main__':
    sys.exit(main())
