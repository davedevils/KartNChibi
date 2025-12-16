# 0xA7 - PLAYER_INFO_FULL

**CMD**: `0xA7` (167 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_479080`  
**Handler Ghidra**: `FUN_00479080`

## Description

Sends full player information (PlayerInfo structure - 1224 bytes). Sets a confirmation flag.

## Payload Structure

| Offset | Type       | Size  | Description           |
|--------|------------|-------|-----------------------|
| 0x00   | int32      | 4     | Header value          |
| 0x04   | PlayerInfo | 1224  | Full PlayerInfo (0x4C8)|

**Total Size**: 1228 bytes (4 + 0x4C8)

## C Structure

```c
struct PlayerInfoFullPacket {
    int32_t headerValue;        // +0x00 - Stored at +0x1A8
    PlayerInfo playerInfo;      // +0x04 - Full 1224-byte structure
};
```

## Handler Logic (IDA)

```c
// sub_479080
int __thiscall sub_479080(void *this, int a2)
{
    // Read 4 bytes header
    sub_44E910(a2, (char*)&dword_80E1A8 + this, 4);
    
    // Read full PlayerInfo (1224 bytes)
    int result = sub_44E910(a2, (char*)&dword_80E1B8 + this, 0x4C8);
    
    byte_8CCDC0[this] = 1;  // Set flag
    
    return result;
}
```

## Notes

- This confirms the PlayerInfo structure is **exactly 1224 bytes (0x4C8)**
- The 4-byte header is stored at offset +0x1A8 from base
- The PlayerInfo is stored at offset +0x1B8 from base

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_479080     | 1228 bytes   |
| Ghidra | FUN_00479080   | 1228 bytes   |

**Status**: ✅ CERTIFIED

