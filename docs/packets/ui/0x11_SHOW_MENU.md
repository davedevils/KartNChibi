# 0x11 SHOW_MENU (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_4793C0` @ line 220131
**Handler Ghidra:** `FUN_004793c0` @ line 84664

## Purpose

Transition to Main Menu UI. **NO PAYLOAD** - trigger only.

## Payload Structure

```c
// NO PAYLOAD!
// Total: 0 bytes (header only)
```

## Handler Code (IDA)

```c
char __stdcall sub_4793C0(int a1)
{
  if (dword_F727F4 != 2) {
    sub_404410(dword_B23288, 5); // Set UI state = 5
    return sub_4538B0();         // Transition
  }
  return result;
}
```

## Handler Code (Ghidra)

```c
void FUN_004793c0(void)
{
  if (DAT_00f727f4 != 2) {
    FUN_00404410(5);  // UI state = 5
    FUN_004538b0();
  }
  return;
}
```

## Behavior

- Sets UI state to **5** (MAIN_MENU)
- Requires `dword_F727F4 != 2`

## Server Implementation

```cpp
void sendShowMenu(Session::Ptr session) {
    Packet pkt(0x11);
    // NO PAYLOAD
    session->send(pkt);
}
```
