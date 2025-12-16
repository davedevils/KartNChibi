# 0x107 - EXTENDED_DATA_LIST

**CMD**: `0x107` (263 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47E400`  
**Handler Ghidra**: `FUN_0047e400`

## Description

Sends a list of extended data entries. Contains a count followed by 52-byte entries.

## Payload Structure

| Offset | Type   | Size   | Description           |
|--------|--------|--------|-----------------------|
| 0x00   | int32  | 4      | Entry count           |
| 0x04+  | entries| 52 × n | Extended data entries |

### Per-Entry Structure (52 bytes / 0x34)

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00-0x14| bytes| 20   | Header data           |
| 0x14   | int32  | 4    | ID field 1            |
| 0x1C   | int32  | 4    | State field           |
| 0x20-0x34| bytes| 20   | Additional data       |

**Total Size**: 4 + (52 × entryCount) bytes

## C Structure

```c
struct ExtendedDataEntry {
    uint8_t header[20];         // +0x00
    int32_t id1;                // +0x14
    int32_t state;              // +0x1C (index 7 in int32 array)
    uint8_t extra[20];          // +0x20
};

struct ExtendedDataListPacket {
    int32_t count;              // +0x00 - Number of entries
    ExtendedDataEntry entries[]; // +0x04 - 52 bytes each
};
```

## Handler Logic (IDA)

```c
// sub_47E400
int __thiscall sub_47E400(void *this, int a2)
{
    int count;
    int v7[14];  // 56 bytes buffer for 52-byte entry
    
    int *v3 = (int*)((char*)&unk_866D70 + this);
    sub_4500C0(v3);  // Clear
    sub_44E910(a2, &count, 4);
    
    for (int i = 0; i < count; ++i) {
        sub_44E910(a2, v7, 0x34);  // Read 52 bytes
        sub_4500E0(v3, v7);
        
        // Special item handling
        if (*(int*)((char*)&dword_80E66C + this) == v7[5] && !v7[7]) {
            // ... update item
        }
    }
    
    return count;
}
```

## Cross-Validation

| Source | Function       | Payload Read     |
|--------|----------------|------------------|
| IDA    | sub_47E400     | 4 + 52*n bytes   |
| Ghidra | FUN_0047e400   | 4 + 52*n bytes   |

**Status**: ✅ CERTIFIED

