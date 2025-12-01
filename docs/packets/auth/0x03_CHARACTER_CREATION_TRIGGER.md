# Packet 0x03 - S_CHARACTER_CREATION_TRIGGER

## Overview
Server-to-Client packet that triggers the character creation screen when the player has no existing character.

## Direction
**Server → Client**

## Packet Structure
```
[No payload - empty packet]
```

## Handler
Client handler: `sub_479220` (case 3 in main packet switch)

```c
char __stdcall sub_479220(int a1) {
    return sub_473730((int)byte_11B4528);
}
```

This calls `sub_473730` which initializes the character creation UI:
- Loads character creation resources
- Sets up name input field
- Enables confirmation buttons

## Flow
1. Client sends 0x07 (auth request)
2. Server responds 0x07 with `Driver ID = -1` (no character)
3. Server sends 0x02 (connection confirm, code=1)
4. **Server sends 0x03** (trigger character creation screen)
5. Client displays name input UI
6. Client sends 0x04 with chosen name
7. Server responds 0x04 with result

## Usage
Send this packet when:
- Account has no character (`Driver ID = -1`)
- After sending session info (0x07) and connection confirm (0x02)

## Related Packets
- `0x04` - Character Creation Response
- `0x07` - Session Info (contains Driver ID)

## Notes
- This is a zero-byte packet - just the CMD header
- Client will show UI for entering character name
- Must be sent INSTEAD of channel list (0x0E) when no character exists

