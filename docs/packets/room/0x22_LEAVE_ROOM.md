# CMD 0x22 (34) - Leave Room

**Direction:** Server → Client  
**Handler:** `sub_479920`

## Structure

```c
struct LeaveRoom {
    int32 roomId;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | roomId |

## Behavior

1. Removes player from room data structure
2. Returns player to lobby
3. Clears room-related UI elements

## Raw Packet

```
22 04 00 00 00 00 00 00
01 00 00 00              // roomId = 1
```

