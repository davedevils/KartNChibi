# 0x114 - STRING_UPDATE

**CMD**: `0x114` (276 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47E620`  
**Handler Ghidra**: `FUN_0047e620`

## Description

Updates a string value by ID.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | ID                    |
| 0x04   | string | var  | String value          |

**Total Size**: 4 + string bytes

## C Structure

```c
struct StringUpdatePacket {
    int32_t id;                 // +0x00
    // string value;            // Variable length
};
```

## Handler Logic (IDA)

```c
// sub_47E620
char __stdcall sub_47E620(int a1)
{
    char v5[12];
    
    sub_44E910(a1, &a1, 4);      // Read ID
    sub_44EB30(v1, v5);          // Read string
    
    // Copy to destination at offset +8
    char* dest = sub_4501D0(dword_1A79220, a1) + 8;
    // ... string copy
}
```

## Cross-Validation

| Source | Function       | Payload Read     |
|--------|----------------|------------------|
| IDA    | sub_47E620     | 4 + string bytes |
| Ghidra | FUN_0047e620   | 4 + string bytes |

**Status**: ✅ CERTIFIED

