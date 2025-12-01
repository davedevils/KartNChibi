# CMD 0x36 (54) - Lap / Checkpoint Info

**Direction:** Server → Client

## Structure

```c
struct LapInfo {
    int32 playerId;
    int32 lap;
    int32 checkpoint;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | playerId |
| 0x04 | 4 | int32 | lap |
| 0x08 | 4 | int32 | checkpoint |

## Raw Packet

```
36 0C 00 00 00 00 00 00
05 00 00 00              // playerId = 5
02 00 00 00              // lap = 2
08 00 00 00              // checkpoint = 8
```

## Notes

- Broadcast to all players in race
- Used for position display and minimap
- checkpoint may be progress percentage or waypoint index

