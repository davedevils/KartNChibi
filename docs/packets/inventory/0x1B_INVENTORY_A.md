# CMD 0x1B (27) - Inventory Type A (Vehicles)

**Direction:** Server → Client  
**Handler:** `sub_478EC0`

## Structure

```c
struct InventoryTypeA {
    int32 itemCount;
    VehicleData items[itemCount];
};

struct VehicleData {  // 0x2C = 44 bytes
    int32 vehicleId;
    int32 uniqueId;
    int32 durability;
    int32 maxDurability;
    int32 speed;
    int32 acceleration;
    int32 handling;
    int32 boost;
    int32 unknown1;
    int32 unknown2;
    int32 equipped;       // 1 = currently equipped
};
```

## Fields per Vehicle

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | vehicleId |
| 0x04 | 4 | int32 | uniqueId |
| 0x08 | 4 | int32 | durability |
| 0x0C | 4 | int32 | maxDurability |
| 0x10 | 4 | int32 | speed |
| 0x14 | 4 | int32 | acceleration |
| 0x18 | 4 | int32 | handling |
| 0x1C | 4 | int32 | boost |
| 0x20 | 4 | int32 | unknown1 |
| 0x24 | 4 | int32 | unknown2 |
| 0x28 | 4 | int32 | equipped |

## Example

```
1B [size] 00 00 00 00 00
02 00 00 00              // itemCount = 2
// Vehicle 1
01 00 00 00              // vehicleId = 1
64 00 00 00              // uniqueId = 100
50 00 00 00              // durability = 80
64 00 00 00              // maxDurability = 100
...                       // rest of stats
01 00 00 00              // equipped = 1 (yes)
// Vehicle 2
...
```

## Notes

- Sent on login and garage entry
- Contains all owned vehicles
- Durability affects race performance

