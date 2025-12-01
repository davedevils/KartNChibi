# CMD 0x21 (33) - Room Full

**Direction:** Server → Client  
**Handler:** `sub_4798E0`

## Structure

```c
struct RoomFull {
    int32 roomId;
    int32 maxPlayers;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | roomId |
| 0x04 | 4 | int32 | maxPlayers |

## Raw Packet

```
21 08 00 00 00 00 00 00
01 00 00 00              // roomId = 1
08 00 00 00              // maxPlayers = 8
```

## Behavior

1. Client displays "Room is full" message
2. Player cannot join
3. Returns to room list

## Associated Messages

- `MSG_MAX_ROOM_USER_8`
- `MSG_MAX_ROOM_USER_16`

