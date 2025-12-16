# 0x10D - ITEM_BLOCK

**CMD**: `0x10D` (269 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47D9A0`  
**Handler Ghidra**: `FUN_0047d9a0`

## Description

Sends an item data block (48 bytes).

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | bytes | 48   | Item data (0x30)      |

**Total Size**: 48 bytes (0x30)

## C Structure

```c
struct ItemBlockPacket {
    int32_t data[12];           // +0x00 - 48 bytes
};
```

## Handler Logic (IDA)

```c
// sub_47D9A0
int __thiscall sub_47D9A0(void *this, int a2)
{
    int v4[12];  // 48 bytes
    
    sub_44E910(a2, v4, 0x30);  // Read 48 bytes
    return sub_4523A0((int*)((char*)&unk_863D68 + this), v4);
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47D9A0     | 48 bytes     |
| Ghidra | FUN_0047d9a0   | 48 bytes     |

**Status**: ✅ CERTIFIED

