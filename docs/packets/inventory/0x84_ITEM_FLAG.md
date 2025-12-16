# 0x84 - ITEM_FLAG

**CMD**: `0x84` (132 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B8D0`  
**Handler Ghidra**: `FUN_0047b8d0`

## Description

Sets a flag on an item. Finds the item by ID and sets byte at offset +0x46 to 1.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Item ID               |

**Total Size**: 4 bytes

## C Structure

```c
struct ItemFlagPacket {
    int32_t itemId;             // +0x00 - Item ID to flag
};
```

## Handler Logic (IDA)

```c
// sub_47B8D0
int __stdcall sub_47B8D0(int a1)
{
    int itemId;
    
    sub_44E910(a1, &itemId, 4);
    
    int item = sub_4519E0(dword_1A5CE10, itemId);
    if (item)
        *(uint8_t*)(item + 70) = 1;  // Set flag at offset +0x46
    
    return item;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47B8D0     | 4 bytes      |
| Ghidra | FUN_0047b8d0   | 4 bytes      |

**Status**: ✅ CERTIFIED

