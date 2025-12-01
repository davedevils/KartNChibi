# CMD 0x2C (44) - State Change

**Direction:** Client → Server

## Structure

```c
struct StateChange {
    int32 newState;
    int32 padding;        // Usually 0
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | newState |
| 0x04 | 4 | int32 | padding |

## State Values (observed)

| Value | Meaning |
|-------|---------|
| 0 | State 0 |
| 1 | State 1 |
| 2 | State 2 |
| 3 | State 3 |

## Raw Packet Examples

From captured traffic:
```
08 00 2C 00 00 00 00 00 02 00 00 00 00 00 00 00  // state = 2
08 00 2C 00 00 00 00 00 03 00 00 00 00 00 00 00  // state = 3
08 00 2C 00 00 00 00 00 00 00 00 00 00 00 00 00  // state = 0
08 00 2C 00 00 00 00 00 01 00 00 00 00 00 00 00  // state = 1
```

## Notes

- Sent frequently during gameplay
- Server should acknowledge or update state
- Exact meaning of each state TBD

