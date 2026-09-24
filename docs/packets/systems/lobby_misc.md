# Lobby misc

A catch-all group of opcodes that did not belong to their own system: a channel-select stub, two
room-screen member-row click actions, an options telemetry echo, an unused gift-inbox alias, a
gacha/player-preview popup, a system-notice broadcast, a wait-room side-list row remove, and a
fixed-stage redirect variant. Most of the group is dead client-facing capability (builder exists,
nothing calls it) rather than one cohesive feature. Reference: KnC.exe.raw, image base 0x400000.

## Packets

| Dir | Op | Name | Layout | Status |
|-----|----|------|--------|--------|
| C2S | 0x026 | C_NICK_SELECT (room member info) | i32 memberId | implemented (correct no-op) |
| C2S | 0x130 | C_LOBBY_TELEMETRY (option 11) | f32 optionValue | implemented, stored on the account |
| S2C | 0x018 | S_GAME_18 | i32; cstr(256); i32; i32 | **missing, this spec** |
| S2C | 0x095 | S_GIFT_INBOX_LIST (giftInboxAll) | u32 count + count x 212B | implemented (correctly left unsent) |
| S2C | 0x0AA | S_PLAYER_PREVIEW / S_GACHA_ROLL_RESULT | i32; wstr; i32; i32; b44; b56 | **missing, this spec** |
| S2C | 0x0B6 | S_DISPLAY_TEXT (system notice) | wstr(<=46 UTF-16 units); i32 | **missing, this spec** |
| S2C | 0x117 | S_ENTITY_DATA_279 (wait-room side-list remove) | i32 playerId | **missing, this spec** |
| S2C | 0x11D | S_ENTITY_DATA_285 (stage 23 ack) | empty | **already implemented, see note** |
| S2C | 0x11E | S_ENTITY_DATA_286 (redirect, stage 11 fixed) | cstr host(128); i32 port | **missing, this spec** |

Note on S2C 0x11D: the extraction batch's grep for `S_ENTITY_DATA_285`/`0x11D` missed the literal
`0x011D` already in source. `server/game/src/GameServer.cpp:791-797`, inside
`case CMD::C_STAGE23_OPEN:`, already does
`GhostHandler::sendRecordBoard(session, false, this); session->send(Packet::fromCmdFull(0x011D));`
  an unconditional zero-payload send of opcode 0x11D, matching `live_chibikart` (3 frames, all
0 bytes) and the client handler `FUN_0047e980` (0x47E980, reads nothing, gated on
`DAT_00f727f4 != 2`, then advances the stage machine). This packet needs no further work; kept in
the table only so the group's opcode count matches the source `_todo` list.

## C2S 0x026 RoomMemberInfoRequest (C_NICK_SELECT)

**Trigger.** Client sender `FUN_00480850` (0x480850, callsite 0x480884): `BeginPacket(0x26)`, raw
4-byte write, `Send`. Fired from a room-screen member-row click (not the kick icon, which is a
separate hit-test sending opcode 0x39 from the same enclosing dispatcher family, and not the ready
toggle, which is opcode 0x33). Server: `GameServer.cpp:745-749`

```cpp
// no client handler for these three so a reply would be dropped anyway
case CMD::C_NICK_SELECT:
case CMD::C_NICK_ACTION:
case CMD::C_ENTITY_REQ_242:
    break;
```

confirms the client's S2C dispatch table has no handler that would consume a reply sent back on
this opcode, i.e. the request is fire-and-forget from the client's own design, not an
implementation gap. Ghidra confirms no other C2S opcode picks up a "you clicked member X" reply.

**Layout.** `i32 memberId` (4 raw bytes, the clicked row's member/character id).

**Consumer.** Room-screen member-row selection. Whatever visual feedback the click produces (row
highlight) is client-local; no server round trip is expected or possible.

**Data source.** None. Member data is already fully carried by the existing `S_ROOM_MEMBER`
broadcast (opcode 0x021, `PacketBuilder::roomMember`, `PacketBuilder.cpp:417`).

**Implementation.** No change required the current `break;` is the correct behavior. Optional:
log `memberId` for analytics if member-selection telemetry is ever wanted; do not add a reply, the
client will not read one.

## C2S 0x130 Option11Report (C_LOBBY_TELEMETRY)

**Trigger.** Client sender `FUN_00483c10` (0x483C10, callsite 0x483C57), fired once when
`LobbyInit` finishes. Reads option #11's current value via `FUN_0046c1e0(0xb)` (returns a float),
writes it raw after `BeginPacket(0x130)`.

**Layout.** `f32 optionValue` (4 raw bytes, an IEEE-754 float, not an int despite the opcode's
generic-looking name confirmed by decompile, the local is declared `float`).

**Consumer.** Per `docs/packets/PACKET_REGISTRY.md`'s 0x012F prose, option 11 gates the client's
own **local** room-invite popup display; this report is the client telling the server what that
local setting currently is.

**Data source.** `accounts.invite_opt_out`, migration 065. The value is held on the session and
written back when it changes, and the login burst reads it into a fresh session. The random
invite picker skips every opted out session, so the popup is never spent on a client that would
throw it away.

**Implementation.** `GameServer.cpp:873-877` (`case CMD::C_LOBBY_TELEMETRY`) is a comment plus
`break;`, no read call at all. This is safe: `Packet` framing already knows the payload length
from the header, so skipping the read causes no desync. No change required. Optional: parse the
f32 and stash it on the session if the 0x12F pre-filter above is ever implemented; otherwise leave
as-is.

## S2C 0x018 S_GAME_18

**Trigger.** No known trigger condition no live capture (`live_chibikart`/`live_ours` both
absent for this opcode) and no server anywhere (ours or the reference chibikart.gg emulator, by
absence of evidence) has been observed sending it. Client handler `FUN_0047c7a0` (0x47C7A0,
reached via the S2C dispatch stub at 0x47797F, which is otherwise unrelated to the C2S opcode
0x18 `C_CHANNEL_SELECT` that happens to share the same numeric value in the opposite direction)
is a live, non-trivial handler, not a stub.

**Layout.**

| Field | Type | Notes |
|-------|------|-------|
| unk1 | i32 | read, never referenced again dead field |
| unk2 | cstr | ASCII, NUL-terminated, dest `char[256]` |
| unk3 | i32 | passed through |
| unk4 | i32 | passed through |

After the four reads, the handler calls `FUN_00405eb0(unk2, unk3, unk4)` the exact same sink
function the S2C 0x019 `ServerRedirect` handler (`FUN_00479340`) and the S2C 0x11E handler below
call, which stores a pending redirect target and arms a deferred reconnect handled one frame later
from the main loop (avoids the double-OVERLAPPED-arm race the legacy opcode 0x54 path hits, see
the group's 0x019 row).

**Consumer.** Almost certainly a third redirect/reconnect variant, structurally identical in its
consuming call to 0x019 (host, port, mode) `unk2`=host, `unk3`=port, `unk4`=mode, `unk1`
unused. No UI text or screen was traced past the sink call.

**Data source.** Same as 0x019: a target host/port already available wherever a login-to-game
handoff or game-server transfer decision is made (`server/game/src/packets/gen/CharCreatePackets.cpp:383`
`CharCreatePackets::serverRedirect`, `server/login/src/handlers/HandshakeHandler.cpp:696-716`).

**Implementation.** No server writer exists anywhere in `server/` or `shared/` (confirmed by
tree-wide grep for `S_GAME_18` and the opcode; only the `Protocol.h:136` constant exists). Given
the shared sink function, this looks like a redundant/legacy redirect path rather than a distinct
feature. No action needed unless a fourth redirect variant is specifically required; if it is,
model the builder on `CharCreatePackets::serverRedirect` (cstr host, i32 port, i32 mode) and
confirm `unk1`'s purpose first (it is currently read and discarded by the client, so any value is
safe to send).

## S2C 0x095 GiftInboxReceivedList (giftInboxAll)

**Trigger.** Would be the gift/mailbox screen refresh, same as 0x96/0x97, but is intentionally
never sent. `server/game/src/GameServer.cpp:2718-2721` documents why:

```cpp
// The mailbox is container B, reached by 0x95 and 0x97 which share one
// handler. A chibikart capture carries the GM Isma welcome mail there, and
// only that container gets append 0x99, update 0x9A and delete by id 0x9B.
// We used to fill container A on 0x96 and blank B, so nothing ever showed.
appendPkt(PacketBuilder::giftInboxListSent(inbox));   // 0x97 the mailbox
```

Client handler `FUN_0047be00` (0x47BE00) is shared by opcodes 0x95 and 0x97 it clears its
212-byte-record list (`FUN_00450220`) before appending. Sending an empty 0x95 in the same login
burst as a filled 0x97 wipes the just-filled mailbox; commit `21c4e8a`
("fix(gifts): mail went into the wrong container and an empty packet wiped the right one",
2026-09-02) removed the 0x95 send for exactly this reason.

**Layout.** `u32 count; count x record[0xD4=212B]`, same record shape as `PacketBuilder::giftRecord`
(used by 0x96/0x97) `+0x00 id, +0x04 category, +0x08 senderId, +0x0C 0, +0x10 sender name (48B
fixed), +0x40 0, +0x44 packed wstr date/time/subject`.

**Consumer.** Gift inbox mailbox UI list the same on-screen list as 0x97, since they share a
client handler and container.

**Data source.** `gift_log` (`server/scripts/007_emu_buildout.sql:242`, `read_flag` column added
`server/scripts/037_pendants.sql:58`), already queried twice per login burst for 0x96/0x97 at
`GameServer.cpp:2703-2744`.

**Implementation.** `PacketBuilder::giftInboxAll` (`PacketBuilder.cpp:2302`, header
`PacketBuilder.h:612`) exists and is correct in shape but has zero call sites this is by design,
not an oversight. No action required. If a genuinely separate archive/received-alias screen is
ever added that is not container B, it must not be sent in the same burst as 0x97 (or must clear
and repopulate 0x97's own container afterward), or it will re-introduce the mailbox-wipe bug the
2026-09-02 fix removed.

## S2C 0x0AA GhostSessionStart / PlayerPreview (S_PLAYER_PREVIEW, S_GACHA_ROLL_RESULT)

**Trigger.** Unwired. In-code comment at `PacketBuilder.cpp:1780`-area identifies the client
handler `FUN_0047cba0` (0x47CBA0) by its legacy name, "gacha roll result popup" i.e. this should
fire after a gacha/box-roll purchase resolves, to show the rolled character/vehicle/item. No
handler in this group's `server_refs` performs a gacha roll; the feature that should call this
builder was not located while reviewing `lobby_misc`'s own opcodes.

**Layout.** Fixed-size, no count-driven trailing arrays (older registry note describing a
count-prefixed 176-byte tail is refuted by decompile no loop exists past the fixed header):

| Field | Type | Notes |
|-------|------|-------|
| playerId | i32 | |
| name | wstr | UTF-16, NUL-terminated, dest `DAT_00c1a940` |
| level | i32 | |
| rank | i32 | |
| vehicle | 44B (b44) | 11 x i32: id, templateId, durability, maxDurability, stats[0..6] |
| item | 56B (b56) | 5 x i32 (id, templateId, quantity, slot, equipped) + 9 x i32(0) padding |

Total 112 + 2*(nameLen+1) bytes; matches `live_chibikart`'s single 142-byte capture exactly
(name "christoferbuss", 14 chars).

**Consumer.** Player-preview / gacha-roll-result popup shows the rolled item/character to the
player after a gacha pull.

**Data source.** No gacha catalog or roll-log table exists in the schemas reviewed for this
group. Vehicle/kart template data and item template data already back the garage/shop systems and
would supply the `vehicle`/`item` blocks once a roll result is known; the roll itself (what gets
picked, odds, currency spent) is a shop/gacha-system concern outside `lobby_misc`.

**Implementation.** `PacketBuilder::playerPreview(int32_t playerId, const std::u16string& name,
int32_t level, int32_t rank, const VehicleInfo&, const ItemInfo&)` (`PacketBuilder.cpp:1791`,
header `PacketBuilder.h:402`) is fully implemented and field-order-correct, but has zero callers.
Wire it into whichever handler performs a gacha/box purchase once that system exists; this spec
cannot locate that handler because it is outside `lobby_misc`'s packet set. No shape work is
needed on `playerPreview` itself.

## S2C 0x0B6 SystemNotice (S_DISPLAY_TEXT)

**Trigger.** Unwired no live capture either (absent from both `live_chibikart` and `live_ours`
for this opcode, unlike its immediate 0xB4/0xB5 neighbors which have both). Would be a
generic-purpose text dialog/notification box.

**Layout.**

| Field | Type | Notes |
|-------|------|-------|
| text | wstr | UTF-16, NUL-terminated, dest fixed `undefined1[94]` stack buffer (≤46 UTF-16 code units including NUL) **client does not clamp or bounds-check this read** |
| param | i32 | passed through to the dialog call, zeroed trailing struct field alongside it |

**Consumer.** `FUN_0043D690(1)` / `FUN_00406FF0` a generic dialog/notification popup.

**Data source.** None ad hoc text, for a GM/system announcement feature.

**Implementation.** `PacketBuilder::displayText(const std::u16string& text, int32_t param)`
(`PacketBuilder.cpp:914`, header `PacketBuilder.h:230`) exists, writes `wstr` then `i32` in the
client's expected order, but has zero call sites. To use it:
1. Wire a call into a GM/admin broadcast command or a system-notice handler.
2. **Must** clamp `text` to <=46 UTF-16 code units (92 bytes + NUL) before calling
   `writeWString`, inside `displayText` itself the client's fixed 94-byte stack destination at
   `FUN_0047AC60` has no bounds check, so an unclamped longer string is a stack buffer overflow
   client-side the moment this path is wired up. No such clamp exists in `displayText` today.

## S2C 0x117 WaitRoomSideListRemove (S_ENTITY_DATA_279)

**Trigger.** Unwired. Client handler `FUN_0047e750` (trampoline at 0x47E750) reads one field and
calls `FUN_0040f5f0(playerId)`, a thiscall that scans an 8-slot array (`this+0x1cc9c`, stride
0x28) for a matching id and clears that slot; safe no-op if not found or the list was never
populated. This is a small **wait-room side list**, distinct from the main room-member roster
carried by `S_ROOM_MEMBER` (0x021). The companion add opcode is 0x116 (`S_PLAYER_NOTICE`,
`PacketBuilder::playerNotice`, `PacketBuilder.cpp:2332`-area) per the group's own 0x116 row, its
only current sender is a GM `/notice` debug command, not a real room-join/leave path, so the
side-list feature is not reachable by ordinary play today.

**Layout.** `i32 player_id` (4 bytes).

**Consumer.** Removes one row from the wait-room side list by player id.

**Data source.** None pure UI-list removal echo, keyed by the same player/character id already
used for room membership; no new table needed.

**Implementation.** No server code sends opcode 0x117 anywhere (confirmed by tree-wide grep; only
the `Protocol.h:372` constant exists), and its companion add (0x116) is itself not wired into a
real feature path. If the wait-room side-list panel is ever prioritized: add
`PacketBuilder::waitRoomSideListRemove(int32_t playerId)` (mirror `playerNotice`'s shape, single
i32), and call it alongside a real (non-debug-command) 0x116 add, from the same room join/leave
events that already send `S_ROOM_MEMBER` / `PacketBuilder::playerDisconnect`
(`GameServer.cpp`, room-leave path around line 1707). Until then, leave both 0x116 and 0x117
unimplemented there is no player-facing feature currently driving them.

## S2C 0x11E ServerRedirectStage11 (S_ENTITY_DATA_286)

**Trigger.** Unwired, no live capture either direction. Client handler `FUN_0047e9b0` (0x47E9B0)
reads a host string and a port, then calls `FUN_00405eb0(host, port, 0xb)` the same
redirect/reconnect sink used by S2C 0x019 (general `ServerRedirect`) and S2C 0x018 above, but with
the target stage hardcoded to `11` (0xb) instead of taking a `mode` field off the wire.

**Layout.**

| Field | Type | Notes |
|-------|------|-------|
| host | cstr | ASCII, NUL-terminated, dest `char[128]` |
| port | i32 | raw 4 bytes |

Size = `strlen(host) + 1 + 4`.

**Consumer.** A server-transfer/redirect flow fixed to stage 11, alongside the general-purpose
0x019 redirect used elsewhere in this group and the still-unexplained 0x018 variant above. No
evidence pins what makes stage 11 special or when the client would need this specific variant
instead of 0x019.

**Data source.** None same as 0x019, a target host/port from server config or a login-to-game
handoff (`CharCreatePackets::serverRedirect`, `HandshakeHandler.cpp:696-716`).

**Implementation.** No server writer exists anywhere (confirmed by tree-wide grep; only the
`Protocol.h:379` constant exists). If a cross-server/stage-11 handoff is ever needed: add a
`PacketBuilder` method writing ASCII `cstr(host)` then `u32(port)`, matching the client's read
exactly, and call it from whichever event needs the fixed stage-11 target. Given three
redirect-shaped opcodes now cataloged in this group (0x018, 0x019, 0x11E) with only 0x019 wired,
resolve which one the intended login-to-game handoff should actually use before adding a fourth
sender see open questions.

## Design

**What it is for the player.** Not one feature a bin of loosely related utility packets left
over once the other systems (channel, room, gift, ghost, redirect) were specified. Two of the
nine `_todo` opcodes (0x026, 0x130) are already correctly implemented as silent no-ops. One
(S2C 0x11D) is already fully implemented; the batch's "missing" finding was a stale grep miss.
The remaining six are genuinely unwired: three duplicate/variant server-redirect senders (0x018,
0x019 already live, 0x11E), a dead gift-inbox alias that must stay dead (0x095), a gacha-result
popup whose trigger lives in a different system (0x0AA), a system-notice broadcast with no
current sender (0x0B6), and a wait-room side-list panel whose companion add path is itself only a
GM debug command (0x117).

**Tables.**

| Table | Columns used here | Role |
|-------|--------------------|------|
| `gift_log` | `id, sender_id, target_id, target_name, category, base_key, price_key, message, sent_at, read_flag` | already backs 0x96/0x97 (and correctly-unsent 0x95) mailbox queries at `GameServer.cpp:2703-2744` |

No new table is required for any `_todo` opcode in this group. A gacha catalog/roll-log table
would be needed to wire 0x0AA, but that belongs to whichever shop/gacha system performs the roll,
not to `lobby_misc`.

**Handlers.**

No new handler files are required. Only new `PacketBuilder` free functions, called from existing
handlers, if/when each dead opcode is prioritized:

| Builder to add | Call from | Opcode |
|-----------------|-----------|--------|
| (none reuse `CharCreatePackets::serverRedirect` shape) | login/game handoff decision | 0x11E, possibly 0x018 |
| `PacketBuilder::waitRoomSideListRemove(playerId)` (+ a real 0x116 add) | room join/leave path near `GameServer.cpp:1707` | 0x117 (+ 0x116) |
| (none `displayText` already exists) | new GM/admin broadcast command | 0x0B6 |
| (none `playerPreview` already exists) | gacha/box-roll handler (outside this group) | 0x0AA |

**Flows.**

Room-screen member row click (already correct):

| # | Dir | Op | Packet | Trigger |
|---|-----|----|--------|---------|
| 1 | C2S | 0x026 | member id | player clicks a room member row for info (not kick, not ready) |
| | | | no reply | client has no S2C handler for 0x026, server correctly sends nothing |

Options telemetry (already correct):

| # | Dir | Op | Packet | Trigger |
|---|-----|----|--------|---------|
| 1 | C2S | 0x130 | f32 option 11 value | client's LobbyInit finishes |
| | | | no reply | server stores it on the account and the invite picker honours it |

Stage 23 open (already implemented, included for completeness):

| # | Dir | Op | Packet | Trigger |
|---|-----|----|--------|---------|
| 1 | C2S | 0x11D | empty | player opens the stage-23 (record/best-times) screen |
| 2 | S2C | record board | ghost/record rows | server pushes ranks before the ack, must precede step 3 |
| 3 | S2C | 0x11D | empty | ack that opens the screen with ranks already populated |

Hypothetical gift-inbox refresh (0x95 must never appear here, included to document the hazard):

| # | Dir | Op | Packet | Trigger |
|---|-----|----|--------|---------|
| 1 | | 0x97 | gift rows (waiting) | login burst, `read_flag = 0` rows |
| 2 | | 0x96 | gift rows (taken) | login burst, `read_flag = 1` rows |
| X | | 0x95 | (do not send) | shares client handler/container with 0x97 sending after step 1 wipes it |

**Open questions.**

1. What actually triggers S2C 0x018 and S2C 0x11E. Both share the redirect sink function
   `FUN_00405eb0` with the working 0x019 path but no live capture or C2S counterpart pins when the
   client should receive either instead of 0x019. Possible that one or both are legacy/dead
   client code never exercised by any server, stock or otherwise.
2. Whether the wait-room side list (0x116/0x117) is a real, shippable feature or a leftover
   developer panel its only known sender anywhere in the codebase is a GM `/notice` command, not
   a room join/leave event.
3. Where the gacha/box-roll purchase flow that should trigger S2C 0x0AA actually lives not
   found among this group's `server_refs`; needs investigation in the shop system.
4. `unk1` on S2C 0x018 is read but never referenced by the client past the read confirmed dead
   on the client side, but its intended server-side meaning (if any) is unknown.
