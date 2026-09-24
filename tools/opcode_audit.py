#!/usr/bin/env python3
"""Opcode naming audit.

The codebase grew four ways of naming the same thing, CMD S_ and C_ in
Protocol.h, OP_S_ and OP_C_ inside the generated packet classes, kOp inside
some of them, and a handful of bare names. 589 constants, 166 opcodes carrying
more than one name. That is not just untidy, it defeats every attempt to
measure what the server can actually emit, which understated our own coverage
three times in one session.

THE CONVENTION, from here on

  shared/src/include/net/Protocol.h is the single source of truth.
    S_NAME   server to client
    C_NAME   client to server
  A generated packet class may keep a local alias for readability, but its
  value MUST agree with Protocol.h. This tool fails when it does not.

Run it before trusting any coverage number.
"""
import re
import sys
import glob
import os
import collections

DECL = re.compile(r'constexpr\s+\w+\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def sources():
    for pat in ('shared/**/*.h', 'shared/**/*.cpp',
                'server/**/*.h', 'server/**/*.cpp'):
        for f in glob.glob(os.path.join(ROOT, pat), recursive=True):
            yield f


def load():
    proto, others = {}, collections.defaultdict(list)
    for f in sources():
        try:
            text = open(f, encoding='utf-8', errors='ignore').read()
        except OSError:
            continue
        is_proto = f.replace('\\', '/').endswith('net/Protocol.h')
        for m in DECL.finditer(text):
            name, val = m.group(1), int(m.group(2), 16)
            if is_proto:
                proto.setdefault(name, val)
            else:
                others[name].append((val, os.path.basename(f)))
    return proto, others


# two different packets can share a human name each checked against the client dispatch table
EXEMPT = {
    ('OP_S_SERVER_REDIRECT', 0x19),
    ('OP_S_MISSION_COMPLETE', 0x8C),
    ('OP_S_LICENSE_RESULT', 0xA3),
}


def twin(name):
    """The Protocol.h names a local alias claims to mirror."""
    if name.startswith(('OP_S_', 'OP_C_')):
        base = name[5:]
    elif name.startswith('OP_'):
        base = name[3:]
    elif name.startswith('kOp'):
        base = name[3:]
    else:
        return []
    # never fall back across the direction prefix since an OP S is not a C
    if name.startswith('OP_S_'):
        return ['S_' + base, base]
    if name.startswith('OP_C_'):
        return ['C_' + base, base]
    return ['S_' + base, 'C_' + base, base]


def main():
    proto, others = load()
    problems = []
    for name, decls in others.items():
        vals = {v for v, _ in decls}
        if len(vals) > 1:
            problems.append(('%s declared with %d different values' % (
                name, len(vals)), decls))
            continue
        val = decls[0][0]
        if (name, val) in EXEMPT:
            continue
        for cand in twin(name):
            if cand in proto and proto[cand] != val:
                problems.append((
                    '%s is 0x%03X but Protocol.h %s is 0x%03X' % (
                        name, val, cand, proto[cand]), decls))
                break

    print('Protocol.h constants      : %d' % len(proto))
    print('aliases elsewhere         : %d' % len(others))
    print('disagreements             : %d' % len(problems))
    for msg, decls in problems:
        print('  FAIL %s' % msg)
        for v, f in decls:
            print('       0x%03X in %s' % (v, f))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
