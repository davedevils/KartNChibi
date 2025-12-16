# 0x9F - ADD_ACCESSORY

**CMD**: `0x9F` (159 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47C370`  
**Handler Ghidra**: `FUN_0047c370`

## Description

Adds a new accessory to the player's inventory. Contains a full AccessoryData structure (28 bytes).

## Payload Structure

| Offset | Type          | Size | Description           |
|--------|---------------|------|-----------------------|
| 0x00   | AccessoryData | 28   | Accessory data        |

**Total Size**: 28 bytes (0x1C)

## C Structure

```c
struct AddAccessoryPacket {
    AccessoryData accessory;    // +0x00 - Full accessory data (28 bytes)
};
```

## Handler Logic (IDA)

```c
// sub_47C370
char __thiscall sub_47C370(void *this, int a2)
{
    int v5[7];  // AccessoryData buffer (28 bytes)
    
    sub_44E910(a2, v5, 0x1C);   // Read 28 bytes
    
    int result = sub_450D20((int*)((char*)&unk_846030 + this), v5);
    if (result < 0)
        sub_4641E0(byte_F727E8, "MSG_UNKNOWN_ERROR", 2);
    
    return result;
}
```

## Notes

- If addition fails (result < 0), displays "MSG_UNKNOWN_ERROR"
- Accessory is added to internal list at offset `unk_846030`

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47C370     | 28 bytes     |
| Ghidra | FUN_0047c370   | 28 bytes     |

**Status**: ✅ CERTIFIED
