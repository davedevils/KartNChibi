# 0xB4 SYSTEM_MESSAGE (Server → Client)

**Handler:** `sub_47CD60`

## Purpose

Displays a formatted system message with multiple strings.

## Payload Structure

```c
struct SystemMessage {
    int32   code;
    wchar_t format[14];      // Format string
    wchar_t content[8192];   // Message content
    int32   type;            // Display type
};
```

## Size

Variable (due to wstrings)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Message code |
| 0x04 | wstring | var | Format pattern |
| var | wstring | var | Message content |
| var | int32 | 4 | Display type (0-2) |

## Display Types

| Type | Behavior |
|------|----------|
| 0 | Format and display via swprintf |
| 1 | Chat display (state 8) |
| 2 | Room display (state 9) |

## Notes

- Used for server announcements
- Supports formatted messages with placeholders

