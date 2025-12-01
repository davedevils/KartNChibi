# CMD 0x73 (115) - Disconnect

**Direction:** Client → Server

## Structure

```c
// No payload
```

## Raw Packet

```
00 00 73 00 00 00 00 00
```

Note: Size field is 0, indicating no payload.

## Behavior

Client sends this when:
- User exits game
- Connection lost
- Logout requested

## Server Action

1. Clean up session
2. Remove from room if in one
3. Notify other players if needed
4. Close connection

## Notes

- Graceful disconnect notification
- Server may not receive if connection already broken
- Implement timeout handling as backup

