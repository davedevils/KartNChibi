# 0x102 - POSITION_UPDATE_B

**CMD**: `0x102` (258 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47F340`  
**Handler Ghidra**: `FUN_0047f340`

## Description

Updates a position entry value (without changing state).

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Entity/Player ID      |
| 0x04   | int32 | 4    | Value                 |

**Total Size**: 8 bytes

## C Structure

```c
struct PositionUpdateBPacket {
    int32_t entityId;           // +0x00 - Entity ID
    int32_t value;              // +0x04 - Value to set
};
```

## Handler Logic (IDA)

```c
// sub_47F340
int __thiscall sub_47F340(void *this, int a2)
{
    int entityId, value;
    
    sub_44E910(a2, &entityId, 4);
    sub_44E910(a2, &value, 4);
    
    int result = sub_452110((char*)&unk_855870 + this, entityId);
    *(int*)(result + 8) = value;  // Set value at offset +8
    
    return result;
}
```

## Notes

- Similar to 0x101 but does NOT change the state field
- Only updates the value at offset +8

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47F340     | 8 bytes      |
| Ghidra | FUN_0047f340   | 8 bytes      |

**Status**: ✅ CERTIFIED

