# 0x0D FLAG_SET / SLOT_RESET (Server → Client)

**Packet ID:** 0x0D (13 decimal)  
**Handler:** `sub_47ADE0` / `FUN_0047ade0`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Sets a ready flag and resets player slot data. Used to prepare the client for state transitions (entering rooms, starting races, etc.).

## Payload Structure

```c
// No payload - 0 bytes
```

## Size

**0 bytes** (header only)

## Handler Decompilation (IDA)

```c
LONGLONG __stdcall sub_47ADE0(int a1)
{
    dword_B23150 = 1;                        // Set "ready" flag
    return sub_4B5160((int)dword_2EBD6A0);   // Reset player slots
}
```

## Behavior

1. **Set Ready Flag**: `dword_B23150 = 1` (signals client is ready for next state)
2. **Reset Slots**: Calls `sub_4B5160` to clear/reset player slot data

## Context: Client State Preparation

In the client code, `sub_4B5160` is called **before** state transitions:

```c
// Example from client code (not via packet)
sub_4B5160((int)dword_2EBD6A0);  // Reset slots first
// ... setup code ...
if (dword_12124B8 == -1)
    sub_43ED70(&off_5C8310, 1, ...);   // State 1
else if (byte_1A20B21 == 2 || 6)
    sub_43ED70(&off_5C8310, 4, ...);   // State 4
else if (byte_1ADF924 == 1)
    sub_43ED70(&off_5C8310, 12, ...);  // State 12 (possibly char creation?)
else
    sub_43ED70(&off_5C8310, 6, ...);   // State 6
```

The packet 0x0D triggers the **same reset function** (`sub_4B5160`) that the client uses internally before state changes!

## Important Variables

| Variable | Address | Description |
|----------|---------|-------------|
| `dword_B23150` | 0xB23150 | Ready flag (set to 1) |
| `dword_2EBD6A0` | 0x2EBD6A0 | Player slots array (930 DWORDs, ~16 slots) |

## Use Cases

- **Before entering game room**: Clear previous player data
- **Before race start**: Reset slot states
- **State transition prep**: Signal client is ready for next phase

## Server Implementation

```cpp
void sendSlotReset(Session::Ptr session) {
    Packet pkt(0x0D);
    // No payload needed
    session->send(pkt);
}
```

## Notes

- The `a1` parameter is **not used** - payload is empty
- `dword_B23150` flag may be checked elsewhere to confirm readiness
- Likely sent before other state-changing packets (0x11, 0x12, etc.)
- Related to multiplayer room/race slot management
