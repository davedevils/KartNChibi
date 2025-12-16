# 0x6F - SHOP_RESPONSE

**CMD**: `0x6F` (111 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B3C0`  
**Handler Ghidra**: `FUN_0047b3c0`

## Description

Response to a shop purchase request. Contains the result code and optional item data if purchase was successful.

## Payload Structure (Variable)

### Header

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Result code           |

### If Result == 1 (Success - Vehicle)

| Offset | Type          | Size | Description              |
|--------|---------------|------|--------------------------|
| 0x04   | VehicleData   | 44   | Purchased vehicle data   |

**Total Size**: 48 bytes

### If Result == 12 (Success - Item)

| Offset | Type       | Size | Description           |
|--------|------------|------|-----------------------|
| 0x04   | ItemData36 | 36   | Purchased item data   |

**Total Size**: 40 bytes

### Otherwise (Error)

**Total Size**: 4 bytes only

## C Structure

```c
struct ShopResponseHeader {
    int32_t result;             // +0x00 - Result code
};

struct ShopResponseVehicle {
    int32_t result;             // +0x00 - Always 1
    VehicleData vehicle;        // +0x04 - 44 bytes (0x2C)
};

struct ShopResponseItem {
    int32_t result;             // +0x00 - Always 12
    uint8_t itemData[36];       // +0x04 - 36 bytes (0x24)
};
```

## Result Codes

| Code | Meaning                              | Payload After |
|------|--------------------------------------|---------------|
| 1    | Success - Vehicle purchased          | 44 bytes      |
| 12   | Success - Item purchased             | 36 bytes      |
| Other | Error (various meanings)            | 0 bytes       |

## Handler Logic (IDA)

```c
// sub_47B3C0
int __thiscall sub_47B3C0(void *this, int a2)
{
    int result;
    char vehicleData[44];  // v6
    char itemData[36];     // v5
    
    if (byte_F78F50 == 1) {  // Special mode flag
        sub_44E910(a2, &result, 4);
        
        if (result == 1) {
            sub_44E910(a2, vehicleData, 0x2C);  // 44 bytes
            sub_44EF90((int*)((char*)&unk_848648 + this), vehicleData);
            dword_F78FD4 = 7;
            dword_F78F64 = result;
            byte_8CCD98[this] = 0;
            return result;
        }
        if (result != 12) {
            dword_F78FD4 = 7;
            dword_F78F64 = result;
            return result;
        }
        // result == 12
        sub_44E910(a2, itemData, 0x24);  // 36 bytes
        return sub_44EE10((int*)((char*)&unk_849780 + this), itemData);
    }
    
    // Normal mode
    sub_44E910(a2, &result, 4);
    
    if (result == 1) {
        sub_44E910(a2, vehicleData, 0x2C);
        return sub_44EF90((int*)((char*)&unk_848648 + this), vehicleData);
    }
    if (result == 12) {
        sub_44E910(a2, itemData, 0x24);
        return sub_44EE10((int*)((char*)&unk_849780 + this), itemData);
    }
    
    return result;
}
```

## Cross-Validation

| Source | Function       | Payload Read                    |
|--------|----------------|---------------------------------|
| IDA    | sub_47B3C0     | 4 + (44 or 36 or 0) bytes       |
| Ghidra | FUN_0047b3c0   | 4 + (44 or 36 or 0) bytes       |

**Status**: ✅ CERTIFIED
