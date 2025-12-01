# CMD 0x0E (14) - Show Channel Select / Server List

**Direction:** Server → Client  
**Handler:** `sub_4793F0`

## Structure

```c
struct ShowChannelSelect {
    int32_t count;           // Number of channels
    // For each channel:
    struct Channel {
        int32_t id;          // Channel ID (0, 2, 4, 5, 6, 8)
        wchar_t name[];      // Null-terminated UTF-16LE name
        int32_t players;     // Current player count
        int32_t maxPlayers;  // Maximum players
        int32_t type;        // 0=Rookie, 1=Advanced, 2=Master
    } channels[count];
    wchar_t footer[];        // Empty null-terminated wstring (required!)
};
```

## Channel Types

| Type | Name | UI Position Y |
|------|------|---------------|
| 0 | Rookie | 204 |
| 1 | Advanced | 343 |
| 2 | Master | 482 |

## Example Channel IDs (from reverse)

- 0, 2 → Rookie
- 4, 5 → Advanced  
- 6, 8 → Master

## Raw Packet Example

```
00 00 0E 00 00 00 00 00   // Header: CMD=0x0E
06 00 00 00               // count = 6 channels
// Channel 0 (Rookie 1):
00 00 00 00               // id = 0
52 00 6F 00 6F 00 ... 00  // name = "Rookie 1" (wstring)
0A 00 00 00               // players = 10
64 00 00 00               // max = 100
00 00 00 00               // type = 0 (Rookie)
// ... more channels ...
00 00                     // footer = empty wstring (null terminator)
```

## Behavior

1. `sub_424210` - Initialize channel list
2. Loop: `sub_424250` - Add each channel
3. `sub_44EB60` - Read footer wstring
4. `sub_424320` - Sort channels
5. `sub_404410(state, 4)` - **Change to State 4 (Channel Select)**
6. `sub_4538B0` - Finalize

## Notes

- **MUST** send valid PlayerInfo (0x07) before this!
- PlayerInfo with DriverID = -1 means "new player" (will skip to create character)
- Footer wstring is required even if empty
- Client changes to State 4 (Channel Select screen with Rookie/Advanced/Master tabs)

