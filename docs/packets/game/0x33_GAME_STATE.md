# CMD 0x33 (51) - Game State

**Direction:** Server → Client  
**Handler:** `sub_4799B0`

## Structure

```c
struct GameState {
    int32 playerId;
    int32 state;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | playerId |
| 0x04 | 4 | int32 | state |

## State Values

| Value | Meaning |
|-------|---------|
| 2 | Special state (sets global flag) |
| Other | Normal state update |

## Behavior

1. If state == 2:
   - Sets global flag `dword_BCE228 = 1`
   - Triggers special handling
2. Otherwise:
   - Normal state update for player

## Raw Packet

```
33 08 00 00 00 00 00 00
05 00 00 00              // playerId = 5
02 00 00 00              // state = 2
```

