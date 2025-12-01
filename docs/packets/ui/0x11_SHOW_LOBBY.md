# CMD 0x11 (17) - Show Lobby

**Direction:** Server → Client  
**Handler:** `sub_479600`

## Structure

```c
struct ShowLobby {
    int32 lobbyId;
    int32 playerCount;
    int32 maxPlayers;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | lobbyId |
| 0x04 | 4 | int32 | playerCount |
| 0x08 | 4 | int32 | maxPlayers |

## Raw Packet

```
11 0C 00 00 00 00 00 00
01 00 00 00              // lobbyId = 1
0A 00 00 00              // playerCount = 10
64 00 00 00              // maxPlayers = 100
```

## Behavior

1. Client transitions to lobby screen
2. Room list fetched separately
3. Player can create or join rooms

## Notes

- Client state changes to LOBBY (8)
- Room list packets follow this

