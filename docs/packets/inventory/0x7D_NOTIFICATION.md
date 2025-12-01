# 0x7D NOTIFICATION (Server → Client)

**Handler:** `sub_47B6B0`

## Purpose

Sends a notification with text.

## Payload Structure

```c
struct Notification {
    int32   code;
    wchar_t message[];
};
```

## Size

Variable

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Notification code |
| 0x04 | wstring | var | Message text |

## Notes

- Checks various UI states
- May trigger popup via `sub_476330`

