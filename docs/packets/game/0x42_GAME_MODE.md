# 0x42 GAME_MODE (Server → Client)

**Handler:** `sub_47A0A0`

## Purpose

Sets the current game mode.

## Payload Structure

```c
struct GameMode {
    int32 mode;
};
```

## Size

**4 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Game mode value |

## Mode Values

| Value | Description |
|-------|-------------|
| 5 | Special mode (calls `sub_4B6220`) |
| Other | Standard mode (calls `sub_4B23C0`) |

## Notes

- Single int32 packet
- Affects game behavior/rules

