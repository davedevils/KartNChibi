# 0xF6 - RACE_INIT

**CMD**: `0xF6` (246 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47DC70`  
**Handler Ghidra**: `FUN_0047dc70`

## Description

Initializes race state. Sets race data pointer and resets counter.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Race value            |

**Total Size**: 4 bytes

## C Structure

```c
struct RaceInitPacket {
    int32_t raceValue;          // +0x00 - Race initialization value
};
```

## Handler Logic (IDA)

```c
// sub_47DC70
int __stdcall sub_47DC70(int a1)
{
    int v1 = 171160 * dword_C7086C;  // Player slot index
    
    // Read 4 bytes into race data array
    int result = sub_44E910(a1, &dword_1BC08E8[v1], 4);
    
    dword_1BC08EC[v1] = 0;  // Reset counter
    
    return result;
}
```

## Notes

- Uses player slot index (`dword_C7086C`) multiplied by 171160 (0x29CF8) for offset calculation
- Resets a counter at offset +4 from the data pointer

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47DC70     | 4 bytes      |
| Ghidra | FUN_0047dc70   | 4 bytes      |

**Status**: ✅ CERTIFIED

