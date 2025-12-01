# Shop Packets

Shop transactions and item purchases.

## Server → Client

| CMD | Name | Description |
|-----|------|-------------|
| [0x68](0x68_SHOP_LOOKUP.md) | Shop Lookup | Item query response |
| [0x69](0x69_SHOP_ITEM.md) | Shop Item | Item data (7 bytes) |
| [0x6A](0x6A_SHOP_UPDATE.md) | Shop Update | Update (6 bytes) |
| [0x6F](0x6F_SHOP_RESPONSE.md) | Shop Response | Purchase result |
| [0x72](0x72_DATA_BLOCK.md) | Data Block | Shop config (104 bytes) |
| [0x73](0x73_SLOT_UPDATE.md) | Slot Update | Update slot values |
| [0x74](0x74_UNKNOWN.md) | Unknown | Sets shop flags |

## Client → Server

| CMD | Name | Description |
|-----|------|-------------|
| 0x6E | Purchase Request | Buy item |
| 0x70 | Shop Enter | Open shop |
| 0x71 | Shop Exit | Close shop |
