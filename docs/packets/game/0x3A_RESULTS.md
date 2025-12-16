# 0x3A RESULTS (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_47AE00` @ line 221272
**Handler Ghidra:** `FUN_0047ae00`

## Purpose

Race results notification.

## Payload Structure (4 bytes)

```c
struct Results {
    int32_t resultCode;    // Result/ranking code
};  // Total: 4 bytes
```

## Handler Code (IDA)

```c
int __stdcall sub_47AE00(int a1)
{
  int resultCode;
  sub_44E910(a1, &resultCode, 4);
  return sub_402400(dword_B0C8A8, resultCode);
}
```

## Server Implementation

```cpp
void sendResults(Session::Ptr session, int32_t resultCode) {
    Packet pkt(0x3A);
    pkt.writeInt32(resultCode);
    session->send(pkt);
}
```

## Notes

- Simple result code packet
- Full race results may use additional packets
