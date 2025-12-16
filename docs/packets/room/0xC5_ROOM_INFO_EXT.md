# 0xC5 - ROOM_INFO_EXT

**CMD**: `0xC5` (197 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47FAE0`  
**Handler Ghidra**: `FUN_0047fae0`

## Description

Extended room information with multiple integers and strings.

## Payload Structure

| Offset | Type   | Size   | Description           |
|--------|--------|--------|-----------------------|
| 0x00   | int32  | 4      | Unknown 1             |
| 0x04   | int32  | 4      | Unknown 2             |
| +      | string | var    | String A              |
| +      | int32×13| 52    | Data block (13 ints)  |
| +      | string | var    | String B              |
| +      | string | var    | String C              |

**Total Size**: Variable (60 + strings bytes)

## C Structure

```c
struct RoomInfoExtPacket {
    int32_t unknown1;           // +0x00
    int32_t unknown2;           // +0x04
    // string stringA;          // Variable
    int32_t dataBlock[13];      // 52 bytes
    // string stringB;          // Variable
    // string stringC;          // Variable
};
```

## Handler Logic (IDA)

```c
// sub_47FAE0
int __thiscall sub_47FAE0(void *this, const char **a2)
{
    sub_44E910(a2, &v4, 4);
    sub_44E910(a2, &v5, 4);
    sub_44EB30(a2, v6);  // String A
    
    // Read 13 int32s
    sub_44E910(a2, &v7, 4);
    sub_44E910(a2, &v8, 4);
    // ... (13 total)
    
    sub_44EB30(a2, v20);  // String B
    sub_44EB30(a2, v21);  // String C
}
```

## Cross-Validation

| Source | Function       | Payload Pattern         |
|--------|----------------|-------------------------|
| IDA    | sub_47FAE0     | 8 + str + 52 + str×2    |
| Ghidra | FUN_0047fae0   | 8 + str + 52 + str×2    |

**Status**: ✅ CERTIFIED

