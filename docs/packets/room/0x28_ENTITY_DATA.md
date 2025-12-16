# 0x28 - ENTITY_DATA

**CMD**: `0x28` (40 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_479B20`  
**Handler Ghidra**: `FUN_00479b20`

## Description

Contains entity/object data for the room. Fixed-size packet of 104 bytes.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | bytes  | 104  | Entity data block     |

**Total Size**: 104 bytes (0x68)

## C Structure

```c
struct EntityDataPacket {
    uint8_t data[104];          // +0x00 - Raw entity data
};
```

## Handler Logic (IDA)

```c
// sub_479B20
char __stdcall sub_479B20(int a1)
{
    int v2[27];  // 104 bytes buffer (27 ints * 4 - stack guard)
    
    sub_44E910(a1, v2, 0x68);   // Read 104 bytes
    return sub_45AAF0(&unk_F17A50, v2);  // Process entity data
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_479B20     | 104 bytes    |
| Ghidra | FUN_00479b20   | 104 bytes    |

**Status**: ✅ CERTIFIED
