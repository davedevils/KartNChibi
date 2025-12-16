# 0x03 CHARACTER_CREATION_TRIGGER (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_479220` @ line 220061
**Handler Ghidra:** `FUN_00479220` @ line 84596

## Purpose

Triggers the character creation UI. **NO PAYLOAD** - this is a trigger-only packet.

## Payload Structure

```c
// NO PAYLOAD!
// Total: 0 bytes (header only)
```

## Handler Code (IDA)

```c
char __stdcall sub_479220(int a1)
{
  return sub_473730((int)byte_11B4528);  // Open character creation UI
}
```

## Handler Code (Ghidra)

```c
void FUN_00479220(void)
{
  FUN_00473730();  // Open character creation UI
  return;
}
```

## Behavior

Opens the character creation dialog (nickname input, character selection).

## When to Send

Send this packet when:
- Login response (0x07) returns `driverId == -1` (no character)
- Player has account but no character created yet

## Server Implementation

```cpp
void sendCharacterCreationTrigger(Session::Ptr session) {
    Packet pkt(0x03);
    // NO PAYLOAD - just header
    session->send(pkt);
}
```

## Sequence

```
Client                              Server
  │                                   │
  │  ════ CMD 0x07 (login) ═══════►  │
  │                                   │
  │  ◄════ CMD 0x07 (response) ════  │  driverId = -1 (no char)
  │       [driverId:-1]               │
  │       [PlayerInfo:1224]           │
  │                                   │
  │  ◄════ CMD 0x03 ═══════════════  │  ← THIS PACKET
  │                                   │
  │  [Shows character creation UI]    │
  │  [User enters nickname]           │
  │  [User clicks OK]                 │
  │                                   │
  │  ════ CMD 0x04 ═══════════════►  │  Create request
  │                                   │
```

## Cross-Reference

- Related: [0x07_SESSION_INFO.md](0x07_SESSION_INFO.md) - Checks driverId
- Related: [0x04_REGISTRATION_RESPONSE.md](0x04_REGISTRATION_RESPONSE.md) - Response to creation
