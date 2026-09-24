# KnC Protocol

Reverse engineered from KnC.exe (Chibi Kart / Kart n' Crazy).

## Packet Structure

```
+------+------+------+---------+
| 0-1  | 2-3  | 4-7  | 8..     |
| Size | Opcode| Pad  | Payload |
| u16  | u16  | 0x00 | var     |
+------+------+------+---------+
```

- Size: payload size, little-endian u16.
- Opcode: command, little-endian u16. Full 16-bit value.
- Padding: 4 bytes, always 0x00 (8-byte alignment).
- Total packet = 8 + Size field.

### Opcode is u16

Old docs treated bytes [2] and [3] as separate `cmd` + `flag` fields. Wrong.

Evidence from `DevClient/KnC-new.exe.c`:
- `sub_44EBE0` places opcode as WORD at offset 2 with padding at 4-7.
- Dispatcher has contiguous switch cases 1..309 including 256, 257, 258...309. Only valid if byte [3] is high byte of u16.
- Client sends opcode 258 via `sub_44ECD0(pkt, 258)`. On wire = `[02 01]` LE. Looks like `cmd=0x02, flag=0x01` through narrow reader but is single opcode `0x0102 = 258`.

Padding bytes always zero. Client aligns payload to 8-byte boundary. Server can emit any byte value, client accepts. Canonical = zero-fill.

### Legacy Compatibility

`Protocol.h` keeps `cmd()` and `flag()` byte aliases via anonymous union. `packet.cmd() == 0x30` works identically to `(packet.opcode() & 0xFF) == 0x30`.

`packet.flag() == 0x01` is NOT a variant selector. It is high byte of opcode, nonzero only when opcode is 256..511. Code dispatching on "flag 0 vs flag 1 modes for same CMD" is wrong. Rewrite to dispatch on full `opcode()`.

### GOA Client (old Korean build) 4-byte header

```
[0-1] Size u16
[2-3] Opcode u16
[4..] Payload
```

No padding bytes. See [docs/reverse/GOA/NETWORK.md](../reverse/GOA/NETWORK.md) and [docs/reverse/GOA/COMPARISON.md](../reverse/GOA/COMPARISON.md). Server targeting both clients needs two wire encoders.

## Data Types

| Type | Size | Notes |
|------|------|-------|
| `int8` | 1 | Signed 8-bit |
| `uint8` | 1 | Unsigned 8-bit |
| `int16` | 2 | Signed 16-bit LE |
| `uint16` | 2 | Unsigned 16-bit LE |
| `int32` | 4 | Signed 32-bit LE |
| `uint32` | 4 | Unsigned 32-bit LE |
| `string` | N+1 | Null-terminated ASCII |
| `wstring` | 2N+2 | Null-terminated UTF-16LE |

## Packet Index

Full per opcode detail lives in [PACKET_REGISTRY.md](PACKET_REGISTRY.md). Category
breakdown below for orientation.

### Server -> Client

| Category | Packets | Purpose |
|----------|---------|---------|
| Auth | 0x01, 0x12, 0x8E-0x90 | Login, heartbeat resp, init |
| UI | 0x0E-0x12 (flag=0x01) | Screen transitions |
| Inventory | 0x1B-0x1E, 0x78-0x7A | Items, vehicles |
| Room | 0x21-0x23, 0x30, 0x3E-0x3F, 0x62-0x63 | Room management |
| Chat | 0x2D, 0x2E | Messaging |
| Game | 0x14, 0x31-0x3C | Race, items, results |
| Shop | 0x6F, 0x72-0x74 | Shop transactions |

### Client -> Server

| Category | Packets | Purpose |
|----------|---------|---------|
| Client | 0x07, 0x19, 0x2C, 0x32, 0x4D, 0x73, 0xA6, 0xD0, 0xFA | Auth, heartbeat, state |

## Game States

| Value | State | Trigger |
|-------|-------|---------|
| 0 | DISCONNECTED | - |
| 1 | CONNECTING | Client startup |
| 2 | CONNECTED | Connection success |
| 3 | AUTHENTICATING | Login in progress |
| 4 | AUTHENTICATED | Login success |
| 5 | MENU | Main menu |
| 6 | GARAGE | Garage screen |
| 7 | SHOP | Shop screen |
| 8 | LOBBY | Game lobby |
| 9 | ROOM | In room |
| 10 | LOADING | Loading race |
| 11 | RACING | In race |
| 12 | RESULTS | Race results |
| 13 | TUTORIAL | Tutorial mode |

## Common Structures

See [STRUCTURES_VERIFIED.md](STRUCTURES_VERIFIED.md).

| Structure | Size | Used By |
|-----------|------|---------|
| PlayerInfo | 0x4C8 (1224) | CMD 0x07 |
| VehicleData | 0x2C (44) | CMD 0x1B, 0x3E, 0x78 |
| ItemData | 0x38 (56) | CMD 0x1C, 0x3E |
| AccessoryData | 0x1C (28) | CMD 0x1D, 0x1E |
| SmallItem | 0x20 (32) | CMD 0x79, 0x7A |
| ChatMessage | ~116 | CMD 0x2D |

Full handler mapping: [PACKET_REGISTRY.md](PACKET_REGISTRY.md).

## Error Messages

| Message | Category |
|---------|----------|
| `MSG_SERVER_NOT_READY` | Auth, server starting |
| `MSG_DB_ACCESS_FAIL` | Auth, database error |
| `MSG_REINPUT_IDPASS` | Auth, wrong credentials |
| `MSG_INVALID_ID` | Auth, user not found |
| `MSG_DURABILITY_ZERO` | Game, item broken |
| `MSG_DURABILITY_LOW` | Game, low durability |
| `MSG_MAX_ROOM_USER_8/16` | Room, room full |
| `MSG_TEAM_CHANGE_FAIL` | Room |
| `MSG_UNKNOWN_ERROR` | General |

Full list: [messages.md](messages.md).

## Notes

- CMD Offset: client uses -1 internally, server sends actual values.
- Rate Limit: heartbeat (0xA6) = 1000ms minimum.
- Timeout: connection = 5500ms (0x157C).
- Strings: chat = UTF-16LE, error messages = ASCII.

## Catalog containers

Two part containers exist and they are not interchangeable.

| Container | Opcode | Handler | Stride | Key at | Holds |
|-----------|--------|---------|--------|--------|-------|
| accessory | 0xC2 | sub_47F800 | 0xDC | rec+0x08 | paint, number plate, antenna, driver meshes, kart body parts |
| factory car | 0x108 | sub_00480210 | 0x120 | rec+0x08 | craftable body parts, drives Car Craft and the car factory |

0xC2 record fields the client actually reads.

| Off | Field | Reader |
|-----|-------|--------|
| +0x08 | lookup key | sub_004510c0, sub_00450ff0 |
| +0x0c | required level | sub_00419020 drops the row below it |
| +0x10 | asset name | nif base and `Parts/<name>_%02d.png` |
| +0x34 | owner kind | sub_00418c40, 0 driver 1 kart |
| +0x38 | shop sub tab | sub_00418c80, see table below |
| +0x3c | driver lock | -1 means no lock |
| +0x40 | label | drawn raw, no localisation pass |

0xC2 sub tab ids at +0x38, also the equip slot in sub_00484770 case 3.

| Id | Tab | Equip slot |
|----|-----|------------|
| 0 | Car / Paint | kart rec +0x08 |
| 1 | Car / Number Plate | kart rec +0x0c |
| 2 | Character / Cloth | char rec +0x08 |
| 3 | Character / Face | char rec +0x0c |
| 4 | Character / Head | char rec +0x10 |
| 5 | Character / Face alt | char rec +0x14 |
| 6 | Character / Back | char rec +0x18 |
| 8 | Car / Antenna | kart rec +0x10 |

Any other id shows on no tab, use it to park rows the shop must not list.

0x108 record fields.

| Off | Field | Reader |
|-----|-------|--------|
| +0x08 | lookup key | sub_0044fbc0 |
| +0x0c | category | 0 cover 1 booster 2 tires 3 f_fender 4 r_fender 5 bumper 6 wing |
| +0x10 | required level | sub_00419020 |
| +0x14 | model dir | `Car/FactoryCar/<CAT>/%s/` and `parts/%s<suffix>_B_%02d.png` |
| +0x35 | label | reveal popup and tile |
| +0x80 | tier | under 5 Basic, under 0x14 Unique, under 0x41 Epic, else Legend |

## Opcodes past 0xFF

`Packet(uint8_t cmd, uint8_t flag)` truncates. Use `Packet::fromCmdFull(0x108)`
or the burst lands in handler 0x08 and eats the rest of the stream.

## Reward blob and gacha prize category

Same switch in both, sub_00456e60 for the gacha reveal.

| Cat | Lookup | Name at | Size |
|-----|--------|---------|------|
| 0 | sub_00450060 driver | +0x4c | 0x2C |
| 1 | sub_0044f6f0 kart | +0x41 | 0x38 |
| 2 | sub_004508c0 item | +0x35 | 0x1C |
| 3 | sub_004510c0 accessory | +0x40 | 0x1C |
| 4 | sub_00452b50 | +0x39 | 0x30 |
| 5 | sub_0044fbc0 factory part | +0x35 | 0x84 |

Cat 3 draws `Parts/<name>_01.png`, cat 5 draws `parts/<model><suffix>_B_01.png`.
A prize whose art does not ship reveals as an empty tile.

## Room and race traps

Each of these cost a full debug cycle, all measured in game.

| Opcode | Trap |
|--------|------|
| 0x40 | BOTH in race motion AND the waiting room start request. The client sends 19 byte motion in the waiting room too, so routing on room state alone starts the race the moment the player walks in. Discriminate on payload size, a start request carries no position |
| 0x35 | Track select, BOTH directions. sub_00479C20 reads two int32. A zero payload echo reads off the end of the frame and the client drops the socket with "Failed to connect!" |
| 0xBE | NOT a race array reset. sub_00478B50 wipes TWENTY containers, every catalogue the login stream filled. Broadcasting it at race start left the client with vehicles=0 |
| 0x3F | player despawn, sub_0047A050 destroys the car with that id and decrements the population counter. Not room info |
| 0x63 | opens a chat panel, sub_0047AC30. Not a create room ack |

## Track ids and map ids are two spaces

| Space | Who speaks it |
|-------|---------------|
| track_id | 0xC3 catalogue, 0x35 track select, the client |
| map_id | room settings, StartGridLoader, the World path |

`track_catalog.map_id` is the only link. Track 10 Forest_01 links to map 1, and
map 10 is track 30 Desert_01, so swapping them loads the Desert grid under a
Forest world and the spawn pose puts the camera under the kart.

ONE publisher per container. Login owns 0xC3 from track_catalog. A second
publisher keyed on the maps table replaced those rows and nothing downstream
resolved.

### Track catalog tuning floats

The 0xC3 record (PACKET_REGISTRY.md 4.4) also carries the license grade at
record+72 (0x48) and the lap count at record+80 (0x50, clamped 1..9). Before
those two, record+48/+52/+56 (0x30/0x34/0x38) are three tuning floats the
client copies straight into DAT_005EB6F0/F4/F8 at `world_track_init` (0x4875C0):
0.4 (engine/steering scale, guess), 0.6 (engine force durability penalty
scale, guess), 90.0 (turn force baseline, guess). record+64 (0x40) is
fall_off_timeout_ms, client default 500. Our server sends the first two from
`track_catalog.tuning_engine_setup_bits`/`tuning_engine_force_bits` (raw float
bit pattern columns, named by `server/scripts/056_column_renames.sql`, the
pre rename names are in `013_kart_part_wire.sql` which is left unedited) and
the third from `tuning_turn_force`. Two more dead FLOAT columns from the
original schema, never fed to the wire, are dropped by that same rename
migration. The table is created in `server/scripts/012_wire_all.sql`, the
two raw columns are added in `013_kart_part_wire.sql`, which also sets
tracks 1 and 2 to 50 and 100 raw, floats near zero. Detail
and addresses in `docs/reverse/physics/INPUT_AND_STATS.md` and
`docs/packets/RECOVERED_RE_NOTES.md` (StartGridLoader.h).

## Client asset defects that look like server bugs

| Asset | Defect |
|-------|--------|
| Car/Parts/m_name01.dds | shipped blanked, 2176 bytes flat grey. Real 16512 byte art is renamed `-m_name01.dds`. NAMEBOX_001 and _004 use it |
| Car/Body/High/Rudolf_01, MINI.small | only BLUE GREEN PURPPLE RED YELLOW. A kart painted BLACK loads no texture, renders white and fails SetBody |
| NAMEBOX_NORMAL | the stock plate. The twenty numbered ones are shop items. Kart bodies carry only an O_NAME dummy, never plate geometry |

Body colours present on EVERY kart model: BLUE, GREEN, PURPPLE, RED, YELLOW.

## Empty tables that break a whole screen

| Table | Symptom when empty |
|-------|--------------------|
| kart_catalog | "kart_catalog miss" per kart, no race stats at all |
| room_object_def category 1 | no Floor record, no track.COL, no navmesh, "Set body fail #1" |
| shop_option | option count zero, tile draw derefs null, screen freezes with no error |

## Opcode coverage audit

Method: client senders from `ida_senders_dump.txt` against the routes in
`GameServer.cpp`, and client S2C handlers from `ida_dispatcher_dump.txt` (real
opcode = column + 1, nullsub rows dropped) against what the server builds and
what the wire log actually carried.

| Direction | Total | Covered | Gap |
|-----------|-------|---------|-----|
| C2S senders | 90 | 82 routed, echoed or swallowed | 8 unhandled |
| S2C handlers | 186 live | 138 | 48 never fed |

C2S the client can send and the server does not handle at all:

| Opcode | Sender |
|--------|--------|
| 0x06E | FUN_00481E20 |
| 0x0A1 | FUN_00482BA0 |
| 0x114 | FUN_00483790, int32 then a utf16 string, caller FUN_00455840 |
| 0x118 | FUN_00483840 |
| 0x121 | FUN_004839A0 |
| 0x123 | FUN_00483A40 |
| 0x125 | FUN_00483AE0 |
| 0x12F | FUN_00483B80 |

S2C handlers the client owns that nothing ever feeds, grouped:

| Range | Handlers | Note |
|-------|----------|------|
| 0x071 0x074 0x077 0x07A 0x07B 0x081 0x084 0x085 | 8 | lobby and list family |
| 0x095 0x099 0x09B 0x09C 0x0A5 0x0AD | 6 | board and popup family |
| 0x0F4 to 0x0F9 | 6 | consecutive block, buddy and social neighbours of 0xFB 0xFC |
| 0x103 0x104 | 2 | pet definition and owned pet list |
| 0x114 to 0x124 | 13 | the 16 bit feature block, newest screens |
| 0x007 0x018 0x019 0x01B 0x01D 0x025 0x027 0x04D 0x0BC 0x0C1 0x0C9 0x112 0x136 | 13 | scattered |

Neither list is proof of a bug on its own. A handler with no feed is only a
problem when a screen reads the container it fills, which is what the container
map above is for.

## Dispatcher map, the real one

Stop using the column plus one rule from the ida dump. The client carries the
map itself, in two tables, and it can be read exactly.

sub_4777C0 does:

```
EAX = opcode          ; uint16
EAX = EAX - 1
if (EAX > 0x134) goto default
EAX = byte [0x478638 + EAX]      ; index table, 0x135 bytes, 215 means default
JMP  dword [0x4782D8 + EAX*4]    ; jump table, 216 entries
```

Each jump target is a 13 byte thunk:

```
57              PUSH EDI
8B CE           MOV  ECX, ESI
E8 rel32        CALL handler
5F 5E C2 04 00  POP EDI / POP ESI / RET 4
```

So handler = stub + 8 + rel32. 215 opcodes are handled, every one resolves.
Anchors that match live behaviour: 0x14 FUN_00479CC0, 0x3E FUN_00479D60,
0x42 FUN_0047A0A0, 0xBE FUN_00478B50, 0xC2 FUN_0047F800, 0xC3 FUN_0047F990.

## 0x07 PlayerInfo, login only

`[driverId u32][PlayerInfo 1224]`

| Offset | Field |
|--------|-------|
| 0x000 | character id, client echoes it in 0xA7 |
| 0x004 | username wstring |
| 0x486 | display name wstring, 12 units |
| 0x4A4 | gold |
| 0x4A8 | astro |
| 0x4AC | cash |
| 0x4B0 | selected kart OWNED INSTANCE id, matches 0x1C |
| 0x4B4 | selected character OWNED INSTANCE id, matches 0x1B |

0x4B0 and 0x4B4 are instance ids, not base keys. Minus one means no selection
and the lobby stand builds nothing.

## Waiting room, 0x13 then 0x32 then 0x21

All room slots start disabled. 0x21 reaches sub_40D650, reads slot.enabled and
returns before the character and kart lookup when it is still zero.

Order per member:

```
S2C 0x13  RoomContext
S2C 0x32  { slot u32, enabled u32 = 1 }
S2C 0x21  RoomMember
```

The membership table is EIGHT wide. Enabling slot 8 and up walks off it and the
real slots stop resolving. The client says MSG_MAX_ROOM_USER_8. Cap the room at
eight seats.

0x21 payload, packed, no alignment padding anywhere, size 187 plus the name:

| Field | Size |
|-------|------|
| slot | u32 |
| team | u32 |
| player_id | u32 |
| display_name | wstring |
| level_index | u8 |
| gm_badge | u8 |
| pccafe | u8 |
| title_key | u32, no pad before it |
| character | 0x2C |
| kart | 0x38 |
| ready_state | u32 |
| pet_base_key | u32 |
| custom_car | 0x3C |

Do not confuse the ids:

```
0x21 player_id        = room player identity
character +0x00       = owned character instance id
character +0x04       = driver base key
kart      +0x00       = owned kart instance id
kart      +0x04       = kart base key
```

Ship the exact owned record bytes already published in 0x1B and 0x1C.

## 0x54 redirect, client bug

Payload is `[u32 0][ascii ip][u32 port]`, sub_47AA00 reads exactly that and our
frame matches. The bug is in the client.

sub_47AA00 opens the world connection inside the dispatch, and the parse loop
then subtracts the frame it just consumed from a counter the handler already
zeroed. The count lands on minus the frame size, 26 for `127.0.0.1`. sub_476A50
then arms ReadFile at `buffer + count` which is a negative offset onto its own
OVERLAPPED, and the world stream shifts.

Two things are needed and no server change avoids either:

| Fix | Where |
|-----|-------|
| clamp the count to zero before the read | patched into KnC.exe, thunk in the int3 pad at 0x52F140 |
| drop the second arm with no parse between | clientpatch only, needs a writable flag |

## Car record

Base is what sub_490A70 receives as this, 0x01B19090 in the shipped build.
Stride 0xA7260.

| Offset | Field |
|--------|-------|
| +0x740 | slot used |
| +0x748 | display name wstring |
| +0x9D8 | visible, sub_49A920 writes it |
| +0x9DA | zero when this car is the local player |
| +0x2DE8 | draw gate, sub_49A920 sets it from +0x2F40 |
| +0x2F40 | mesh count |
| +0x3244 | position xyz |
| +0x3750 | active |
| +0xA7940 | seventeen float stat block, sub_48F710 adds part stats here |

sub_48DB30 zeroes +0x33A4 for 0x50 dwords at car create, which covers the
podium block and the follow camera triple.

Follow camera reads three floats per car, absolute in the shipped build:

| Address | Meaning |
|---------|---------|
| 0x01B1C510 | follow distance base |
| 0x01B1C514 | unused by case 1 |
| 0x01B1C518 | look at height |

Nothing in the 215 handlers writes them and no packet carries them. They stay
zero, so the eye sits on the kart origin and the view is under the wheels.
Measured good values are 8.4 and 3.5, which is also what the client forces on
stage 13.

## Lobby bar buttons

sub_42CF50 registers each button with sub_44C580. Quest id 15 and Room Craft
id 9 are never registered even though sub_42BCE0 handles both, 15 opens stage
26 and 9 opens stage 19.

Button group slot layout, stride 0x1E0, 50 slots, first slot at group + 4:

| Offset | Field |
|--------|-------|
| +0x10 | state, 1 normal 2 hover 3 pressed |
| +0x14 | x |
| +0x18 | y |
| +0x1C | width |
| +0x20 | height |
| +0x24 | id |
| +0x28 | texture object for _00, then +0xC0 and +0x158 |

sub_42CA10 draws the slots then repaints two disabled plates over them,
Common_Top_Quest_03 at 449,4 and Common_Bottom_RoomCraft_03 at 606,731, which
is exactly where the injected buttons sit. The je at 0x42CC8B guards that pair.
