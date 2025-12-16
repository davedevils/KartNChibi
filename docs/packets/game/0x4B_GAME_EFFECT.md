# 0x4B GAME_EFFECT (Server → Client)

**Packet ID:** 0x4B (75 decimal)  
**Handler:** `sub_47A460` / `FUN_0047a460`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Triggers game effects based on type (visual effects, sounds, etc.).

## Payload Structure

```c
struct GameEffect {
    int32 entityId;     // Target entity
    int32 effectType;   // 10 or 16 determines effect category
    int32 param1;       // Effect parameter 1
    int32 param2;       // Effect parameter 2
};
```

## Size

**16 bytes** (4 × int32)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Entity ID (validated) |
| 0x04 | int32 | 4 | Effect type (10 or 16) |
| 0x08 | int32 | 4 | Parameter 1 |
| 0x0C | int32 | 4 | Parameter 2 |

## Effect Types

| Type | Handler | Description |
|------|---------|-------------|
| 10 (0x0A) | sub_4CA1A0 | Effect type A |
| 16 (0x10) | sub_4C6910 | Effect type B |

## Handler Logic

```c
// IDA: sub_47A460 / Ghidra: FUN_0047a460
read(&entityId, 4);
read(&effectType, 4);
read(&param1, 4);
read(&param2, 4);

if (validateEntity(entityId) >= 0) {
    if (effectType == 10) {
        triggerEffectA(param1, param2);
    } else if (effectType == 16) {
        triggerEffectB(param1, param2);
    }
}
```

## Server Implementation

```cpp
void sendGameEffect(Session::Ptr session, int entityId, int type, int p1, int p2) {
    Packet pkt(0x4B);
    pkt.writeInt32(entityId);
    pkt.writeInt32(type);  // 10 or 16
    pkt.writeInt32(p1);
    pkt.writeInt32(p2);
    session->send(pkt);
}
```

## Notes

- Entity ID is validated before processing
- Only effect types 10 and 16 are handled
- Other effect types are silently ignored

