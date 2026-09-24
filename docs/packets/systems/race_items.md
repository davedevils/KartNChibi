# Race items, five packets

Group race_items. Client binary `KnC.exe.raw`, image base 0x400000. Five opcodes that were
flagged as missing: C2S `0x00F2`, S2C `0x0069`, `0x00C9`, `0x00CD` and `0x00EE`. The rest of
the item wire (`0x47`, `0x49`, `0x4B`, `0x57`, `0x5C`, `0x5F`, `0xCF`) is routed and is context
only. The item flow is in `docs/systems/RACE_COMBAT.md`.

| Opcode | Name | Our server |
|--------|------|------------|
| C2S `0x00F2` | ability class | routed to `ItemHandler::handleAbilityClass` |
| S2C `0x0069` | hit broadcast | sent for every hit report the receivers handle |
| S2C `0x00C9` | sound cue | sent on every item use and by the CPU cars |
| S2C `0x00CD` | shield token | sent when a shield pops |
| S2C `0x00EE` | kart period update | not sent, the kart list and the repair ack carry the durability |

## C2S 0x00F2 ability class

Sender `FUN_004833A0`, one raw u32, no answer awaited. `sub_4B82A0` only calls it for classes
8, 9, 10 and 11. Classes 10 and 11 come from `sub_4CACE0` when a real shield absorbs a hit.

Our server: case `C_ENTITY_REQ_242` routes to `ItemHandler::handleAbilityClass`. A class
outside 8 to 11 is logged and dropped. For 10 and 11 the server takes the next value of a
process wide counter as the token and sends S2C `0x00CD` to the other racers.

## S2C 0x0069 hit broadcast

Handler `FUN_0047AF00`:

```
+0x00  u32  racer id, resolved with FUN_0048DEA0, dropped when unknown
+0x04  i16  effect code, 100 200 300 go to FUN_00495C30, 700 to FUN_004C3A60, 1000 to FUN_004C4A10
+0x06  u8   read, unused
size   7
```

Our server: `RaceHandler::handleHitReport` takes the C2S `0x69` of the victim, filters it with
`ItemPackets::hitShouldRebroadcast` (the 1100 flash is victim local and 200 is never sent) and
sends `ItemPackets::hitBroadcast` to the whole room. The sender `FUN_00481B60` skips
`sub_495C30` for itself and only writes the frame, so it needs the echo too.

## S2C 0x00C9 sound cue

Handler `FUN_0047CD20` reads one u32, resolves it with `FUN_0048DEA0` and plays the cached cue
of that car (`FUN_0049A730`, handle at +0x2110 of the car, positional off stage 9). Protocol.h
names the opcode `S_FRIEND_STATE_UPDATE`, it is the sound cue channel.

```
+0x00  u32  racer id
size   4
```

Our server: `RaceHandler::handleItemSpawn` sends `ItemPackets::playSoundCue` to the other
racers on every C2S `0x47`, and the CPU cars send it when they fire.

## S2C 0x00CD shield token

Handler `FUN_0047D1C0`:

```
+0x00  u32  racer id, resolved with FUN_0048DEA0
+0x04  u32  token, stored in DAT_005f1934 even when the id is unknown
size   8
```

With a known racer it checks `FUN_004CB160` and plays the shield visual `FUN_004CACE0`. The
token is what the client echoes on C2S `0x00CD` (`FUN_00483180`) the next time its ability
blocks a hit.

Our server: sent by `ItemHandler::handleAbilityClass` for classes 10 and 11 as above.
`ItemHandler::handleAbilityFire` receives the C2S `0x00CD` echo in a race and logs it,
outside a race it is dropped. The echo is not matched against the token.

## S2C 0x00EE kart period update

Handler `FUN_0047D880` reads a u32 id, resolves it, then 16 bytes. It writes the four dwords
into the locally selected kart record (`FUN_0044F450(DAT_01a20b1c)`) at +0x28, +0x2C, +0x30
and +0x34. +0x2C and +0x30 are the period mode and value of the owned kart record
(`InventoryPackets::KartRow`), the durability when the mode is 3. +0x28 and +0x34 are not
identified.

```
+0x00  u32  racer id
+0x04  u32  unknown, record +0x28
+0x08  u32  period mode, record +0x2C
+0x0C  u32  period value, record +0x30
+0x10  u32  unknown, record +0x34
size   20
```

Our server: not sent. After a race the wear is written to `owned_kart.period_value` and the
full `0x1C` kart list goes out again, a repair scroll answers with
`InventoryPackets::useItemAckWithKartPeriod`, which carries the new period too.
`PacketBuilder::entityPosition` has the 20 byte shape and no caller.

## Open questions

1. The fields at kart record +0x28 and +0x34 behind `0x00EE`.
2. The hazards behind ability classes 8 and 9, the client reads nothing back so it only
   matters for a server side count.
3. Whether the stock server checked the `0x00CD` echo against its token.
