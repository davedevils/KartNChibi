# CMD 0x37 (55) - Item Use

**Direction:** Server → Client

## Structure

```c
struct ItemUse {
    int32 playerId;
    int32 itemId;
    int32 targetId;       // -1 if no target
    float posX;
    float posY;
    float posZ;
    float dirX;
    float dirY;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | playerId |
| 0x04 | 4 | int32 | itemId |
| 0x08 | 4 | int32 | targetId |
| 0x0C | 4 | float | posX |
| 0x10 | 4 | float | posY |
| 0x14 | 4 | float | posZ |
| 0x18 | 4 | float | dirX |
| 0x1C | 4 | float | dirY |

## Item Types (speculation)

| ID | Item |
|----|------|
| 1 | Missile |
| 2 | Bomb |
| 3 | Shield |
| 4 | Boost |
| 5 | Oil |
| 6 | Banana |

## Notes

- Position/direction used for projectile spawn
- Broadcast to all players in race
- Client handles visual effect

