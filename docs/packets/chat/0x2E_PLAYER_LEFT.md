# CMD 0x2E (46) - Player Left Chat

**Direction:** Server → Client  
**Handler:** `sub_479710`

## Structure

```c
struct PlayerLeftChat {
    int32 playerId;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | playerId |

## Behavior

1. Removes player from chat list
2. If player was selected for whisper, deselects them
3. Updates chat UI

## Raw Packet

```
2E 04 00 00 00 00 00 00
05 00 00 00              // playerId = 5
```

