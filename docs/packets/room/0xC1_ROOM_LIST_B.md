# 0xC1 - ROOM_LIST_B

**CMD**: `0xC1` (193 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47F6B0`  
**Handler Ghidra**: `FUN_0047f6b0`

## Description

Alternative room list packet. Similar to 0xBF but with different header structure.

## Payload Structure

| Offset | Type   | Size   | Description           |
|--------|--------|--------|-----------------------|
| 0x00   | int32  | 4      | Unknown 1             |
| 0x04   | int32  | 4      | Unknown 2             |
| 0x08   | int32  | 4      | Unknown 3             |
| 0x0C   | int32  | 4      | Unknown 4             |
| 0x10   | int32  | 4      | Unknown 5             |
| +      | string | var    | String A              |
| +      | string | var    | String B              |
| +      | string | var    | String C              |
| +      | int32  | 4      | Entry count           |
| +      | entries| 16 × n | Room entries          |

**Total Size**: Variable (24 + strings + 16*n bytes)

## C Structure

```c
struct RoomListBPacket {
    int32_t unknown1;           // +0x00
    int32_t unknown2;           // +0x04
    int32_t unknown3;           // +0x08
    int32_t unknown4;           // +0x0C
    int32_t unknown5;           // +0x10
    // string stringA;          // Variable length
    // string stringB;          // Variable length
    // string stringC;          // Variable length
    int32_t entryCount;         // 
    // RoomEntry entries[]      // 16 bytes each
};
```

## Handler Logic (IDA)

```c
// sub_47F6B0
void __thiscall sub_47F6B0(void *this, const char **a2)
{
    int v4, v6, v7, v8, v9, v10;
    char v11[33], v12[33], v13[34];
    
    sub_451D50(v14);  // Initialize
    
    // Read 5 int32 values
    sub_44E910(a2, &v6, 4);
    sub_44E910(a2, &v7, 4);
    sub_44E910(a2, &v8, 4);
    sub_44E910(a2, &v9, 4);
    sub_44E910(a2, &v10, 4);
    
    // Read 3 strings
    sub_44EB30(a2, v11);
    sub_44EB30(a2, v12);
    sub_44EB30(a2, v13);
    
    // Read count and entries
    sub_44E910(a2, &v4, 4);
    sub_451CD0(v14);
    
    for (int i = 0; i < v4; ++i) {
        int entry[4];
        sub_44E910(a2, entry, 0x10);  // 16 bytes
        sub_451CF0(v14, entry);
    }
}
```

## Cross-Validation

| Source | Function       | Payload Pattern       |
|--------|----------------|-----------------------|
| IDA    | sub_47F6B0     | 20 + strs + 4 + 16*n  |
| Ghidra | FUN_0047f6b0   | 20 + strs + 4 + 16*n  |

**Status**: ✅ CERTIFIED

