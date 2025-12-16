# 0x101 - POSITION_UPDATE_A

**CMD**: `0x101` (257 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47F2F0`  
**Handler Ghidra**: `FUN_0047f2f0`

## Description

Updates a position entry with state=2 and a value.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Entity/Player ID      |
| 0x04   | int32 | 4    | Value                 |

**Total Size**: 8 bytes

## C Structure

```c
struct PositionUpdateAPacket {
    int32_t entityId;           // +0x00 - Entity ID
    int32_t value;              // +0x04 - Value to set
};
```

## Handler Logic (IDA)

```c
// sub_47F2F0
int __thiscall sub_47F2F0(void *this, int a2)
{
    int entityId, value;
    
    sub_44E910(a2, &entityId, 4);
    sub_44E910(a2, &value, 4);
    
    int result = sub_452110((char*)&unk_855870 + this, entityId);
    *(int*)(result + 8) = value;  // Set value at offset +8
    *(int*)(result + 4) = 2;      // Set state to 2
    
    return result;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47F2F0     | 8 bytes      |
| Ghidra | FUN_0047f2f0   | 8 bytes      |

**Status**: ✅ CERTIFIED

