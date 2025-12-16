# 0x1E ITEM_UPDATE (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_478FE0` @ line 219933
**Handler Ghidra:** `FUN_00478fe0` @ line 84474

## Purpose

Updates a single accessory item in inventory. **NO COUNT** - single item only.

## Payload Structure (28 bytes)

```c
struct ItemUpdate {
    AccessoryData accessory;    // Single accessory (28 bytes)
};  // Total: 28 bytes
```

## Field Details

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | AccessoryData | 28 | Single accessory update |

## Handler Code (IDA)

```c
int __thiscall sub_478FE0(void *this, int a2)
{
  int *inventory;
  int accessoryData[7];  // 28 bytes
  
  sub_44E910(a2, accessoryData, (const char*)0x1C);  // Read 28 bytes
  
  inventory = (int*)(&unk_846030 + this);
  sub_450E30(inventory, accessoryData[1]);  // Find by uniqueId
  return sub_450D20(inventory, accessoryData);  // Update
}
```

## Handler Code (Ghidra)

```c
void FUN_00478fe0(void)
{
  undefined1 accessoryData[4];
  undefined4 uniqueId;  // accessoryData[1]
  
  FUN_0044e910(accessoryData, 0x1c);  // Read 28 bytes
  FUN_00450e30(uniqueId);             // Find by uniqueId
  FUN_00450d20(accessoryData);        // Update entry
  return;
}
```

## Server Implementation

```cpp
void sendItemUpdate(Session::Ptr session, const AccessoryData& accessory) {
    Packet pkt(0x1E);
    
    // Write single AccessoryData (28 bytes)
    pkt.writeInt32(accessory.accessoryId);
    pkt.writeInt32(accessory.uniqueId);    // Used to find existing entry
    pkt.writeInt32(accessory.slot);
    pkt.writeInt32(accessory.bonus1);
    pkt.writeInt32(accessory.bonus2);
    pkt.writeInt32(accessory.bonus3);
    pkt.writeInt32(accessory.equipped);
    
    session->send(pkt);
}
```

## Notes

- **NO COUNT** - updates single item only
- Uses `uniqueId` (offset +0x04) to find existing item in inventory
- Inventory stored at address 0x846030
- Used for equip/unequip operations

## Difference from 0x1D

| Packet | Purpose | Has Count | Size |
|--------|---------|-----------|------|
| 0x1D | Full list | Yes | 4 + 28*n |
| 0x1E | Single update | No | 28 |
