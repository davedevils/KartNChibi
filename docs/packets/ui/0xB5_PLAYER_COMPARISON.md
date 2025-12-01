# 0xB5 PLAYER_COMPARISON (Server → Client)

**Handler:** `sub_47CF80`

## Purpose

Sends two complete PlayerInfo structures for comparison screen.

## Payload Structure

```c
struct PlayerComparison {
    PlayerInfo player1;      // 0x4C8 (1224 bytes)
    PlayerInfo player2;      // 0x4C8 (1224 bytes)
    wchar_t    unknown[];    // Additional wstring
};
```

## Size

**2456+ bytes** (one of the largest packets!)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x000 | PlayerInfo | 1224 | First player data |
| 0x4C8 | PlayerInfo | 1224 | Second player data |
| 0x990 | wstring | var | Additional data |

## PlayerInfo Structure

See [STRUCTURES.md](../STRUCTURES.md) - 0x4C8 bytes containing:
- Player ID, name, level
- Stats, rankings
- Equipment, vehicles
- Achievements

## Notes

- Used for player vs player comparison UI
- Both players' full data sent
- Triggers comparison screen display

