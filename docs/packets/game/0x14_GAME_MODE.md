# CMD 0x14 (20) - Game Mode Info

**Direction:** Server → Client  
**Handler:** `sub_479CC0`

## Structure

```c
struct GameModeInfo {
    int32 gameMode;
    int32 unknown1;
    int32 mapId;
    int32 unknown2;
    int32 maxPlayers;
    int32 currentPlayers;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | gameMode |
| 0x04 | 4 | int32 | unknown1 |
| 0x08 | 4 | int32 | mapId |
| 0x0C | 4 | int32 | unknown2 |
| 0x10 | 4 | int32 | maxPlayers |
| 0x14 | 4 | int32 | currentPlayers |

## Game Modes

| Value | Mode | Description |
|-------|------|-------------|
| 3 | RACE | Normal race |
| 8 | BATTLE | Battle mode |

## Behavior

Only processed if `gameMode == 3 || gameMode == 8`

Otherwise packet is ignored.

## Notes

- Sets client to state 11 (ROOM_READY)
- Prepares for race/battle start
- Contains room configuration for game

