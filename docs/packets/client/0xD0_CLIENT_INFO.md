# CMD 0xD0 (208) - Client Info

**Direction:** Client → Server

## Structure

```c
struct ClientInfo {
    int16 unknown1;       // 0xFF 0x00
    int16 unknown2;       // 0xFF 0x00
    int16 unknown3;       // 0x8F 0x00
    int16 unknown4;       // 0x21 0x00
    int16 unknown5;       // 0x34 0x00
    int16 unknown6;       // 0x7C 0x00
    int16 unknown7;       // 0x06 0x00
    int16 unknown8;       // 0xCE 0x00
    int16 unknown9;       // 0x34 0x00
    int16 unknown10;      // 0x7C 0x00
    int16 padding;        // 0x00 0x00
    char ipAddress[];     // Null-terminated ASCII
};
```

## Raw Packet Example

From captured traffic:
```
21 00 D0 00 00 00 00 00
FF 00 FF 00 8F 00 21 00
34 00 7C 00 06 00 CE 00
34 00 7C 00 00 00
31 32 37 2E 30 2E 30 2E  // "127.0.0."
31 0A 00                  // "1\n\0"
```

## Fields

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| 0x00-0x14 | 22 | int16[11] | Display/config values |
| 0x16 | var | string | IP address |

## Notes

- Contains client display settings (resolution?)
- IP address includes `\n` character
- Sent periodically during session
- Total size ~33 bytes

