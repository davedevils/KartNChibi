# 0xF3 - TRACK_DATA

**CMD**: `0xF3` (243 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47DAA0`  
**Handler Ghidra**: `FUN_0047daa0`

## Description

Sends track/map data block. Contains 156 bytes of track information.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | bytes | 156  | Track data (0x9C)     |

**Total Size**: 156 bytes (0x9C)

## C Structure

```c
struct TrackDataPacket {
    uint8_t trackData[156];     // +0x00 - Track data
};
```

## Handler Logic (IDA)

```c
// sub_47DAA0
int __thiscall sub_47DAA0(void *this, int a2)
{
    int v4[40];  // 160 bytes buffer
    
    sub_44E910(a2, v4, 0x9C);  // Read 156 bytes
    return sub_452DA0((int*)((char*)&unk_852270 + this), v4);
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47DAA0     | 156 bytes    |
| Ghidra | FUN_0047daa0   | 156 bytes    |

**Status**: ✅ CERTIFIED

