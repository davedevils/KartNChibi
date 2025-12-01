# CMD 0x3C (60) - Race End

**Direction:** Server → Client

## Structure

```c
struct RaceEnd {
    int32 reason;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | reason |

## Reason Values

| Value | Meaning |
|-------|---------|
| 0 | Normal completion |
| 1 | Time limit reached |
| 2 | Host ended race |
| 3 | Not enough players |
| 4 | Server shutdown |

## Raw Packet

```
3C 04 00 00 00 00 00 00
00 00 00 00              // Normal completion
```

## Notes

- Followed by 0x3A (Results) packet
- Client transitions to results screen
- Cleans up race-specific resources

