# 0x64 ROOM_STATUS (Server → Client)

**Handler:** `sub_47ACD0`

## Purpose

Updates room status counters.

## Payload Structure

```c
struct RoomStatus {
    int32 roomId;
    int32 status;
};
```

## Size

**8 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Room ID |
| 0x04 | int32 | 4 | Status value |

## Status Values

| Value | Effect |
|-------|--------|
| 0 | Decrement counter |
| 1 | Increment counter |

## Notes

- Used for room player count tracking
- Updates global counters

