# 0x9D ADD_VEHICLE (Server → Client)

**Handler:** `sub_47C2D0`

## Purpose

Adds a new vehicle to the player's inventory.

## Payload Structure

```c
struct AddVehicle {
    VehicleData vehicle;     // 0x2C (44 bytes)
};
```

## Size

**44 bytes** (0x2C)

## VehicleData Structure

See [STRUCTURES.md](../STRUCTURES.md) for full VehicleData definition.

```c
struct VehicleData {
    int32 vehicleId;
    int32 templateId;
    int32 durability;
    int32 maxDurability;
    int32 color;
    int32 stats[6];
    // ... total 44 bytes
};
```

## Notes

- Called when player purchases/receives a vehicle
- Updates vehicle list via `sub_44FC20`

