# 0x70/0x71 INVENTORY_UPDATE (Server → Client)

**Packet ID:** 0x70 (112) and 0x71 (113 decimal)  
**Handler:** `sub_47B300` / `FUN_0047b300`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Updates inventory slot state. Both 0x70 and 0x71 use the same handler.

## Payload Structure

```c
struct InventoryUpdate {
    int32 unused;      // Read but not directly used
    int32 slotIndex;   // Slot to update
};
```

## Size

**8 bytes** (2 × int32)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Unused field |
| 0x04 | int32 | 4 | Slot index |

## Handler Logic

```c
// IDA: sub_47B300 / Ghidra: FUN_0047b300
read(&unused, 4);       // local_4 - read but not used
read(&slotIndex, 4);
updateInventorySlot(slotIndex);  // sub_44EF20
```

## Difference Between 0x70 and 0x71

Both packets use the **exact same handler**. The difference may be in:
- Server-side interpretation
- Client-side context (which was last used)
- Reserved for future differentiation

## Server Implementation

```cpp
void sendInventoryUpdate(Session::Ptr session, int unused, int slotIndex) {
    Packet pkt(0x70);  // or 0x71
    pkt.writeInt32(unused);  // Reserved
    pkt.writeInt32(slotIndex);
    session->send(pkt);
}
```

## Notes

- Two CMD codes share same handler
- First int32 is read but appears unused
- Likely for slot refresh/sync operations
- Part of inventory management system

