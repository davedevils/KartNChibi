# CMD 0x3F (63) - Full Room Info

**Direction:** Server → Client  
**Handler:** `sub_479AB0`

## Structure

```c
struct RoomInfo {
    int32 roomId;
    int32 hostId;
    int32 mapId;
    int32 gameMode;
    int32 maxPlayers;
    int32 currentPlayers;
    int32 state;          // 0=waiting, 1=starting, 2=racing
    char roomName[32];    // Null-terminated
    PlayerSlot slots[16]; // Max 16 players
};

struct PlayerSlot {       // ~64 bytes per slot
    int32 playerId;
    int32 team;
    int32 ready;
    int32 characterId;
    int32 vehicleId;
    char playerName[32];
    int32 level;
    int32 unknown[2];
};
```

## Fields

### Header

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | roomId |
| 0x04 | 4 | int32 | hostId |
| 0x08 | 4 | int32 | mapId |
| 0x0C | 4 | int32 | gameMode |
| 0x10 | 4 | int32 | maxPlayers |
| 0x14 | 4 | int32 | currentPlayers |
| 0x18 | 4 | int32 | state |
| 0x1C | 32 | string | roomName |

### Per Player Slot (64 bytes)

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | playerId (-1 = empty) |
| 0x04 | 4 | int32 | team |
| 0x08 | 4 | int32 | ready |
| 0x0C | 4 | int32 | characterId |
| 0x10 | 4 | int32 | vehicleId |
| 0x14 | 32 | string | playerName |
| 0x34 | 4 | int32 | level |
| 0x38 | 8 | int32[2] | unknown |

## Game Modes

| Value | Mode |
|-------|------|
| 0 | Race |
| 1 | Battle |
| 2 | Team Race |
| 3 | Team Battle |
| 4 | Time Attack |

## Room States

| Value | State |
|-------|-------|
| 0 | Waiting |
| 1 | Countdown |
| 2 | Racing |
| 3 | Results |

## Notes

- Sent when entering a room
- Total size varies: header + (slots * 64)
- Empty slots have playerId = -1

