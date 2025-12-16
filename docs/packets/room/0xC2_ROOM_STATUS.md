# 0xC2 - ROOM_STATUS

**CMD**: `0xC2` (194 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47F800`  
**Handler Ghidra**: `FUN_0047f800`

## Description

Sends room status information with strings and entries.

## Payload Structure

| Offset | Type   | Size   | Description           |
|--------|--------|--------|-----------------------|
| 0x00   | int32  | 4      | Unknown 1             |
| 0x04   | int32  | 4      | Unknown 2             |
| 0x08   | int32  | 4      | Unknown 3             |
| 0x0C   | int32  | 4      | Unknown 4             |
| +      | string | var    | String A              |
| +      | int32  | 4      | Unknown 5             |
| +      | int32  | 4      | Unknown 6             |
| +      | int32  | 4      | Unknown 7             |
| +      | string | var    | String B              |
| +      | string | var    | String C              |
| +      | int64×2| 16     | Two 8-byte values     |
| +      | int32  | 4      | Entry count           |
| +      | entries| 16 × n | Entries               |

**Total Size**: Variable (52 + strings + 16*n bytes)

## C Structure

```c
struct RoomStatusPacket {
    int32_t unknown1;           // +0x00
    int32_t unknown2;           // +0x04
    int32_t unknown3;           // +0x08
    int32_t unknown4;           // +0x0C
    // string stringA;          // Variable length
    int32_t unknown5;           // 
    int32_t unknown6;           // 
    int32_t unknown7;           // 
    // string stringB;          // Variable length
    // string stringC;          // Variable length
    int64_t value1;             // 8 bytes
    int64_t value2;             // 8 bytes
    int32_t entryCount;         // 
    // entries[]                // 16 bytes each
};
```

## Handler Logic (IDA)

```c
// sub_47F800
void __thiscall sub_47F800(void *this, const char **a2)
{
    int v6, v8, v9, v10, v11, v13, v14, v15, v18;
    char v12[36], v16[33], v17[35];
    
    sub_451D50(v19);
    
    // Read header int32s
    sub_44E910(a2, &v8, 4);
    sub_44E910(a2, &v9, 4);
    sub_44E910(a2, &v10, 4);
    sub_44E910(a2, &v11, 4);
    
    sub_44EB30(a2, v12);          // String A
    sub_44E910(a2, &v13, 4);
    sub_44E910(a2, &v14, 4);
    sub_44E910(a2, &v15, 4);
    sub_44EB30(a2, v16);          // String B
    sub_44EB30(a2, v17);          // String C
    
    // Read 2 x 8-byte values
    sub_44E910(a2, &v18, 8);
    sub_44E910(a2, &v18+2, 8);
    
    // Read count and entries
    sub_44E910(a2, &v6, 4);
    for (int i = 0; i < v6; ++i) {
        int entry[4];
        sub_44E910(a2, entry, 0x10);
    }
}
```

## Cross-Validation

| Source | Function       | Payload Pattern           |
|--------|----------------|---------------------------|
| IDA    | sub_47F800     | 16 + str + 12 + strs + 20 + 16*n |
| Ghidra | FUN_0047f800   | 16 + str + 12 + strs + 20 + 16*n |

**Status**: ✅ CERTIFIED

