# CMD 0x07 (7) - Client Auth Info

**Direction:** Client → Server  

## Structure

```c
struct ClientAuthInfo {
    char odbc[4];         // "178\0" - DB connection string?
    int32 userId;         // 4
    int32 unknown1;       // 5 or 0
    // ... display values same as 0xD0
};
```

## Raw Packet (from capture)

```
20 00 07 00 00 00 00 00     // Size=0x20, CMD=0x07
31 37 38 00                 // "178\0"
04 00 00 00                 // userId or version = 4
05 00 00 00                 // unknown = 5
00 00                       // padding
FF 00 FF 00 8F 00 21 00     // display values
34 00 7C 00 06 00 CE 00
34 00 7C 00 00 00
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | string | odbc/version ("178") |
| 0x04 | 4 | int32 | userId (4) |
| 0x08 | 4 | int32 | unknown (5) |
| 0x0C | 2 | int16 | padding |
| 0x0E | 20 | int16[10] | displayValues |

## Context

- Sent early in connection sequence
- After initial heartbeat exchange
- Contains same display data as 0xD0/0xFA

## Server Response

Server should respond with 0x8F:
```
04 00 8F 00 00 00 00 00 00 00 00 00
```

