# 0x0A CONNECTION_OK (Server → Client)

**Handler:** `sub_47D3B0`

## Purpose

Sent by server to confirm successful connection and provide initial data.

## Payload Structure

```c
struct ConnectionOK {
    int32 unknown1;           // +0x00
    int32 unknown2;           // +0x04
    int32 unknown3;           // +0x08
    int32 unknown4;           // +0x0C
    int32 unknown5;           // +0x10
    int32 unknown6;           // +0x14
    int32 unknown7;           // +0x18
    int32 unknown8;           // +0x1C
    int16 unknown9;           // +0x20
    int32 unknown10;          // +0x22
};
```

## Size

**38 bytes** (0x26)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Unknown |
| 0x04 | int32 | 4 | Unknown |
| 0x08 | int32 | 4 | Unknown |
| 0x0C | int32 | 4 | Unknown |
| 0x10 | int32 | 4 | Unknown |
| 0x14 | int32 | 4 | Unknown |
| 0x18 | int32 | 4 | Unknown |
| 0x1C | int32 | 4 | Unknown |
| 0x20 | int16 | 2 | Unknown |
| 0x22 | int32 | 4 | Unknown |

## Notes

- Critical packet for connection handshake
- Sent after TCP connection established
- Must be sent before any other packets

