# 0x83 - SHOP_DATA_ALT

**CMD**: `0x83` (131 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B7D0`  
**Handler Ghidra**: `FUN_0047b7d0`

## Description

Alternative shop data packet. Same structure as 0x82 (396 bytes), likely for updating existing shop entries.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | bytes  | 396  | Shop item data        |

**Total Size**: 396 bytes (0x18C)

## C Structure

```c
struct ShopDataAltPacket {
    // Same as 0x82 - Complex structure (396 bytes)
    uint8_t data[396];
};
```

## Handler Logic (IDA)

```c
// sub_47B7D0
int __thiscall sub_47B7D0(void *this, int a2)
{
    char v5[396];  // Full structure buffer
    
    // Initialize buffer to zeros
    memset(v5, 0, sizeof(v5));
    
    sub_44E910(a2, v5, 0x18C);   // Read 396 bytes
    
    // Update existing shop item (uses sub_451C20)
    return sub_451C20((int*)((char*)&unk_84A960 + this), v5);
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47B7D0     | 396 bytes    |
| Ghidra | FUN_0047b7d0   | 396 bytes    |

**Status**: ✅ CERTIFIED

