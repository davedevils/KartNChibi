# 0x104 - ACCESSORY_LIST

**CMD**: `0x104` (260 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47E3A0`  
**Handler Ghidra**: `FUN_0047e3a0`

## Description

Sends a list of accessory data. Contains a count followed by 28-byte accessory entries.

## Payload Structure

| Offset | Type   | Size   | Description           |
|--------|--------|--------|-----------------------|
| 0x00   | int32  | 4      | Entry count           |
| 0x04+  | entries| 28 × n | AccessoryData entries |

**Total Size**: 4 + (28 × entryCount) bytes

## C Structure

```c
struct AccessoryListPacket {
    int32_t count;              // +0x00 - Number of entries
    AccessoryData entries[];    // +0x04 - 28 bytes each
};
```

## Handler Logic (IDA)

```c
// sub_47E3A0
int __thiscall sub_47E3A0(void *this, int a2)
{
    int count;
    int v6[7];  // 28 bytes
    
    int *v2 = (int*)((char*)&unk_857258 + this);
    sub_451510(v2);  // Clear
    sub_44E910(a2, &count, 4);
    
    for (int i = 0; i < count; ++i) {
        sub_44E910(a2, v6, 0x1C);  // Read 28 bytes
        sub_451530(v2, v6);        // Add entry
    }
    
    return count;
}
```

## Cross-Validation

| Source | Function       | Payload Read     |
|--------|----------------|------------------|
| IDA    | sub_47E3A0     | 4 + 28*n bytes   |
| Ghidra | FUN_0047e3a0   | 4 + 28*n bytes   |

**Status**: ✅ CERTIFIED

