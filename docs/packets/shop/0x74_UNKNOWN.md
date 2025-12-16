# 0x74 - UNKNOWN

**CMD**: `0x74` (116 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B620`  
**Handler Ghidra**: `FUN_0047b620`

## Description

Updates shop/inventory state with two integer values. Sets a confirmation flag.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Value A               |
| 0x04   | int32  | 4    | Value B               |

**Total Size**: 8 bytes

## C Structure

```c
struct Unknown74Packet {
    int32_t valueA;             // +0x00 - Value A
    int32_t valueB;             // +0x04 - Value B
};
```

## Handler Logic (IDA)

```c
// sub_47B620
int __thiscall sub_47B620(void *this, int a2)
{
    int valueA, valueB;
    
    sub_44E910(a2, &valueA, 4);
    sub_44E910(a2, &valueB, 4);
    
    sub_44F0E0((char*)&unk_848648 + this, valueA);
    byte_F78F60 = 1;        // Set flag
    dword_F78F64 = valueB;  // Store value
    
    return valueB;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47B620     | 8 bytes      |
| Ghidra | FUN_0047b620   | 8 bytes      |

**Status**: ✅ CERTIFIED
