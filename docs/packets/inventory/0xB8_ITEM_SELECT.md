# 0xB8 - ITEM_SELECT

**CMD**: `0xB8` (184 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_484690`  
**Handler Ghidra**: `FUN_00484690`

## Description

Selects an item by type and ID. The type determines which inventory list to update.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Item type (0-5)       |
| 0x04   | int32 | 4    | Item ID               |

**Total Size**: 8 bytes

## C Structure

```c
struct ItemSelectPacket {
    int32_t itemType;           // +0x00 - Item type selector
    int32_t itemId;             // +0x04 - Item ID to select
};
```

## Item Types

| Type | Description            | Function Called |
|------|------------------------|-----------------|
| 0    | Vehicle                | sub_44FDE0      |
| 1    | Item                   | sub_44F400      |
| 2    | Accessory type A       | sub_451640      |
| 3    | Accessory type B       | sub_450E30      |
| 5    | Unknown type           | sub_452760      |

## Handler Logic (IDA)

```c
// sub_484690
char __thiscall sub_484690(void *this, int a2)
{
    int v4, v5;  // type, id
    
    sub_4538B0();  // Clear state
    sub_44E910(a2, &v5, 4);  // Read type
    sub_44E910(a2, &v4, 4);  // Read id
    
    switch (v5) {
        case 0: return sub_44FDE0((char*)&unk_844720 + this, v4);
        case 1: return sub_44F400((char*)&unk_845228 + this, v4);
        case 2: return sub_451640((char*)&unk_847C38 + this, v4);
        case 3: return sub_450E30((int*)((char*)&unk_846030 + this), v4);
        case 5: return sub_452760((char*)&unk_863D68 + this, v4);
    }
    return v5;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_484690     | 8 bytes      |
| Ghidra | FUN_00484690   | 8 bytes      |

**Status**: ✅ CERTIFIED

