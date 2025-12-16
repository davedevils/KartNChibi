# PlayerInfo Structure (0x4C8 = 1224 bytes)

**Used by:** CMD 0x07 (LoginServer → Client), CMD 0xA7 (GameServer → Client)

## Overview

This is the main player data structure sent during login and game session.
Total size: **1224 bytes** (0x4C8)

The first field before PlayerInfo (4 bytes) is the **Driver ID**:
- If **-1** (0xFFFFFFFF): New player, no character
- If **0**: Also treated as no character in some contexts
- If **>= 1**: Existing player with character

## Memory Layout

Base address: `dword_80E1B8` (after 4-byte Driver ID at `dword_80E1A8`)

### Verified Offsets (from IDA)

```
Offset   Address     Type      Size    Description
─────────────────────────────────────────────────────
0x000    80E1B8      int32     4       [Start of PlayerInfo]
0x004    80E1BC      wchar[]   ~1154   Username/Account name (wstring)
0x486    80E63E      wchar[]   26      Driver Name (13 chars max + null)
0x4A0    80E658      byte      1       Flag 1
0x4A1    80E659      byte      1       Flag 2
0x4A4    80E65C      int32     4       Gold (in-game currency)
0x4A8    80E660      int32     4       Astros (premium currency)
0x4AC    80E664      int32     4       Cash / Experience?
0x4B0    80E668      int32     4       Current Vehicle ID (init: -1)
0x4B4    80E66C      int32     4       Current Driver Outfit ID (init: -1)
0x4B8    80E670      byte      1       Unknown flag
0x4B9    80E671      byte      1       Unknown flag
0x4BA    80E672      byte      1       Unknown flag
0x4BB    80E673      byte      1       Unknown flag
0x4BC    80E674      int32     4       Unknown
0x4C0    80E678      int32     4       Unknown
0x4C4    80E67C      int32     4       Unknown
─────────────────────────────────────────────────────
0x4C8    80E680      -         -       [End - Next structure starts here]
```

## C Structure Definition

```c
struct PlayerInfo {
    // Total size: 0x4C8 = 1224 bytes
    
    // 0x000-0x485: Account data area (~1158 bytes)
    uint8_t  accountData[0x486];     // Contains username, stats, inventory...
    
    // 0x486: Driver Name (UTF-16LE)
    wchar_t  driverName[13];         // 26 bytes (12 chars + null)
    
    // 0x4A0: Flags
    uint8_t  flag1;                  // 0x4A0
    uint8_t  flag2;                  // 0x4A1
    uint8_t  padding[2];             // 0x4A2-0x4A3
    
    // 0x4A4: Currency
    int32_t  gold;                   // 0x4A4 - In-game currency
    int32_t  astros;                 // 0x4A8 - Premium currency
    int32_t  cash;                   // 0x4AC - Additional currency/XP
    
    // 0x4B0: Current Equipment
    int32_t  vehicleId;              // 0x4B0 - Current vehicle (-1 = none)
    int32_t  driverId;               // 0x4B4 - Current driver outfit (-1 = none)
    
    // 0x4B8: Additional flags
    uint8_t  flags[4];               // 0x4B8-0x4BB
    
    // 0x4BC: Unknown trailing data
    int32_t  unknown1;               // 0x4BC
    int32_t  unknown2;               // 0x4C0
    int32_t  unknown3;               // 0x4C4
};  // Total: 0x4C8 = 1224 bytes
```

## Full Packet Structure (0x07 / 0xA7)

```
Offset   Size    Description
──────────────────────────────
0x00     4       Driver ID (int32) - at dword_80E1A8
0x04     1224    PlayerInfo structure
──────────────────────────────
Total:   1228 bytes
```

## Initialization (from `sub_478A60`)

```c
*(int *)((char *)&dword_80E1A8 + this) = 0;      // Driver ID = 0
*(int *)((char *)&dword_80E1B4 + this) = 0;      // Unknown
memset((char *)&dword_80E1B8 + this, 0, 0x4C8u); // Clear PlayerInfo
*(int *)((char *)&dword_80E668 + this) = -1;     // Vehicle ID = -1
*(int *)((char *)&dword_80E66C + this) = -1;     // Driver ID = -1
```

## Reading from Packet (from `sub_479020` - 0x07 handler)

```c
// Read Driver ID (4 bytes)
sub_44E910(a2, (char *)&dword_80E1A8 + this, (const char *)4);

// Read PlayerInfo (1224 bytes)
sub_44E910(a2, (char *)&dword_80E1B8 + this, (const char *)0x4C8);

// Check for OGPlanet launcher
result = FindWindowA("TOGPLauncherFrame", 0);
if (result)
    SendMessageA(result, 0x4C8u, 0x2710u, 30);

// Mark data as received
byte_8CCDC0[this] = 1;
```

## Server Implementation

```cpp
void sendSessionInfo(Session::Ptr session, int32_t driverId) {
    Packet pkt(0x07);  // or 0xA7 for GameServer
    
    // Write Driver ID
    pkt.writeInt32(driverId);  // -1 = no character, >0 = has character
    
    // Write PlayerInfo (1224 bytes)
    std::vector<uint8_t> playerInfo(1224, 0);
    
    if (driverId > 0) {
        // Fill from database...
        
        // Driver name at offset 0x486 (UTF-16LE)
        const char16_t* name = u"TestDriver";
        for (int i = 0; name[i] && i < 12; ++i) {
            playerInfo[0x486 + i*2] = name[i] & 0xFF;
            playerInfo[0x486 + i*2 + 1] = (name[i] >> 8) & 0xFF;
        }
        
        // Gold at offset 0x4A4
        *(int32_t*)&playerInfo[0x4A4] = 10000;
        
        // Astros at offset 0x4A8
        *(int32_t*)&playerInfo[0x4A8] = 500;
        
        // Vehicle ID at offset 0x4B0 (or -1 for none)
        *(int32_t*)&playerInfo[0x4B0] = 1;
        
        // Driver outfit at offset 0x4B4 (or -1 for none)
        *(int32_t*)&playerInfo[0x4B4] = 1;
    }
    
    pkt.writeBytes(playerInfo.data(), 1224);
    session->send(pkt);
}
```

## Important Notes

1. **Driver ID check**: Client checks `dword_12124B8` for -1 in many places
2. **Vehicle/Driver IDs**: Initialized to -1, must be set to valid IDs
3. **Name position**: Driver name is at offset 0x486, NOT at the beginning!
4. **The first ~1158 bytes** contain account data, stats, inventory references
5. **Currency values** are stored near the end of the structure

## Related Packets

| Packet | Direction | Description |
|--------|-----------|-------------|
| 0x07 | S→C | Login session info (LoginServer) |
| 0xA7 | S→C | Session confirm (GameServer) |
| 0x04 | S→C | Registration response (includes name) |
