# 0x77 UPDATE_LIST (Server → Client)

**Handler:** `sub_47B340`

## Purpose

Updates specific inventory items.

## Payload Structure

```c
struct UpdateList {
    int32 count;
    struct {
        int32 itemId;
        int32 value;
    } updates[count];
};
```

## Size

**4 + 8N bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Number of updates |
| 0x04 | int32 | 4 | Item ID 1 |
| 0x08 | int32 | 4 | New value 1 |
| ... | ... | ... | ... |

## Notes

- Partial inventory update
- More efficient than full list

