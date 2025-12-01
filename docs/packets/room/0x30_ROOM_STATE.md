# CMD 0x30 (48) - Room/Channel State

**Direction:** Bidirectional  
**Handler:** `sub_479760` (S→C)

## Overview

This packet is used both as a **request** (C→S with flag=0x01) and a **response** (S→C).

## Client → Server (Request)

When client sends 0x30 with `flag=0x01`, it's requesting current state.

```c
struct RoomStateQuery {
    int32 unknown;    // Usually 0
};
```

**Server must respond with 0x30** (not 0x0B ACK!)

## Server → Client (Response)

```c
struct RoomStateUpdate {
    int32 roomId;     // Current room/channel ID (0 = none)
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | roomId/channelId |

## Behavior (S→C)

1. Updates global room ID reference (`dword_BCE220`)
2. Iterates through all players in room
3. Updates ready states
4. Resets local player flags if affected

## Raw Packets

Request (C→S):
```
04 00 30 01 00 00 00 00   // Size=4, CMD=0x30, Flag=0x01
00 00 00 00               // unknown = 0
```

Response (S→C):
```
04 00 30 00 00 00 00 00   // Size=4, CMD=0x30, Flag=0x00
00 00 00 00               // roomId = 0 (no room selected)
```

## Notes

- Client sends this during state transitions (Login → Channel Select)
- **Critical:** Server must respond with 0x30, not 0x0B!
- Used to sync client/server state

