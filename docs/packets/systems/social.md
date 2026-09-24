# Social system spec: room invite, friend add, notes

Scope: the seven opcodes in group "social" marked `_todo` in the extraction batch:
C2S 0x029, S2C 0x027, S2C 0x06C, S2C 0x06E, S2C 0x06F, S2C 0x082, S2C 0x083.

Client reference: `KnC.exe.raw`, image base 0x400000. Server reference: this repository.

Two of the seven (S2C 0x06C, S2C 0x06F) turned out to already be implemented under a
different, newer builder than the one the extraction batch found dead. That is called out
per-opcode below. The other five are real gaps.

## Summary table

| dir | op | name | status now | verdict |
|---|---|---|---|---|
| C2S | 0x029 | NamePopupActionB | not parsed, falls to `break` | GAP |
| S2C | 0x027 | DeadDialog39 | env-gated probe only, client discards it | DEAD BY DESIGN, no fix needed |
| S2C | 0x06C | RoomInvite | **already sent** via `SocialHandler::doRoomInvite` | ALREADY DONE |
| S2C | 0x06E | ForceJoinInvitedRoom | never sent | GAP |
| S2C | 0x06F | FriendAddResult | **already sent** via `doFriendAdd`/`bindFriendPair` | ALREADY DONE |
| S2C | 0x082 | NoteAppend | never sent, notes are stored but never delivered | GAP |
| S2C | 0x083 | NoteAppendSorted | never sent, same cause as 0x082 | GAP |

## Per-opcode detail

### C2S 0x029 NamePopupActionB

**Trigger.** Client sender is `sub_4808F0` (call site 0x480924): `BeginPacket(0x29)` then
`FUN_0044eb00` (write UTF-16 wstr) then `Send`. It is the sibling of C2S 0x025
`NamePopupActionA` (`sub_4807B0`, call site 0x4807E4) which is already wired
(`server/game/src/GameServer.cpp:701`, replies `S_ROOM_STRING`). Both opcodes write the
same global text-edit buffer at `0x00E1EFF8`, which every text-input dialog in the client
shares, so the buffer write alone cannot identify which screen is open. The client-side
comment context (PACKET_REGISTRY.md:1026-1033) establishes: both are the two buttons of
popup id 7, both clear `dword_B23390`, close all popups, then raise `MSG_WAIT` type 0, so
an answer is expected. `xrefs_to 0xE1EFF8` (30 writer sites) shows the buffer is generic
UI-wide, so button A vs button B cannot be told apart from the write site; only the two
distinct opcodes distinguish the buttons.

Server side: `GameServer.cpp:745-748` groups `C_NICK_SELECT` / `C_NICK_ACTION` (0x29) /
`C_ENTITY_REQ_242` into one case that falls straight to `break;` with the comment "no
client handler for these three so a reply would be dropped anyway." Confirmed live in the
current tree, not stale. If the client never gets an answer, `MSG_WAIT` state is presumably
cleared by the popup already having closed client-side before the wait fires (not proven
from the binary; see open questions).

**Layout.** One UTF-16LE wstr, NUL-terminated, no length prefix. `size = 2*(len+1)`.

**Consumer.** Same popup family as 0x025 (`C_NICK_QUERY`), a text-entry dialog (popup id 7)
that asks for a nickname and offers two action buttons; opcode identifies which button was
pressed.

**Data source.** None new; whatever the eventual action needs (name lookup against
`characters`).

**Implementation.** Do not guess the semantic meaning (add friend vs invite vs something
else) without a live client capture pinning popup id 7 to a specific screen. If/when the
meaning is confirmed, mirror the working 0x025 handler
(`GameServer.cpp:701`, `SocialHandler`-style name lookup) instead of leaving the case as a
silent `break`. Until then, leave as-is; sending an S2C reply here is unnecessary since the
client has no handler waiting on 0x29's server-side echo (unlike 0x25/S_ROOM_STRING).

**Confidence.** Medium (wire shape and trigger context are proven; the concrete UI action is
inferred only).

---

### S2C 0x027 DeadDialog39

**Trigger.** Never happens in production traffic. The only server call site is
`SocialHandler::deadDialogProbe` (`server/game/src/handlers/SocialHandler.cpp:555-563`),
itself gated by `deadDialogProbeEnabled()` (line 203-206) which requires
`getenv("KNC_SOCIAL_DEAD_DIALOG_PROBE") == "1"`, off by default. `deadDialogProbe` is
called once, from `SocialHandler.cpp:548`, alongside its sibling
`SocialPackets::deadDialog37` (S2C 0x025). Even if fired, the client handler
`FUN_00479A30` (0x479A30) reads the three fields into locals and calls `FUN_00463cd0`,
which decompiles to an empty-body `return;` (true nullsub). Nothing is drawn or stored.

**Layout.** `u32 id` (read, then explicitly zeroed, never used) then
`wstr name` (UTF-16LE, NUL-term, into a 28-byte / 13-wchar client buffer) then
`cstr msg` (ASCII, NUL-term, into a 256-byte client buffer).
`size = 4 + 2*(name+1) + strlen(msg)+1`. Server builder
`SocialPackets::deadDialog39` (`server/game/src/packets/gen/SocialPackets.cpp:545-557`)
writes that exact order and clamps to the same sizes (`checkSize` line 556).

**Consumer.** None. The handler is a nullsub; this opcode currently drives no client
feature.

**Data source.** None.

**Implementation.** No action. This is intentionally dead: the source comment at
`SocialHandler.cpp` above `deadDialogProbe` states it must never run in production, and the
client itself discards the payload regardless. Document only; do not wire a real send path
unless a future client build is found to actually consume `FUN_00463cd0`.

**Confidence.** High.

---

### S2C 0x06C RoomInvite already implemented, group data was stale

**Trigger (client).** Sent to the invited player when someone on their friends/room roster
uses "invite to room" (C2S 0x06C `RoomInviteByName`, handled server-side by
`SocialHandler::doRoomInvite`, `server/game/src/handlers/SocialHandler.cpp:1313-1361`,
routed via `GameServer.cpp:799-806` `case CMD::C_ROOM_INVITE`). Client handler is
`FUN_0047b030` (0x47B030). On receipt it gates on client UI state before showing anything:
if the current screen is one of two specific states it **auto-declines** by sending
C2S 0x06D (`RoomInviteAnswer`) with code 6 (in race/mission), or with code 4/5 if a
smalltalk/other modal is open, or code 7 if already in the same room (checked against the
room id field). Only when none of those apply does it call `FUN_0045BF60()` to open the
invite popup, stashing `senderId`/`roomId`/`roomPassword` in globals
(`DAT_00f33a5c`, `DAT_00f33a60`, etc.) for later opcodes (0x06D accept via C2S 0x2F, or
S2C 0x06E) to read back.

**Layout.** Mixed encoding, matches client field-for-field:
`u32 reply_key; wstr inviter_name (dest 26B/12 units+NUL); cstr unknown (dest 34B, no client reader); u32 unknown (no client reader); u32 room_id; wstr room_password (dest 18B/8 units+NUL); u8 flag`.
Field 2 is the inviter name, field 6 is the room password do not swap.

**Consumer.** The room-invite popup shown to the invited player, and the source of the
room id/password the client later replays on accept.

**Data source.** Existing `rooms` in-memory state (`srv->getRoom`), `room->settings()`
for the password. No schema change.

**Implementation already done.** `server/game/src/packets/gen/SocialPackets.cpp:506-524`
(`SocialPackets::roomInvite`, opcode `OP_ROOM_INVITE = 0x006C`,
`server/game/include/packets/gen/SocialPackets.h:233-241` for the `RoomInvite` struct)
writes the exact same 7-field mixed-encoding shape, called from
`SocialHandler::doRoomInvite` (`SocialHandler.cpp:1358`, `target->send(...)`). This
supersedes the dead `PacketBuilder::invitePopup` (`PacketBuilder.cpp:1197-1211`) that the
extraction batch found with zero call sites that function is still unused today and can
be deleted, but the opcode itself is live and correct via the newer `SocialPackets`
builder. `doRoomInvite` sets `inv.flag = 0` unconditionally (enables the "already in that
room" answer-7 branch client-side) and `inv.replyKey = s->characterId` (the id the
C2S 0x06D answer echoes back, per `RoomInviteAnswer` verified elsewhere in this group).

**Confidence.** High (re-read current source, confirmed builder, struct, and call site).

---

### S2C 0x06E ForceJoinInvitedRoom

**Trigger (client).** Handler `FUN_0047b190` (0x47B190) takes no wire fields at all; on
receipt it unconditionally calls `FUN_00481e20(DAT_00f33a5c, &DAT_00f33a60)`, the exact
same function that builds and sends C2S 0x06E `JoinInvitedRoom` (room id + UTF-16LE
password), using the globals last populated by the S2C 0x06C handler
(`FUN_0047b030`). `get_xrefs_to 0x481e20` shows its **only** caller anywhere in the client
is `FUN_0047b190` the room-invite popup's own Accept button does not call it directly
(accept normally goes through C2S 0x2F, the regular room-join path). So S2C 0x06E is not
part of the popup-accept flow; it is a **push-to-rejoin** trigger: the server sends an
empty 0x06E frame and the client immediately echoes back C2S 0x06E carrying whatever
room id/password S2C 0x06C (or S2C 0x134, per PACKET_REGISTRY.md:1832) last cached. If
the server sends S2C 0x06E without ever having sent a preceding 0x06C/0x134, the client
joins with stale/empty globals.

**Layout.** S2C: empty payload (0 bytes) pure trigger, `PacketBuilder::shopCall()`
(`server/game/src/packets/PacketBuilder.cpp:1213-1216`) already writes this shape.

**Consumer.** Forces an already-invited client to (re)join the room it was last invited to,
without the player clicking anything e.g. a room owner recalling an accepted member, or
resuming a join after a disconnect/reconnect while an invite was pending.

**Data source.** None new. Needs the same room id/password already available wherever
`doRoomInvite` builds S2C 0x06C (`SocialHandler.cpp:1349-1358`).

**Implementation.** `PacketBuilder::shopCall()` exists and matches the wire shape but has
zero call sites anywhere in `server/`, confirmed by fresh grep. To use it: send
`PacketBuilder::shopCall()` to a session immediately after (or instead of) sending it a
fresh `SocialPackets::roomInvite` when the intent is to force rather than prompt e.g.
from `SocialHandler::doRoomInvite` add a variant, or from the C2S 0x06D
(`RoomInviteAnswer`) accept-adjacent path if the design wants a server-driven auto-join.
Given the client only calls the C2S 0x06E sender from this handler, S2C 0x06E is a
legitimate second join path alongside the existing C2S 0x2F accept and does not currently
need to exist for the basic invite flow to work it is additive (force/rejoin), not
required. Recommend leaving unimplemented until a concrete "force join" feature is
designed (see open questions), since sending it with no prior 0x06C will misjoin the
client with garbage globals.

**Confidence.** High on mechanics (fresh decompile + xrefs), medium on the intended feature
this exists for.

---

### S2C 0x06F FriendAddResult already implemented, group data was stale

**Trigger (client).** Sent to append one record to one of the client's two 100-slot arrays
(friend list or pending-request list) without a full-list replace. Handler `FUN_0047b3c0`
(0x47B3C0), re-decompiled:
- reads `u32 resultCode`
- `resultCode == 1`: reads 0x2C (44) raw bytes and calls `FUN_0044ef90` **the same
  append function** used by the S2C 0x076 `FriendList` handler (`FUN_0047b1b0`, confirmed
  via `get_xrefs_to 0x44ef90`: callers are `FUN_0047b1b0` and `FUN_0047b3c0` only). So the
  44-byte body is one `FriendRecord`, byte-identical to a single entry of 0x076's array.
- `resultCode == 12`: reads 0x24 (36) raw bytes and calls `FUN_0044ee10` **the same
  append function** used by the S2C 0x078 `FriendRequestList` handler (`FUN_0047b220`,
  confirmed via `get_xrefs_to 0x44ee10`: callers are `FUN_0047b220` and `FUN_0047b3c0`
  only). So the 36-byte body is one `RequestRecord`, byte-identical to a single entry of
  0x078's array.
- any other `resultCode`: no extra body, bare status code only.

The extraction batch's client-side read was correct on bytes (44/40 total incl. the u32)
but mis-labeled the payload as a shop-purchase vehicle/item record; the xref evidence above
shows it is actually the friend-list and friend-request-list single-record append, which
matches the live_chibikart sample (`0x0C 00 00 00 26 00 00 00 53 00 72 00 4B 00 69 00 6E
00 68 00 61 00 73 00 ...` → resultCode=12, requester_id=38, name "SrKinhas...", i.e. a
`RequestRecord`).

**Layout.**
```
+0x00  u32  resultCode
-- resultCode == 1 --
+0x04  blob[0x2C]  FriendRecord   { u32 player_id; wchar[14] name (28B, NUL-term); u32 level; i32 status_a; i32 status_b }
-- resultCode == 12 --
+0x04  blob[0x24]  RequestRecord  { u32 requester_id; wchar[14] name (28B, NUL-term); u32 level }
-- any other code --
(nothing)
size 4, 48, or 40
```
`resultCode` values are the shared `MSG_FRIEND_*`/`MSG_NOTE_*` table
(`server/game/include/packets/gen/SocialPackets.h:100-112`): 0 add, 1 add-ok
(`MSG_FRIEND_ADD_OK`), 2 dup, 3 max, 4 accepter-max, 5 no-user, 6 del, 7-11 note-family
codes, 12 request-queued (`MSG_FRIEND_REQ_QUEUED`).

**Consumer.** Live-update of the messenger's friend list / pending-request tabs without a
full re-push (0x076/0x078 remain the full-replace opcodes).

**Data source.** `friend_requests` table (existing), `characters` for name/level.

**Implementation already done.** `OP_FRIEND_ADD = 0x006F`
(`server/game/include/packets/gen/SocialPackets.h:67`). Builders
`SocialPackets::friendAddResultAccepted(const FriendRow&)` (writes `MSG_FRIEND_ADD_OK` +
`FriendRecord`, `SocialPackets.cpp:398-405`) and
`SocialPackets::friendAddResultQueued(const RequestRow&)` (writes `MSG_FRIEND_REQ_QUEUED` +
`RequestRecord`, `SocialPackets.cpp:407-414`) both exist and match the client shape
exactly (`checkSize` against `SIZE_FRIEND_REC`/`SIZE_REQUEST_REC`). Call sites:
`SocialHandler.cpp:1419` (`doFriendAdd`, queues the pending request to the target),
`SocialHandler.cpp:1672` and `:1678` (`bindFriendPair`, sends the accepted `FriendRecord`
to both parties once a request is accepted). This entirely supersedes the dead
`PacketBuilder::shopPurchaseVehicle`/`shopPurchaseSmallItem`/`shopPurchaseError`
(`PacketBuilder.cpp:1137-1171`) that the extraction batch found with zero callers those
three are genuinely unused today and can be deleted; the opcode itself is live via the
`SocialPackets` builders. No further work needed for 0x06F.

**Confidence.** High (re-read current source, confirmed builders, call sites, and shared
append-function identity via xrefs).

---

### S2C 0x082 NoteAppend and S2C 0x083 NoteAppendSorted

**Trigger (client).** Handlers `FUN_0047b710` (0x82) and `FUN_0047b7d0` (0x83) both read a
raw 396-byte (0x18C) blob and append it whole as one entry into the client's note-list
array (`FUN_00451b70`, base container `0x01A5CE10`, count field `0x01A5FC7C`, cap 30
slots, 396-byte stride; past the cap the append drops the oldest slot with no error). 0x83
additionally calls `FUN_00451a20()` after the append a no-arg bubble-sort of the whole
30-slot array by `sort_key_1` then `sort_key_2` ascending. Per PACKET_REGISTRY.md:2047-2069
the intended split is: 0x83 for the bulk inbox load (append-then-resort once, so the whole
list lands sorted), 0x82 for a single live arrival (append only, no full resort needed).

Neither opcode is ever sent server-side: `extendedData130`/`extendedData131`
(`PacketBuilder.cpp:2153-2175`, declared `PacketBuilder.h`) have zero call sites anywhere
in `server/` (grep confirmed). Consistent with this, `player_notes` rows are written by
`SocialHandler::handleNoteSend` (`SocialHandler.cpp:1887-1932`, C2S 0x081) but the table is
never `SELECT`ed anywhere else in the server no login-burst push, no on-demand list
request handler exists. The note system currently only lets a sender confirm their note was
queued (`SocialPackets::messengerResult` with `MSG_NOTE_SEND`/etc.); the recipient's client
never receives the note at all.

**Layout.** Both opcodes: one raw 396-byte blob per note, no count prefix (append is
one-record-per-packet; the client owns the list). Record layout, from the client's own
render code (already reversed in PACKET_REGISTRY.md:2054-2065):
```
+0x00  u32        note_id
+0x04  wstr[13]   sender_name   drawn column 1 row 1 (26 bytes incl NUL)
+0x1E  wstr[11]   sort_key_1    drawn column 2 row 1 (22 bytes incl NUL)
+0x34  wstr[9]    sort_key_2    drawn column 2 row 2 (18 bytes incl NUL)
+0x46  u8         read_flag     1 greys the row
+0x48  wstr       body          10 wchar preview then "...", remaining space to +0x18C
size   396 fixed
```
`sort_key_1`/`sort_key_2` are almost certainly a date/time split (date, time) given they
drive the 0x83 sort and sit in the column-2 two-line display slot next to the sender name;
not proven from the binary which exact fields populate them (see open questions).

**Consumer.** The messenger's Notes/mailbox tab: populates the 30-row inbox list
(0x083 on open/login) and appends a single new arrival while the tab may already be open
(0x082, e.g. right after a `C_NOTE_SEND` from another player currently online).

**Data source.** Existing `player_notes` table
(`server/scripts/007_emu_buildout.sql:711-722`: `id, from_id, to_id, to_name, body,
is_read, created_at`). All fields needed for the 396-byte record are already present
(`id`→note_id, sender display name via `from_id`→`characters`, `is_read`→read_flag,
`body`→body, `created_at`→sort_key_1/sort_key_2).

**Implementation.**
1. Add a handler that loads a character's unread/recent `player_notes` rows (`to_id = ?`,
   ordered by `created_at`, capped at 30) and packs each into the 396-byte record above via
   `PacketBuilder::extendedData131` (0x83), sent once either on login/lobby-enter (folded
   into the existing login burst alongside 0x076/0x078/0x079) or on a dedicated "open
   messenger notes tab" request if a C2S opcode for that exists elsewhere in the messenger
   group (not in this batch; check the full registry before adding a new C2S opcode).
2. In `SocialHandler::handleNoteSend`, after the successful `INSERT`, if the recipient is
   currently online (`onlineSession`, same helper used by `doFriendAdd`), send them
   `PacketBuilder::extendedData130` (0x82) with the freshly-inserted row packed the same
   way, so the note shows up live without requiring a reconnect.
3. Both should reuse one shared "pack a `player_notes` row into 396 bytes" helper next to
   `SocialPackets`/`PacketBuilder` rather than duplicating the field layout twice.
4. `extendedData130`/`extendedData131` already accept a raw `const uint8_t* data396` no
   signature change needed, only a caller and the row-to-blob packer.

**Confidence.** High on wire shape and dead-code status (fresh decompile + grep + registry
cross-check), medium on which `player_notes` column maps to `sort_key_1` vs `sort_key_2`.

## Design: notes and room-invite/friend-add delivery

### What it's for

Three related player-to-player features share this opcode range:
- **Room invite**: invite a named player to your current room; they get a popup with
  accept/decline, or an auto-decline if they're busy.
- **Friend add**: request a named player as a friend; they get a live list update when the
  request lands and when it's accepted, without a full list re-push.
- **Notes**: an offline-capable one-way message (like a mailbox), stored in the DB and
  meant to be delivered to the recipient's messenger UI whenever they're around to see it.
  Currently notes are the one piece of this that is fully un-delivered: the DB insert works,
  the sender gets a confirmation, and the recipient never sees anything.

### Tables

All existing, no schema change required by this spec:
- `player_notes` (`server/scripts/007_emu_buildout.sql:711-722`) `id, from_id, to_id,
  to_name, body, is_read, created_at`. Already used by C2S 0x081 (`NoteSend`). Needed by
  the new S2C 0x082/0x083 delivery path (read-only, no new columns).
- `friend_requests`, `characters` already used by `doFriendAdd`/`bindFriendPair` for
  S2C 0x06F, no change.
- in-memory `rooms`/`Room::settings()` already used by `doRoomInvite` for S2C 0x06C, no
  change.

### Handlers

- `server/game/src/handlers/SocialHandler.cpp` add note delivery:
  - a new method, e.g. `SocialHandler::pushNotes(Session::Ptr s)`, called from wherever the
    login/lobby burst already sends `friendList`/`friendRequestList`/`blockList`, that
    queries `player_notes WHERE to_id = ?` and sends one `extendedData131` (0x83) frame.
  - extend `handleNoteSend` (`SocialHandler.cpp:1887`) to also push a live
    `extendedData130` (0x82) to the recipient's session if online, after the insert
    succeeds.
- `PacketBuilder.cpp` `extendedData130`/`extendedData131` need no change, just callers.
  `shopCall()` (0x06E) needs no change either, just a caller once the force-rejoin feature
  is designed (see open question below); it is not required for the base invite flow.
- No change needed to `doRoomInvite` (0x06C) or `doFriendAdd`/`bindFriendPair` (0x06F);
  both already emit the correct frames.
- C2S 0x029 (`C_NICK_ACTION`) stays a no-op `break` in `GameServer.cpp:746-748` until its
  concrete UI meaning is pinned down (see open question).

### Flows

**Room invite (already working, documented for completeness):**

| # | dir | op | packet | trigger |
|---|---|---|---|---|
| 1 | C2S | 0x06C | RoomInviteByName | inviter types a name and confirms "invite to room" |
| 2 | S2C | 0x06C | RoomInvite | server resolves the name, sends the invite record to the target |
| 3 | C2S | 0x06D | RoomInviteAnswer | target's client auto-declines (busy/in-race/same-room) or the player declines |
| 3' | C2S | 0x2F | (room join, different opcode) | target accepts via the normal room-join path, using the room id/password cached from step 2 |

**Force-rejoin (new, optional, not required by the base flow):**

| # | dir | op | packet | trigger |
|---|---|---|---|---|
| 1 | S2C | 0x06C | RoomInvite | must precede 0x06E or the client joins with stale globals |
| 2 | S2C | 0x06E | ForceJoinInvitedRoom | server decides to pull the player back into that room (e.g. room owner recall) |
| 3 | C2S | 0x06E | JoinInvitedRoom | client echoes the cached room id/password automatically, no user action |
| 4 | | | (existing) `LobbyHandler::handleJoinRoom` | server processes it exactly like a 0x2F join |

**Friend add (already working, documented for completeness):**

| # | dir | op | packet | trigger |
|---|---|---|---|---|
| 1 | C2S | 0x06F | FriendAddByName | requester types a name and confirms "add friend" |
| 2 | S2C | 0x06F | FriendAddResult (code 0 to requester) | server validates and inserts the pending row |
| 3 | S2C | 0x06F | FriendAddResult (code 12, RequestRecord, to target if online) | live-appends the pending request to the target's request tab |
| 4 | C2S | 0x070 | FriendReqAccept | target accepts |
| 5 | S2C | 0x06F | FriendAddResult (code 1, FriendRecord, to both) | live-appends the new friend to both parties' friend lists |

**Notes (new the actual gap this spec closes):**

| # | dir | op | packet | trigger |
|---|---|---|---|---|
| 1 | C2S | 0x081 | NoteSend | sender fills the note-compose screen and confirms |
| 2 | S2C | 0x081 | MessengerResult | sender gets a `MSG_NOTE_SEND`/error confirmation (existing) |
| 3 | S2C | 0x082 | NoteAppend | if recipient is online now, server pushes the new note live (new) |
| 4 | S2C | 0x083 | NoteAppendSorted | on recipient's next login/lobby-enter, server pushes their full note backlog, sorted (new) |
| 5 | C2S | 0x084 | NoteMarkRead | recipient opens a note |
| 6 | S2C | 0x084 | NoteMarkReadAck | existing shared echo handler, unchanged |
| 7 | C2S | 0x085 | NoteDelete | recipient deletes a note |
| 8 | S2C | 0x085 | NoteDeleteAck | existing shared echo handler, unchanged |

### Open questions

1. **C2S 0x029 exact meaning.** Confirmed it is the second button of the same popup id 7
   text-entry dialog as the working C2S 0x025, and confirmed the server currently drops it
   silently with no functional harm (no client-side handler waits on an 0x29-keyed reply
   either). What action the second button represents (confirm vs cancel, or a different
   target entirely such as block/invite) is not resolvable from static analysis of the
   shared text-edit-buffer write sites; needs a live client capture with the popup open to
   correlate which UI screen is active when 0x25 vs 0x29 fires.
2. **S2C 0x06E's intended feature.** The mechanics (empty trigger in, cached-room echo out)
   are fully proven, but no current server flow needs to force a player back into a room  
   accept already works via C2S 0x2F. Before wiring a sender, decide what UI/game event
   should cause a forced rejoin (room owner recall? reconnect-resume?) since sending it
   without a very recent 0x06C misjoins the client with stale globals.
3. **Notes sort_key_1/sort_key_2 exact source columns.** Very likely `created_at` split
   into a date string and a time string (matching the two-line column-2 display and the
   0x83 ascending bubble sort), but not proven against the renderer's exact date-formatting
   code; when implementing the row-to-blob packer, decompile the note-list draw routine
   (near `FUN_00451b70`/`FUN_00451a20`) once to confirm the exact string format expected in
   those two fields before shipping, rather than guessing a format.
4. **Where to trigger the 0x83 note-backlog push.** This spec assumes it belongs next to
   the existing `friendList`/`friendRequestList`/`blockList` push in the login/lobby burst;
   confirm that assumption against whichever function currently sends those three (not
   included in this batch) before adding the call, in case notes are meant to load lazily
   when the messenger's notes tab is actually opened instead.
