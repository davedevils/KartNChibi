# One real chibikart race, decoded end to end

Captured through the MITM on 10:40:35.095, from the launch packet to the finish packet.
8802 frames over 10:40:35.095 to 10:43:30.927. Layouts from [WIRE_FORMAT.md](WIRE_FORMAT.md).

## Phase order

```
S2C 0x14  race launch, mode then track then players
S2C 0x3A  RACE GO, this is the real go, NOT 0x34
C2S 0x40  motion, 19 byte packed, 10 Hz
C2S 0x67  per frame tick, by far the loudest packet in a race
C2S 0x41  checkpoint crossing, prev then next
S2C 0x45  item usage broadcast
C2S 0x39  finish
S2C 0x3C  race end for one player
```

## Every opcode in the race window

| dir | op | name | count | ours | first body decoded |
|-----|----|------|-------|------|--------------------|
| C2S | 0x67 | ? | 5625 | **NO** | 0 |
| C2S | 0x40 | S_GAME_STATE_40 | 1590 | yes | 244, 38469634, 13152, 3701961414844544000, x2=? |
| S2C | 0x45 | S_ITEM_USAGE | 668 | **NO** | 7, x3=? |
| S2C | 0x40 | S_GAME_STATE_40 | 509 | yes | 1, 37, 1000, 4884153797019855093, x2=? |
| C2S | 0xA6 | ? | 173 | yes |  |
| S2C | 0x0C | S_DATA_PAIRS | 85 | **NO** | 2, x3=? |
| C2S | 0x41 | ? | 42 | yes | 0, x2=? |
| C2S | 0xCF | S_FRIEND_STATS_UPDATE | 26 | **NO** | 6, x4=? |
| C2S | 0x58 | S_PLAYER_STATUS | 25 | **NO** |  |
| C2S | 0x49 | S_PLAYER_DATA | 13 | **NO** | 6, x3=? |
| C2S | 0x47 | S_PLAYER_ACTION | 10 | **NO** | 2, x6=? |
| S2C | 0x47 | S_PLAYER_ACTION | 10 | **NO** | 7, x6=? |
| S2C | 0x33 | S_GAME_STATE | 4 | yes | 37, x2=? |
| S2C | 0x44 | S_GAME_UPDATE | 3 | **NO** | 0 |
| S2C | 0x3E | S_PLAYER_JOIN | 2 | **NO** | 7, wstr=? |
| S2C | 0x0D | S_FLAG_SET | 2 | **NO** | (empty)=? |
| C2S | 0x0D | S_FLAG_SET | 2 | **NO** | (empty)=? |
| C2S | 0xB4 | S_SYSTEM_MESSAGE | 2 | **NO** | 32, wstr=? |
| C2S | 0x5C | S_ROOM_DATA_5C | 2 | **NO** | 7, x3=? |
| S2C | 0x42 | S_GAME_MODE | 2 | **NO** | 7 |
| C2S | 0xA1 | S_MISSION_COMPLETE | 2 | **NO** | 3 |
| S2C | 0x14 | S_GAME_MODE_14 | 1 | **NO** | 3, x6=? |
| S2C | 0x3A | S_RESULTS | 1 | **NO** | 0 |
| S2C | 0xB4 | S_SYSTEM_MESSAGE | 1 | **NO** | 7, wstr=? |
| S2C | 0x3D | S_ROOM_STATE_3D | 1 | **NO** | 7, x2=? |
| S2C | 0x3C | S_RACE_END | 1 | **NO** | 7, x2=? |

## The finish

```
S2C 0x3C  07 00 00 00 96 00 00 00 00 90 01 00 00 00 00 00 00
fields    playerId=7  a=150  u8=0  b=400  c=0
```
