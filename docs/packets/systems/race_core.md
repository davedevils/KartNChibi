# Race core, five packets

Group race_core. Client binary `KnC.exe.raw`, image base 0x400000. The five packets that were
missing from the race lifecycle: C2S `0x003B`, S2C `0x0031`, `0x00F0`, `0x011F` and `0x0131`.
The rest of the group (`0x0D`, `0x2D`, `0x2F`, `0x40`, `0x64`, `0x65`, `0x68`, `0x6A`,
`0x118`, `0x125`, `0x3A`, `0x3E`, `0x3F`, `0x44`, `0xA5`) is routed and is context only. The
whole race flow is in `docs/systems/RACE_COMBAT.md`.

| Opcode | Name | Our server |
|--------|------|------------|
| C2S `0x003B` | LeaveRace | routed, race bookkeeping then the room is left |
| S2C `0x0031` | RoomListRowPairUpdate | not sent, a lobby list field update that nothing needs |
| S2C `0x00F0` | racer despawn | sent to the others when a racer leaves mid race |
| S2C `0x011F` | RaceTimerArm | builder only, never sent, the value encoding is unknown |
| S2C `0x0131` | ItemRollStream | sent once per human racer with the grid |

## C2S 0x003B LeaveRace

Empty body, sent by `FUN_004810C0`. Two callers: the window message dispatcher
`FUN_00403420` case 0x7E9, and the command dispatcher `FUN_00401A10` case 0x1B. The second
leaves at once in the waiting room or the result screen (`DAT_01a20b21` 2 or 6) and asks
`MSG_GAME_EXIT` first while racing. The client drives its own screen change and waits for
nothing.

Our server: case `0x3B` in `GameServerDispatch.cpp` runs `RaceHandler::handlePlayerLeave`
(roster removal, S2C `0xF0` to the others, the same wallet dock the client applies to
itself, the end of the race when everybody left has finished), then `leaveCurrentRoom`. Mid
race it also sends S2C `0x12` and the room list, `FUN_004810C0` only writes and waits so the
lobby answer is what moves the client on.

## S2C 0x0031 RoomListRowPairUpdate

Handler `sub_479950` reads `u32 id, i32 val1, i32 val2` (12 bytes), stores val1 and val2 in
`DAT_00bce208` and `DAT_00bce20c` and calls `FUN_00407600`, which finds the lobby room list
row with that id (table at +0x6824, stride 0x88, up to 0x200 rows) and writes val1 and val2
at +0x687C and +0x6880. It is a two field update of a room already listed, not a position.
chibikart sends it 8 times in a capture, `05 00 00 00 03 00 00 00 10 00 00 00`.

Our server: not sent. The room list is rebuilt with `0x2E` and `0x2D` on every change.
`PacketBuilder::position` and `position32` exist with no caller. C2S `0x31` has no client
sender and no route.

## S2C 0x00F0 racer despawn

Handler `sub_47D930` reads one i32, resolves it with `FUN_0048DEA0` to a race slot and sets
the slot removal flag through `FUN_00498F40` (stride 0xA7260). The registry name
CarLapFlagSet is wrong, it marks a car as gone. Protocol.h also names the opcode
`S_FRIEND_REMOVE`, the handler is the same.

```
+0x00  i32  racer id
size   4
```

Our server: `RaceHandler::handlePlayerLeave` sends `PacketBuilder::entityRemove(playerId)` to
every other racer when the room is racing. The id is the character id, the same id every
other race frame and the `0x3E` grid spawn use, which is what `FUN_0048DEA0` looks up.

## S2C 0x011F RaceTimerArm

Handler `FUN_0047EA10` reads one i32 and calls `FUN_004B1190` on the timer at 0x2EB89A0: the
arm time goes to +0xEE8, the raw value to +0xF18, value minus `DAT_01adf3e8` to +0xF1C, and
the state to 2. It then pulses a HUD element and starts an animation. `DAT_01adf3e8` is read
at three sites and never written by them.

Our server: `RacePackets::raceTimerArm` builds it, nothing sends it. Without a capture the
value (tick count, duration or something else) cannot be chosen, and a wrong value draws a
wrong clock. The race runs without it.

## S2C 0x0131 ItemRollStream

Handler `FUN_00478E80` resets the ring cursor `_DAT_005f1934` and reads 100 raw u32 into the
buffer at `DAT_008ccf40`. No count prefix. The client takes one value per item box pickup.

```
+0x00  u32[100]
size   400
```

Our server: `RaceHandler::sendGrid` sends `RacePackets::itemRollStream` to each human racer
right after the `0x3E` grid, 100 values from `std::mt19937`. The client still rolls its item
with its own weights and reports it on C2S `0x49`, the server mirrors it.

## Open questions

- The encoding of the `0x011F` value.
- Whether the stock server kept the `0x0131` values to check the `0x49` reports. Nothing on
  the wire shows a check.
