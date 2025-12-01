# 0x57 RACE_STATUS (Server → Client)

**Handler:** `sub_47AB40`

## Purpose

Updates race status for a player.

## Payload Structure

```c
struct RaceStatus {
    int32 playerId;
    int32 status;
    int32 param;
};
```

## Size

**12 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Player ID |
| 0x04 | int32 | 4 | Status value |
| 0x08 | int32 | 4 | Additional parameter |

## Status Values

| Value | Description |
|-------|-------------|
| 10 | Race state 10 |
| 16 | Race state 16 |

## Notes

- Different handlers based on status value
- Updates race-specific player data

