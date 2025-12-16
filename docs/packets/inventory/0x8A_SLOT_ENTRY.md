# 0x8A - SLOT_ENTRY

**CMD**: `0x8A` (138 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B990`  
**Handler Ghidra**: `FUN_0047b990`

## Description

Adds a single slot entry and sets a flag. Contains an 8-byte entry.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Slot ID               |
| 0x04   | int32  | 4    | Value                 |

**Total Size**: 8 bytes

## C Structure

```c
struct SlotEntryPacket {
    int32_t slotId;             // +0x00 - Slot ID
    int32_t value;              // +0x04 - Value
};
```

## Handler Logic (IDA)

```c
// sub_47B990
LONGLONG __thiscall sub_47B990(void *this, int a2)
{
    int v4[2];  // 8 bytes
    
    sub_44E910(a2, v4, 8);
    sub_450AB0((int*)((char*)&unk_839E70 + this), v4);  // Add entry
    
    byte_8CCDD0[this] = 1;  // Set flag
    
    // Get timestamp
    LONGLONG result = sub_44ED50(&off_5E1B00);
    *(int*)((char*)&dword_8CCDD8 + this) = (int)result;
    *(int*)((char*)&dword_8CCDDC + this) = (int)(result >> 32);
    
    return result;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47B990     | 8 bytes      |
| Ghidra | FUN_0047b990   | 8 bytes      |

**Status**: ✅ CERTIFIED

