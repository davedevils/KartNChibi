# 0x72 - DATA_BLOCK

**CMD**: `0x72` (114 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B550`  
**Handler Ghidra**: `FUN_0047b550`

## Description

Sends a data block for shop/UI state management. Sets internal state flag to 3.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | bytes  | 104  | Data block            |

**Total Size**: 104 bytes (0x68)

## C Structure

```c
struct DataBlockPacket {
    uint8_t data[104];          // +0x00 - Raw data block
};
```

## Handler Logic (IDA)

```c
// sub_47B550
int __stdcall sub_47B550(int a1)
{
    sub_44E910(a1, &unk_F78F68, 0x68);   // Read 104 bytes
    dword_F78FD4 = 3;                    // Set state to 3
    return result;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47B550     | 104 bytes    |
| Ghidra | FUN_0047b550   | 104 bytes    |

**Status**: ✅ CERTIFIED
