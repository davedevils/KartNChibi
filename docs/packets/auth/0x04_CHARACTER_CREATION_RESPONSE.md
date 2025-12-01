# Packet 0x04 - S_CHARACTER_CREATION_RESPONSE

## Overview
Server-to-Client packet containing the result of character creation.

## Direction
**Server → Client**

## Packet Structure
```
[result_code:4]         // 0 = success, 1-3 = error
[name:wstring]          // Character name (if success)
[vehicle_data:0x2C]     // 44 bytes vehicle info (if success)
[driver_data:0x38]      // 56 bytes driver info (if success)
```

## Result Codes
| Code | Message | Description |
|------|---------|-------------|
| 0 | MSG_SAVE_DONE | Character created successfully |
| 1 | MSG_INVALID_NICK | Invalid nickname (bad characters) |
| 2 | MSG_ALREADY_REGIST | Name already registered |
| 3 | MSG_INVALID_NICK | Invalid nickname (too short/long) |

## Handler
Client handler: `sub_479230` (case 4 in main packet switch)

```c
char __thiscall sub_479230(void *this, const wchar_t **a2) {
    sub_4538B0();
    sub_44E910((int)a2, &v5, (const char *)4);  // Read result code
    
    if (v5) {  // Error
        switch (v5) {
            case 1: return sub_4707B0(..., "MSG_INVALID_NICK", 2, 0);
            case 2: return sub_4707B0(..., "MSG_ALREADY_REGIST", 2, 0);
            case 3: return sub_4707B0(..., "MSG_INVALID_NICK", 2, 0);
        }
    } else {  // Success
        sub_44EB60(a2, ...);              // Read name
        sub_44E910((int)a2, v6, 0x2C);    // Read 44 bytes vehicle data
        sub_44E910((int)a2, v7, 0x38);    // Read 56 bytes driver data
        return sub_4641E0(..., "MSG_SAVE_DONE", 1);
    }
    return sub_4641E0(..., "MSG_UNKNOWN_ERROR", 2);
}
```

## Flow
1. Server sends 0x03 (character creation trigger)
2. Client shows name input UI
3. Client sends chosen name (packet format TBD)
4. **Server validates and sends 0x04 with result**
5. If success: client proceeds to channel selection
6. If error: client shows error message

## Success Response Structure (code=0)
```
Offset  Size    Description
0x00    4       Result code (0)
0x04    var     Character name (wstring)
+0x00   0x2C    Vehicle data (44 bytes)
+0x2C   0x38    Driver data (56 bytes)
```

## Error Response Structure (code=1,2,3)
```
Offset  Size    Description
0x00    4       Result code (1, 2, or 3)
```

## Related Packets
- `0x03` - Character Creation Trigger
- `0x07` - Session Info

## Notes
- On success, server should also save character to database
- After successful creation, server should send channel list (0x0E)

