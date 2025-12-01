# PlayerInfo Structure (0x4C8 = 1224 bytes)

**Used by:** CMD 0x07 (session info S→C)

## Overview

This is the main player data structure sent during login. The first 4 bytes (DriverID) 
are critical: if set to -1 (0xFFFFFFFF), the client treats this as a new player 
who needs to create a character.

## Structure (partial, from reverse)

```c
struct PlayerInfo {
    // Total size: 0x4C8 = 1224 bytes
    
    int32_t  driverId;        // 0x00: Driver/Character ID. -1 = new player!
    wchar_t  name[24];        // 0x04: Driver name (UTF-16LE, null-terminated)
    int32_t  level;           // 0x34: Player level
    int32_t  exp;             // 0x38: Experience points
    int32_t  gold;            // 0x3C: Gold currency
    int32_t  cash;            // 0x40: Premium currency
    // ... rest is inventory, stats, equipped items
};
```

## Critical: DriverID

The global variable `dword_12124B8` stores the driver ID:
- If `-1`: New player, client shows "Create Driver" screen or skips to lobby
- If `>= 0`: Existing player, client proceeds to channel selection

This value is checked **everywhere** in the client code:
```c
if (dword_12124B8 == -1) {
    // Handle new player case
}
```

## Handler Code

From `sub_479020` (CMD 0x07 handler):

```c
// Read account ID (4 bytes)
sub_44E910(a2, (char *)&dword_80E1A8 + (_DWORD)this, (const char *)4);
// Read PlayerInfo (1224 bytes)
sub_44E910(a2, (char *)&dword_80E1B8 + (_DWORD)this, (const char *)0x4C8);
// Check for OGPlanet launcher window
result = FindWindowA("TOGPLauncherFrame", 0);
if (result) SendMessageA(result, 0x4C8u, 0x2710u, 30);
byte_8CCDC0[this] = 1;  // Mark as received
```

## Size Verification

Multiple locations in code reference 0x4C8:
```c
memset((char *)&dword_80E1B8 + (_DWORD)this, 0, 0x4C8u);  // Initialize
sub_44E910(a2, ..., (const char *)0x4C8);                  // Read from packet
```

## Example Server Code

```cpp
std::vector<uint8_t> playerInfo(1224, 0);

// Driver ID = 1 (NOT -1!)
playerInfo[0] = 0x01;

// Driver name "TestDriver" at offset 4 (UTF-16LE)
const char16_t* name = u"TestDriver";
for (int i = 0; name[i] && i < 24; ++i) {
    playerInfo[4 + i*2] = name[i] & 0xFF;
    playerInfo[4 + i*2 + 1] = (name[i] >> 8) & 0xFF;
}

// Level = 10 at offset 0x34
playerInfo[0x34] = 10;
```

## Notes

- Server **MUST** set DriverID to a value >= 0 for existing players
- All-zeros PlayerInfo will have DriverID = 0 which is valid (not -1)
- Name should be a valid UTF-16LE string
- Currency and stats affect UI display

