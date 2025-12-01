# 0xB6 DISPLAY_TEXT (Server → Client)

**Handler:** `sub_47AC60`

## Purpose

Displays a text message on screen with effects.

## Payload Structure

```c
struct DisplayText {
    wchar_t message[8192];   // UTF-16LE text
    int32   displayCode;     // Effect/position code
};
```

## Size

Variable (due to wstring)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | wstring | var | Message to display |
| var | int32 | 4 | Display code |

## Notes

- Message truncated at position 47 internally
- Uses floating text display system
- Display code determines position/animation

