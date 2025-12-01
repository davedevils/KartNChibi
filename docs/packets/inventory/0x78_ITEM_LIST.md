# CMD 0x78 (120) - Item List Update

**Direction:** Server → Client  
**Handler:** `sub_47B220`

## Structure

```c
struct ItemListUpdate {
    int32 count;
    ItemEntry entries[count];
};

struct ItemEntry {
    uint8 data[0x2C];     // 44 bytes per item
};
```

## Fields per Item (0x2C = 44 bytes)

Same as VehicleData structure:

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | itemId |
| 0x04 | 4 | int32 | uniqueId |
| 0x08-0x28 | 36 | int32[9] | stats/unknown |

## Behavior

1. Clears existing list at target location
2. Reads count
3. Loops and reads each 44-byte entry
4. Adds to inventory structure

## Notes

- Generic item list, 44 bytes per item
- Used for various item categories

