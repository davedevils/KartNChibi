# 0x27 ROOM_INFO_ALT (Server → Client)

**Handler:** `sub_479A30`

## Purpose

Alternative room info format.

## Payload Structure

```c
struct RoomInfoAlt {
    wchar_t data[128];       // 256 bytes
    int32   params[8];       // 32 bytes
};
```

## Size

**~288 bytes**

## Notes

- Similar to 0x25 but different handling
- Used in specific room contexts

