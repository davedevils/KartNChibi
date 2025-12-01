# 0x7B INV_SLOT (Server → Client)

**Handler:** `sub_47B670`

## Purpose

Updates a specific inventory slot.

## Payload Structure

```c
struct InvSlot {
    int32 slotId;
    int32 itemId;
};
```

## Size

**8 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Slot ID |
| 0x04 | int32 | 4 | Item ID |

## Notes

- Updates single slot
- Used for quick updates

