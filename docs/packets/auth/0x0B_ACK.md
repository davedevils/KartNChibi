# 0x0B ACK (Server → Client)

**Handler:** `sub_479190`

## Purpose

Simple acknowledgment packet with no payload.

## Payload Structure

```c
// No payload - 0 bytes
```

## Size

**0 bytes**

## Notes

- Calls `sub_4803A0` with value 11
- Used as generic acknowledgment
- Triggers internal state update

