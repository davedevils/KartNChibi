# 0x2E PLAYER_LEFT (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_479710` @ line 220298
**Handler Ghidra:** `FUN_00479710`

## Purpose

Notification that a player has left (chat/lobby context).

## Payload Structure (4 bytes)

```c
struct PlayerLeft {
    int32_t playerId;    // Player who left
};  // Total: 4 bytes
```

## Handler Code (IDA)

```c
int __stdcall sub_479710(int a1)
{
  int playerId;
  
  sub_44E910(a1, &playerId, 4);
  sub_4084B0(dword_B35240, playerId);  // Remove from list
  
  // Additional UI update logic
  if (dword_B4CA60 >= 0) {
    if (dword_B3BA64[34 * dword_B4CA60] == playerId) {
      // Handle if it was the selected player
    }
  }
  return result;
}
```

## Server Implementation

```cpp
void sendPlayerLeft(Session::Ptr session, int32_t playerId) {
    Packet pkt(0x2E);
    pkt.writeInt32(playerId);
    session->send(pkt);
}
```
