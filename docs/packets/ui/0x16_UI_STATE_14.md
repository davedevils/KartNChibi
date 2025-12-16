# 0x16 UI_STATE_14 (Server → Client)

**Packet ID:** 0x16 (22 decimal)  
**Handler:** `sub_47E8B0` / `FUN_0047e8b0`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Triggers UI state 14 (specific menu/screen).

## Payload Structure

```c
// No payload - 0 bytes
```

## Size

**0 bytes** (header only)

## Handler Logic

```c
// IDA: sub_47E8B0 / Ghidra: FUN_0047e8b0
void handler_0x16() {
    if (dword_F727F4 != 2) {  // UI gate check
        setUIState(14);
    }
}
```

## Server Implementation

```cpp
void sendUIState14(Session::Ptr session) {
    Packet pkt(0x16);
    // No payload
    session->send(pkt);
}
```

## Notes

- Trigger-only packet (no payload)
- Blocked if UI gate is set to 2
- Sets UI state to 14
