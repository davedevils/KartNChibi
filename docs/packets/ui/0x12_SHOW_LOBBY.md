# 0x12 SHOW_LOBBY (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_4795A0` @ line 220232
**Handler Ghidra:** `FUN_004795a0` @ line 84756

## ⚠️ CORRECTION: Flag Byte NOT Used!

**Previous documentation was INCORRECT!** This handler does NOT read the flag byte from header. It ALWAYS sets UI state to 8 (LOBBY).

## Purpose

Transition to Lobby UI. **NO PAYLOAD** - trigger only.

## Payload Structure

```c
// NO PAYLOAD!
// Total: 0 bytes (header only)
```

## Handler Code (IDA)

```c
void __thiscall sub_4795A0(void *this, int a2)
{
  int v2, v3;

  if (dword_F727F4 != 2) {
    sub_484010(this);            // Initialize
    sub_404410(dword_B23288, 8); // Set UI state = 8 (ALWAYS!)
    sub_4538B0();                // Transition
    
    // Durability check if already in ROOM(9) or RACING(11)
    if (dword_B23610 == 9 || dword_B23610 == 11) {
      v2 = sub_44F450(dword_1A576D8, dword_1A20B1C);
      if (v2 && *(int*)(v2 + 44) == 3) {
        v3 = *(int*)(v2 + 48);
        if (v3 <= 0)
          sub_4641E0(byte_F727E8, "MSG_DURABILITY_ZERO", 1);
        else if (v3 <= 10)
          sub_4641E0(byte_F727E8, "MSG_DURABILITY_LOW", 1);
      }
    }
  }
}
```

## Handler Code (Ghidra)

```c
void FUN_004795a0(void)
{
  int iVar1;
  
  if (DAT_00f727f4 != 2) {
    FUN_00484010();
    FUN_00404410(8);  // UI state = 8 (ALWAYS!)
    FUN_004538b0();
    
    // Durability check
    if ((DAT_00b23610 == 9) || (DAT_00b23610 == 0xb)) {
      iVar1 = FUN_0044f450(DAT_01a20b1c);
      if ((iVar1 != 0) && (*(int*)(iVar1 + 0x2c) == 3)) {
        if (*(int*)(iVar1 + 0x30) < 1)
          FUN_004641e0("MSG_DURABILITY_ZERO", 1);
        else if (*(int*)(iVar1 + 0x30) < 0xb)
          FUN_004641e0("MSG_DURABILITY_LOW", 1);
      }
    }
  }
  return;
}
```

## Behavior

1. Sets UI state to **8** (LOBBY)
2. If player was in ROOM (state 9) or RACING (state 11):
   - Checks vehicle durability
   - Shows MSG_DURABILITY_ZERO if durability <= 0
   - Shows MSG_DURABILITY_LOW if durability <= 10

## Game States Referenced

| Variable | Value | Meaning |
|----------|-------|---------|
| dword_B23610 | 9 | ROOM |
| dword_B23610 | 11 (0xB) | RACING |

## Server Implementation

```cpp
void sendShowLobby(Session::Ptr session) {
    Packet pkt(0x12);
    // NO PAYLOAD - flag byte in header is NOT used by this handler
    session->send(pkt);
}
```

## Important Notes

- **Flag byte in header [3] is NOT read by this handler!**
- Previous documentation claiming flag=0 for lobby, flag=1 for room was **INCORRECT**
- This packet ALWAYS transitions to LOBBY (state 8)
- Room entry is handled by different packets (0x3E Player Join, etc.)

## Cross-Reference

- Related: [0x3F_ROOM_INFO.md](../room/0x3F_ROOM_INFO.md) - Room list
- Related: [0x3E_PLAYER_JOIN.md](../room/0x3E_PLAYER_JOIN.md) - Entering room
