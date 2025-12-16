# 0x1B INVENTORY_VEHICLES (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_478EC0` @ line 219867
**Handler Ghidra:** `FUN_00478ec0` @ line 84408

## Purpose

Sends full vehicle inventory list to client.

## Payload Structure

```c
struct InventoryVehicles {
    int32_t count;                    // Number of vehicles
    VehicleData vehicles[count];      // Vehicle entries (44 bytes each)
};
```

## Field Details

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Vehicle count |
| 0x04 | VehicleData[] | 44 * count | Array of vehicles |

## VehicleData Structure (44 bytes = 0x2C)

```c
struct VehicleData {
    int32_t vehicleId;      // +0x00 - Vehicle template ID
    int32_t uniqueId;       // +0x04 - Unique instance ID
    int32_t durability;     // +0x08 - Current durability
    int32_t maxDurability;  // +0x0C - Maximum durability
    int32_t stats[7];       // +0x10 - Vehicle stats (28 bytes)
    // stats[0] = speed
    // stats[1] = acceleration
    // stats[2] = handling
    // stats[3] = drift
    // stats[4] = boost
    // stats[5] = weight
    // stats[6] = special
};  // Total: 44 bytes
```

## Handler Code (IDA)

```c
int __thiscall sub_478EC0(void *this, int a2)
{
  int *inventory = (int*)(&unk_844720 + this);
  int count;
  int vehicleData[11];  // 44 bytes
  
  sub_44FC90(inventory);                    // Clear inventory
  sub_44E910(a2, &count, (const char*)4);   // Read count
  
  for (int i = 0; i < count; i++) {
    sub_44E910(a2, vehicleData, (const char*)0x2C);  // Read 44 bytes
    sub_44FCB0(inventory, vehicleData);              // Add to inventory
  }
  return count;
}
```

## Handler Code (Ghidra)

```c
void FUN_00478ec0(void)
{
  int count;
  undefined1 vehicleData[44];
  
  FUN_0044fc90();                    // Clear inventory
  FUN_0044e910(&count, 4);           // Read count
  
  int i = 0;
  if (count > 0) {
    do {
      FUN_0044e910(vehicleData, 0x2c);  // Read 44 bytes
      FUN_0044fcb0(vehicleData);        // Add to inventory
      i++;
    } while (i < count);
  }
  return;
}
```

## Server Implementation

```cpp
void sendInventoryVehicles(Session::Ptr session, const std::vector<VehicleData>& vehicles) {
    Packet pkt(0x1B);
    
    pkt.writeInt32(vehicles.size());  // Count
    
    for (const auto& v : vehicles) {
        pkt.writeInt32(v.vehicleId);
        pkt.writeInt32(v.uniqueId);
        pkt.writeInt32(v.durability);
        pkt.writeInt32(v.maxDurability);
        for (int i = 0; i < 7; i++) {
            pkt.writeInt32(v.stats[i]);
        }
    }
    
    session->send(pkt);
}
```

## Notes

- Called `sub_44FC90` clears inventory before adding
- Vehicle inventory stored at address 0x844720
- First int32 of VehicleData is used as equipped vehicle ID

