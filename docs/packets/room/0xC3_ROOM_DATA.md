# 0xC3 - ROOM_DATA

**CMD**: `0xC3` (195 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47F990`  
**Handler Ghidra**: `FUN_0047f990`

## Description

Sends room data with multiple integers and strings.

## Payload Structure

| Offset | Type   | Size   | Description           |
|--------|--------|--------|-----------------------|
| 0x00   | int32  | 4      | Unknown 1             |
| 0x04   | int32  | 4      | Unknown 2             |
| 0x08   | int32  | 4      | Unknown 3             |
| +      | string | var    | String A              |
| +      | int32×14| 56    | Data block (14 ints)  |
| +      | string | var    | String B              |

**Total Size**: Variable (68 + strings bytes)

## C Structure

```c
struct RoomDataPacket {
    int32_t unknown1;           // +0x00
    int32_t unknown2;           // +0x04
    int32_t unknown3;           // +0x08
    // string stringA;          // Variable
    int32_t dataBlock[14];      // 56 bytes
    // string stringB;          // Variable
};
```

## Handler Logic (IDA)

```c
// sub_47F990
int __thiscall sub_47F990(void *this, const char **a2)
{
    // Read 3 int32s
    sub_44E910(a2, &v4, 4);
    sub_44E910(a2, &v5, 4);
    sub_44E910(a2, &v6, 4);
    
    sub_44EB30(a2, v7);  // String A
    
    // Read 14 int32s
    sub_44E910(a2, &v8, 4);
    sub_44E910(a2, &v9, 4);
    // ... (14 total)
    
    sub_44EB30(a2, v22);  // String B
}
```

## Cross-Validation

| Source | Function       | Payload Pattern     |
|--------|----------------|---------------------|
| IDA    | sub_47F990     | 12 + str + 56 + str |
| Ghidra | FUN_0047f990   | 12 + str + 56 + str |

**Status**: ✅ CERTIFIED

