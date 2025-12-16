# 0x1D INVENTORY_ACCESSORIES (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_478F80` @ line 219911
**Handler Ghidra:** `FUN_00478f80` @ line 84452

## Purpose

Sends full accessory inventory list to client.

## Payload Structure

```c
struct InventoryAccessories {
    int32_t count;                      // Number of accessories
    AccessoryData accessories[count];   // Accessory entries (28 bytes each)
};
```

## Field Details

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Accessory count |
| 0x04 | AccessoryData[] | 28 * count | Array of accessories |

## AccessoryData Structure (28 bytes = 0x1C)

```c
struct AccessoryData {
    int32_t accessoryId;    // +0x00 - Accessory template ID
    int32_t uniqueId;       // +0x04 - Unique instance ID
    int32_t slot;           // +0x08 - Equipment slot
    int32_t bonus1;         // +0x0C - Bonus stat 1
    int32_t bonus2;         // +0x10 - Bonus stat 2
    int32_t bonus3;         // +0x14 - Bonus stat 3
    int32_t equipped;       // +0x18 - 0=inventory, 1=equipped
};  // Total: 28 bytes
```

## Handler Code (IDA)

```c
int __thiscall sub_478F80(void *this, int a2)
{
  int *inventory = (int*)(&unk_847C38 + this);
  int count;
  int accessoryData[7];  // 28 bytes
  
  sub_451510(inventory);                    // Clear inventory
  sub_44E910(a2, &count, (const char*)4);   // Read count
  
  for (int i = 0; i < count; i++) {
    sub_44E910(a2, accessoryData, (const char*)0x1C);  // Read 28 bytes
    sub_451530(inventory, accessoryData);              // Add to inventory
  }
  return count;
}
```

## Handler Code (Ghidra)

```c
void FUN_00478f80(void)
{
  int count;
  undefined1 accessoryData[28];
  
  FUN_00451510();                       // Clear inventory
  FUN_0044e910(&count, 4);              // Read count
  
  int i = 0;
  if (count > 0) {
    do {
      FUN_0044e910(accessoryData, 0x1c);  // Read 28 bytes
      FUN_00451530(accessoryData);        // Add to inventory
      i++;
    } while (i < count);
  }
  return;
}
```

## Server Implementation

```cpp
void sendInventoryAccessories(Session::Ptr session, const std::vector<AccessoryData>& accessories) {
    Packet pkt(0x1D);
    
    pkt.writeInt32(accessories.size());  // Count
    
    for (const auto& acc : accessories) {
        pkt.writeInt32(acc.accessoryId);
        pkt.writeInt32(acc.uniqueId);
        pkt.writeInt32(acc.slot);
        pkt.writeInt32(acc.bonus1);
        pkt.writeInt32(acc.bonus2);
        pkt.writeInt32(acc.bonus3);
        pkt.writeInt32(acc.equipped);
    }
    
    session->send(pkt);
}
```

## Notes

- Accessory inventory stored at address 0x847C38

