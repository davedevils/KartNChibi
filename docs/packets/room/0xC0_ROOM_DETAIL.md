# 0xC0 - ROOM_DETAIL

**CMD**: `0xC0` (192 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47F4F0`  
**Handler Ghidra**: `FUN_0047f4f0`

## Description

Sends detailed room information including strings and large data blocks.

## Payload Structure

| Offset | Type   | Size   | Description           |
|--------|--------|--------|-----------------------|
| 0x00   | int32  | 4      | Unknown 1             |
| 0x04   | int32  | 4      | Unknown 2             |
| 0x08   | int32  | 4      | Unknown 3             |
| 0x0C   | byte   | 1      | Unknown 4             |
| 0x0D   | int32  | 4      | Unknown 5             |
| 0x11   | int32  | 4      | Unknown 6             |
| 0x15   | int32  | 4      | Unknown 7             |
| 0x19   | int32  | 4      | Unknown 8             |
| +      | string | var    | String A              |
| +      | string | var    | String B              |
| +      | string | var    | String C              |
| +      | bytes  | 32     | Fixed block 1 (0x20)  |
| +      | bytes  | 68     | Fixed block 2 (0x44)  |
| +      | int32  | 4      | Entry count           |
| +      | entries| 16 × n | Entries               |

**Total Size**: Variable (129 + strings + 16*n bytes)

## C Structure

```c
struct RoomDetailPacket {
    int32_t unknown1;           // +0x00
    int32_t unknown2;           // +0x04
    int32_t unknown3;           // +0x08
    uint8_t unknown4;           // +0x0C
    int32_t unknown5;           // +0x0D
    int32_t unknown6;           // +0x11
    int32_t unknown7;           // +0x15
    int32_t unknown8;           // +0x19
    // string stringA;          // Variable
    // string stringB;          // Variable
    // string stringC;          // Variable
    uint8_t fixedBlock1[32];    // 0x20 bytes
    uint8_t fixedBlock2[68];    // 0x44 bytes
    int32_t entryCount;         // 
    // entries[entryCount]      // 16 bytes each
};
```

## Cross-Validation

| Source | Function       | Payload Pattern              |
|--------|----------------|------------------------------|
| IDA    | sub_47F4F0     | 29 + strs + 100 + 4 + 16*n   |
| Ghidra | FUN_0047f4f0   | 29 + strs + 100 + 4 + 16*n   |

**Status**: ✅ CERTIFIED

