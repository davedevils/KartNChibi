# 0x44 GAME_UPDATE (Server → Client)

**Handler:** `sub_47A0E0`

## Purpose

Updates game state with a single value.

## Payload Structure

```c
struct GameUpdate {
    int32 value;
};
```

## Size

**4 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Update value |

## Notes

- Calls `sub_4B0CF0` with value
- Used for incremental game updates

