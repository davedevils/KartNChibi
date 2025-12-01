# CMD 0x4D (77) - Request Data

**Direction:** Client → Server  
**Builder:** `sub_4790C0`

## Structure

```c
struct RequestData {
    uint8 requestData[0x114];  // 276 bytes
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 276 | bytes | requestData |

## Behavior (Client Side)

1. Initialize packet with CMD 0x4D
2. Write 276 bytes from `unk_5DE9DC`
3. Send to server

## Raw Packet

```
4D 14 01 00 00 00 00 00
[276 bytes of data]
```

## Notes

- Large fixed-size request
- Data source is static memory location
- Server should respond with appropriate data

