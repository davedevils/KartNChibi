# 0x2A - WHISPER_ENABLE

**CMD**: `0x2A` (42 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_479B60`  
**Handler Ghidra**: `FUN_00479b60`

## Description

Enables whisper mode. Sets a global flag and prepares the chat input for whisper messages. **No payload is read from the packet.**

## Payload Structure

**No payload** - This packet has no data beyond the header.

**Total Size**: 0 bytes

## Handler Logic (IDA)

```c
// sub_479B60
char __stdcall sub_479B60(int a1)
{
    // NOTE: Does NOT read any data from packet!
    dword_B23390 = 1;           // Enable whisper flag
    sub_4465E0(dword_E1EF70);   // UI update
    
    // Prepare whisper prefix in chat
    if (wcslen(&Format))
        swprintf(Buffer, L"/w %s ", &Format);
    else
        wcscpy(Buffer, L"/w ");
    
    return sub_4538B0();
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_479B60     | 0 bytes      |
| Ghidra | FUN_00479b60   | 0 bytes      |

**Status**: ✅ CERTIFIED
