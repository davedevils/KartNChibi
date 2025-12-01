# 0x4E TIMESTAMP (Server → Client)

**Handler:** `sub_479160`

## Purpose

Sets a timestamp marker with no payload.

## Payload Structure

```c
// No payload - 0 bytes
```

## Size

**0 bytes**

## Notes

- No data read from packet
- Sets internal flag to 1
- Records current time via `sub_44ED50`
- Used for synchronization/timing

