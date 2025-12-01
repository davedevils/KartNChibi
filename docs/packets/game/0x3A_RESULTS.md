# CMD 0x3A (58) - Race Results

**Direction:** Server → Client

## Structure

```c
struct RaceResults {
    int32 playerCount;
    ResultEntry entries[playerCount];
};

struct ResultEntry {
    int32 playerId;
    int32 position;
    int32 time;
    int32 xpGained;
    int32 coinsGained;
    int32 unknown[3];
};
```

## Fields per Entry (28 bytes)

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | playerId |
| 0x04 | 4 | int32 | position |
| 0x08 | 4 | int32 | time |
| 0x0C | 4 | int32 | xpGained |
| 0x10 | 4 | int32 | coinsGained |
| 0x14 | 12 | int32[3] | unknown |

## Raw Packet

```
3A 3C 00 00 00 00 00 00  // Size 0x3C = 60
02 00 00 00              // playerCount = 2
// Player 1
05 00 00 00              // playerId = 5
01 00 00 00              // position = 1
F4 55 01 00              // time = 87540ms
64 00 00 00              // xpGained = 100
32 00 00 00              // coinsGained = 50
00 00 00 00 00 00 00 00 00 00 00 00
// Player 2
07 00 00 00              // playerId = 7
02 00 00 00              // position = 2
2C 56 01 00              // time = 87596ms
32 00 00 00              // xpGained = 50
19 00 00 00              // coinsGained = 25
00 00 00 00 00 00 00 00 00 00 00 00
```

## Notes

- Sent after all players finish or time limit
- Contains rewards for each player
- Client shows results screen

