# 0x85 - ITEM_UPDATE_B

**CMD**: `0x85` (133 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B900`  
**Handler Ghidra**: `FUN_0047b900`

## Description

Updates an item by ID. Calls `sub_451BC0` with the item ID.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Item ID               |

**Total Size**: 4 bytes

## C Structure

```c
struct ItemUpdateBPacket {
    int32_t itemId;             // +0x00 - Item ID to update
};
```

## Handler Logic (IDA)

```c
// sub_47B900
char __stdcall sub_47B900(int a1)
{
    int itemId;
    
    sub_44E910(a1, &itemId, 4);
    return sub_451BC0(dword_1A5CE10, itemId);
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47B900     | 4 bytes      |
| Ghidra | FUN_0047b900   | 4 bytes      |

**Status**: ✅ CERTIFIED

