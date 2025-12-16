# 0x23 PLAYER_UPDATE (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_479BE0` @ line 220514
**Handler Ghidra:** `FUN_00479be0`

## Purpose

Updates player state in room (ready status, team, etc.).

## Payload Structure (8 bytes)

```c
struct PlayerUpdate {
    int32_t playerId;    // Player ID
    int32_t state;       // New state/status
};  // Total: 8 bytes
```

## Handler Code (IDA)

```c
int __stdcall sub_479BE0(int a1)
{
  int playerId, state;
  
  sub_44E910(a1, &playerId, 4);
  sub_44E910(a1, &state, 4);
  return sub_407660(dword_B35240, playerId, state);
}
```

## Server Implementation

```cpp
void sendPlayerUpdate(Session::Ptr session, int32_t playerId, int32_t state) {
    Packet pkt(0x23);
    pkt.writeInt32(playerId);
    pkt.writeInt32(state);
    session->send(pkt);
}
```

## State Values

| Value | Meaning |
|-------|---------|
| 0 | Not ready |
| 1 | Ready |
| 2+ | Other states (team, etc.) |
