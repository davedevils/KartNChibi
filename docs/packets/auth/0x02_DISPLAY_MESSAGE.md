# 0x02 DISPLAY_MESSAGE (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_478D40` @ line 219798 → calls `sub_464030` @ line 208618
**Handler Ghidra:** `FUN_00478d40` @ line 84337

## Purpose

Displays a **popup message dialog** AND **sets critical connection state variable**.

## ⚠️ CRITICAL: Connection Confirmation

This packet sets `dword_F727F4` (at `byte_F727E8 + 12`):

```c
*(_DWORD *)(this + 12) = a3;  // this = byte_F727E8, so this+12 = dword_F727F4
```

| dword_F727F4 | Meaning | Behavior |
|--------------|---------|----------|
| **0** | Not confirmed | Client shows MSG_CONNECT_FAIL, UI handlers return early |
| **1** | Connection OK | All UI handlers work normally |
| **2** | Force disconnect | Calls `sub_4775C0` to disconnect |

**Server MUST send CMD 0x02 with code=1 after login to confirm connection!**

## Payload Structure

```c
struct DisplayMessage {
    wchar_t message[];    // UTF-16LE null-terminated (can be empty)
    int32_t code;         // Action code (CRITICAL - see above)
};  // Variable size
```

## Field Details

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | wstring | var | Message text (UTF-16LE, null-terminated) |
| var | int32 | 4 | Code: 0=info, 1=confirm connection, 2=disconnect |

## Handler Code (IDA)

```c
char __stdcall sub_478D40(const wchar_t **a1)
{
  wchar_t String[160];
  int v3;

  sub_44EB60(a1, String);              // Read wstring
  sub_44E910((int)v1, &v3, (const char *)4);  // Read code (4 bytes)
  return sub_464030((int)byte_F727E8, String, v3);
}

// In sub_464030:
char __thiscall sub_464030(int this, wchar_t *String, int a3)
{
  // ... setup ...
  *(_DWORD *)(this + 12) = a3;  // SET dword_F727F4 = code!
  if ( a3 == 2 )
    sub_4775C0(dword_12124B0, 0);  // DISCONNECT if code=2
  // ... display popup ...
}
```

## Handler Code (Ghidra)

```c
void FUN_00478d40(void)
{
  undefined4 local_148;  // code
  undefined1 local_144[320];  // message buffer
  
  FUN_0044eb60(local_144);          // Read wstring
  FUN_0044e910(&local_148, 4);      // Read code
  FUN_00464030(local_144, local_148);
}
```

## Code Values

| Code | Popup Title | dword_F727F4 | Behavior |
|------|-------------|--------------|----------|
| **0** | "Information" | 0 | Shows popup, button position differs |
| **1** | "Information" | 1 | **CONFIRMS CONNECTION**, shows popup |
| **2** | "Warning" | 2 | Shows popup + **DISCONNECTS CLIENT** |

## Server Implementation

```cpp
// CRITICAL: Must send after 0x07 to confirm connection
void sendConnectionConfirm(Session::Ptr session) {
    Packet pkt(0x02);
    
    // Empty message (can show custom welcome message)
    pkt.writeUInt16(0);  // Empty wstring null terminator
    
    // CODE MUST BE 1 to confirm connection!
    pkt.writeInt32(1);
    
    session->send(pkt);
}

// Disconnect with message
void sendDisconnect(Session::Ptr session, const std::u16string& reason) {
    Packet pkt(0x02);
    pkt.writeWString(reason);
    pkt.writeInt32(2);  // Code 2 = disconnect after popup
    session->send(pkt);
}

// Info popup (doesn't confirm connection!)
void sendInfoPopup(Session::Ptr session, const std::u16string& msg) {
    Packet pkt(0x02);
    pkt.writeWString(msg);
    pkt.writeInt32(0);  // Code 0 = info popup only
    session->send(pkt);
}
```

## Login Sequence

```
Client                              Server
  │                                   │
  │  ════ CMD 0x07 (login) ═══════►  │
  │                                   │
  │  ◄════ CMD 0x07 (session) ═════  │  PlayerInfo
  │                                   │
  │  ◄════ CMD 0x02 ═══════════════  │  ← THIS PACKET
  │       [message:wstring ""]        │
  │       [code:int32 = 1]            │  ← MUST BE 1!
  │                                   │
  │  [dword_F727F4 = 1]               │  Client ready for UI
  │                                   │
  │  ◄════ CMD 0x0E ═══════════════  │  Channel list
  │                                   │
```

## Error Messages

For error scenarios, use code=2:

| Scenario | Message | Code |
|----------|---------|------|
| Account banned | "MSG_ACCOUNT_BANNED" | 2 |
| Server maintenance | "MSG_SERVER_NOT_READY" | 2 |
| Invalid credentials | "MSG_REINPUT_IDPASS" | 2 |
| Database error | "MSG_DB_ACCESS_FAIL" | 2 |

## Notes

- **CRITICAL**: Without sending code=1, all UI handlers will fail!
- Empty message "" is valid (just null terminator)
- Code=2 calls `sub_4775C0` to disconnect after user closes popup
- The popup blocks user interaction until closed

## Cross-Reference

- Related: [0x01_LOGIN_RESPONSE.md](0x01_LOGIN_RESPONSE.md) - For ASCII error messages
- Related: [0x07_SESSION_INFO.md](0x07_SESSION_INFO.md) - Sent before this
- Related: [0x0E_SHOW_MENU.md](../ui/0x0E_SHOW_MENU.md) - Sent after this
