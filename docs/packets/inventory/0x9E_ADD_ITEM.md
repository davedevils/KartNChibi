# 0x9E ADD_ITEM (Server → Client)

**Handler:** `sub_47C320`

## Purpose

Adds a new item to the player's inventory.

## Payload Structure

```c
struct AddItem {
    ItemData item;           // 0x38 (56 bytes)
};
```

## Size

**56 bytes** (0x38)

## ItemData Structure

See [STRUCTURES.md](../STRUCTURES.md) for full ItemData definition.

```c
struct ItemData {
    int32 itemId;
    int32 templateId;
    int32 quantity;
    int32 durability;
    int32 maxDurability;
    int32 slot;
    int32 equipped;
    // ... total 56 bytes
};
```

## Notes

- Called when player purchases/receives an item
- Updates item list via `sub_44F5D0`

