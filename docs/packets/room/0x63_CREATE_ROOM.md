# CMD 0x63 (99) - Create Room Response

**Direction:** Server → Client

## Structure

```c
struct CreateRoomResponse {
    int32 roomId;         // New room ID, or -1 on failure
};
```

## Raw Packet

```
04 00 63 00 00 00 00 00 00 00 00 00
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | roomId |

## Result Codes

| roomId | Meaning |
|--------|---------|
| > 0 | Success, room created |
| 0 | Error |
| -1 | Creation failed |

## Notes

- Response to client's create room request
- On success, client transitions to room screen
- Follow with 0x3F (Room Info) to populate room data

