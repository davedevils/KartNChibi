# Mission system

Solo objective runs (collect / reach-finish / time-limited) picked from a mission menu off the
lobby. A mission plays out on its own track under `World/Mission/<name>/`, separate from
multiplayer race rooms. Reference: KnC.exe.raw, image base 0x400000.

## Packets

| Dir | Op | Name | Layout | Status |
|-----|----|------|--------|--------|
| C2S | 0x08C | C_MISSION_COMPLETE | i32 missionId | implemented |
| C2S | 0x08E | C_MISSION_MENU (screen refresh) | empty | implemented |
| C2S | 0x08F | C_MISSION_MENU_OPEN | empty | implemented |
| C2S | 0x090 | C_MISSION_START | i32 missionId | implemented |
| C2S | 0x121 | C_WAYPOINT_REACHED | i32 waypointIndex | implemented |
| S2C | 0x087 | S_EXT_87 (mission def) | 188B fixed record | implemented |
| S2C | 0x088 | S_EXT_88 (progress list) | i32 count + rows(8B) | implemented |
| S2C | 0x08A | mission unlocked | u32 missionId, u32 cleared | implemented |
| S2C | 0x08C | S_EQUIP_ITEM / mission complete (collision) | see note below | implemented, buggy |
| S2C | 0x08F | mission menu ack | empty | implemented |
| S2C | 0x090 | mission start ack | u32 missionId, u32 goldAfter | implemented |
| S2C | 0x120 | S_ENTITY_DATA_288, MissionCheckpointPath | i32 count + count x 12B xyz | **missing, this spec** |
| S2C | 0x122 | S_ENTITY_DATA_290, MissionGo | empty | **missing, this spec** |

Existing-row note (not part of this spec's scope, flagged by the mission group's own data):
S2C 0x8C is double-booked. `Protocol.h:184 CMD::S_EQUIP_ITEM=0x8C` is the real, verified binding
for client handler `FUN_0047b9e0`. Separately `server/game/src/packets/gen/MissionPackets.cpp:17`
privately defines its own `OP_S_MISSION_COMPLETE=0x8C`, which collides with it; every mission
completion (`MissionHandler.cpp:495`) currently lands in the client's equip/unequip branch and
corrupts an equip-loadout slot instead of showing a mission-complete screen. Left alone here
because it is outside the two `_todo` opcodes, but it blocks mission completion from ever
rendering correctly and should be fixed before this system is called done.

## S2C 0x120 MissionCheckpointPath

**Trigger.** Client handler `FUN_0047ea70` (0x47EA70) is reached only through the S2C dispatch
table at 0x4777C0. No other code calls it. Decompile:

```
void __fastcall FUN_0047ea70(int param_1)
{
  piVar1 = (int *)(&DAT_008cceac + param_1);
  *(undefined4 *)(&DAT_008ccea8 + param_1) = 0;
  FUN_0044e910(piVar1,4);              // i32 count
  if (0 < *piVar1) {
    puVar2 = &DAT_008cceb0 + param_1;
    do {
      FUN_0044e910(puVar2,0xc);        // 12 raw bytes per entry
      iVar3 = iVar3 + 1;
      puVar2 = puVar2 + 0xc;
    } while (iVar3 < *piVar1);
  }
  FUN_004b5560();                       // generic clamp helper, stores param2 into +0x440, capped at 11
  return;
}
```

`param_1` is the per-connection slot offset used throughout this whole family of mission handlers
(same base pattern as 0x87/0x88/0x8A/0x8F/0x90), so `DAT_008cceb0` is a per-slot fixed array, not a
single global; other reads of that array happen at the same variable-offset addressing and did not
show up as direct xrefs. `get_xrefs_to(0x8cceac)` and `get_xrefs_to(0x8cceb0)` both return only this
function's own writes, confirming the array is populated here and consumed elsewhere through the
same offset base (mission checkpoint / minimap or path-guide rendering, not decompiled further).
There is no server writer anywhere in `server/`: grep for the opcode, for
`S_ENTITY_DATA_288`, and for any `PacketBuilder`/`MissionPackets` reference to 0x120 finds only the
`Protocol.h:381` constant declaration.

If this packet never arrives, the per-slot count field defaults to whatever was last in memory
(zeroed once per session at `DAT_00...ea8+param_1=0` inside this same handler, so first-ever call is
safe) and the checkpoint-path array stays empty; whatever downstream feature reads it (guide path
markers / minimap route) simply has nothing to draw. No hang or error message is caused by omission
alone.

**Layout.**

| Field | Type | Notes |
|-------|------|-------|
| count | i32 | number of path points that follow |
| points[count] | 12 bytes each | opaque to the client's read loop; plausibly float x,y,z, consistent with every other track-geometry record in this codebase (`TrackVec3`, `SpawnPackets::ColCheckpoints::points`) |

No trailing field after the loop (the registry's `wire_layout` string implies one extra i32; the
fresh decompile shows none treat the registry note as spurious for this opcode).

**Consumer.** Feeds the per-slot checkpoint/path array the mission screen or in-mission HUD draws
its route guide or checkpoint markers from.

**Data source.** `mission_def.world_name` (e.g. `Mission_01`) is the track folder under
`World/Mission/<world_name>/`, the same convention normal race tracks use under
`World/<themeFolder>/<trackFolder>/track.COL`
(`server/game/src/packets/gen/SpawnPackets.cpp:534 trackBase`, called with `themeFolder="Mission"`).
`SpawnPackets::loadColCheckpoints` / `loadTrackCheckpoints` already parse `track.COL` into
`ColCheckpoints{ checkpointCount, points (TrackVec3, ordered along the track) }` for the normal race
lap tracker (`server/game/include/packets/gen/SpawnPackets.h:188`). That `points` vector is exactly
the xyz stream this opcode wants.

**Implementation.**
1. Add `MissionPackets::missionCheckpointPath(const std::vector<SpawnPackets::TrackVec3>& points)`
   in `server/game/src/packets/gen/MissionPackets.cpp`/`.h`: `BeginPacket(0x120)`, write
   `i32 count`, then `count` x 3 floats (12 bytes) each, matching the client's raw-byte read.
2. In `MissionHandler::handleStartMission` (`server/game/src/handlers/MissionHandler.cpp:399`),
   after looking up `mission_def` and before/around the existing `missionStartAck` send, call
   `SpawnPackets::loadTrackCheckpoints("Mission", worldName, col)` (need `worldName` selected in
   the existing `SELECT ... FROM mission_def` query, alongside `time_limit_ms`) and send
   `MissionPackets::missionCheckpointPath(col.points)`.
3. Reuse `SpawnPackets::TrackVec3` as the point type; no new struct required.

## S2C 0x122 MissionGo

**Trigger.** Client handler `FUN_0047eb70` (0x47EB70), reached only via the 0x4777C0 dispatch
table. Full decompile:

```
void FUN_0047eb70(void)
{
  FUN_00472770();                                  // closes the mission briefing popup
  FUN_004a05c0(DAT_01b19740,1);                    // snaps the local car onto the track spline
  FUN_0043ed70(0xc,0xffffffff,0x3f800000);         // camera signal: preset/code 0xc, arg -1, arg 1.0f
  return;
}
```

Zero calls to any of the wire-read primitives (`FUN_0044e910`/`eb30`/`eb60`), confirming the
payload is empty any bytes sent here would be ignored. This is the release-the-car signal: it
closes whatever popup is still open from the start-mission stage (opened by S2C 0x90's
`FUN_004538b0()+FUN_00404410(0x19)`), puts the car on the track's start spline, and switches to
camera preset 0xC (the in-race chase camera code used elsewhere in the client, same call pattern
as `FUN_0043ed70` used at race starts). Without it the client sits in the mission-start/briefing UI
indefinitely with the car never released onto the track. No server writer exists anywhere in
`server/`; grep for the opcode, `S_ENTITY_DATA_290`, and `MissionGo` finds only the
`Protocol.h:382` constant.

**Layout.** Empty payload, both directions of evidence agree (client makes no reads; nothing in
`server/` currently builds it).

**Consumer.** Releases the player's car onto the mission track and switches to the racing camera;
the go/start signal for a mission run, mirroring what a race-start countdown does for a normal
room race.

**Data source.** None, no fields.

**Implementation.**
1. Add `MissionPackets::missionGo()` returning an empty `Packet(CMD::S_ENTITY_DATA_290)`
   (same pattern as `missionMenuAck()` at `MissionPackets.cpp:258`).
2. Send it from `MissionHandler::handleStartMission`, right after the 0x120 checkpoint-path send
   (see open question below on ordering relative to 0x90).
3. Add the `CMD::S_ENTITY_DATA_290` (0x122) name to `MissionPackets.h`'s opcode block alongside the
   other local `OP_S_MISSION_*` constants, or reference `Protocol.h`'s existing constant directly  
   either is fine, follow whichever convention the file already uses for the opcode in question
   (the file currently redefines locally; match that).

## Design

**What it is for the player.** Pick a mission from the mission menu, see its objective/timer,
press Start, get dropped onto a solo instanced track with a route to follow, complete the
objective (collect N things, or reach the finish) within the time limit, get gold/exp and an
unlock notification back in the menu.

**Tables.**

| Table | Columns used here | Role |
|-------|--------------------|------|
| `mission_def` | `mission_id, mission_kind, goal_count, time_limit_ms, reward_currency_a/b, reward_item_type/key, world_name, str_key_*` | static mission catalog, source for S2C 0x87 and now for 0x120's track lookup |
| `char_mission_state` | `char_id, mission_id, started_at_ms, deadline_ms, counter` | one active mission per character, written by `handleStartMission`, cleared on complete |
| `char_mission_progress` | `char_id, mission_id, cleared` | per-character unlock/clear record, source for S2C 0x88 and 0x8A |

No new table needed for 0x120/0x122: point data comes from the existing per-track `track.COL`
file via `SpawnPackets::loadTrackCheckpoints`, not from a DB table.

**Handlers.**

| Handler | File | Role |
|---------|------|------|
| `MissionHandler::handleOpenMissionMenu` | `MissionHandler.cpp:373` | sends 0x87 x N defs, 0x88 progress, 0x8F ack |
| `MissionHandler::handleStartMission` | `MissionHandler.cpp:399` | writes `char_mission_state`, sends 0x90 ack; **extend to send 0x120 then 0x122** |
| `MissionHandler::handleMissionComplete` | `MissionHandler.cpp:446` | pays reward, writes progress, sends (broken) 0x8C and 0x8A |

**Flows.**

Mission menu open:

| # | Dir | Op | Packet | Trigger |
|---|-----|----|--------|---------|
| 1 | C2S | 0x8F | open mission menu | player presses Mission button (lobby stage 4) |
| 2 | S2C | 0x87 | mission def x N | server replies with the catalog |
| 3 | S2C | 0x88 | progress list | server replies with per-character clear state |
| 4 | S2C | 0x8F | menu ack | opens UI stage 24, drops MSG_WAIT |

Mission start (current + this spec's addition):

| # | Dir | Op | Packet | Trigger |
|---|-----|----|--------|---------|
| 1 | C2S | 0x90 | start mission, missionId | player presses Start on a mission row |
| 2 | S2C | 0x90 | start ack, missionId + goldAfter | server validates `mission_def`, writes `char_mission_state`; opens UI stage 25 |
| 3 | S2C | 0x120 | checkpoint path, count + xyz points | **new**: server loads `track.COL` for `world_name`, sends the route |
| 4 | S2C | 0x122 | mission go, empty | **new**: closes the briefing popup, snaps car to spline, switches to race camera |
| 5 | C2S | 0x121 | waypoint reached, index (repeated) | client crosses each point while racing, no reply |
| 6 | C2S | 0x8C | mission complete, missionId | client's FINISH state machine branch reports completion |
| 7 | S2C | 0x8C / 0x8A | reward + unlock (currently broken, see collision note) | server pays and records the clear |

**Open questions.**

1. Exact send point of 0x120/0x122 relative to 0x90: no client-side or wire evidence pins whether
   the client expects a further C2S ack (map-loaded, asset-ready) between the start ack and the
   checkpoint path/go signal no such C2S opcode was found between 0x90 and 0x121 in `Protocol.h`.
   This spec assumes a server-initiated burst (0x90, 0x120, 0x122) sent back-to-back from
   `handleStartMission`, matching the pattern already used for the 0x87/0x88/0x8F menu burst. If
   this stalls in practice (client still loading the track scene when 0x122 arrives and releases
   the car), a short delay or a currently-unidentified ready signal may be needed.
2. Whether `DAT_008cceb0`'s consumer is the mission HUD, a minimap, or just a rally-style
   follow-path renderer was not traced past the write side (`get_xrefs_to` on the array only
   returns this handler's own writes; the reader uses the same per-slot offset addressing and
   was not separately located).
3. `FUN_004b5560(param1, param2)` at the end of the 0x120 handler is a generic two-arg
   clamp-into-`+0x440` helper; the actual arguments passed at this call site were not visible in
   the decompiled call (`FUN_004b5560();`), so its exact effect here (some kind of counter/lap
   cap) is not fully pinned, though it does not affect the wire layout.
4. The S2C 0x8C opcode collision (mission complete vs S_EQUIP_ITEM) is a real bug affecting step 7
   of the flow above but is outside this spec's two `_todo` opcodes; flagged here so it is not
   mistaken for solved.
