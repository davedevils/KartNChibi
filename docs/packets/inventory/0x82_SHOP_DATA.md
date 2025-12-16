# 0x82 - SHOP_DATA

**CMD**: `0x82` (130 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B710`  
**Handler Ghidra**: `FUN_0047b710`

## Description

Sends detailed shop item data. Contains a large data structure (396 bytes) for shop display.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | bytes  | 396  | Shop item data        |

**Total Size**: 396 bytes (0x18C)

## C Structure

```c
struct ShopDataPacket {
    // Complex structure - 396 bytes
    int32_t field1[7];          // +0x00 - 28 bytes
    int16_t field2;             // +0x1C
    int32_t field3;             // +0x1E
    int32_t field4;             // +0x22
    int32_t field5;             // +0x26
    int32_t field6;             // +0x2A
    int32_t field7;             // +0x2E
    int16_t field8;             // +0x32
    int32_t field9;             // +0x34
    int32_t field10;            // +0x38
    int32_t field11;            // +0x3C
    int32_t field12;            // +0x40
    int16_t field13;            // +0x44
    char field14;               // +0x46
    char extraData[320];        // +0x47 - String/display data
    int16_t footer;             // +0x188
    // Total: 0x18C (396 bytes)
};
```

## Handler Logic (IDA)

```c
// sub_47B710
int __thiscall sub_47B710(void *this, int a2)
{
    char v4[396];  // Full structure buffer
    
    // Initialize buffer to zeros
    memset(v4, 0, sizeof(v4));
    
    sub_44E910(a2, v4, 0x18C);   // Read 396 bytes
    
    return sub_451B70((int*)((char*)&unk_84A960 + this), v4);
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47B710     | 396 bytes    |
| Ghidra | FUN_0047b710   | 396 bytes    |

**Status**: ✅ CERTIFIED

