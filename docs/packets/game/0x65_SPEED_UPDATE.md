# 0x65 SPEED_UPDATE (Server → Client)

**Handler:** `sub_47AD60`

## Purpose

Updates player speed value.

## Payload Structure

```c
struct SpeedUpdate {
    int32 playerId;
    float speed;
};
```

## Size

**8 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Player ID |
| 0x04 | float | 4 | Speed value |

## Notes

- Uses player index lookup
- Only applies if player matches and state == 3
- Calls `sub_4ADB90` with speed value

