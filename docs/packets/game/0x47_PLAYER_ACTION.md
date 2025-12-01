# 0x47 PLAYER_ACTION (Server → Client)

**Handler:** `sub_47A110`

## Purpose

Notifies about a player action with multiple parameters.

## Payload Structure

```c
struct PlayerAction {
    int32 playerId;
    int32 actionType;
    int32 param1;
    int32 param2;
    int32 param3;
    int32 param4;
};
```

## Size

**24 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Player ID |
| 0x04 | int32 | 4 | Action type |
| 0x08 | int32 | 4 | Parameter 1 |
| 0x0C | int32 | 4 | Parameter 2 |
| 0x10 | int32 | 4 | Parameter 3 |
| 0x14 | int32 | 4 | Parameter 4 |

## Action Types

| Type | Behavior |
|------|----------|
| 2+ | Various actions (switch statement) |

## Notes

- Reads 5 int32 values
- Action type determines behavior via switch

