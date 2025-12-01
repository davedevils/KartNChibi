# CMD 0x79 (121) - Item List Type B

**Direction:** Server → Client  
**Handler:** `sub_47B290`

## Structure

```c
struct ItemListTypeB {
    int32 count;
    ItemEntryB entries[count];
};

struct ItemEntryB {
    uint8 data[0x20];     // 32 bytes per item
};
```

## Fields per Item (0x20 = 32 bytes)

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | itemId |
| 0x04 | 4 | int32 | uniqueId |
| 0x08 | 4 | int32 | quantity |
| 0x0C | 4 | int32 | unknown1 |
| 0x10 | 4 | int32 | unknown2 |
| 0x14 | 4 | int32 | unknown3 |
| 0x18 | 4 | int32 | unknown4 |
| 0x1C | 4 | int32 | unknown5 |

## Notes

- 32 bytes per item (smaller than 44-byte items)
- Different category of items

