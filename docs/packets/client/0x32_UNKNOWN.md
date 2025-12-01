# CMD 0x32 (50) - Unknown Request

**Direction:** Client → Server

## Structure

```c
struct Unknown32 {
    int32 flag;           // Usually 0 or 1
};
```

## Raw Packet (from capture)

```
04 00 32 01 00 00 00 00 00 00 00 00
         ^^-- Flag in header? Or CMD variant?
```

Note: The `01` at offset [3] might be a flag, not part of CMD.

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | unknown (0) |

## Context

- Sent during active session
- Purpose unclear
- May be related to state polling

## Server Response

Unknown - needs testing.

