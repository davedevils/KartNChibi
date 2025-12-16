# 0x100 - POSITION_REMOVE

**CMD**: `0x100` (256 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47F2C0`  
**Handler Ghidra**: `FUN_0047f2c0`

## Description

Removes a position entry by ID.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Entity/Player ID      |

**Total Size**: 4 bytes

## C Structure

```c
struct PositionRemovePacket {
    int32_t entityId;           // +0x00 - Entity ID to remove
};
```

## Handler Logic (IDA)

```c
// sub_47F2C0
char __thiscall sub_47F2C0(void *this, int a2)
{
    int entityId;
    
    sub_44E910(a2, &entityId, 4);
    return sub_4520D0((char*)&unk_855870 + this, entityId);
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47F2C0     | 4 bytes      |
| Ghidra | FUN_0047f2c0   | 4 bytes      |

**Status**: ✅ CERTIFIED

