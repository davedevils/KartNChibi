# 0x78 - ITEM_LIST

**CMD**: `0x78` (120 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B220`  
**Handler Ghidra**: `FUN_0047b220`

## Description

Sends the player's item inventory list. Contains a count followed by ItemData36 structures (36 bytes each).

## Payload Structure

| Offset | Type       | Size     | Description           |
|--------|------------|----------|-----------------------|
| 0x00   | int32      | 4        | Item count            |
| 0x04   | ItemData36 | 36 × n   | Array of item data    |

**Total Size**: 4 + (36 × itemCount) bytes

## C Structure

```c
struct ItemData36 {
    // 36 bytes (0x24) - see STRUCTURES.md
};

struct ItemListPacket {
    int32_t count;              // +0x00 - Number of items
    ItemData36 items[];         // +0x04 - Array of ItemData36
};
```

## Handler Logic (IDA)

```c
// sub_47B220
int __thiscall sub_47B220(void *this, int a2)
{
    int *v2 = (int*)((char*)&unk_849780 + this);
    int v5;     // count
    int v6[10]; // ItemData36 buffer (36 bytes + padding)
    
    sub_44EDF0(v2);                 // Clear existing inventory
    sub_44E910(a2, &v5, 4);         // Read count
    
    for (int i = 0; i < v5; ++i) {
        sub_44E910(a2, v6, 0x24);   // Read 36 bytes (ItemData36)
        sub_44EE10(v2, v6);         // Add to inventory
    }
    
    return v5;
}
```

## Cross-Validation

| Source | Function       | Payload Read      |
|--------|----------------|-------------------|
| IDA    | sub_47B220     | 4 + 36*n bytes    |
| Ghidra | FUN_0047b220   | 4 + 36*n bytes    |

**Status**: ✅ CERTIFIED
