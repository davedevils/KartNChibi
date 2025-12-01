# 0x76 INVENTORY_LIST (Server → Client)

**Handler:** `sub_47B1B0`

## Purpose

Sends a list of vehicles.

## Payload Structure

```c
struct InventoryList {
    int32 count;
    VehicleData vehicles[count];  // 0x2C each
};
```

## Size

**4 + 44N bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Number of vehicles |
| 0x04 | VehicleData[] | 44×N | Vehicle array |

## Notes

- Same handler as 0x1B
- Used for full inventory refresh

