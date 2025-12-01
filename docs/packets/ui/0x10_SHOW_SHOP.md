# CMD 0x10 (16) - Show Shop

**Direction:** Server → Client  
**Handler:** `sub_4795C0`

## Structure

```c
struct ShowShop {
    int32 shopCategory;   // Which shop section
    int32 playerCoins;
    int32 playerCash;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | shopCategory |
| 0x04 | 4 | int32 | playerCoins |
| 0x08 | 4 | int32 | playerCash |

## Shop Categories

| Value | Category |
|-------|----------|
| 0 | All items |
| 1 | Vehicles |
| 2 | Parts |
| 3 | Accessories |
| 4 | Special |

## Raw Packet

```
10 0C 00 00 00 00 00 00
00 00 00 00              // shopCategory = All
E8 03 00 00              // playerCoins = 1000
64 00 00 00              // playerCash = 100
```

## Notes

- Client state changes to SHOP (7)
- Shop item list sent separately
