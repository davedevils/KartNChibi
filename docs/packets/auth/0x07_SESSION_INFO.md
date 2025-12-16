# 0x07 SESSION_INFO (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_479020` @ line 219945
**Handler Ghidra:** `FUN_00479020` @ line 84488

## Purpose

Sent by server in response to successful login (0x07 client request). Contains the Driver ID and full PlayerInfo structure.

## Payload Structure (1228 bytes)

```c
struct SessionInfo {
    int32_t  driverId;           // +0x00 - Driver/Character ID
    uint8_t  playerInfo[1224];   // +0x04 - PlayerInfo structure (0x4C8)
};  // Total: 1228 bytes (0x4CC)
```

## Field Details

| Offset | Type | Size | Address | Description |
|--------|------|------|---------|-------------|
| 0x00 | int32 | 4 | 80E1A8 | Driver ID (-1 = no character, 0 = also no char, >= 1 = valid) |
| 0x04 | struct | 1224 | 80E1B8 | PlayerInfo structure |
| **Total** | - | **1228** | - | - |

## Driver ID Values

| Value | Meaning | Next Action |
|-------|---------|-------------|
| -1 (0xFFFFFFFF) | No character exists | Server sends 0x03 (Character Creation) |
| 0 | Also treated as no character | Server sends 0x03 |
| >= 1 | Valid character ID | Proceed to channel select |

## Handler Code (IDA)

```c
HWND __thiscall sub_479020(void *this, int a2)
{
  HWND result;

  sub_44E910(a2, (char *)&dword_80E1A8 + (_DWORD)this, (const char *)4);    // Driver ID
  sub_44E910(a2, (char *)&dword_80E1B8 + (_DWORD)this, (const char *)0x4C8); // PlayerInfo
  
  result = FindWindowA("TOGPLauncherFrame", 0);
  if ( result )
    result = (HWND)SendMessageA(result, 0x4C8u, 0x2710u, 30);
  
  byte_8CCDC0[(_DWORD)this] = 1;  // Mark as received
  return result;
}
```

## Handler Code (Ghidra)

```c
void __fastcall FUN_00479020(int param_1)
{
  HWND hWnd;
  
  FUN_0044e910(&DAT_0080e1a8 + param_1, 4);      // Driver ID
  FUN_0044e910(&DAT_0080e1b8 + param_1, 0x4c8); // PlayerInfo
  
  hWnd = FindWindowA("TOGPLauncherFrame", (LPCSTR)0x0);
  if (hWnd != (HWND)0x0) {
    SendMessageA(hWnd, 0x4c8, 10000, 0x1e);
  }
  (&DAT_008ccdc0)[param_1] = 1;  // Mark as received
  return;
}
```

## Server Implementation

```cpp
void sendSessionInfo(Session::Ptr session, int32_t driverId, const PlayerInfo& info) {
    Packet pkt(0x07);
    
    // Write Driver ID
    pkt.writeInt32(driverId);  // -1 or 0 = no character, > 0 = character ID
    
    // Write PlayerInfo (1224 bytes)
    pkt.writeBytes(reinterpret_cast<const uint8_t*>(&info), 1224);
    
    session->send(pkt);
}

// For new account (no character):
void sendSessionInfoNoCharacter(Session::Ptr session) {
    Packet pkt(0x07);
    pkt.writeInt32(-1);  // No character
    
    // Still need to send 1224 bytes (can be zeros)
    std::vector<uint8_t> emptyInfo(1224, 0);
    pkt.writeBytes(emptyInfo.data(), 1224);
    
    session->send(pkt);
    
    // Then send 0x03 to trigger character creation
}
```

## Sequence

```
Client                              Server
  │                                   │
  │  ════ CMD 0x07 (login) ═══════►  │
  │       [version][action][user][pw] │
  │                                   │
  │  ◄════ CMD 0x07 (response) ════  │  ← THIS PACKET
  │       [driverId:4]                │
  │       [PlayerInfo:1224]           │
  │                                   │
  │  [If driverId == -1]              │
  │  ◄════ CMD 0x03 ══════════════   │  Character creation
  │                                   │
  │  [If driverId > 0]                │
  │  ◄════ CMD 0x02 ══════════════   │  Connection confirm
  │       [message:wstring]           │
  │       [code:4 = 1]                │
  │                                   │
```

## Notes

- **CRITICAL**: byte_8CCDC0 flag is set to 1 after receiving
- This flag is checked by other handlers before processing
- OGPlanet launcher integration: sends window message if launcher found
- See [PLAYER_INFO.md](PLAYER_INFO.md) for full PlayerInfo structure

## Cross-Reference

- Client sends [0x07_CLIENT_AUTH.md](../client/0x07_CLIENT_AUTH.md)
- Related: [0x02_DISPLAY_MESSAGE.md](0x02_DISPLAY_MESSAGE.md) - sent after
- Related: [0x03_CHARACTER_CREATION_TRIGGER.md](0x03_CHARACTER_CREATION_TRIGGER.md) - if no character
- Related: [0xA7_SESSION_CONFIRM.md](0xA7_SESSION_CONFIRM.md) - GameServer equivalent

