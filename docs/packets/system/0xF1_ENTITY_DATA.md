# 0xF1 - ENTITY_DATA

**CMD**: `0xF1` (241 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47D970`  
**Handler Ghidra**: `FUN_0047d970`

## Description

Sends entity data block (36 bytes) for entity initialization or update.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | bytes | 36   | Entity data (0x24)    |

**Total Size**: 36 bytes (0x24)

## C Structure

```c
struct EntityDataPacket {
    int32_t data[9];            // +0x00 - 36 bytes of entity data
};
```

## Handler Logic (IDA)

```c
// sub_47D970
void __stdcall sub_47D970(int a1)
{
    int v1[9];  // 36 bytes
    
    sub_44E910(a1, v1, 0x24);   // Read 36 bytes
    sub_451C10(v1);             // Process entity data
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47D970     | 36 bytes     |
| Ghidra | FUN_0047d970   | 36 bytes     |

**Status**: ✅ CERTIFIED

