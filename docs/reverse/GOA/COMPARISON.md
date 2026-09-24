# GOA vs Modern KnC

## Quick verdict

| Layer | Compatible? | Notes |
|---|---|---|
| Opcode numbering (`cmd - 1`) | **Yes** | Identical convention |
| No-op opcode set | **Yes** | 11 exact matches |
| Error message tokens (`MSG_*`) | **Yes** | Exact match |
| Packet header | **Partial** | GOA 4-byte, modern 8-byte (4B padding added) |
| Handler count | Partial | GOA ~127 active, modern ~40 documented (modern has more undocumented) |
| Socket I/O | No | GOA = `select` + non-blocking. No IOCP. |
| Packet crypto | None in either? | GOA confirmed plaintext. Modern unconfirmed. |
| Anti-cheat | Irrelevant | GOA bundles GameGuard + IAT sentinels. Does not affect wire protocol. |

## Opcode indexing

Both clients: `switch(opcode - 1)`. GOA confirmed in `sub_43EBA0`. `-1` is intentional (opcode 0 reserved as invalid).

## Packet header

**GOA** (from `sub_428FF0`):
```c
size_ptr = this + 12; // wire[0..1]
opcode_ptr = this + 14; // wire[2..3]
payload = this + 16; // wire[4..]
```

**Modern** (from `sub_44EBE0` in `DevClient/KnC-new.exe.c`):
```c
size_ptr = this + 12; // wire[0..1]
opcode_ptr = this + 14; // wire[2..3]
payload = this + 20; // wire[8..]
```

| Version | Header | Layout |
|---|---|---|
| GOA | 4 bytes | `[size u16][opcode u16][payload]` |
| Modern | 8 bytes | `[size u16][opcode u16][pad u32 = 0][payload]` |

Both use real 2-byte opcode at offset [2..3]. No "Flag byte" in either version. Earlier docs claiming `[size u16][cmd u8][flag u8][reserved u32]` were wrong.

Server targeting both clients: only difference is 4 padding bytes after opcode. Opcode/size/payload encoding identical.

## Flag byte (Q2) - does not exist

Opcode is uint16 in both. GOA dispatches 1..201. Modern dispatches 1..309. Opcodes 256..309 in modern are race entity streaming packets added after GOA. No flag byte, just wider opcode range.

| | GOA | Modern |
|---|---|---|
| Opcode field | 16 bits | 16 bits |
| Range dispatched | 1..201 | 1..309 |
| High byte used? | Never | 256..309 only |
| Formula | `opcode - 1` | `opcode - 1` |

Anywhere modern docs say "flag = 0x01 distinguishes variant" actually means opcode >= 256.

## Ignored opcodes

```
GOA: 31 32 36 54 55 56 67 72 101 109 111 113
 120 121 122 128 129 131-138 150 171 174

Modern: 8 31 32 36 54 55 56 67 72 102 109
 126 127 128 134 139 141 145-148 160 187

Shared: 31 32 36 54 55 56 67 72 109 128 134
```

11 shared no-ops. Random overlap ~5%. Confirms inherited opcode numbering.

Notable differences:
- **Opcode 8**: GOA has handler `sub_441E50`, modern ignores. Typed notification (wstring + u8 + 2xu32). Replaced by 0xB4 SYSTEM_MESSAGE in modern.
- **120-122** (0x78-0x7A): ignored in GOA, active in modern (Item List/Item Add). Inventory system added after GOA.
- **131-138** (0x83-0x8A): all no-ops in GOA, modern only ignores 134. Range partially implemented later.

## Opcode 8 decompile (`sub_441E50`)

```c
void Handler_08(CPacket* pkt) {
 sub_42B6D0(&off_57E0D8); // UI context
 wchar_t v2[?];
 int v3[3], v4[6];
 ReadWString(v2); // variable
 ReadBytes(v3, 1); // u8 type
 ReadBytes(v3+1, 4); // u32 param
 ReadBytes(v4, 4); // u32 param
 return sub_42F990(v2); // display wstring
}
```

Payload: `[wstring][u8][u32][u32]`. Generic typed server notification. Deprecated by 0xB4.

## Error messages

| Token | GOA | Modern |
|---|---|---|
| `MSG_SERVER_NOT_READY` | `strstr` in `sub_440230` | documented |
| `MSG_DB_ACCESS_FAIL` | `strstr` in `sub_440230` | documented |
| `MSG_REINPUT_IDPASS` | `strstr` in `sub_440230` | documented |
| `MSG_INVALID_ID` | `strstr` in `sub_440230` | documented |

All handled via opcode 1 (display text). Identical tokens.

## I/O model

| | GOA | Modern |
|---|---|---|
| Socket API | `WSASocketA` | ? |
| Blocking | Non-blocking (`FIONBIO`) | ? |
| Wait | `select()` 1s timeout | ? |
| Async | Poll loop | ? |
| recv/send | Dynamically resolved (custom `GetProcAddress`) | ? |
| TCP_NODELAY | Off (Nagle on) | ? |
| SO_KEEPALIVE | On | ? |

## Wire crypto (Q3) - none

Send pipeline: `sub_4441A0` -> `sub_4290E0` -> `sub_428E10`/`sub_428F10` -> `sub_43E110`. Zero `Crypt*` calls. Plaintext.

Receive side: `CryptEncrypt`/`CryptImportKey` in `sub_47F690` are for anti-cheat telemetry (GameGuard), not packet payload.

Opcode 7 handler (`sub_440490`): just reads 4B + 216B into session state. No key import, no decrypt.

**GOA wire = plaintext TCP.**

## Key numbers

| Fact | GOA | Modern |
|---|---|---|
| Header size | 4 bytes | 8 bytes |
| Opcode range | 1..201 | 1..309 |
| PlayerInfo size | 216 bytes | 1224 bytes |
| Login opcode | 7 | 7 |
| Login timeout | 10 000 ms | ? |
| Connect timeout | 1 000 ms | 5 500 ms |
| Session confirm | 0xA7, 16B | 0xA7, 16B |
| Reconnect opcode | 0x9D | ? (maybe 0x54 SERVER_REDIRECT) |
| VehicleData | 44 bytes | ? |
| ItemData | 56 bytes | ? |
| Heartbeat 0xA6 | 3-step position upload | single periodic message |

## Anti-cheat

GOA bundles: GameGuard (InitNPSC), IAT hook sentinels (send/WSASend/timeGetTime), InitToolhelp32, SpeedCheckEventThread, `_Dect1` string crypto (208 callers), custom GetProcAddress (`sub_483370`), ReadMemCrc, CheckRsaBase, GetProcessList/GetModuleList.

None of this affects the wire protocol.

## Deprecated opcodes worth decompiling

| Opcode | GOA handler | Status |
|---|---|---|
| 8 (0x08) | `sub_441E50` | Done (typed notification) |
| 101 (0x65) | `sub_442170` | Open |
| 109 (0x6D) | `sub_4424D0` | Open |
| 174 (0xAE) | `sub_447140` | Open |

## Open work

- [ ] Decompile `sub_47F690` to confirm recv plaintext
- [ ] Find where `byte_7A6F30` / `dword_7A6F34` are set (login state close)
- [ ] Decompile GOA handlers 101, 109, 150, 174
- [ ] Map ~60 C->S senders in 0x444xxx-0x446xxx block
- [ ] Find `sub_43E9F0` (opcode 0x9C sender)
