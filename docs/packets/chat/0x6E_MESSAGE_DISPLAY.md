# 0x6E MESSAGE_DISPLAY (Server → Client)

**Packet ID:** 0x6E (110 decimal)  
**Handler:** `sub_47B190` / `FUN_0047b190`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Triggers display of a message using data from a previous packet (0x6C).

## Payload Structure

```c
// No payload - 0 bytes
```

## Size

**0 bytes** (header only)

## Handler Logic

```c
// IDA: sub_47B190 / Ghidra: FUN_0047b190
void handler_0x6E() {
    // Uses globals set by previous 0x6C packet:
    // - dword_F33A5C (roomId)
    // - word_F33A60 (message wstring)
    sub_481E20(this, dword_F33A5C, &word_F33A60);
}
```

## Related Packets

| Packet | Purpose |
|--------|---------|
| 0x6C | Sends message data (fills globals) |
| 0x6E | Triggers display (uses globals) |

## Workflow

```
Server                          Client
   |                               |
   |---- 0x6C PLAYER_MESSAGE ----->| (fills F33A5C, F33A60, etc.)
   |                               |
   |---- 0x6E MESSAGE_DISPLAY ---->| (uses globals to show message)
   |                               |
```

## Server Implementation

```cpp
void sendMessageDisplay(Session::Ptr session) {
    // Make sure 0x6C was sent first to populate globals
    Packet pkt(0x6E);
    // No payload
    session->send(pkt);
}
```

## Notes

- **Trigger-only packet** - no data
- **MUST** send 0x6C before 0x6E
- Uses globals from 0x6C to display the message
- Part of a two-packet message display system

