# 0x45 ITEM_USAGE (Server → Client)

**Handler:** `sub_47A560`

## Purpose

Notifies about item usage in race.

## Payload Structure

```c
struct ItemUsage {
    int32 playerId;
    int32 itemId;
    int32 targetId;
};
```

## Size

**12 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Player who used item |
| 0x04 | int32 | 4 | Item ID used |
| 0x08 | int32 | 4 | Target (if applicable) |

## Notes

- Used for item effects in race
- Calls `sub_4B4B00` with parameters

