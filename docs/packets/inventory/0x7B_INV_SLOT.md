# 0x7B - INV_SLOT

**CMD**: `0x7B` (123 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B670`  
**Handler Ghidra**: `FUN_0047b670`

## Description

Updates an inventory slot. Contains an item ID and a slot/value parameter.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Item ID               |
| 0x04   | int32  | 4    | Slot / Value          |

**Total Size**: 8 bytes

## C Structure

```c
struct InvSlotPacket {
    int32_t itemId;             // +0x00 - Item ID
    int32_t slot;               // +0x04 - Slot or value
};
```

## Handler Logic (IDA)

```c
// sub_47B670
char __thiscall sub_47B670(void *this, int a2)
{
    int itemId, slot;
    
    sub_44E910(a2, &itemId, 4);
    sub_44E910(a2, &slot, 4);
    
    return sub_44F260((char*)&unk_84A598 + this, itemId);
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47B670     | 8 bytes      |
| Ghidra | FUN_0047b670   | 8 bytes      |

**Status**: ✅ CERTIFIED
