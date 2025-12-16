# 0x10F - ITEM_LIST_UPDATE

**CMD**: `0x10F` (271 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47DA00`  
**Handler Ghidra**: `FUN_0047da00`

## Description

Updates multiple items. Contains a count followed by 48-byte item entries.

## Payload Structure

| Offset | Type   | Size   | Description           |
|--------|--------|--------|-----------------------|
| 0x00   | int32  | 4      | Entry count           |
| 0x04+  | entries| 48 × n | Item entries          |

### Per-Entry Structure (48 bytes / 0x30)

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Item ID               |
| 0x04   | int32 | 4    | Unknown 1             |
| 0x08   | int32 | 4    | Unknown 2             |
| 0x0C   | int32 | 4    | Value A (index 3)     |
| 0x10   | int32 | 4    | Value B (index 4)     |
| 0x14   | int32 | 4    | Value C (index 5)     |
| 0x18   | int32 | 4    | Value D (index 6)     |
| 0x1C   | int32 | 4    | Value E (index 7)     |
| 0x20-0x30| bytes| 16  | Additional data       |

**Total Size**: 4 + (48 × entryCount) bytes

## C Structure

```c
struct ItemEntry {
    int32_t itemId;             // +0x00
    int32_t unknown1;           // +0x04
    int32_t unknown2;           // +0x08
    int32_t valueA;             // +0x0C - Updated from v8[3]
    int32_t valueB;             // +0x10 - Updated from v8[4]
    int32_t valueC;             // +0x14 - Updated from v8[5]
    int32_t valueD;             // +0x18 - Updated from v8[6]
    int32_t valueE;             // +0x1C - Updated from v8[7]
    uint8_t extra[16];          // +0x20
};

struct ItemListUpdatePacket {
    int32_t count;              // +0x00 - Number of entries
    ItemEntry entries[];        // +0x04 - 48 bytes each
};
```

## Handler Logic (IDA)

```c
// sub_47DA00
char __thiscall sub_47DA00(void *this, int a2)
{
    int count;
    int v8[12];  // 48 bytes
    
    sub_44E910(a2, &count, 4);
    
    for (int i = 0; i < count; ++i) {
        sub_44E910(a2, v8, 0x30);
        
        int* item = sub_452830(&unk_863D68 + this, v8[0]);
        item[3] = v8[3];  // offset +0x0C
        item[4] = v8[4];  // offset +0x10
        item[5] = v8[5];  // offset +0x14
        item[6] = v8[6];  // offset +0x18
        item[7] = v8[7];  // offset +0x1C
    }
}
```

## Cross-Validation

| Source | Function       | Payload Read     |
|--------|----------------|------------------|
| IDA    | sub_47DA00     | 4 + 48*n bytes   |
| Ghidra | FUN_0047da00   | 4 + 48*n bytes   |

**Status**: ✅ CERTIFIED

