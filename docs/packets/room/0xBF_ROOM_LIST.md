# 0xBF - ROOM_LIST

**CMD**: `0xBF` (191 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47F390`  
**Handler Ghidra**: `FUN_0047f390`

## Description

Sends a list of rooms in the lobby. Contains header info, strings, and room entries.

## Payload Structure

| Offset | Type   | Size   | Description                |
|--------|--------|--------|----------------------------|
| 0x00   | int32  | 4      | Unknown 1                  |
| 0x04   | int32  | 4      | Unknown 2                  |
| 0x08   | int32  | 4      | Unknown 3                  |
| 0x0C   | int32  | 4      | Unknown 4                  |
| 0x10   | int32  | 4      | Unknown 5                  |
| 0x14   | string | var    | String A (null-terminated) |
| +      | bytes  | 20     | Fixed data block           |
| +      | string | var    | String B (null-terminated) |
| +      | string | var    | String C (null-terminated) |
| +      | int32  | 4      | Room count                 |
| +      | entries| 16 × n | Room entries               |

### Per-Room Entry Structure (16 bytes)

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Room ID               |
| 0x04   | int32 | 4    | Unknown A             |
| 0x08   | int32 | 4    | Unknown B             |
| 0x0C   | int32 | 4    | Unknown C             |

**Total Size**: Variable (20 + strings + 20 + strings + 4 + 16*n bytes)

## C Structure

```c
struct RoomEntry {
    int32_t roomId;             // +0x00
    int32_t unknownA;           // +0x04
    int32_t unknownB;           // +0x08
    int32_t unknownC;           // +0x0C
};

struct RoomListPacket {
    int32_t unknown1;           // +0x00
    int32_t unknown2;           // +0x04
    int32_t unknown3;           // +0x08
    int32_t unknown4;           // +0x0C
    int32_t unknown5;           // +0x10
    // string stringA;          // Variable length
    uint8_t fixedData[20];      // Fixed 0x14 bytes
    // string stringB;          // Variable length
    // string stringC;          // Variable length
    int32_t roomCount;          // 
    RoomEntry rooms[];          // 16 bytes each
};
```

## Handler Logic (IDA)

```c
// sub_47F390
void __thiscall sub_47F390(void *this, const char **a2)
{
    int v4, v6, v7, v8, v9, v10;
    char v11[36], v13[33], v14[35];
    int v12[5], v15[19];
    
    sub_451D50(v15);  // Initialize room list
    
    // Read 5 int32 values
    sub_44E910(a2, &v6, 4);
    sub_44E910(a2, &v7, 4);
    sub_44E910(a2, &v8, 4);
    sub_44E910(a2, &v9, 4);
    sub_44E910(a2, &v10, 4);
    
    sub_44EB30(a2, v11);           // Read string A
    sub_44E910(a2, v12, 0x14);     // Read 20 bytes
    sub_44EB30(a2, v13);           // Read string B
    sub_44EB30(a2, v14);           // Read string C
    
    sub_44E910(a2, &v4, 4);        // Read room count
    sub_451CD0(v15);
    
    for (int i = 0; i < v4; ++i) {
        int entry[4];
        sub_44E910(a2, entry, 0x10);  // Read 16 bytes
        sub_451CF0(v15, entry);
    }
    
    // ... rest of handler
}
```

## Cross-Validation

| Source | Function       | Payload Read              |
|--------|----------------|---------------------------|
| IDA    | sub_47F390     | 20 + strs + 20 + 4 + 16*n |
| Ghidra | FUN_0047f390   | 20 + strs + 20 + 4 + 16*n |

**Status**: ✅ CERTIFIED

