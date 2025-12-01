# CMD 0x1E (30) - Single Item Update

**Direction:** Server → Client  
**Handler:** `sub_478FE0`

## Structure

```c
struct SingleItemUpdate {
    uint8 itemData[0x1C];  // 28 bytes - same as AccessoryData
};
```

## Fields

Same as AccessoryData (0x1C bytes):

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | accessoryId |
| 0x04 | 4 | int32 | uniqueId |
| 0x08 | 4 | int32 | slot |
| 0x0C | 4 | int32 | bonus1 |
| 0x10 | 4 | int32 | bonus2 |
| 0x14 | 4 | int32 | bonus3 |
| 0x18 | 4 | int32 | equipped |

## Behavior

1. Looks up item by uniqueId
2. Updates existing item in inventory
3. Refreshes UI if item is displayed

## Notes

- Used for real-time item updates (durability change, equip/unequip)
- Single item, not a list

