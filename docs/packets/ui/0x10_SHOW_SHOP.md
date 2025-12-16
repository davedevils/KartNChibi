# 0x10 SHOW_SHOP (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_479550` @ line 220208
**Handler Ghidra:** `FUN_00479550` @ line 84733

## Purpose

Transition to Shop UI. **NO PAYLOAD** - trigger only.

## Payload Structure

```c
// NO PAYLOAD!
// Total: 0 bytes (header only)
```

## Handler Code (IDA)

```c
char __thiscall sub_479550(void *this, int a2)
{
  if (dword_F727F4 != 2) {
    sub_484010(this);           // Initialize shop
    sub_404410(dword_B23288, 7); // Set UI state = 7
    return sub_4538B0();         // Transition
  }
  return result;
}
```

## Handler Code (Ghidra)

```c
void FUN_00479550(void)
{
  if (DAT_00f727f4 != 2) {
    FUN_00484010();
    FUN_00404410(7);  // UI state = 7
    FUN_004538b0();
  }
  return;
}
```

## Behavior

- Sets UI state to **7** (SHOP)
- Requires `dword_F727F4 != 2`

## Server Implementation

```cpp
void sendShowShop(Session::Ptr session) {
    Packet pkt(0x10);
    // NO PAYLOAD
    session->send(pkt);
}
```
