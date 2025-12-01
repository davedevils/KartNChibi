# CMD 0x1D (29) - Inventory Type C (Accessories)

**Direction:** Server → Client  
**Handler:** `sub_478F80`

## Structure

```c
struct InventoryTypeC {
    int32 itemCount;
    AccessoryData items[itemCount];
};

struct AccessoryData {  // 0x1C = 28 bytes
    int32 accessoryId;
    int32 uniqueId;
    int32 slot;           // Equipment slot
    int32 bonus1;
    int32 bonus2;
    int32 bonus3;
    int32 equipped;
};
```

## Fields per Accessory

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | accessoryId |
| 0x04 | 4 | int32 | uniqueId |
| 0x08 | 4 | int32 | slot |
| 0x0C | 4 | int32 | bonus1 |
| 0x10 | 4 | int32 | bonus2 |
| 0x14 | 4 | int32 | bonus3 |
| 0x18 | 4 | int32 | equipped |

## Notes

- Smaller items (accessories, parts)
- 28 bytes per item vs 44/56 for vehicles/items

