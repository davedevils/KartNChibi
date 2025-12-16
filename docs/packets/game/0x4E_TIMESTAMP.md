# 0x4E TIMESTAMP (Server → Client)

**Packet ID:** 0x4E (78 decimal)  
**Handler:** `sub_479160` / `FUN_00479160`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Triggers timestamp capture for synchronization.

## Payload Structure

```c
// No payload - 0 bytes
```

## Size

**0 bytes** (header only)

## Handler Logic

```c
// IDA: sub_479160 / Ghidra: FUN_00479160
void handler_0x4E() {
    dword_8CCD8C = 1;                    // Set flag
    int64 timestamp = getTimestamp();     // sub_44ED50
    dword_8CCD90 = LOW(timestamp);
    dword_8CCD94 = HIGH(timestamp);
}
```

## Important Variables

| Variable | Address | Description |
|----------|---------|-------------|
| dword_8CCD8C | 0x8CCD8C | Timestamp flag |
| dword_8CCD90 | 0x8CCD90 | Timestamp low DWORD |
| dword_8CCD94 | 0x8CCD94 | Timestamp high DWORD |

## Server Implementation

```cpp
void sendTimestamp(Session::Ptr session) {
    Packet pkt(0x4E);
    // No payload
    session->send(pkt);
}
```

## Notes

- Trigger-only packet
- Client captures current timestamp on receipt
- Used for latency measurement / synchronization
- Timestamp is 64-bit value stored in two 32-bit DWORDs
