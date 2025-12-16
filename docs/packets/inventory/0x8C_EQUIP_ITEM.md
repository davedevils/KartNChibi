# 0x8C EQUIP_ITEM (Server → Client)

**Handler:** `sub_47B9E0` / `FUN_0047b9e0`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Equips an item to a slot.

## Payload Structure

```c
struct EquipItem {
    int32 resultCode;      // 0 = update only, 1 = full data
    int64 slotId;          // 8 bytes slot identifier
    
    // If resultCode == 0 or 1:
    int32 itemId;          // Item to equip
    int32 targetSlot;      // Target slot index
    
    // If resultCode == 1, additional data based on item type:
    // itemType from slot lookup determines which struct
};
```

## Size

**12 bytes minimum** (resultCode=0) or **12 + 8 + itemData** (resultCode=1)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Result code (0=update, 1=full data) |
| 0x04 | int64 | 8 | Slot ID |
| 0x0C | int32 | 4 | Item ID (if result >= 0) |
| 0x10 | int32 | 4 | Target slot (if result >= 0) |
| 0x14 | varies | varies | Item data (if result == 1) |

## Item Types (from slot lookup +0x28)

| Type | Structure | Size |
|------|-----------|------|
| 0 | VehicleData | 0x2C (44) |
| 1 | ItemData | 0x38 (56) |
| 2 | AccessoryData | 0x1C (28) |
| 3 | SmallData | 0x1C (28) |
| 4 | AccessoryData | 0x1C (28) |
| 5 | ExtendedData | 0x30 (48) |

## Handler Logic

```c
// IDA: sub_47B9E0 / Ghidra: FUN_0047b9e0
read(resultCode, 4);
read(slotId, 8);

if (resultCode == 0 || resultCode == 1) {
    markSlotAsModified(slotId);
    read(itemId, 4);
    read(targetSlot, 4);
    
    if (resultCode == 1) {
        int itemType = getSlotType(slotId);
        switch (itemType) {
            case 0: read(vehicleData, 0x2C); break;
            case 1: read(itemData, 0x38); break;
            case 2: read(accessoryData, 0x1C); break;
            // etc...
        }
    }
}
```

## Notes

- Complex variable structure
- resultCode determines if full item data is included
- Item type is looked up from existing slot data, not sent in packet
- Used for equipping vehicles, items, accessories to character

