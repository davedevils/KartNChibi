# 0xCD - AUDIO_CONTROL

**CMD**: `0xCD` (205 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47D1C0`  
**Handler Ghidra**: `FUN_0047d1c0`

## Description

Controls audio playback. Interacts with the audio system to play or modify sounds.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Audio ID              |
| 0x04   | int32 | 4    | Value / Volume        |

**Total Size**: 8 bytes

## C Structure

```c
struct AudioControlPacket {
    int32_t audioId;            // +0x00 - Audio ID
    int32_t value;              // +0x04 - Value (volume/state)
};
```

## Handler Logic (IDA)

```c
// sub_47D1C0
int __stdcall sub_47D1C0(int a1)
{
    int v4;  // audioId
    int a1_copy;  // value
    
    sub_44E910(a1, &v4, 4);      // Read audio ID
    sub_44E910(a1, &a1_copy, 4); // Read value
    
    int result = sub_48DEA0(byte_1B19090, v4);  // Find audio
    
    if (result < 0) {
        dword_5F1934 = a1_copy;  // Store value
    } else {
        result = sub_4CB160(dword_2F00B98, result, 0);
        if (result >= 0) {
            sub_4CACE0(dword_2F00B98, result, 0);
        }
        dword_5F1934 = a1_copy;
    }
    
    return result;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47D1C0     | 8 bytes      |
| Ghidra | FUN_0047d1c0   | 8 bytes      |

**Status**: ✅ CERTIFIED

