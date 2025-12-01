# CMD 0x19 (25) - Server Query

**Direction:** Client → Server

## Structure

```c
struct ServerQuery {
    char ipAddress[11];   // Null-terminated ASCII
    int32 port;
    int32 unknown;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 11 | string | ipAddress |
| 0x0B | 4 | int32 | port |
| 0x0F | 4 | int32 | unknown |

## Raw Packet Example

From captured traffic:
```
13 00 19 00 00 00 00 00
31 32 37 2E 30 2E 30 2E  // "127.0.0."
31 0A 00                  // "1\n\0"
61 C3 00 00              // port (0xC361 = 50017)
04 00 00 00              // unknown
```

## Notes

- Client sends its local IP and port info
- Used for NAT traversal / P2P setup
- The `\n` (0x0A) in IP string is unusual

