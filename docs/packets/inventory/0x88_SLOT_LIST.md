# 0x88 - SLOT_LIST

**CMD**: `0x88` (136 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B930`  
**Handler Ghidra**: `FUN_0047b930`

## Description

Sends a list of slot data entries. Contains a count followed by 8-byte slot entries.

## Payload Structure

| Offset | Type   | Size   | Description           |
|--------|--------|--------|-----------------------|
| 0x00   | int32  | 4      | Entry count           |
| 0x04+  | entries| 8 × n  | Slot entries          |

### Per-Entry Structure (8 bytes)

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Slot ID / Index       |
| 0x04   | int32  | 4    | Value                 |

**Total Size**: 4 + (8 × entryCount) bytes

## C Structure

```c
struct SlotEntry88 {
    int32_t slotId;             // +0x00
    int32_t value;              // +0x04
};

struct SlotListPacket {
    int32_t count;              // +0x00 - Number of entries
    SlotEntry88 entries[];      // +0x04 - Array of entries
};
```

## Handler Logic (IDA)

```c
// sub_47B930
int __thiscall sub_47B930(void *this, int a2)
{
    int count;
    int v6[2];  // 8 bytes
    
    int *v2 = (int*)((char*)&unk_839E70 + this);
    sub_450A20(v2);             // Clear existing data
    sub_44E910(a2, &count, 4);  // Read count
    
    for (int i = 0; i < count; ++i) {
        sub_44E910(a2, v6, 8);  // Read 8 bytes
        sub_450AB0(v2, v6);     // Add entry
    }
    
    return count;
}
```

## Cross-Validation

| Source | Function       | Payload Read     |
|--------|----------------|------------------|
| IDA    | sub_47B930     | 4 + 8*n bytes    |
| Ghidra | FUN_0047b930   | 4 + 8*n bytes    |

**Status**: ✅ CERTIFIED

