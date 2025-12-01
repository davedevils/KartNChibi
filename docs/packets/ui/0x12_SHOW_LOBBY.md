# Packet 0x12 - S_SHOW_LOBBY

## Overview
Server-to-Client packet that displays the game lobby (state 8).

## Direction
**Server → Client**

## Packet Structure
```
[No payload - empty packet]
```

## Handler
Client handler: `sub_4795A0` (case 18 in main packet switch)

```c
void __thiscall sub_4795A0(void *this, int a2) {
    if (dword_F727F4 != 2) {  // Not disconnected
        sub_484010(this);              // Initialize lobby
        sub_404410(dword_B23288, 8);   // Change state to 8 (Lobby)
        sub_4538B0();
        
        // Check vehicle durability
        if (dword_B23610 == 9 || dword_B23610 == 11) {
            // Show durability warnings if needed
            if (durability > 0 && durability <= 10) {
                sub_4641E0(..., "MSG_DURABILITY_LOW", 1);
            } else if (durability <= 0) {
                sub_4641E0(..., "MSG_DURABILITY_ZERO", 1);
            }
        }
    }
}
```

## State Change
Sets client state to **8** (Lobby)

## Prerequisites
- `dword_F727F4 != 2` (connection not closed)
- Player must have character data loaded (via 0xA7)
- Recommended: Send inventory packets (0x1B, 0x1C, 0x1D) before this

## Lobby Features
- Shows player character and stats
- Displays gold/cash currency
- Allows room creation (CREATE button)
- Game mode selection (Single/Team, Speed/Item)
- Access to Shop, Garage, etc.

## Related Packets
- `0xA7` - Session Confirm (player data)
- `0x1B` - Vehicle Inventory
- `0x1C` - Item Inventory
- `0x1D` - Accessory Inventory
- `0x11` - Show Menu (state 5, different screen!)

## GameServer Flow
```
1. Client connects to GameServer
2. Server sends 0x02 (connection confirm)
3. Client sends 0xA7 (session confirm with player info)
4. Server sends 0xA7 (session confirm with full player data)
5. Server sends 0x1B, 0x1C, 0x1D (inventories)
6. Server sends 0x12 (show lobby)
```

## Notes
- This is the MAIN GAME LOBBY
- NOT the menu - use 0x11 for menu
- Requires player data to be set up first

## ⚠️ Common Mistake
**0x12 = Lobby (state 8)**
**0x11 = Menu (state 5, NOT lobby!)**

Previously documented as "SHOW_ROOM" which was incorrect.

