# 0x115 - ITEM_VALUE

**CMD**: `0x115` (277 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47E680`  
**Handler Ghidra**: `FUN_0047e680`

## Description

Updates an item's value at offset +16.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Item ID               |
| 0x04   | int32 | 4    | Value                 |

**Total Size**: 8 bytes

## C Structure

```c
struct ItemValuePacket {
    int32_t itemId;             // +0x00
    int32_t value;              // +0x04
};
```

## Handler Logic (IDA)

```c
// sub_47E680
int __thiscall sub_47E680(void *this, int a2)
{
    int itemId, value;
    
    sub_44E910(a2, &itemId, 4);
    sub_44E910(a2, &value, 4);
    
    int item = sub_451690((char*)&unk_847C38 + this, itemId);
    *(int*)(item + 16) = value;  // Set value at offset +16
    
    return item;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47E680     | 8 bytes      |
| Ghidra | FUN_0047e680   | 8 bytes      |

**Status**: ✅ CERTIFIED

