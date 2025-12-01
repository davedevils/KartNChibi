# 0x9F ADD_ACCESSORY (Server → Client)

**Handler:** `sub_47C370`

## Purpose

Adds a new accessory to the player's inventory.

## Payload Structure

```c
struct AddAccessory {
    AccessoryData accessory;  // 0x1C (28 bytes)
};
```

## Size

**28 bytes** (0x1C)

## AccessoryData Structure

See [STRUCTURES.md](../STRUCTURES.md) for full AccessoryData definition.

```c
struct AccessoryData {
    int32 accessoryId;
    int32 templateId;
    int32 slot;
    int32 equipped;
    int32 durability;
    int32 unknown1;
    int32 unknown2;
};
```

## Notes

- Called when player purchases/receives an accessory
- Updates accessory list via `sub_451F60`

