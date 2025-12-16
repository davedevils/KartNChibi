# 0x2B - WHISPER_DISABLE

**CMD**: `0x2B` (43 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_479BD0`  
**Handler Ghidra**: `FUN_00479bd0`

## Description

Disables whisper mode. Sets the whisper flag to 0. **No payload is read from the packet.**

## Payload Structure

**No payload** - This packet has no data beyond the header.

**Total Size**: 0 bytes

## Handler Logic (IDA)

```c
// sub_479BD0
void __stdcall sub_479BD0(int a1)
{
    // NOTE: Does NOT read any data from packet!
    dword_B23390 = 0;  // Disable whisper flag
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_479BD0     | 0 bytes      |
| Ghidra | FUN_00479bd0   | 0 bytes      |

**Status**: ✅ CERTIFIED
