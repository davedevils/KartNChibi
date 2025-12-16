# 0xB6 DISPLAY_TEXT (Server → Client)

**Packet ID:** 0xB6 (182 decimal)  
**Handler:** `sub_47AC60` / `FUN_0047ac60`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Displays text message on screen.

## Payload Structure

```c
struct DisplayText {
    wchar_t text[8192];  // wstring (null-terminated UTF-16LE)
    int32   duration;    // Display duration/type
};
```

## Size

**Variable:** wstring + 4 bytes

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | wstring | var | Text to display (max 8192 wchars) |
| +0x00 | int32 | 4 | Duration/type parameter |

## Handler Logic

```c
// IDA: sub_47AC60 / Ghidra: FUN_0047ac60
read_wstring(&text);   // UTF-16LE
read(&duration, 4);

text[47] = 0;  // Truncate at position 47 (safety)
int displayId = createDisplay(1);
showText(displayId, 0, text);
```

## Server Implementation

```cpp
void sendDisplayText(Session::Ptr session, const std::wstring& text, int duration) {
    Packet pkt(0xB6);
    pkt.writeWString(text);
    pkt.writeInt32(duration);
    session->send(pkt);
}
```

## Notes

- Uses wstring (UTF-16LE)
- Text is truncated at position 47 for safety
- Used for announcements, notifications
- duration parameter affects display behavior
