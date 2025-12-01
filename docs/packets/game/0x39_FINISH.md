# CMD 0x39 (57) - Player Finished

**Direction:** Server → Client

## Structure

```c
struct PlayerFinish {
    int32 playerId;
    int32 position;       // 1st, 2nd, 3rd...
    int32 time;           // Finish time in ms
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | playerId |
| 0x04 | 4 | int32 | position |
| 0x08 | 4 | int32 | time |

## Raw Packet

```
39 0C 00 00 00 00 00 00
05 00 00 00              // playerId = 5
01 00 00 00              // position = 1st
F4 55 01 00              // time = 87540ms (1:27.540)
```

## Notes

- Broadcast when player crosses finish line
- Other players see position update
- Used for leaderboard during race

