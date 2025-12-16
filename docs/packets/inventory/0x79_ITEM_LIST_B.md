# 0x79 - ITEM_LIST_B

**CMD**: `0x79` (121 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B290`  
**Handler Ghidra**: `FUN_0047b290`

## Description

Sends a secondary item list. Contains a count followed by SmallItem structures (32 bytes each).

## Payload Structure

| Offset | Type      | Size     | Description           |
|--------|-----------|----------|-----------------------|
| 0x00   | int32     | 4        | Item count            |
| 0x04   | SmallItem | 32 × n   | Array of small items  |

**Total Size**: 4 + (32 × itemCount) bytes

## C Structure

```c
struct SmallItem {
    // 32 bytes (0x20) - see STRUCTURES.md
};

struct ItemListBPacket {
    int32_t count;              // +0x00 - Number of items
    SmallItem items[];          // +0x04 - Array of SmallItem
};
```

## Handler Logic (IDA)

```c
// sub_47B290
int __thiscall sub_47B290(void *this, int a2)
{
    int *v2 = (int*)((char*)&unk_84A598 + this);
    int v5;     // count
    int v6[9];  // SmallItem buffer (32 bytes + padding)
    
    sub_44F130(v2);                 // Clear existing list
    sub_44E910(a2, &v5, 4);         // Read count
    
    for (int i = 0; i < v5; ++i) {
        sub_44E910(a2, v6, 0x20);   // Read 32 bytes (SmallItem)
        sub_44F150(v2, v6);         // Add to list
    }
    
    return v5;
}
```

## Cross-Validation

| Source | Function       | Payload Read      |
|--------|----------------|-------------------|
| IDA    | sub_47B290     | 4 + 32*n bytes    |
| Ghidra | FUN_0047b290   | 4 + 32*n bytes    |

**Status**: ✅ CERTIFIED
