# 0x0A CONNECTION_OK (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_47D3B0` @ line 222963
**Handler Ghidra:** `FUN_0047d3b0` @ line 87401

## Purpose

First packet sent by server after TCP connection. Provides initial player state data.

## Payload Structure (38 bytes)

```c
struct ConnectionOK {
    uint8_t  flag1;              // +0x00 - 0x80E658
    uint8_t  flag2;              // +0x01 - 0x80E659
    int32_t  gold;               // +0x02 - 0x80E65C (currency)
    int32_t  astros;             // +0x06 - 0x80E660 (premium currency)
    int32_t  cash;               // +0x0A - 0x80E664 (XP or additional currency)
    int32_t  vehicleId;          // +0x0E - 0x80E668 (current vehicle, -1 = none)
    int32_t  driverId;           // +0x12 - 0x80E66C (current driver outfit, -1 = none)
    uint8_t  unknown1;           // +0x16 - 0x80E670
    uint8_t  unknown2;           // +0x17 - 0x80E671
    uint8_t  unknown3;           // +0x18 - 0x80E672
    uint8_t  unknown4;           // +0x19 - 0x80E673
    int32_t  unknown5;           // +0x1A - 0x80E674
    int32_t  unknown6;           // +0x1E - 0x80E678
    int32_t  unknown7;           // +0x22 - 0x80E67C
};  // Total: 38 bytes (0x26)
```

## Field Details

| Offset | Type | Size | Address | Description |
|--------|------|------|---------|-------------|
| 0x00 | uint8 | 1 | 80E658 | Flag 1 |
| 0x01 | uint8 | 1 | 80E659 | Flag 2 |
| 0x02 | int32 | 4 | 80E65C | Gold (in-game currency) |
| 0x06 | int32 | 4 | 80E660 | Astros (premium currency) |
| 0x0A | int32 | 4 | 80E664 | Cash/XP |
| 0x0E | int32 | 4 | 80E668 | Current Vehicle ID (-1 = none) |
| 0x12 | int32 | 4 | 80E66C | Current Driver ID (-1 = none) |
| 0x16 | uint8 | 1 | 80E670 | Unknown flag |
| 0x17 | uint8 | 1 | 80E671 | Unknown flag |
| 0x18 | uint8 | 1 | 80E672 | Unknown flag |
| 0x19 | uint8 | 1 | 80E673 | Unknown flag |
| 0x1A | int32 | 4 | 80E674 | Unknown |
| 0x1E | int32 | 4 | 80E678 | Unknown |
| 0x22 | int32 | 4 | 80E67C | Unknown |
| **Total** | - | **38** | - | - |

## Handler Code (IDA)

```c
int __thiscall sub_47D3B0(void *this, int a2)
{
  sub_44E910(a2, &byte_80E658[(_DWORD)this], (const char *)1);
  sub_44E910(a2, &byte_80E659[(_DWORD)this], (const char *)1);
  sub_44E910(a2, (char *)&dword_80E65C + (_DWORD)this, (const char *)4);
  sub_44E910(a2, (char *)&dword_80E660 + (_DWORD)this, (const char *)4);
  sub_44E910(a2, (char *)&dword_80E664 + (_DWORD)this, (const char *)4);
  sub_44E910(a2, (char *)&dword_80E668 + (_DWORD)this, (const char *)4);
  sub_44E910(a2, (char *)&dword_80E66C + (_DWORD)this, (const char *)4);
  sub_44E910(a2, (char *)&unk_80E670 + (_DWORD)this, (const char *)1);
  sub_44E910(a2, (char *)&unk_80E671 + (_DWORD)this, (const char *)1);
  sub_44E910(a2, (char *)&unk_80E672 + (_DWORD)this, (const char *)1);
  sub_44E910(a2, &byte_80E673[(_DWORD)this], (const char *)1);
  sub_44E910(a2, (char *)&unk_80E674 + (_DWORD)this, (const char *)4);
  sub_44E910(a2, (char *)&unk_80E678 + (_DWORD)this, (const char *)4);
  return sub_44E910(a2, (char *)&unk_80E67C + (_DWORD)this, (const char *)4);
}
```

## Handler Code (Ghidra)

```c
void __fastcall FUN_0047d3b0(int param_1)
{
  FUN_0044e910(&DAT_0080e658 + param_1,1);
  FUN_0044e910(&DAT_0080e659 + param_1,1);
  FUN_0044e910(&DAT_0080e65c + param_1,4);
  FUN_0044e910(&DAT_0080e660 + param_1,4);
  FUN_0044e910(&DAT_0080e664 + param_1,4);
  FUN_0044e910(&DAT_0080e668 + param_1,4);
  FUN_0044e910(&DAT_0080e66c + param_1,4);
  FUN_0044e910(&DAT_0080e670 + param_1,1);
  FUN_0044e910(&DAT_0080e671 + param_1,1);
  FUN_0044e910(&DAT_0080e672 + param_1,1);
  FUN_0044e910(&DAT_0080e673 + param_1,1);
  FUN_0044e910(&DAT_0080e674 + param_1,4);
  FUN_0044e910(&DAT_0080e678 + param_1,4);
  FUN_0044e910(&DAT_0080e67c + param_1,4);
  return;
}
```

## Server Implementation

```cpp
void sendConnectionOK(Session::Ptr session) {
    Packet pkt(0x0A);
    
    // Flags
    pkt.writeUInt8(0);          // flag1
    pkt.writeUInt8(0);          // flag2
    
    // Currency (from database or defaults)
    pkt.writeInt32(10000);      // gold
    pkt.writeInt32(500);        // astros
    pkt.writeInt32(0);          // cash/xp
    
    // Equipment (-1 = none)
    pkt.writeInt32(-1);         // vehicleId
    pkt.writeInt32(-1);         // driverId
    
    // Unknown flags
    pkt.writeUInt8(0);
    pkt.writeUInt8(0);
    pkt.writeUInt8(0);
    pkt.writeUInt8(0);
    
    // Unknown values
    pkt.writeInt32(0);
    pkt.writeInt32(0);
    pkt.writeInt32(0);
    
    session->send(pkt);
}
```

## Notes

- **CRITICAL**: Must be first packet sent after TCP connection
- Addresses (80E658-80E67C) are in PlayerInfo structure end area
- Same offsets as PlayerInfo 0x4A0-0x4C4 section
- Currency values initialized here, updated by 0x07

## Cross-Reference

- See [PLAYER_INFO.md](PLAYER_INFO.md) for full structure
- Related to 0x07 Session Info packet
