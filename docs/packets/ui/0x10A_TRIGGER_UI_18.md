# 0x10A - TRIGGER_UI_18

**CMD**: `0x10A` (266 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47E500`  
**Handler Ghidra**: `FUN_0047e500`

## Description

Triggers UI state 18. **No payload** - this is a control packet.

## Payload Structure

**No payload** (0 bytes)

## C Structure

```c
// No payload structure - control packet only
```

## Handler Logic (IDA)

```c
// sub_47E500
char __thiscall sub_47E500(void *this, int a2)
{
    // Guard: only execute if dword_F727F4 != 2
    if (dword_F727F4 != 2)
    {
        sub_484010(this);
        sub_404410(dword_B23288, 18);  // Set UI state 18
        return sub_4538B0();           // Clear state
    }
    return result;
}
```

## Notes

- This packet has **NO payload**
- Triggers UI state 18
- Guarded by `dword_F727F4 != 2` condition
- References the critical `dword_F727F4` state variable

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47E500     | 0 bytes      |
| Ghidra | FUN_0047e500   | 0 bytes      |

**Status**: ✅ CERTIFIED

