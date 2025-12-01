# 0x8C EQUIP_ITEM (Server → Client)

**Handler:** `sub_47B9E0`

## Purpose

Equips an item to a slot.

## Payload Structure

```c
struct EquipItem {
    int32 equipType;
    int64 slotId;
    // If equipType == 1:
    int32 itemId;
    int32 value;
    // Then based on item type:
    VehicleData vehicle;     // 0x2C if vehicle
    ItemData item;           // 0x38 if item
    AccessoryData accessory; // 0x1C if accessory
};
```

## Size

Variable (depends on item type)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Equip type (0/1) |
| 0x04 | int64 | 8 | Slot ID |
| ... | var | var | Item data |

## Equip Types

| Type | Data Size |
|------|-----------|
| 0 | Vehicle (0x2C) |
| 1 | Item (0x38) |
| 2 | Accessory (0x1C) |

## Notes

- Complex variable structure
- Type determines payload format

