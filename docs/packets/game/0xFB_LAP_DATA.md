# 0xFB - LAP_DATA

**CMD**: `0xFB` (251 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47DB60`  
**Handler Ghidra**: `FUN_0047db60`

## Description

Sends lap data block. Contains 112 bytes of lap/race information.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | bytes | 112  | Lap data (0x70)       |

**Total Size**: 112 bytes (0x70)

## C Structure

```c
struct LapDataPacket {
    uint8_t lapData[112];       // +0x00 - Lap data
};
```

## Handler Logic (IDA)

```c
// sub_47DB60
int __thiscall sub_47DB60(void *this, int a2)
{
    int v4[29];  // 116 bytes buffer
    
    sub_44E910(a2, v4, 0x70);  // Read 112 bytes
    return sub_452180((int*)((char*)&unk_854288 + this), v4);
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47DB60     | 112 bytes    |
| Ghidra | FUN_0047db60   | 112 bytes    |

**Status**: ✅ CERTIFIED

