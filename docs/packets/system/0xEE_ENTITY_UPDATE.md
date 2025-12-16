# 0xEE - ENTITY_UPDATE

**CMD**: `0xEE` (238 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47D880`  
**Handler Ghidra**: `FUN_0047d880`

## Description

Updates an entity in the game world with position or state data.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Entity ID             |
| 0x04   | bytes | 16   | Entity data (0x10)    |

**Total Size**: 20 bytes

## C Structure

```c
struct EntityUpdatePacket {
    int32_t entityId;           // +0x00 - Entity ID
    int32_t data[4];            // +0x04 - 16 bytes of data
};

// Entity data breakdown (speculative)
struct EntityData {
    int32_t x;                  // +0x00
    int32_t y;                  // +0x04
    int32_t z;                  // +0x08
    int32_t state;              // +0x0C
};
```

## Handler Logic (IDA)

```c
// sub_47D880
int *__stdcall sub_47D880(int a1)
{
    int entityId;
    int v5[4];  // 16 bytes
    
    sub_44E910(a1, &entityId, 4);
    
    int result = sub_48DEA0(byte_1B19090, entityId);  // Find entity
    
    if (result >= 0) {
        sub_44E910(a1, v5, 0x10);  // Read 16 bytes
        
        int* entity = sub_451D30(&unk_1B1C51C + 171160 * result, 0);
        if (entity) {
            entity[0] = v5[0];
            entity[1] = v5[1];
            entity[2] = v5[2];
            entity[3] = v5[3];
        }
    }
    
    return result;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47D880     | 20 bytes     |
| Ghidra | FUN_0047d880   | 20 bytes     |

**Status**: ✅ CERTIFIED

