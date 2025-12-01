# 0x02 DISPLAY_MESSAGE (Server → Client)

**Handler:** `sub_478D40`

## Purpose

Displays a message to the player AND **sets the connection state flag** (`dword_F727F4`).

⚠️ **CRITICAL**: This packet MUST be sent after login (0x07) to confirm the connection!

## Payload Structure

```c
struct DisplayMessage {
    wchar_t message[];    // UTF-16LE null-terminated (can be empty)
    int32   code;         // Connection state code
};
```

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | wstring | var | Message text (UTF-16LE, null-terminated) |
| var | int32 | 4 | State code (see below) |

## State Codes

| Code | dword_F727F4 | Client Behavior |
|------|--------------|-----------------|
| 0 | 0 | ❌ Connection fail - shows MSG_CONNECT_FAIL, may disconnect |
| **1** | 1 | ✅ **Connection OK** - UI can proceed to next state |
| 2 | 2 | ❌ Forces disconnect (`sub_4775C0`) |

## Minimum Packet (Empty Message)

For connection confirmation with no message:
```
Payload: [00 00] [01 00 00 00]
         └─────┘ └──────────┘
         wstring  code=1
         (empty)
```
Total payload: 6 bytes

## Internal Details

- `sub_478D40`: Packet handler
  - Reads wstring into local buffer (max 160 wchars)
  - Reads int32 code
  - Calls `sub_464030(byte_F727E8, message, code)`

- `sub_464030`: Display function
  - Stores code at `*(this + 12)` → `dword_F727F4`
  - If code == 2: calls `sub_4775C0()` to disconnect
  - Formats and displays message

## Login Flow Usage

```
Client                              Server
   |                                   |
   |---- 0x07 (login request) -------->|
   |                                   |
   |<--- 0x07 (session info) ----------|
   |<--- 0x02 (code=1) ----------------| ← REQUIRED!
   |<--- 0x0E (channel list) ----------|
   |                                   |
   | Now client can go to State 4      |
   | (Channel Select)                  |
```

## Critical Notes

1. **Without 0x02 (code=1)**, the client stays stuck in login state
2. The client checks `dword_F727F4` in multiple places:
   - `if (!dword_F727F4)` → shows connection fail
   - `if (dword_F727F4 != 2)` → allows UI operations
3. Must be sent BEFORE 0x0E (channel list) for proper state transition

## Example Server Code

```cpp
void sendDisplayMessage(Session::Ptr session, const std::u16string& msg, int32_t code) {
    Packet pkt(0x02);
    
    // Write UTF-16LE wstring with null terminator
    for (char16_t c : msg) {
        pkt.writeUInt8(c & 0xFF);
        pkt.writeUInt8((c >> 8) & 0xFF);
    }
    pkt.writeUInt8(0);
    pkt.writeUInt8(0);
    
    pkt.writeInt32(code);
    session->send(pkt);
}

// Usage after login success:
sendDisplayMessage(session, u"", 1);  // Empty message, code=1
```
