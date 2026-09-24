#!/usr/bin/env python3
"""Opcode truncation audit.

Packet(uint8_t cmd, uint8_t flag) narrows silently, so Packet r(SOMETHING_0x114)
goes out on the wire as 0x14 and the client never sees the reply it is waiting on.
The header says use fromCmdFull for anything over 255. Nothing enforced it.

Run it with opcode_audit py before trusting a reply path.
"""
import re, io, glob, os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
vals = {}
for f in glob.glob('shared/**/*.h', recursive=True) + glob.glob('server/**/*.h', recursive=True):
    txt = io.open(f, encoding='utf-8', errors='ignore').read()
    cls = None
    for line in txt.splitlines():
        m = re.match(r'\s*(?:class|struct|namespace)\s+([A-Za-z_]\w*)', line)
        if m: cls = m.group(1)
        m = re.search(r'constexpr\s+\w+\s+([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;', line)
        if m:
            n, v = m.group(1), int(m.group(2), 16)
            vals.setdefault(n, v)
            if cls: vals.setdefault(cls + '::' + n, v)
bad = []
pat = re.compile(r'Packet\s+\w+\s*\(\s*([A-Za-z_][\w:]*|0x[0-9A-Fa-f]+)\s*[,)]')
for f in glob.glob('server/**/*.cpp', recursive=True):
    for i, line in enumerate(io.open(f, encoding='utf-8', errors='ignore'), 1):
        if 'fromCmdFull' in line: continue
        m = pat.search(line)
        if not m: continue
        tok = m.group(1)
        v = int(tok, 16) if tok.startswith('0x') else vals.get(tok, vals.get(tok.split('::')[-1]))
        if v is not None and v > 0xFF:
            bad.append((f.replace(os.sep, '/'), i, tok, v))
print('Packet ctor calls that silently truncate an opcode over 0xFF: %d' % len(bad))
for f, i, t, v in bad:
    print('  %s:%d  %s = 0x%03X sent as 0x%02X' % (f, i, t, v, v & 0xFF))

# constructor takes 16 bits so narrowing comes only from a byte cast which dropped 0x104 0x10C 0x10D 0x10E for months
narrow = []
cast = re.compile(r'Packet\s*(?:\w+\s*)?\(\s*static_cast<\s*uint8_t\s*>')
param = re.compile(r'Packet\s+\w+\s*\([^)]*\buint8_t\s+(op|opcode|cmd)\b')
for f in glob.glob('server/**/*.cpp', recursive=True) + glob.glob('server/**/*.h', recursive=True):
    for i, line in enumerate(io.open(f, encoding='utf-8', errors='ignore'), 1):
        if cast.search(line):
            narrow.append((f.replace(os.sep, '/'), i, 'byte cast into a Packet', line.strip()))
        if param.search(line):
            narrow.append((f.replace(os.sep, '/'), i, 'helper takes a byte opcode', line.strip()))
print('Byte sized opcode paths that can still narrow: %d' % len(narrow))
for f, i, why, line in narrow:
    print('  %s:%d  %s  %s' % (f, i, why, line[:90]))
