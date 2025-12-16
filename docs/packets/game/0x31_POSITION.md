# 0x31 POSITION (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_479950` @ line 220395
**Handler Ghidra:** `FUN_00479950`

## Purpose

Position/state synchronization during gameplay.

## Payload Structure (12 bytes)

```c
struct Position {
    int32_t field1;    // Unknown (passed to function)
    int32_t field2;    // Stored in dword_BCE208
    int32_t field3;    // Stored in dword_BCE20C
};  // Total: 12 bytes
```

## Handler Code (IDA)

```c
int __stdcall sub_479950(int a1)
{
  int field1, field2, field3;
  
  sub_44E910(a1, &field1, 4);
  sub_44E910(a1, &field2, 4);
  sub_44E910(a1, &field3, 4);
  
  dword_BCE208 = field2;
  dword_BCE20C = field3;
  
  return sub_407600(dword_B35240, edx, field1, field2, field3);
}
```

## Server Implementation

```cpp
void sendPosition(Session::Ptr session, int32_t f1, int32_t f2, int32_t f3) {
    Packet pkt(0x31);
    pkt.writeInt32(f1);
    pkt.writeInt32(f2);
    pkt.writeInt32(f3);
    session->send(pkt);
}
```

## Notes

- Used for in-game synchronization
- Fields likely represent position or state data
- Values stored globally for game logic
