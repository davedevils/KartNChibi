# 0x1C INVENTORY_ITEMS (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_478F20` @ line 219889
**Handler Ghidra:** `FUN_00478f20` @ line 84430

## Purpose

Sends full item inventory list to client.

## Payload Structure

```c
struct InventoryItems {
    int32_t count;              // Number of items
    ItemData items[count];      // Item entries (56 bytes each)
};
```

## Field Details

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Item count |
| 0x04 | ItemData[] | 56 * count | Array of items |

## ItemData Structure (56 bytes = 0x38)

```c
struct ItemData {
    int32_t itemId;         // +0x00 - Item template ID
    int32_t uniqueId;       // +0x04 - Unique instance ID
    int32_t quantity;       // +0x08 - Stack count
    int32_t unknown[11];    // +0x0C - Unknown fields (44 bytes)
    // Possibly includes:
    // - expiration date
    // - slot position
    // - enhancement level
    // - bound status
};  // Total: 56 bytes
```

## Handler Code (IDA)

```c
int __thiscall sub_478F20(void *this, int a2)
{
  int *inventory = (int*)(&unk_845228 + this);
  int count;
  int itemData[14];  // 56 bytes
  
  sub_44F2B0(inventory);                    // Clear inventory
  sub_44E910(a2, &count, (const char*)4);   // Read count
  
  for (int i = 0; i < count; i++) {
    sub_44E910(a2, itemData, (const char*)0x38);  // Read 56 bytes
    sub_44F2D0(inventory, itemData);              // Add to inventory
  }
  return count;
}
```

## Handler Code (Ghidra)

```c
void FUN_00478f20(void)
{
  int count;
  undefined1 itemData[56];
  
  FUN_0044f2b0();                    // Clear inventory
  FUN_0044e910(&count, 4);           // Read count
  
  int i = 0;
  if (count > 0) {
    do {
      FUN_0044e910(itemData, 0x38);  // Read 56 bytes
      FUN_0044f2d0(itemData);        // Add to inventory
      i++;
    } while (i < count);
  }
  return;
}
```

## Server Implementation

```cpp
void sendInventoryItems(Session::Ptr session, const std::vector<ItemData>& items) {
    Packet pkt(0x1C);
    
    pkt.writeInt32(items.size());  // Count
    
    for (const auto& item : items) {
        pkt.writeBytes(reinterpret_cast<const uint8_t*>(&item), 56);
    }
    
    session->send(pkt);
}
```

## Notes

- Item inventory stored at address 0x845228
- Called `sub_44F2B0` clears inventory before adding

