# 0x45 ITEM_USAGE (Server → Client)

**Packet ID:** 0x45 (69 decimal)  
**Handler:** `sub_47A560` / `FUN_0047a560`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Notifies client about item usage in game.

## Payload Structure

```c
struct ItemUsage {
    int32 playerId;     // Player using item
    int32 itemType;     // Type of item used
    int32 targetId;     // Target entity (if applicable)
};
```

## Size

**12 bytes** (3 × int32)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Player ID |
| 0x04 | int32 | 4 | Item type |
| 0x08 | int32 | 4 | Target ID |

## Handler Logic

```c
// IDA: sub_47A560 / Ghidra: FUN_0047a560
read(&playerId, 4);
read(&itemType, 4);
read(&targetId, 4);
processItemUsage(playerId, itemType, targetId);
```

## Server Implementation

```cpp
void sendItemUsage(Session::Ptr session, int playerId, int itemType, int targetId) {
    Packet pkt(0x45);
    pkt.writeInt32(playerId);
    pkt.writeInt32(itemType);
    pkt.writeInt32(targetId);
    session->send(pkt);
}
```

## Notes

- Used for in-race item effects (missiles, shields, etc.)
- targetId may be -1 if no specific target
- Part of game synchronization system
