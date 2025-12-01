# CMD 0x23 (35) - Room Player Update

**Direction:** Server → Client  
**Handler:** `sub_479BE0`

## Structure

```c
struct RoomPlayerUpdate {
    int32 playerId;
    int32 slotId;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | playerId |
| 0x04 | 4 | int32 | slotId |

## Behavior

Updates a player's slot position in the room grid.

## Raw Packet

```
23 08 00 00 00 00 00 00
05 00 00 00              // playerId = 5
02 00 00 00              // slotId = 2
```

