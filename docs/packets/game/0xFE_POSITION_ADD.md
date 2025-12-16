# 0xFE - POSITION_ADD

**CMD**: `0xFE` (254 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47F270`  
**Handler Ghidra**: `FUN_0047f270`

## Description

Adds a position entry with default values. Creates an entry with state=1 and value=0.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Entity/Player ID      |

**Total Size**: 4 bytes

## C Structure

```c
struct PositionAddPacket {
    int32_t entityId;           // +0x00 - Entity ID
};
```

## Handler Logic (IDA)

```c
// sub_47F270
int __thiscall sub_47F270(void *this, int a2)
{
    int v4[3];  // 12-byte entry
    
    sub_44E910(a2, &a2, 4);  // Read ID
    
    v4[0] = a2;   // Entity ID
    v4[1] = 1;    // State = 1
    v4[2] = 0;    // Value = 0
    
    return sub_451FB0((int*)((char*)&unk_855870 + this), v4);
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47F270     | 4 bytes      |
| Ghidra | FUN_0047f270   | 4 bytes      |

**Status**: ✅ CERTIFIED

