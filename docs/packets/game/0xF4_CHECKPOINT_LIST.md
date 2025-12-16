# 0xF4 - CHECKPOINT_LIST

**CMD**: `0xF4` (244 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47DB00`  
**Handler Ghidra**: `FUN_0047db00`

## Description

Sends a list of checkpoint data. Contains a count followed by 8-byte entries.

## Payload Structure

| Offset | Type   | Size   | Description           |
|--------|--------|--------|-----------------------|
| 0x00   | int32  | 4      | Entry count           |
| 0x04+  | entries| 8 × n  | Checkpoint entries    |

### Per-Entry Structure (8 bytes)

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Checkpoint ID         |
| 0x04   | int32 | 4    | Checkpoint value      |

**Total Size**: 4 + (8 × entryCount) bytes

## C Structure

```c
struct CheckpointEntry {
    int32_t checkpointId;       // +0x00
    int32_t value;              // +0x04
};

struct CheckpointListPacket {
    int32_t count;              // +0x00 - Number of entries
    CheckpointEntry entries[];  // +0x04 - Array of entries
};
```

## Handler Logic (IDA)

```c
// sub_47DB00
int __thiscall sub_47DB00(void *this, int a2)
{
    int count;
    int v6[2];  // 8 bytes
    
    int *v2 = (int*)((char*)&unk_8540F0 + this);
    sub_452BB0(v2);  // Clear existing data
    sub_44E910(a2, &count, 4);  // Read count
    
    for (int i = 0; i < count; ++i) {
        sub_44E910(a2, v6, 8);   // Read 8 bytes
        sub_452C40(v2, v6);      // Add entry
    }
    
    return count;
}
```

## Cross-Validation

| Source | Function       | Payload Read     |
|--------|----------------|------------------|
| IDA    | sub_47DB00     | 4 + 8*n bytes    |
| Ghidra | FUN_0047db00   | 4 + 8*n bytes    |

**Status**: ✅ CERTIFIED

