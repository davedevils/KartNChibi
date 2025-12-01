# 0x58 PLAYER_STATUS (Server → Client)

**Handler:** `sub_47ABE0`

## Purpose

Updates player status with flag.

## Payload Structure

```c
struct PlayerStatus {
    int32 playerId;
    int8  status;
};
```

## Size

**5 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Player ID |
| 0x04 | int8 | 1 | Status flag |

## Notes

- Compact status update
- Uses `sub_48B430` for lookup
- Updates `sub_48B460` with status

