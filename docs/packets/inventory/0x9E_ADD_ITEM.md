# 0x9E - ADD_ITEM

**CMD**: `0x9E` (158 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47C320`  
**Handler Ghidra**: `FUN_0047c320`

## Description

Adds a new item to the player's inventory. Contains a full ItemData structure (56 bytes).

## Payload Structure

| Offset | Type     | Size | Description           |
|--------|----------|------|-----------------------|
| 0x00   | ItemData | 56   | Item data             |

**Total Size**: 56 bytes (0x38)

## C Structure

```c
struct AddItemPacket {
    ItemData item;              // +0x00 - Full item data (56 bytes)
};
```

## Handler Logic (IDA)

```c
// sub_47C320
char __thiscall sub_47C320(void *this, int a2)
{
    int v5[14];  // ItemData buffer (56 bytes)
    
    sub_44E910(a2, v5, 0x38);   // Read 56 bytes
    
    int result = sub_44F2D0((int*)((char*)&unk_845228 + this), v5);
    if (result < 0)
        sub_4641E0(byte_F727E8, "MSG_UNKNOWN_ERROR", 2);
    
    return result;
}
```

## Notes

- If addition fails (result < 0), displays "MSG_UNKNOWN_ERROR"
- Item is added to internal list at offset `unk_845228`

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47C320     | 56 bytes     |
| Ghidra | FUN_0047c320   | 56 bytes     |

**Status**: ✅ CERTIFIED
