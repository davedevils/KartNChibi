# CMD 0x12 (18) - Show Room

**Direction:** Server → Client  
**Handler:** `sub_479680`

## Structure

```c
struct ShowRoom {
    int32 roomId;
    int32 slot;           // Player's slot in room
    int32 team;           // 0 = none, 1 = red, 2 = blue
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | roomId |
| 0x04 | 4 | int32 | slot |
| 0x08 | 4 | int32 | team |

## Team Values

| Value | Team |
|-------|------|
| 0 | None (FFA) |
| 1 | Red |
| 2 | Blue |

## Raw Packet

```
12 0C 00 00 00 00 00 00
01 00 00 00              // roomId = 1
00 00 00 00              // slot = 0 (first slot)
01 00 00 00              // team = Red
```

## Behavior

1. Client transitions to room screen
2. Room player list displayed
3. Ready button shown

## Notes

- Client state changes to ROOM (9)
- Full room info (0x3F) follows

