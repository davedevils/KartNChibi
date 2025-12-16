# 0xED - ITEM_ADD_EXT

**CMD**: `0xED` (237 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47D5E0`  
**Handler Ghidra**: `FUN_0047d5e0`

## Description

Adds an item to the player's inventory with extended data. Contains a header block, type indicator, and type-specific item data.

## Payload Structure

### Header (Fixed)
| Offset | Type   | Size | Description             |
|--------|--------|------|-------------------------|
| 0x00   | bytes  | 20   | Header data (0x14)      |
| 0x14   | bytes  | 28   | Item reference (0x1C)   |

### Type-Specific Data (After Header)

| Type | Size  | Data Structure        |
|------|-------|-----------------------|
| 0    | 44    | VehicleData (0x2C)    |
| 1    | 56    | ItemData (0x38)       |
| 2    | 28    | AccessoryData (0x1C)  |
| 3    | 28    | AccessoryData (0x1C)  |
| 4    | 48    | Extended item (0x30)  |

**Total Size**: 48 + type-specific bytes

## C Structure

```c
struct ItemAddExtHeader {
    uint8_t headerData[20];     // +0x00 - Header (0x14)
    uint8_t itemRef[28];        // +0x14 - Item reference (0x1C)
};

struct ItemAddExtPacket {
    ItemAddExtHeader header;    // 48 bytes
    uint32_t type;              // From header - determines payload type
    union {
        VehicleData vehicle;    // Type 0: 44 bytes
        ItemData item;          // Type 1: 56 bytes
        AccessoryData accessory; // Type 2,3: 28 bytes
        uint8_t extended[48];   // Type 4: 48 bytes
    } data;
};
```

## Handler Logic (IDA)

```c
// sub_47D5E0
_DWORD *__thiscall sub_47D5E0(void *this, int a2)
{
    uint8_t header[20];
    int itemRef[7];  // 28 bytes
    int itemData[15];
    
    sub_44E910(a2, header, 0x14);       // Read header
    sub_456A40(&unk_ECAAD8, header);    // Process header
    sub_44E910(a2, itemRef, 0x1C);      // Read item ref
    
    // Find and update item
    int* item = sub_4516D0(dword_1A5A0E8, itemRef[1]);
    if (item)
        qmemcpy(item, itemRef, 0x1C);
    
    // Read type-specific data
    switch (type) {
        case 0:  // Vehicle
            sub_44E910(a2, itemData, 0x2C);
            break;
        case 1:  // Item
            sub_44E910(a2, itemData, 0x38);
            break;
        case 2:  // Accessory A
        case 3:  // Accessory B
            sub_44E910(a2, itemData, 0x1C);
            break;
        case 4:  // Extended
            sub_44E910(a2, itemData, 0x30);
            break;
    }
}
```

## Cross-Validation

| Source | Function       | Payload Pattern           |
|--------|----------------|---------------------------|
| IDA    | sub_47D5E0     | 20 + 28 + type-specific   |
| Ghidra | FUN_0047d5e0   | 20 + 28 + type-specific   |

**Status**: ✅ CERTIFIED

