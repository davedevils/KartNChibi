# 0x65 SPEED_UPDATE (Server → Client)

**Packet ID:** 0x65 (101 decimal)  
**Handler:** `sub_47AD60` / `FUN_0047ad60`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Updates player speed in race.

## Payload Structure

```c
struct SpeedUpdate {
    int32 playerId;     // Target player
    float speed;        // Speed value
};
```

## Size

**8 bytes** (int32 + float)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Player ID |
| 0x04 | float | 4 | Speed value |

## Handler Logic

```c
// IDA: sub_47AD60 / Ghidra: FUN_0047ad60
read(&playerId, 4);
read(&speed, 4);

int idx = lookupPlayer(playerId);
if (idx >= 0) {
    // Check if this is current player's team member
    if (playerTeam[idx] == playerTeam[localPlayerIdx] && gameState == 3) {
        updateSpeed(speed);
    }
}
```

## Server Implementation

```cpp
void sendSpeedUpdate(Session::Ptr session, int playerId, float speed) {
    Packet pkt(0x65);
    pkt.writeInt32(playerId);
    pkt.writeFloat(speed);
    session->send(pkt);
}
```

## Notes

- Only processed if game state is 3 (racing)
- Only affects same-team players
- Speed is IEEE 754 float
- Part of race synchronization system
