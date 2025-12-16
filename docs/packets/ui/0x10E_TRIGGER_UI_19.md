# 0x10E - TRIGGER_UI_19

**CMD**: `0x10E` (270 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47D9D0`  
**Handler Ghidra**: `FUN_0047d9d0`

## Description

Triggers UI state 19. **No payload** - this is a control packet.

## Payload Structure

**No payload** (0 bytes)

## C Structure

```c
// No payload structure - control packet only
```

## Handler Logic (IDA)

```c
// sub_47D9D0
char __stdcall sub_47D9D0(int a1)
{
    // Guard: only execute if dword_F727F4 != 2
    if (dword_F727F4 != 2)
    {
        sub_404410(dword_B23288, 19);  // Set UI state 19
        return sub_4538B0();           // Clear state
    }
    return result;
}
```

## Notes

- **NO payload**
- Triggers UI state 19
- Guarded by `dword_F727F4 != 2` condition

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47D9D0     | 0 bytes      |
| Ghidra | FUN_0047d9d0   | 0 bytes      |

**Status**: ✅ CERTIFIED

