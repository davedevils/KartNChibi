# 0x109 - LARGE_DATA

**CMD**: `0x109` (265 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47E4C0`  
**Handler Ghidra**: `FUN_0047e4c0`

## Description

Sends a large data block (132 bytes). Purpose specific to game state.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | bytes | 132  | Data block (0x84)     |

**Total Size**: 132 bytes (0x84)

## C Structure

```c
struct LargeDataPacket {
    uint8_t data[132];          // +0x00 - Data block
};
```

## Handler Logic (IDA)

```c
// sub_47E4C0
int __thiscall sub_47E4C0(void *this, int a2)
{
    int v4[33];  // 132 bytes
    
    sub_44E910(a2, v4, 0x84);  // Read 132 bytes
    return sub_44F760((int*)((char*)&unk_878E84 + this), v4);
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47E4C0     | 132 bytes    |
| Ghidra | FUN_0047e4c0   | 132 bytes    |

**Status**: ✅ CERTIFIED

