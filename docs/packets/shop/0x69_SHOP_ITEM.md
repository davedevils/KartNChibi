# 0x69 SHOP_ITEM (Server → Client)

**Handler:** `sub_47AF00`

## Purpose

Sends individual shop item data.

## Payload Structure

```c
struct ShopItem {
    int32 itemId;
    int16 itemType;
    int8  flags;
};
```

## Size

**7 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Item ID |
| 0x04 | int16 | 2 | Item type |
| 0x06 | int8 | 1 | Flags (availability, etc) |

## Notes

- Compact packet for shop browsing
- Sent for each item in shop list

