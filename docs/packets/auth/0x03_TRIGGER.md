# 0x03 TRIGGER (Server → Client)

**Handler:** `sub_479220`

## Purpose

Simple trigger packet with no payload. Used to signal state changes.

## Payload Structure

```c
// No payload - 0 bytes
```

## Notes

- Smallest possible packet (header only)
- Triggers `sub_473730` internally
- Used for state machine transitions

