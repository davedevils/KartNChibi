# 0x8F UI_STATE_24 (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_47E950` @ line 223949
**Handler Ghidra:** `FUN_0047e950` @ line 88368

## Purpose

Sets UI state to 24. **NO PAYLOAD** - this is a trigger-only packet.

## Payload Structure

```c
// NO PAYLOAD!
// Total: 0 bytes (header only)
```

## Handler Code (IDA)

```c
char __stdcall sub_47E950(int a1)
{
  char result;

  if ( dword_F727F4 != 2 )  // Check connection not force-disconnecting
  {
    sub_404410(dword_B23288, 24);  // Set UI state = 24
    return sub_4538B0();           // UI transition
  }
  return result;
}
```

## Handler Code (Ghidra)

```c
void FUN_0047e950(void)
{
  if (DAT_00f727f4 != 2) {
    FUN_00404410(0x18);  // 0x18 = 24
    FUN_004538b0();
  }
  return;
}
```

## Behavior

1. Checks `dword_F727F4 != 2` (not force-disconnecting)
2. Sets UI state to **24** via `sub_404410`
3. Triggers UI transition via `sub_4538B0`

## UI State 24

State 24 is typically used during the post-login transition phase.

## Server Implementation

```cpp
void sendUIState24(Session::Ptr session) {
    Packet pkt(0x8F);
    // NO PAYLOAD - just header
    session->send(pkt);
}
```

## Notes

- **Requires** `dword_F727F4 != 2` (connection confirmed, not disconnecting)
- Must send 0x02 with code=1 BEFORE this packet works
- Similar to 0x90 (UI State 25)

## Cross-Reference

- Related: [0x02_DISPLAY_MESSAGE.md](../auth/0x02_DISPLAY_MESSAGE.md) - Sets dword_F727F4
- Related: [0x90_UI_STATE_25.md](0x90_UI_STATE_25.md) - Similar packet
