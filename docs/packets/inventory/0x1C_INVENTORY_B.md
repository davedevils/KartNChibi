# CMD 0x1C (28) - Inventory Type B (Items)

**Direction:** Server → Client  
**Handler:** `sub_478F20`

## Structure

```c
struct InventoryTypeB {
    int32 itemCount;
    ItemData items[itemCount];
};

struct ItemData {  // 0x38 = 56 bytes
    int32 itemId;
    int32 uniqueId;
    int32 quantity;
    int32 maxQuantity;
    int32 itemType;
    int32 rarity;
    int32 unknown1;
    int32 unknown2;
    int32 unknown3;
    int32 unknown4;
    int32 unknown5;
    int32 unknown6;
    int32 unknown7;
    int32 equipped;
};
```

## Fields per Item

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | itemId |
| 0x04 | 4 | int32 | uniqueId |
| 0x08 | 4 | int32 | quantity |
| 0x0C | 4 | int32 | maxQuantity |
| 0x10 | 4 | int32 | itemType |
| 0x14 | 4 | int32 | rarity |
| 0x18-0x34 | 28 | int32[7] | unknown |
| 0x34 | 4 | int32 | equipped |

## Item Types (speculative)

| Value | Type |
|-------|------|
| 0 | Consumable |
| 1 | Equipment |
| 2 | Material |
| 3 | Special |

## Notes

- Sent on login and shop entry
- Consumable items have quantity
- Equipment items are usually quantity = 1

