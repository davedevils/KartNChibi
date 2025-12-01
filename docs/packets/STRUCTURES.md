# Data Structures Reference

Confirmed sizes from client 

## Core Structures

| Structure | Size | Hex | Used By |
|-----------|------|-----|---------|
| PlayerInfo | 1224 | 0x4C8 | CMD 0x07, various |
| VehicleData | 44 | 0x2C | CMD 0x1B, 0x3E, 0x78 |
| ItemData | 56 | 0x38 | CMD 0x1C, 0x3E |
| AccessoryData | 28 | 0x1C | CMD 0x1D, 0x1E |
| SmallItem | 32 | 0x20 | CMD 0x79, 0x7A |

## VehicleData (0x2C = 44 bytes)

```c
struct VehicleData {
    int32 vehicleId;      // +0x00
    int32 ownerId;        // +0x04
    int32 durability;     // +0x08
    int32 maxDurability;  // +0x0C
    int32 stats[7];       // +0x10 (28 bytes)
};
```

Handlers:
- `sub_478EC0` (CMD 0x1B) - reads count, then loops reading 0x2C each
- `sub_47B220` (CMD 0x78) - same pattern

## ItemData (0x38 = 56 bytes)

```c
struct ItemData {
    int32 itemId;         // +0x00
    int32 ownerId;        // +0x04
    int32 quantity;       // +0x08
    int32 unknown[11];    // +0x0C (44 bytes)
};
```

Handlers:
- `sub_478F20` (CMD 0x1C) - reads count, then loops reading 0x38 each

## AccessoryData (0x1C = 28 bytes)

```c
struct AccessoryData {
    int32 accessoryId;    // +0x00
    int32 uniqueId;       // +0x04
    int32 slot;           // +0x08
    int32 bonus1;         // +0x0C
    int32 bonus2;         // +0x10
    int32 bonus3;         // +0x14
    int32 equipped;       // +0x18
};
```

Handlers:
- `sub_478F80` (CMD 0x1D) - reads count, then loops reading 0x1C each
- `sub_478FE0` (CMD 0x1E) - reads single 0x1C

## SmallItem (0x20 = 32 bytes)

```c
struct SmallItem {
    int32 itemId;         // +0x00
    int32 uniqueId;       // +0x04
    int32 quantity;       // +0x08
    int32 unknown[5];     // +0x0C (20 bytes)
};
```

Handlers:
- `sub_47B290` (CMD 0x79)
- `sub_47B4F0` (CMD 0x7A)

## Chat Message

From `sub_479630` (CMD 0x2D):

```c
struct ChatMessage {
    int32 senderId;           // +0x00
    wchar_t message[42];      // +0x04 (84 bytes)
    int32 fields[7];          // +0x58 (28 bytes)
};
// Total: ~116 bytes
```

## Player Join (CMD 0x3E)

From `sub_479D60`:

```c
struct PlayerJoin {
    int32 unknown1;
    wchar_t playerName[35];   // 70 bytes
    int32 unknown2;
    int32 unknown3;
    VehicleData vehicle;      // 44 bytes
    ItemData equippedItem;    // 56 bytes
};
// Total: ~180 bytes
```

## Notes

- All multi-byte values are little-endian
- Strings are wchar_t (UTF-16LE) unless noted
- Count fields are int32 (4 bytes)
- Handlers use `sub_44E910` for reading (size as 3rd param)

