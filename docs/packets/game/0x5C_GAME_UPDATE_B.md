# 0x5C GAME_UPDATE_B (Server → Client)

**Packet ID:** 0x5C (92 decimal)  
**Handler:** `sub_47A500` / `FUN_0047a500`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Updates game state with entity validation.

## Payload Structure

```c
struct GameUpdateB {
    int32 entityId;    // Entity to update
    int32 value1;      // Update value 1
    int32 value2;      // Update value 2
};
```

## Size

**12 bytes** (3 × int32)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Entity ID |
| 0x04 | int32 | 4 | Value 1 |
| 0x08 | int32 | 4 | Value 2 |

## Handler Logic

```c
// IDA: sub_47A500 / Ghidra: FUN_0047a500
read(&entityId, 4);
read(&value1, 4);
read(&value2, 4);

if (validateEntity(entityId) >= 0) {
    updateGameState(value1, value2);
}
```

## Server Implementation

```cpp
void sendGameUpdateB(Session::Ptr session, int entityId, int v1, int v2) {
    Packet pkt(0x5C);
    pkt.writeInt32(entityId);
    pkt.writeInt32(v1);
    pkt.writeInt32(v2);
    session->send(pkt);
}
```

## Notes

- Entity ID is validated via sub_48DEA0 before processing
- If validation fails (returns < 0), update is ignored
- Related to in-game state synchronization

