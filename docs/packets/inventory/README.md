# Inventory Packets

Items, vehicles, and equipment management.

## Server → Client

| CMD | Name | Description | Item Size |
|-----|------|-------------|-----------|
| [0x1B](0x1B_INVENTORY_A.md) | Inventory A | Vehicle list | 44 bytes |
| [0x1C](0x1C_INVENTORY_B.md) | Inventory B | Item list | 56 bytes |
| [0x1D](0x1D_INVENTORY_C.md) | Inventory C | Accessory list | 28 bytes |
| [0x1E](0x1E_ITEM_UPDATE.md) | Item Update | Single item | 28 bytes |
| [0x76](0x76_INVENTORY_LIST.md) | Inventory List | Vehicle list | 44 bytes |
| [0x77](0x77_UPDATE_LIST.md) | Update List | Partial update | 8×N bytes |
| [0x78](0x78_ITEM_LIST.md) | Item List | Generic list | 44 bytes |
| [0x79](0x79_ITEM_LIST_B.md) | Item List B | Small items | 32 bytes |
| [0x7A](0x7A_ITEM_ADD.md) | Item Add | Single add | 32 bytes |
| [0x7B](0x7B_INV_SLOT.md) | Inv Slot | Slot update | 8 bytes |
| [0x7D](0x7D_NOTIFICATION.md) | Notification | Item notification | var |
| [0x8C](0x8C_EQUIP_ITEM.md) | Equip Item | Equip to slot | var |
| [0x98](0x98_GIFT.md) | Gift | Gift received | 220 bytes |
| [0x9D](0x9D_ADD_VEHICLE.md) | **Add Vehicle** | New vehicle | 44 bytes |
| [0x9E](0x9E_ADD_ITEM.md) | **Add Item** | New item | 56 bytes |
| [0x9F](0x9F_ADD_ACCESSORY.md) | **Add Accessory** | New accessory | 28 bytes |
| [0xB8](0xB8_REMOVE_ITEM.md) | **Remove Item** | Delete item | 8 bytes |

## Item Sizes Reference

| Type | Size | Contents |
|------|------|----------|
| Vehicle | 0x2C (44) | ID, stats, durability |
| Item | 0x38 (56) | ID, quantity, unknowns |
| Accessory | 0x1C (28) | ID, slot, bonuses |
| Small | 0x20 (32) | Compact format |
