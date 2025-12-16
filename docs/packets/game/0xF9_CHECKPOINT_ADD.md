# 0xF9 - CHECKPOINT_ADD

**CMD**: `0xF9` (249 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47E350`  
**Handler Ghidra**: `FUN_0047e350`

## Description

Adds a single checkpoint entry and sets a confirmation flag with timestamp.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Checkpoint ID         |
| 0x04   | int32 | 4    | Checkpoint value      |

**Total Size**: 8 bytes

## C Structure

```c
struct CheckpointAddPacket {
    int32_t checkpointId;       // +0x00
    int32_t value;              // +0x04
};
```

## Handler Logic (IDA)

```c
// sub_47E350
LONGLONG __thiscall sub_47E350(void *this, int a2)
{
    int v4[2];  // 8 bytes
    
    sub_44E910(a2, v4, 8);
    sub_452C40((int*)((char*)&unk_8540F0 + this), v4);  // Add checkpoint
    
    byte_8CCDD0[this] = 1;  // Set flag
    
    // Get timestamp
    LONGLONG result = sub_44ED50(&off_5E1B00);
    *(int*)((char*)&dword_8CCDD8 + this) = (int)result;
    *(int*)((char*)&dword_8CCDDC + this) = (int)(result >> 32);
    
    return result;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47E350     | 8 bytes      |
| Ghidra | FUN_0047e350   | 8 bytes      |

**Status**: ✅ CERTIFIED

