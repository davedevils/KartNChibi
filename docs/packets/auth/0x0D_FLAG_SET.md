# 0x0D FLAG_SET (Server → Client)

**Handler:** `sub_47ADE0`

## Purpose

Sets internal flag with no payload.

## Payload Structure

```c
// No payload - 0 bytes
```

## Size

**0 bytes**

## Notes

- Sets `dword_B23150 = 1`
- Calls `sub_4B5160`
- Used for state machine control

