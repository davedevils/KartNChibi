# CMD 0x35 (53) - Room Score Update

**Direction:** Server → Client  
**Handler:** `sub_479C20`

## Structure

```c
struct RoomScoreUpdate {
    int32 score1;
    int32 score2;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | score1 |
| 0x04 | 4 | int32 | score2 |

## Behavior

1. Calls `sub_4538B0()` - Refresh UI
2. Updates score display

## Raw Packet

```
35 08 00 00 00 00 00 00
0A 00 00 00              // score1 = 10
05 00 00 00              // score2 = 5
```

## Notes

- Used in team modes for team scores
- May also be used for individual score updates

