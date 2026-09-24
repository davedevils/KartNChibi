# Keepalive / Anticheat System

Source group: `keepalive_anticheat` (opcodes 0x00B, 0x04D, 0x04E, 0x12E). Client reference:
`KnC.exe.raw`, image base 0x400000. Repo: this repository.

## 1. Overview

Two related client subsystems ride these opcodes:

- **Keepalive / delayed-ack** (0x0B, 0x4E): a round-trip timer the client arms on
  packet arrival and echoes back empty. Pure liveness signal, no payload data.
- **Anticheat** (0x4D, 0x12E): two independent probes. 0x4D is a client
  fingerprint/build-info blob the client only sends when the server asks for it.
  0x12E is a file-integrity manifest (pak file count + per-file checksums) that
  arms a background worker thread which disconnects the client on mismatch.

Of the four `_todo` packets, three now ship: S2C 0x4D and S2C 0x4E go out
post-login from `KeepaliveAnticheatHandler::armPostLogin`, and C2S 0x4E is
parsed by `KeepaliveAnticheatHandler::handleDelayedAckFire` off `case 0x4E` in
`GameServerDispatch.cpp`. S2C 0x12E stays unsent, its builder
`KeepaliveAnticheatPackets::clientFileCheck` and its `client_file_manifest`
table (migration 041) exist but have no call site, since the client's pak
checksum algorithm `FUN_00403a10` is still unconfirmed and a wrong manifest
would disconnect real players. None of the four block login, the client
treats all four as optional opportunistic checks, not preconditions for any
screen transition. That is also why chibikart.gg (which reaches gameplay) is
not proof they are unneeded, it just never sends them either.

## 2. Packets

| Dir | Op | Name | Client fn | Payload | Status today |
|-----|-----|------|-----------|---------|---------------|
| C2S | 0x04E | DelayedAckFire | FUN_00485290 (timer, called from FUN_00478ad0 @0x478b05) | empty | parsed, `GameServerDispatch.cpp` case 0x4E calls `KeepaliveAnticheatHandler::handleDelayedAckFire` |
| S2C | 0x04D | ClientInfoProbe | FUN_004790C0 (dispatch slot 0x4783D0, stub 0x4778D6) | empty | sent post-login, `KeepaliveAnticheatHandler::armPostLogin` via `KeepaliveAnticheatPackets::clientInfoProbe()` |
| S2C | 0x04E | DelayedAckArm | FUN_00479160 (dispatch slot 0x4783D4, stub 0x4778E3) | empty | sent post-login, `KeepaliveAnticheatHandler::armPostLogin` via `PacketBuilder::timestamp()` |
| S2C | 0x12E | ClientFileCheck | FUN_0047ED40 (S_ENTITY_DATA_302) | i32 count + count*16B records | never sent, builder `KeepaliveAnticheatPackets::clientFileCheck` exists with no call site |

### 2.1 C2S 0x04E DelayedAckFire

**Trigger.** Purely a client-side timer echo, not a user action. `FUN_00478ad0`
is an `ExceptionList`-wrapped periodic tick handler (called on the client's
regular update loop); it unconditionally calls `FUN_00485290` every tick.
`FUN_00485290` checks a per-connection flag at `DAT_008ccd8c[param_1]`: if the
flag is armed (set to 1 by handling S2C 0x4E, see below) and either 15000ms
have elapsed since the timestamp latched at `DAT_008ccd90[param_1]`, or a
counter-wrap condition trips, it clears the flag and calls
`FUN_004803a0(0x4E)` (BeginPacket + Send, empty body). If S2C 0x4E is never
sent, the flag is never armed and the client never fires this packet; there is
no other trigger and no client-visible failure if it never fires.

**Layout.** Empty body, 0 bytes.

**Consumer.** None visible client-side beyond clearing its own arm flag; it is
the far end of a round-trip latency/keepalive measurement whose only visible
effect lives entirely inside the client (nothing renders, nothing blocks).

**Data source.** None needed, no fields.

**Implementation.** Add `case 0x4E:` to the C2S switch in
`server/game/src/GameServer.cpp` (near the existing `case 0x0B: break;` at
line 1065), reading nothing and optionally updating a per-session
last-seen/latency timestamp (e.g. `session->lastAckAt = now()`), matching how
`case 0x0B` is handled. Confidence: high that the shape is a bare ack; medium
on whether the server needs to consume the latency for anything (no downstream
read of it was found client-side, it only clears the client's own flag).

### 2.2 S2C 0x04D ClientInfoProbe

**Trigger.** Client dispatch slot 0x4783D0 in the S2C jump table (dispatcher
at 0x4777C0) routes opcode 0x4D to a tiny stub at 0x4778D6
(`PUSH EDI; MOV ECX,ESI; CALL 0x4790C0; POP EDI; POP ESI; RET 4`), single
caller confirmed via `get_xrefs_to(0x4790C0)`. `FUN_004790C0` performs **zero**
payload reads (no `FUN_0044e910`/`eb30`/`eb60` calls) receipt of the opcode
itself, regardless of content, is the whole trigger. On receipt it immediately
builds C2S opcode 0x4D: `FUN_0044ecd0(0x4D)` (BeginPacket), then
`FUN_0044e9c0(&DAT_005de9dc, 0x114)` (raw-copy 276 bytes from a static client
build/fingerprint blob), then `FUN_00476b80` (Send). If the server never sends
S2C 0x4D, the client never volunteers this blob; there is no timer or
init-time call site, this is the sole trigger.

**Layout.** S2C 0x4D itself: empty, 0 bytes. The client's C2S 0x4D reply:
`u32 requestType` + 272 raw bytes = 276 bytes total, matching
`GameServer::handleRequestData`'s existing `remaining() < 276` gate,
`readUInt32()` + `readBytes(272)` (GameServer.cpp:3220-3231). The 272 bytes are
opaque (no client-side decode was found; it is a raw copy of a static blob,
presumably build id / file hashes / hardware fingerprint for anti-cheat).

**Consumer.** Drives the client's anti-cheat client-info handshake; without
the probe, the server-side anti-cheat data this blob presumably feeds is never
populated (existing comment at GameServer.cpp:3221 already calls this "no
reply expected", written for the C2S side with no producer of the trigger).

**Data source.** None needed to send the probe (empty body). The 272-byte
reply is currently discarded server-side and needs no table unless the blob
is later decoded and persisted (no schema exists for it, `server/scripts/*.sql`
has no anticheat/fingerprint table).

**Implementation.** Add a `PacketBuilder::clientInfoProbe()` (empty
`CMD::S_CLIENT_INFO_PROBE`/0x4D packet, same shape as `PacketBuilder::ack()`
at PacketBuilder.cpp:174-177) declared next to `timestamp()` in
`server/game/include/packets/PacketBuilder.h:256`. Send it once per session,
after the login burst completes, from the `sender->onDone` callback in
`GameServer::sendPlayerData` (GameServer.cpp:2896-2925), the same place the
final `S_SHOW_LOBBY`/`S_UI_STATE_14` ack and `sendLobbyRoomList` already fire.
`GameServer::handleRequestData` (GameServer.cpp:3220) already parses the
276-byte reply correctly, it just needs the probe to trigger it.

### 2.3 S2C 0x04E DelayedAckArm

**Trigger.** Client dispatch slot 0x4783D4 (immediately after the 0x4D slot,
table is ascending/sorted, cross-checked against the confirmed neighbor slots
0x4783D0=0x4D and 0x4783D8=0x54/GameServerHandoff) routes to stub 0x4778E3
calling `FUN_00479160` (fastcall, `param_1` = a per-connection slot offset).
The handler reads **no** payload bytes; receipt of the opcode is the whole
trigger. It sets `DAT_008ccd8c[param_1] = 1` (arms the flag consumed by C2S
0x4E's sender, FUN_00485290 above) and latches the current time
(`FUN_0044ed50()`, a QueryPerformanceCounter/time-source call) into
`DAT_008ccd90[param_1]`. If never sent, the arm flag stays 0 forever and the
client's periodic tick in FUN_00485290 never fires the C2S 0x4E echo, but
nothing else client-side reads the flag or the timestamp (confirmed no other
xrefs to `DAT_008ccd8c`/`DAT_008ccd90` beyond this arm site and the
FUN_00485290 check/clear site).

**Layout.** Empty, 0 bytes. `PacketBuilder::timestamp()` already builds the
correct empty `CMD::S_TIMESTAMP` packet (PacketBuilder.cpp:1073-1077,
declared PacketBuilder.h:256) but has zero call sites anywhere in the repo.

**Consumer.** Arms the client's own round-trip keepalive timer; the client
never independently pings for idle detection, it strictly answers what the
server arms. If a real server never sends this, the round trip never
completes and no client-visible symptom was found (no disconnect, no UI
change) it appears to be a pure latency/keepalive telemetry loop rather than
an anti-idle-kick mechanism.

**Data source.** None, no fields either direction.

**Implementation.** Call the already-existing `PacketBuilder::timestamp()`
periodically from the game server's connection keepalive path (wherever
per-session ticks/heartbeats are driven; no dedicated periodic loop was found
in `server/game/src/GameServer.cpp` for this purpose, so add one, e.g. a
session-tick timer that calls `session->send(PacketBuilder::timestamp())`
every N seconds) and handle the resulting C2S 0x4E per 2.1 to close the loop.
Since nothing client-side depends on periodicity or timing precision, sending
it once after login (alongside the 0x4D probe in `sendPlayerData`'s
`onDone`) is a safe minimum; a periodic interval is the closer match to
"keepalive" but unproven necessary from the binary alone.

### 2.4 S2C 0x12E ClientFileCheck

**Trigger.** Handler at 0x47ED40 (opcode constant `S_ENTITY_DATA_302` = 0x12E,
Protocol.h:387), reached via the S2C dispatcher at 0x4777C0. No specific
precondition packet is required client-side; it is a bare opcode-triggered
handler like the others in this group. Decompile:

```
FUN_0044e910(&DAT_00b23398,4);              // i32 count
for (i = 0; i < DAT_00b23398; i++)
    FUN_0044e910(dst, 0x10), dst += 0x10;   // count * 16-byte raw records into DAT_00b2339c
FUN_00403dc0();                              // spawn FileCheck worker thread
```

`FUN_00403dc0` (`_beginthreadex` wrapper) spawns `FUN_00403d50`, which loops
calling `FUN_00403ad0` while a run flag is set. `FUN_00403ad0` first checks
that exactly `count` (`*(param_1+4)`, i.e. `DAT_00b23398`) pak files named by
an obfuscated `sprintf` pattern (`pakNNN.dat`-shaped, decoded via a per-index
byte transform, unrelated to the wire records) exist on disk in the working
directory; if the count found does not match, it raises string
`MSG_FILECHECKERROR` (0x59f5a0) via `FUN_0043d730` and calls `FUN_00404080(0)`
(disconnect/terminate path). It then walks the received record array at
`param_1+0x14`, stepping 16 bytes (4 ints) per record: `sprintf("./%s",
local_160-3)` treats the **first 12 bytes** of each 16-byte record as an ASCII
relative path string (`local_160-3` = 3 ints = 12 bytes back from the 4th
int), opens that file, computes a checksum via `FUN_00403a10`, and compares it
against the **last 4 bytes** of the record (`*local_160`, the int at offset
+12). On any mismatch (file open failure or checksum mismatch) it raises the
same `MSG_FILECHECKERROR` string and calls `FUN_00404080(0)`. This confirms
the record split: 12-byte ASCII relative path (embedded NUL-padded, not
length-prefixed) + u32 expected checksum, little-endian, matching the JSON
row's inferred layout exactly.

**Layout.**

| Field | Type | Notes |
|-------|------|-------|
| count | i32 | number of records that follow |
| records[count] | struct[16 bytes] | repeated |
| &nbsp;&nbsp;relative_path | char[12] | ASCII, NUL-padded, e.g. `pak001.dat` |
| &nbsp;&nbsp;expected_checksum | u32 | compared against `FUN_00403a10(file)` |

**Consumer.** Starts the client's background file-integrity anticheat worker
(`FileCheck thread`, string at 0x59f5b4), which polls forever, disconnecting
the client with `MSG_FILECHECKERROR` on any pak file mismatch. Purely an
anti-tamper check against the local install, no gameplay data rides it.

**Data source.** None exists today. Add a small manifest source, either a
static compiled-in table (path, checksum) sourced once from a verified retail
client install, or a new table, e.g. `client_file_manifest(id, relative_path
VARCHAR(12), expected_checksum INT UNSIGNED)`, seeded by a one-time tool that
hashes the real client's pak files with the same algorithm as `FUN_00403a10`
(not fully decompiled here, treat as an unverified checksum function pending
further RE if exact parity with a real server is required).

**Implementation.** Add a `PacketBuilder::clientFileCheck(const
std::vector<FileCheckRecord>&)` builder (12-byte path + u32 checksum per
record, i32 count prefix) to `server/game/include/packets/PacketBuilder.h`
and `.cpp`. Send it once per session from the same post-login hook as 0x4D
(`sendPlayerData`'s `onDone`, GameServer.cpp:2896-2925), since this is also a
one-shot anticheat probe with no user-visible gate. No C2S counterpart exists
or is expected; the client only disconnects itself locally on mismatch.
Confidence: high on wire shape (directly decompiled), medium on whether our
manifest values need to byte-match a real client install for `FUN_00403a10`
(unresolved, would need the exact checksum algorithm reversed to avoid
false-positive disconnects).

## 3. Design

### 3.1 Purpose

For the player, this system is invisible when working: it is anti-cheat
telemetry (build/fingerprint blob, file-integrity manifest) and connection
liveness (delayed-ack round trip), none of it gates any menu, race, or UI
transition. The risk of adding it wrong is a false-positive disconnect
(0x12E's checksum mismatch path), not a missed feature.

### 3.2 Tables

| Table | Status | Purpose |
|-------|--------|---------|
| `client_file_manifest` | new, optional | path + expected checksum for 0x12E; only needed if S2C 0x12E is enabled |

No table is needed for 0x4D (probe reply is discarded today) or for either
0x4E (both directions are empty).

### 3.3 Handlers / builders touched

| File | Change |
|------|--------|
| `server/game/src/GameServer.cpp` | add `case 0x4E:` to the C2S switch (near line 1065) |
| `server/game/include/packets/PacketBuilder.h` / `.cpp` | add `clientInfoProbe()` (0x4D, empty); add `clientFileCheck(...)` (0x12E); `timestamp()` (0x4E) already exists, just needs a caller |
| `server/game/src/GameServer.cpp` (`sendPlayerData`, `onDone` around line 2896-2925) | call `clientInfoProbe()`, `timestamp()`, and (if enabled) `clientFileCheck()` once post-login-burst |

### 3.4 Flows

Keepalive round trip (once S2C 0x4E is wired):

| Step | Dir | Op | Trigger |
|------|-----|-----|---------|
| 1 | S2C | 0x4E DelayedAckArm | server sends after login burst (or periodically) |
| 2 | client | (internal) | arms flag, latches timestamp |
| 3 | C2S | 0x4E DelayedAckFire | client's periodic tick, 15000ms after step 1 |
| 4 | server | (internal) | server case 0x4E now consumes it (e.g. updates last-seen) |

Anticheat client-info probe:

| Step | Dir | Op | Trigger |
|------|-----|-----|---------|
| 1 | S2C | 0x4D ClientInfoProbe | server sends once, post-login-burst |
| 2 | C2S | 0x4D ClientInfoBlob | client answers immediately, unconditionally, 276-byte static blob |
| 3 | server | `handleRequestData` | already implemented (GameServer.cpp:3220), currently discards the 272-byte body beyond size check |

Anticheat file-integrity check (optional, only if manifest data is trustworthy):

| Step | Dir | Op | Trigger |
|------|-----|-----|---------|
| 1 | S2C | 0x12E ClientFileCheck | server sends once, post-login-burst |
| 2 | client | (internal) | spawns `FileCheck` worker thread, verifies pak count then per-file checksums forever, disconnects with `MSG_FILECHECKERROR` on any mismatch |

### 3.5 Open questions

- Whether S2C 0x4E should be sent once (post-login) or on a recurring
  interval was not resolved from the binary; nothing client-side reads the
  latency value or times out waiting for the echo, so periodicity is a policy
  choice, not a client requirement.
- The 272-byte payload of C2S 0x4D (client build/fingerprint blob) is opaque;
  no client-side decode of it was found (it is a raw static copy from
  `DAT_005de9dc`), so its internal fields are unknown pending someone
  correlating it against a real server's stored anti-cheat records, if any
  exist.
- `FUN_00403a10`'s checksum algorithm (used both to verify individual pak
  files and, per file, compared against the wire's `expected_checksum`) was
  not decompiled here; sending S2C 0x12E with mismatched checksums will
  actively disconnect legitimate clients, so this packet should stay
  unimplemented (or implemented with real client pak checksums only) until
  that algorithm is confirmed.
- Whether a real server sends 0x12E at all, or only in specific client
  versions/regions, is unknown; no evidence for it exists in either capture
  log (`mitm_packets.log`, `capture_2026-09-11.log`) grepped for `0x12E`/`302`.
