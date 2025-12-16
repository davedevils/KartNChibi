# 0x5F ENTITY_STATE (Server → Client)

**Packet ID:** 0x5F (95 decimal)  
**Handler:** `sub_47EE70` / `FUN_0047ee70`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Updates entity state value (likely visual effects or timer values).

## Payload Structure

```c
struct EntityState {
    int32 entityId;    // Entity identifier
    int32 stateType;   // 7 or 13 (0x0D)
};
```

## Size

**8 bytes** (2 × int32)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Entity ID |
| 0x04 | int32 | 4 | State type (7 or 13) |

## State Types

| Type | Array Base | Entry Size | Description |
|------|------------|------------|-------------|
| 7 | 0x2EFC818 | 592 (0x250) | Effect array A (sets offset +0x3C to 200) |
| 13 | 0x2ECDA38 | 592 (0x250) | Effect array B (sets offset +0x3C to 200) |

## Handler Logic

```c
// IDA: sub_47EE70 / Ghidra: FUN_0047ee70
read(&entityId, 4);
read(&stateType, 4);

if (stateType == 7) {
    int idx = lookupEntity(entityId);
    if (idx >= 0) {
        effectArrayA[idx].field_3C = 200;
    }
} else if (stateType == 13) {
    int idx = lookupEntity(entityId);
    if (idx >= 0) {
        effectArrayB[idx].field_3C = 200;
    }
}
```

## Server Implementation

```cpp
void sendEntityState(Session::Ptr session, int entityId, int stateType) {
    Packet pkt(0x5F);
    pkt.writeInt32(entityId);
    pkt.writeInt32(stateType);  // 7 or 13
    session->send(pkt);
}
```

## Notes

- Only state types 7 and 13 are handled
- Sets a specific value (200) in entity data structure
- Value 200 may be a duration/timer in ticks or ms
- Entity ID validated via lookup function

