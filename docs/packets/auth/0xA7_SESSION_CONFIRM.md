# CMD 0xA7 (167) - Session Confirm

**Direction:** Server → Client  
**Handler:** `sub_47B0B0`

## Structure

```c
struct SessionConfirm {
    int32 status;         // 0 = OK, 1 = error
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | status |

## Status Values

| Value | Meaning |
|-------|---------|
| 0 | Session valid |
| 1 | Session invalid |
| 2 | Session expired |
| 3 | Kicked |

## Raw Packet

```
A7 04 00 00 00 00 00 00
00 00 00 00              // status = OK
```

## Behavior

1. Client checks status
2. If != 0: disconnects and shows error
3. If == 0: continues normally

## Notes

- Response to heartbeat or session check
- Used to keep session alive
- Server sends this periodically

