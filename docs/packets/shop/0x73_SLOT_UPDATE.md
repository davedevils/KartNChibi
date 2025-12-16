# 0x73 - SLOT_UPDATE

**CMD**: `0x73` (115 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B570`  
**Handler Ghidra**: `FUN_0047b570`

## Description

Updates slot data for multiple items. Contains a count followed by slot update entries.

## Payload Structure

| Offset | Type   | Size   | Description           |
|--------|--------|--------|-----------------------|
| 0x00   | int32  | 4      | Entry count           |
| 0x04+  | entries| 12 × n | Slot update entries   |

### Per-Entry Structure (12 bytes)

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Item ID               |
| 0x04   | int32  | 4    | Slot A value          |
| 0x08   | int32  | 4    | Slot B value          |

**Total Size**: 4 + (12 × entryCount) bytes

## C Structure

```c
struct SlotEntry {
    int32_t itemId;             // +0x00 - Item ID
    int32_t slotA;              // +0x04 - Value for offset +0x24
    int32_t slotB;              // +0x08 - Value for offset +0x28
};

struct SlotUpdatePacket {
    int32_t count;              // +0x00 - Number of entries
    SlotEntry entries[];        // +0x04 - Array of slot entries
};
```

## Handler Logic (IDA)

```c
// sub_47B570
int __stdcall sub_47B570(int a1)
{
    int count;
    int v7[3];  // SlotEntry buffer (12 bytes)
    
    sub_44E910(a1, &count, 4);
    
    // First, reset all items' slots to -2
    for (int i = 0; i < dword_1A5BC2C; ++i) {
        *(int*)(sub_44F030(dword_1A5AAF8, i) + 36) = -2;  // offset +0x24
        *(int*)(sub_44F030(dword_1A5AAF8, i) + 40) = -2;  // offset +0x28
    }
    
    // Then update specified slots
    for (int j = 0; j < count; ++j) {
        sub_44E910(a1, v7, 0xC);   // Read 12 bytes
        int item = sub_44F050(v7[0]);  // Find item by ID
        *(int*)(item + 36) = v7[1];    // Set slot A
        *(int*)(item + 40) = v7[2];    // Set slot B
    }
    
    return count;
}
```

## Cross-Validation

| Source | Function       | Payload Read     |
|--------|----------------|------------------|
| IDA    | sub_47B570     | 4 + 12*n bytes   |
| Ghidra | FUN_0047b570   | 4 + 12*n bytes   |

**Status**: ✅ CERTIFIED
