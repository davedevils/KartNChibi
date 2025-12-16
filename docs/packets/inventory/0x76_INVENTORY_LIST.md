# 0x76 - INVENTORY_LIST

**CMD**: `0x76` (118 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B1B0`  
**Handler Ghidra**: `FUN_0047b1b0`

## Description

Sends the player's vehicle inventory list. Contains a count followed by VehicleData structures.

## Payload Structure

| Offset | Type        | Size     | Description              |
|--------|-------------|----------|--------------------------|
| 0x00   | int32       | 4        | Vehicle count            |
| 0x04   | VehicleData | 44 × n   | Array of vehicle data    |

**Total Size**: 4 + (44 × vehicleCount) bytes

## C Structure

```c
struct VehicleData {
    // 44 bytes (0x2C) - see STRUCTURES.md
};

struct InventoryListPacket {
    int32_t count;              // +0x00 - Number of vehicles
    VehicleData vehicles[];     // +0x04 - Array of VehicleData
};
```

## Handler Logic (IDA)

```c
// sub_47B1B0
int __thiscall sub_47B1B0(void *this, int a2)
{
    int *v2 = (int*)((char*)&unk_848648 + this);
    int v5;     // count
    int v6[12]; // VehicleData buffer (44 bytes)
    
    sub_44EF70(v2);                 // Clear existing inventory
    sub_44E910(a2, &v5, 4);         // Read count
    
    for (int i = 0; i < v5; ++i) {
        sub_44E910(a2, v6, 0x2C);   // Read 44 bytes (VehicleData)
        sub_44EF90(v2, v6);         // Add to inventory
    }
    
    return v5;
}
```

## Cross-Validation

| Source | Function       | Payload Read      |
|--------|----------------|-------------------|
| IDA    | sub_47B1B0     | 4 + 44*n bytes    |
| Ghidra | FUN_0047b1b0   | 4 + 44*n bytes    |

**Status**: ✅ CERTIFIED
