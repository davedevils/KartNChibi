# 0x90 UI_STATE_25 (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_47DF30` @ line 223479
**Handler Ghidra:** `FUN_0047df30` @ line 87920

## Purpose

Sets UI state to 25 and stores two values. Used for specific UI transitions.

## Payload Structure (8 bytes)

```c
struct UIState25 {
    int32_t value1;    // Stored in dword_D09558
    int32_t value2;    // Stored in dword_80E664 (cash/XP field)
};  // Total: 8 bytes
```

## Field Details

| Offset | Type | Size | Address | Description |
|--------|------|------|---------|-------------|
| 0x00 | int32 | 4 | D09558 | Value 1 (passed to sub_450CA0) |
| 0x04 | int32 | 4 | 80E664 | Value 2 (stored in PlayerInfo cash field) |

## Handler Code (IDA)

```c
char __thiscall sub_47DF30(void *this, int a2)
{
  int v2 = a2;
  int value1, value2;

  sub_44E910(a2, &value1, (const char *)4);  // Read value1
  sub_44E910(v2, &value2, (const char *)4);  // Read value2
  
  *(int*)(&dword_80E664 + this) = value2;    // Store value2 in PlayerInfo
  
  int v4 = sub_450CA0(&unk_838FB8 + this, value1);
  dword_D09558 = value1;
  dword_D0955C = *(int*)(v4 + 8);
  
  sub_4538B0();                              // Transition
  return sub_404410(dword_B23288, 25);       // Set UI state = 25
}
```

## Handler Code (Ghidra)

```c
void __thiscall FUN_0047df30(int param_1, undefined4 param_2)
{
  int iVar1;
  int local_4;
  
  local_4 = param_1;
  FUN_0044e910(&param_2, 4);  // Read value1
  FUN_0044e910(&local_4, 4);  // Read value2
  
  *(int*)(&DAT_0080e664 + param_1) = local_4;
  
  iVar1 = FUN_00450ca0(param_2);
  DAT_00d09558 = param_2;
  _DAT_00d0955c = *(undefined4*)(iVar1 + 8);
  
  FUN_004538b0();
  FUN_00404410(0x19);  // UI state = 25 (0x19)
  return;
}
```

## Server Implementation

```cpp
void sendUIState25(Session::Ptr session, int32_t value1, int32_t value2) {
    Packet pkt(0x90);
    pkt.writeInt32(value1);
    pkt.writeInt32(value2);
    session->send(pkt);
}
```

## Notes

- Sets UI state to **25** (0x19)
- Stores value2 at address 0x80E664 (PlayerInfo cash/XP field)
- Value1 used for some lookup via sub_450CA0

## Cross-Reference

- Related: [0x8F_UI_STATE_24.md](0x8F_UI_STATE_24.md) - Similar UI state packet
