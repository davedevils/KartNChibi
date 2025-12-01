# 0xB8 REMOVE_ITEM (Server → Client)

**Handler:** `sub_484690`

## Purpose

Removes an item from the player's inventory by type and ID.

## Payload Structure

```c
struct RemoveItem {
    int32 itemType;
    int32 itemId;
};
```

## Size

**8 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Item type |
| 0x04 | int32 | 4 | Item ID to remove |

## Item Types

| Type | Description | Handler |
|------|-------------|---------|
| 0 | Vehicle | `sub_44FDE0` |
| 1 | Item | `sub_44F400` |
| 2 | Accessory | `sub_451640` |
| 3 | Equipped | `sub_450E30` |
| 5 | Other | `sub_452760` |

## Notes

- Used when items expire, break, or are sold
- Type determines which inventory list to modify

