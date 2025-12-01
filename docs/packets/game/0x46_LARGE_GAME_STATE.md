# 0x46 LARGE_GAME_STATE (Server → Client)

**Handler:** `sub_47A760`

## Purpose

Large game state synchronization packet. One of the biggest packets in the protocol.

## Payload Structure

```c
struct LargeGameState {
    int32   playerId;
    int32   field1;
    int32   field2;
    int32   field3;
    int32   field4;
    int32   field5;
    int32   field6;
    int32   field7;
    int32   field8;
    int32   field9;
    int32   field10;
    int32   data[290];        // ~1160 bytes
    wchar_t string[13];       // 26 bytes
    // ... more fields
};
```

## Size

**~1160+ bytes** (variable)

## Notes

- One of the largest packets in the game
- Used for full game state synchronization
- Contains 290+ int32 values
- Likely used during race start or major state changes

