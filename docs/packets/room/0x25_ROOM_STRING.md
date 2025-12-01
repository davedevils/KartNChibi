# 0x25 ROOM_STRING (Server → Client)

**Handler:** `sub_479AB0`

## Purpose

Sends room data with string content.

## Payload Structure

```c
struct RoomString {
    wchar_t roomData[128];   // 256 bytes
    int32   params[8];       // 32 bytes
};
```

## Size

**~288 bytes**

## Notes

- Contains room description/name
- Additional parameters for room settings

