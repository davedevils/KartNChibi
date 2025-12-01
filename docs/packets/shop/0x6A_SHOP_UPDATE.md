# 0x6A SHOP_UPDATE (Server → Client)

**Handler:** `sub_47AFE0`

## Purpose

Updates shop state or item availability.

## Payload Structure

```c
struct ShopUpdate {
    int16 updateType;
    int32 value;
};
```

## Size

**6 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int16 | 2 | Update type |
| 0x02 | int32 | 4 | Value |

## Notes

- Used to refresh shop display
- Can update prices, availability, etc.

