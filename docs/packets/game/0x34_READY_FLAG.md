# 0x34 READY_FLAG (Server → Client)

**Packet ID:** 0x34 (52 decimal)  
**Handler:** `sub_479A10` / `FUN_00479a10`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Sets a ready flag and triggers a game state update function.

## Payload Structure

```c
// No payload - 0 bytes
```

## Size

**0 bytes** (header only)

## Handler Logic

```c
// IDA: sub_479A10 / Ghidra: FUN_00479a10
void handler_0x34() {
    dword_BCE228 = 1;          // Set ready flag
    sub_40A040(dword_B9ADD0);  // Update game state
}
```

## Important Variables

| Variable | Address | Description |
|----------|---------|-------------|
| dword_BCE228 | 0xBCE228 | Ready flag (set to 1) |
| dword_B9ADD0 | 0xB9ADD0 | Player slots array |

## Server Implementation

```cpp
void sendReadyFlag(Session::Ptr session) {
    Packet pkt(0x34);
    // No payload
    session->send(pkt);
}
```

## Notes

- Trigger packet with no data
- Sets a flag similar to 0x0D but for different purpose
- Likely used during race preparation or room state changes

