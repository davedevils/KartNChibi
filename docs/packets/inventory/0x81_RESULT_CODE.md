# 0x81 - RESULT_CODE

**CMD**: `0x81` (129 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B8A0`  
**Handler Ghidra**: `FUN_0047b8a0`

## Description

Sets a result code for shop/inventory operations. Updates internal state to 7 with the received value.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Result code           |

**Total Size**: 4 bytes

## C Structure

```c
struct ResultCodePacket {
    int32_t resultCode;         // +0x00 - Result code
};
```

## Handler Logic (IDA)

```c
// sub_47B8A0
int __stdcall sub_47B8A0(int a1)
{
    int resultCode;
    
    sub_44E910(a1, &resultCode, 4);
    dword_F78FD4 = 7;           // Set state to 7
    dword_F78F64 = resultCode;  // Store result code
    
    return result;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47B8A0     | 4 bytes      |
| Ghidra | FUN_0047b8a0   | 4 bytes      |

**Status**: ✅ CERTIFIED

