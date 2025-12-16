# 0xF0 - ENTITY_REMOVE

**CMD**: `0xF0` (240 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47D930`  
**Handler Ghidra**: `FUN_0047d930`

## Description

Removes an entity from the game world by ID.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Entity ID             |

**Total Size**: 4 bytes

## C Structure

```c
struct EntityRemovePacket {
    int32_t entityId;           // +0x00 - Entity ID to remove
};
```

## Handler Logic (IDA)

```c
// sub_47D930
int __stdcall sub_47D930(int a1)
{
    int entityId;
    
    sub_44E910(a1, &entityId, 4);  // Read entity ID
    
    int result = sub_48DEA0(byte_1B19090, entityId);  // Find entity
    if (result >= 0)
        return sub_498F40(byte_1B19090, result);  // Remove entity
        
    return result;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47D930     | 4 bytes      |
| Ghidra | FUN_0047d930   | 4 bytes      |

**Status**: ✅ CERTIFIED

