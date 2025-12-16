# 0x2D CHAT_MESSAGE (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_479630` @ line 220269
**Handler Ghidra:** `FUN_00479630`

## Purpose

Displays a chat message from another player.

## Payload Structure (~116+ bytes)

```c
struct ChatMessage {
    int32_t  senderId;       // Sender player ID
    wchar_t  message[];      // Message text (wstring, ~84 bytes for 42 chars)
    int32_t  field1;         // Unknown
    int32_t  field2;         // Unknown
    int32_t  field3;         // Unknown
    int32_t  field4;         // Unknown
    int32_t  field5;         // Unknown
    int32_t  field6;         // Unknown
    int32_t  field7;         // Unknown (team/color?)
};
```

## Field Details

| Order | Type | Size | Description |
|-------|------|------|-------------|
| 1 | int32 | 4 | Sender ID |
| 2 | wstring | var | Message (42 wchars max) |
| 3-9 | int32 | 28 | 7 additional fields |

## Handler Code (IDA)

```c
char __stdcall sub_479630(const wchar_t **a1)
{
  int senderId, f1, f2, f3, f4, f5, f6, f7;
  wchar_t message[42];  // 84 bytes

  sub_44E910(a1, &senderId, 4);
  sub_44EB60(a1, message);
  sub_44E910(a1, &f1, 4);
  sub_44E910(a1, &f2, 4);
  sub_44E910(a1, &f3, 4);
  sub_44E910(a1, &f4, 4);
  sub_44E910(a1, &f5, 4);
  sub_44E910(a1, &f6, 4);
  sub_44E910(a1, &f7, 4);
  
  return sub_408360(dword_B35240, senderId, message, f1, f2, f3, f4, f5, f6, f7);
}
```

## Server Implementation

```cpp
void sendChatMessage(Session::Ptr session, int32_t senderId, 
                     const std::u16string& message,
                     int32_t f1, int32_t f2, int32_t f3, int32_t f4,
                     int32_t f5, int32_t f6, int32_t f7) {
    Packet pkt(0x2D);
    
    pkt.writeInt32(senderId);
    pkt.writeWString(message);  // Max 42 chars
    pkt.writeInt32(f1);
    pkt.writeInt32(f2);
    pkt.writeInt32(f3);
    pkt.writeInt32(f4);
    pkt.writeInt32(f5);
    pkt.writeInt32(f6);
    pkt.writeInt32(f7);
    
    session->send(pkt);
}

// Simple version
void sendChatMessage(Session::Ptr session, int32_t senderId, const std::u16string& msg) {
    sendChatMessage(session, senderId, msg, 0, 0, 0, 0, 0, 0, 0);
}
```

## Notes

- Message is UTF-16LE (wstring)
- Maximum 42 characters
- 7 additional fields purpose unknown (likely: team, color, level, etc.)
