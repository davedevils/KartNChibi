# Packet 0x11 - S_SHOW_MENU

## Overview
Server-to-Client packet that displays the main menu screen (state 5).

## Direction
**Server → Client**

## Packet Structure
```
[No payload - empty packet]
```

## Handler
Client handler: `sub_4793C0` (case 17 in main packet switch)

```c
char __stdcall sub_4793C0(int a1) {
    char result;
    if (dword_F727F4 != 2) {  // Not disconnected
        sub_404410(dword_B23288, 5);  // Change state to 5 (Menu)
        return sub_4538B0();
    }
    return result;
}
```

## State Change
Sets client state to **5** (Menu)

## Prerequisites
- `dword_F727F4 != 2` (connection not closed)

## Related Packets
- `0x0E` - Channel List (state 4)
- `0x12` - Show Lobby (state 8)

## Notes
- This is the "blue menu" screen
- NOT the lobby - use 0x12 for lobby
- Often confused with 0x12 during reverse engineering

## ⚠️ Common Mistake
**0x11 = Menu (state 5), NOT Lobby!**
**0x12 = Lobby (state 8)**

