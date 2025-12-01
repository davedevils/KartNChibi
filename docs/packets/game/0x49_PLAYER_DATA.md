# 0x49 PLAYER_DATA (Server → Client)

**Handler:** `sub_47AAD0`

## Purpose

Updates player data in race context.

## Payload Structure

```c
struct PlayerData {
    int32 playerId;
    int32 value;
    int32 dataType;
};
```

## Size

**12 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Player ID |
| 0x04 | int32 | 4 | Data value |
| 0x08 | int32 | 4 | Data type |

## Notes

- Uses player index lookup
- Updates player-specific data in race

