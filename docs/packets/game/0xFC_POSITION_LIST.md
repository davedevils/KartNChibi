# 0xFC - POSITION_LIST

**CMD**: `0xFC` (252 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47DBB0`  
**Handler Ghidra**: `FUN_0047dbb0`

## Description

Sends a list of position entries. Contains a count followed by 12-byte position entries.

## Payload Structure

| Offset | Type   | Size   | Description           |
|--------|--------|--------|-----------------------|
| 0x00   | int32  | 4      | Entry count           |
| 0x04+  | entries| 12 × n | Position entries      |

### Per-Entry Structure (12 bytes / 0xC)

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Player/Entity ID      |
| 0x04   | int32 | 4    | Position / Rank       |
| 0x08   | int32 | 4    | Time / Score          |

**Total Size**: 4 + (12 × entryCount) bytes

## C Structure

```c
struct PositionEntry {
    int32_t entityId;           // +0x00
    int32_t position;           // +0x04
    int32_t time;               // +0x08
};

struct PositionListPacket {
    int32_t count;              // +0x00 - Number of entries
    PositionEntry entries[];    // +0x04 - Array of entries
};
```

## Handler Logic (IDA)

```c
// sub_47DBB0
int __thiscall sub_47DBB0(void *this, int a2)
{
    int count;
    int v6[3];  // 12 bytes
    
    int *v2 = (int*)((char*)&unk_855870 + this);
    sub_451EA0(v2);  // Clear existing data
    sub_44E910(a2, &count, 4);  // Read count
    
    for (int i = 0; i < count; ++i) {
        sub_44E910(a2, v6, 0xC);   // Read 12 bytes
        sub_451FB0(v2, v6);        // Add entry
    }
    
    return count;
}
```

## Cross-Validation

| Source | Function       | Payload Read     |
|--------|----------------|------------------|
| IDA    | sub_47DBB0     | 4 + 12*n bytes   |
| Ghidra | FUN_0047dbb0   | 4 + 12*n bytes   |

**Status**: ✅ CERTIFIED

