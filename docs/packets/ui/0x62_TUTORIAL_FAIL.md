# 0x62 TUTORIAL_FAIL (Server → Client)

**Packet ID:** 0x62 (98 decimal)  
**Handler:** `sub_479580` / `FUN_00479580`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Triggers tutorial fail UI state.

## Payload Structure

```c
// No payload - 0 bytes
```

## Size

**0 bytes** (header only)

## Handler Logic

```c
// IDA: sub_479580 / Ghidra: FUN_00479580
void handler_0x62() {
    sub_4538B0();              // Clear/reset
    setUIState(13);            // UI state 13 = tutorial fail
}
```

## Server Implementation

```cpp
void sendTutorialFail(Session::Ptr session) {
    Packet pkt(0x62);
    // No payload
    session->send(pkt);
}
```

## Notes

- Trigger-only packet (no payload)
- Sets UI state to 13
- Used when player fails tutorial mode
- No UI gate check - always processed

