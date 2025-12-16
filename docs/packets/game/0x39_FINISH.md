# 0x39 FINISH (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_479CB0` @ line 220553
**Handler Ghidra:** `FUN_00479cb0`

## Purpose

Race finish signal. **NO PAYLOAD** - trigger only.

## Payload Structure

```c
// NO PAYLOAD!
// Total: 0 bytes (header only)
```

## Handler Code (IDA)

```c
char __stdcall sub_479CB0(int a1)
{
  return sub_4538B0();  // Just trigger transition
}
```

## Server Implementation

```cpp
void sendFinish(Session::Ptr session) {
    Packet pkt(0x39);
    // NO PAYLOAD
    session->send(pkt);
}
```

## Notes

- Simple trigger packet
- Calls sub_4538B0() for UI/state transition
