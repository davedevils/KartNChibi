# 0x0F SHOW_GARAGE (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_479520` @ line 220192
**Handler Ghidra:** `FUN_00479520` @ line 84720

## Purpose

Transition to Garage UI. **NO PAYLOAD** - trigger only.

## Payload Structure

```c
// NO PAYLOAD!
// Total: 0 bytes (header only)
```

## Handler Code (IDA)

```c
char __thiscall sub_479520(void *this, int a2)
{
  if (dword_F727F4 != 2) {
    sub_484010(this);           // Initialize garage
    sub_404410(dword_B23288, 6); // Set UI state = 6
    return sub_4538B0();         // Transition
  }
  return result;
}
```

## Handler Code (Ghidra)

```c
void FUN_00479520(void)
{
  if (DAT_00f727f4 != 2) {
    FUN_00484010();
    FUN_00404410(6);  // UI state = 6
    FUN_004538b0();
  }
  return;
}
```

## Behavior

- Sets UI state to **6** (GARAGE)
- Requires `dword_F727F4 != 2` (connection not force-disconnecting)

## Server Implementation

```cpp
void sendShowGarage(Session::Ptr session) {
    Packet pkt(0x0F);
    // NO PAYLOAD
    session->send(pkt);
}
```

## Cross-Reference

- Related: [0x1B_INVENTORY_A.md](../inventory/0x1B_INVENTORY_A.md) - Vehicle list
