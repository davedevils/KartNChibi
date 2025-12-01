# CMD 0x3B (59) - Race Countdown

**Direction:** Server → Client

## Structure

```c
struct Countdown {
    int32 secondsRemaining;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | secondsRemaining |

## Values

| Value | Meaning |
|-------|---------|
| 3 | "3..." |
| 2 | "2..." |
| 1 | "1..." |
| 0 | "GO!" |
| -1 | Race started |

## Raw Packet

```
3B 04 00 00 00 00 00 00
03 00 00 00              // 3 seconds
```

## Notes

- Sent every second during countdown
- Client plays countdown sound/visual
- Players can't move until 0 or -1

