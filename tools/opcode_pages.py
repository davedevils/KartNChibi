#!/usr/bin/env python3
"""One markdown file per opcode under docs/packets/opcodes plus the index and the direction tables.

split   cut section 4 of PACKET_REGISTRY.md into one file per opcode, run once
index   rebuild docs/packets/opcodes/README.md and the S2C and C2S tables of PACKET_REGISTRY.md from the files
check   every file has its header table and every opcode of the tables has a file
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REGISTRY = os.path.join(ROOT, 'docs', 'packets', 'PACKET_REGISTRY.md')
PAGES = os.path.join(ROOT, 'docs', 'packets', 'opcodes')

HEADING = re.compile(r'^#### (0x[0-9A-Fa-f]{4}) (S2C|C2S) (.+?)\s*$')
# a family heading names several opcodes that share one payload block
FAMILY = re.compile(r'^#### ((?:0x[0-9A-Fa-f]{4}(?:,\s*|\s+and\s+|\s+))+)(S2C|C2S)?\s*(.+?)\s*$')
TABLE_ROW = re.compile(r'^\| (0x[0-9A-Fa-f]{4}) \| (.*?) \| (.*?) \| (.*?) \| (.*?) \| (.*?) \| (.*?) \|\s*$')


def norm(op):
    return '0x' + op[2:].upper()


def read(path):
    with open(path, encoding='utf-8') as f:
        return f.read()


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)


def table_rows(lines, start, end):
    rows = {}
    for line in lines[start:end]:
        m = TABLE_ROW.match(line)
        if m and m.group(2) != 'Name':
            rows[norm(m.group(1))] = [g.strip() for g in m.groups()[1:]]
    return rows


def section_bounds(lines, title):
    start = next(i for i, l in enumerate(lines) if l.startswith(title))
    end = next((i for i in range(start + 1, len(lines)) if re.match(r'^## ', lines[i])), len(lines))
    return start, end


def split():
    text = read(REGISTRY)
    lines = text.split('\n')
    s2c_start, s2c_end = section_bounds(lines, '## 2. S2C table')
    c2s_start, c2s_end = section_bounds(lines, '## 3. C2S table')
    pay_start, pay_end = section_bounds(lines, '## 4. Payloads')
    s2c = table_rows(lines, s2c_start, s2c_end)
    c2s = table_rows(lines, c2s_start, c2s_end)

    blocks = {}
    shared = {}
    current = None
    for line in lines[pay_start:pay_end]:
        m = HEADING.match(line)
        f = None if m else FAMILY.match(line)
        if m:
            op = norm(m.group(1))
            current = (op, m.group(2), m.group(3).strip())
            blocks.setdefault(op, []).append([current, []])
            continue
        if f:
            members = [norm(x) for x in re.findall(r'0x[0-9A-Fa-f]{4}', f.group(1))]
            direction = f.group(2) or 'S2C and C2S'
            name = f.group(3).strip() + ' (family ' + ' '.join(members) + ')'
            current = (members[0], direction, name)
            blocks.setdefault(members[0], []).append([current, []])
            for other in members[1:]:
                shared.setdefault(other, []).append((direction, f.group(3).strip(), members[0]))
            continue
        if re.match(r'^### ', line):
            current = None
            continue
        if current is not None:
            blocks[current[0]][-1][1].append(line)

    ops = sorted(set(blocks) | set(s2c) | set(c2s) | set(shared), key=lambda o: int(o, 16))
    written = 0
    for op in ops:
        rows = []
        for direction, table in (('S2C', s2c), ('C2S', c2s)):
            r = table.get(op)
            if r:
                rows.append('| %s | %s | %s | %s | %s | %s | %s |' % (direction, r[0], r[1], r[2], r[3], r[4], r[5]))
        if not rows and op not in blocks and op not in shared:
            continue
        out = ['# %s' % op, '']
        out.append('| Direction | Name | Client fn | Server symbol | Size | System | Status |')
        out.append('|---|---|---|---|---|---|---|')
        out.extend(rows if rows else ['| | no table row | | | | | |'])
        out.append('')
        for (bop, direction, name), body in blocks.get(op, []):
            out.append('## %s %s' % (direction, name))
            out.append('')
            body_text = '\n'.join(body).strip('\n')
            out.append(body_text)
            out.append('')
        for direction, name, primary in shared.get(op, []):
            out.append('## %s %s' % (direction, name))
            out.append('')
            out.append('Shared payload, the block is in [%s](%s.md).' % (primary, primary))
            out.append('')
        write(os.path.join(PAGES, op + '.md'), '\n'.join(out).rstrip('\n') + '\n')
        written += 1
    print('wrote', written, 'opcode files')

    # section 4 of the registry becomes a pointer the two tables rebuild by index
    head = lines[:pay_start]
    tail = lines[pay_end:]
    pointer = ['## 4. Payloads', '',
               'One file per opcode under `opcodes/`, `opcodes/0x0040.md` for `0x0040`, both directions in the same file. '
               '`opcodes/README.md` is the index. `tools/opcode_pages.py index` rebuilds the index and the two tables above from the files, '
               'edit the opcode file, never the tables.', '']
    write(REGISTRY, '\n'.join(head + pointer + tail).rstrip('\n') + '\n')
    index()


def parse_page(path):
    lines = read(path).split('\n')
    op = lines[0][2:].strip()
    rows = {}
    for line in lines:
        m = re.match(r'^\| (S2C|C2S) \| (.*?) \| (.*?) \| (.*?) \| (.*?) \| (.*?) \| (.*?) \|\s*$', line)
        if m:
            rows[m.group(1)] = [g.strip() for g in m.groups()[1:]]
    return op, rows


def pages():
    result = {}
    for name in sorted(os.listdir(PAGES)):
        if name.endswith('.md') and name.startswith('0x'):
            op, rows = parse_page(os.path.join(PAGES, name))
            result[op] = rows
    return result


def index():
    all_pages = pages()
    ops = sorted(all_pages, key=lambda o: int(o, 16))
    out = ['# Opcode pages', '',
           'One file per opcode, both directions inside, the header table then one payload section per direction. '
           'Wire order is top to bottom, `var` means the offset depends on a preceding string, the encoding is marked on every string, blob sizes are hex. '
           'A family block shared by several opcodes sits in the first member, the others point at it. '
           'Generated index, `tools/opcode_pages.py index` rebuilds it and the two tables of `../PACKET_REGISTRY.md`.', '',
           '| Op | S2C | C2S | System | Status |', '|---|---|---|---|---|']
    for op in ops:
        rows = all_pages[op]
        s = rows.get('S2C', [''] * 6)
        c = rows.get('C2S', [''] * 6)
        system = s[4] if s[4] else c[4]
        status = s[5] if s[5] else c[5]
        out.append('| [%s](%s.md) | %s | %s | %s | %s |' % (op, op, s[0], c[0], system, status))
    write(os.path.join(PAGES, 'README.md'), '\n'.join(out) + '\n')

    text = read(REGISTRY)
    lines = text.split('\n')
    for title, direction, subtitles in (
            ('## 2. S2C table', 'S2C', (('### 2.1 S2C 0x0000 to 0x007F', 0, 0x7F), ('### 2.2 S2C 0x0080 to 0x0135', 0x80, 0x135))),
            ('## 3. C2S table', 'C2S', (('### 3.1 C2S 0x0000 to 0x007F', 0, 0x7F), ('### 3.2 C2S 0x0080 to 0x0135', 0x80, 0x135)))):
        start, end = section_bounds(lines, title)
        block = [title, '', 'Generated from `opcodes/`, edit the opcode file.', '']
        for sub, lo, hi in subtitles:
            block += [sub, '', '| Op | Name | Client fn | Server symbol | Size | System | Status |', '|---|---|---|---|---|---|---|']
            for op in ops:
                v = int(op, 16)
                r = all_pages[op].get(direction)
                if r and lo <= v <= hi:
                    block.append('| [%s](opcodes/%s.md) | %s | %s | %s | %s | %s | %s |' % (op, op, r[0], r[1], r[2], r[3], r[4], r[5]))
            block.append('')
        lines = lines[:start] + block + lines[end:]
    write(REGISTRY, '\n'.join(lines).rstrip('\n') + '\n')
    print('index', len(ops), 'opcodes')


def check():
    bad = 0
    for op, rows in pages().items():
        if not rows:
            print('no header table', op)
            bad += 1
    print('check', 'ok' if bad == 0 else '%d bad' % bad)
    return 1 if bad else 0


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'check'
    if cmd == 'split':
        split()
    elif cmd == 'index':
        index()
    else:
        sys.exit(check())
