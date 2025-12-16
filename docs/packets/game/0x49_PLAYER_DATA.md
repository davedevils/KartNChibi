# 0x49 PLAYER_DATA (Server → Client)

**Packet ID:** 0x49 (73 decimal)  
**Handler:** `sub_47AAD0` / `FUN_0047aad0`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Updates a specific field in player data array.

## Payload Structure

```c
struct PlayerData {
    int32 playerId;     // Target player
    int32 value;        // Value to set
    int32 fieldIndex;   // Field offset index
};
```

## Size

**12 bytes** (3 × int32)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Player ID |
| 0x04 | int32 | 4 | Value to set |
| 0x08 | int32 | 4 | Field index |

## Handler Logic

```c
// IDA: sub_47AAD0 / Ghidra: FUN_0047aad0
read(&playerId, 4);
read(&value, 4);
read(&fieldIndex, 4);

int idx = lookupPlayer(playerId);
if (idx >= 0) {
    // Player data array at 0x1BC08F8
    // Entry size: 0x29C98 (171160 bytes)
    playerData[idx * 171160 + fieldIndex] = value;
}
```

## Server Implementation

```cpp
void sendPlayerData(Session::Ptr session, int playerId, int value, int fieldIndex) {
    Packet pkt(0x49);
    pkt.writeInt32(playerId);
    pkt.writeInt32(value);
    pkt.writeInt32(fieldIndex);
    session->send(pkt);
}
```

## Notes

- Updates specific field in large player data structure
- Player ID validated via lookup
- Field index determines which data field to update
- Player data structure is ~171KB per player
