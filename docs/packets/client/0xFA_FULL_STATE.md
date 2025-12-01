# CMD 0xFA (250) - Full Client State

**Direction:** Client → Server

## Structure

```c
struct FullClientState {
    // Large packet containing:
    int16 displayValues[11];  // Same as 0xD0
    // ... more data
    char ipAddress[];         // IP at end
};
```

## Raw Packet Example

From captured traffic:
```
00 00 FA 00 00 00 00 00
8E 00 00 00 00 00 04 00
30 01 00 00 00 00 00 00
00 00 21 00 D0 00 00 00
00 00 FF 00 FF 00 8F 00
21 00 34 00 7C 00 06 00
CE 00 34 00 7C 00 00 00
31 32 37 2E 30 2E 30 2E  // "127.0.0."
31 0A 00                  // "1\n\0"
```

## Fields (Partial)

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| 0x00 | 2 | int16 | Size indicator (0x8E = 142) |
| 0x02-0x08 | varies | - | Unknown data |
| ... | ... | - | More unknown |
| End | var | string | IP address |

## Notes

- Sent on connection init
- Contains full client configuration
- Larger than 0xD0 packet
- Size ~70+ bytes observed
- Contains embedded 0xD0-like data

