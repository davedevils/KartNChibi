# 0x7A - ITEM_ADD

**CMD**: `0x7A` (122 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B4F0`  
**Handler Ghidra**: `FUN_0047b4f0`

## Description

Adds a new item to the player's inventory. Contains a result code followed by SmallItem data if successful.

## Payload Structure

| Offset | Type      | Size   | Description                |
|--------|-----------|--------|----------------------------|
| 0x00   | int32     | 4      | Result code (0 = success)  |
| 0x04   | SmallItem | 32     | Item data (only if result == 0) |

**Total Size**: 4 bytes (error) or 36 bytes (success)

## C Structure

```c
struct ItemAddPacket {
    int32_t result;             // +0x00 - Result code
    // Only present if result == 0:
    SmallItem item;             // +0x04 - 32 bytes (0x20)
};
```

## Handler Logic (IDA)

```c
// sub_47B4F0
int __thiscall sub_47B4F0(void *this, int a2)
{
    int result;
    int v5[9];  // SmallItem buffer (32 bytes)
    
    sub_44E910(a2, &result, 4);
    
    if (result == 0) {
        sub_44E910(a2, v5, 0x20);   // Read 32 bytes
        return sub_44F150((int*)((char*)&unk_84A598 + this), v5);
    }
    
    return result;
}
```

## Cross-Validation

| Source | Function       | Payload Read        |
|--------|----------------|---------------------|
| IDA    | sub_47B4F0     | 4 + (32 if success) |
| Ghidra | FUN_0047b4f0   | 4 + (32 if success) |

**Status**: ✅ CERTIFIED
