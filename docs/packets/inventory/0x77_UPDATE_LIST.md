# 0x77 - UPDATE_LIST

**CMD**: `0x77` (119 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B340`  
**Handler Ghidra**: `FUN_0047b340`

## Description

Updates multiple inventory items' values. Contains a count followed by ID/value pairs.

## Payload Structure

| Offset | Type    | Size   | Description              |
|--------|---------|--------|--------------------------|
| 0x00   | int32   | 4      | Update count             |
| 0x04+  | entries | 8 × n  | Array of update entries  |

### Per-Entry Structure (8 bytes)

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Item ID               |
| 0x04   | int32  | 4    | New value             |

**Total Size**: 4 + (8 × updateCount) bytes

## C Structure

```c
struct UpdateEntry {
    int32_t itemId;             // +0x00 - Item ID to update
    int32_t value;              // +0x04 - New value
};

struct UpdateListPacket {
    int32_t count;              // +0x00 - Number of updates
    UpdateEntry entries[];      // +0x04 - Array of update entries
};
```

## Handler Logic (IDA)

```c
// sub_47B340
int __thiscall sub_47B340(void *this, int a2)
{
    int count;
    int itemId, value;
    
    sub_44E910(a2, &count, 4);     // Read count
    
    _DWORD *v6 = (_DWORD*)((char*)&unk_848648 + this);
    
    for (int i = 0; i < count; ++i) {
        sub_44E910(a2, &itemId, 4);
        sub_44E910(a2, &value, 4);
        
        int item = sub_44F050(v6, itemId);  // Find item by ID
        *(_DWORD*)(item + 40) = value;      // Update value at offset 0x28
    }
    
    return count;
}
```

## Cross-Validation

| Source | Function       | Payload Read      |
|--------|----------------|-------------------|
| IDA    | sub_47B340     | 4 + 8*n bytes     |
| Ghidra | FUN_0047b340   | 4 + 8*n bytes     |

**Status**: ✅ CERTIFIED
