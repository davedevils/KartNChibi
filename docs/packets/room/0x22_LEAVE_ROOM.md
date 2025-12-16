# 0x22 LEAVE_ROOM (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_479920` @ line 220387
**Handler Ghidra:** `FUN_00479920`

## Purpose

Notification that a player has left the room. Simple packet with just player ID.

## Payload Structure (4 bytes)

```c
struct LeaveRoom {
    int32_t playerId;    // Player who left
};  // Total: 4 bytes
```

## Handler Code (IDA)

```c
char __stdcall sub_479920(int a1)
{
  int playerId;
  sub_44E910(a1, &playerId, 4);
  return sub_40D880(dword_B9ADD0, playerId);  // Remove from room list
}
```

## Server Implementation

```cpp
void sendLeaveRoom(Session::Ptr session, int32_t playerId) {
    Packet pkt(0x22);
    pkt.writeInt32(playerId);
    session->send(pkt);
}
```
