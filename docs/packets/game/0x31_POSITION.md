# CMD 0x31 (49) - Position Update

**Direction:** Server → Client  
**Handler:** `sub_479950`

## Structure

```c
struct PositionUpdate {
    int32 unknown1;       // Possibly sequence/tick
    int32 posX;
    int32 posY;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | unknown1 |
| 0x04 | 4 | int32 | posX |
| 0x08 | 4 | int32 | posY |

## Behavior

1. Stores posX and posY globally
2. Updates position display/logic

## Raw Packet

```
31 0C 00 00 00 00 00 00
00 00 00 00              // unknown1
E8 03 00 00              // posX = 1000
D0 07 00 00              // posY = 2000
```

## Notes

- Used during race for position sync
- Coordinates are likely world units
- May need interpolation on client

