# Economy

Shop, gifts, gacha, garage and inventory, kart durability, car craft and room craft on the
game server. Wire layouts are on the opcode pages in `docs/packets/opcodes/`, the owned
records in `docs/packets/STRUCTURES_VERIFIED.md`, car craft in
`docs/packets/systems/carcraft.md`.

## Source files

| Area | File |
|------|------|
| catalogues, buy, sell, extend, gift | `server/game/src/handlers/ShopHandler.cpp`, `packets/gen/ShopPackets.cpp` |
| owned lists, install, remove, delete, repair | `server/game/src/handlers/InventoryHandler.cpp`, `packets/gen/InventoryPackets.cpp` |
| gacha and pets | `server/game/src/handlers/GachaHandler.cpp`, `packets/gen/GachaPetPackets.cpp` |
| garage open | `server/game/src/handlers/GarageHandler.cpp` |
| car craft | `server/game/src/handlers/CarCraftHandler.cpp`, `packets/gen/CustomCarPackets.cpp` |
| room craft | `server/game/src/handlers/RoomCraftHandler.cpp`, `packets/gen/RoomCraftPackets.cpp` |
| kart stats and durability rules | `server/game/include/util/KartDurability.h`, `OwnedKartRow.h`, `ShopRules.h` |

## Owned data

Every owned thing is a row keyed by its own instance id: `owned_character`, `owned_kart`,
`owned_item`, `owned_part`, `owned_pet`, `owned_pendant`, room objects in
`player_item_instance`, car craft parts in `custom_car_part_instance`. The client lists are
built from them at login (`0x1B` characters, `0x1C` karts, `0x1D` items, one `0x1E` per
part, `0x0104` pets) and resent after a change. `owned_kart` is the only kart truth, its stats are the
catalogue stats plus an upgrade delta (migration 064).

## Catalogues

The login burst carries every catalogue the menus read, the drivers on `0xBF`, the karts on
`0xC0`, the price rows on `0xC6` and the rest, so C2S `0x10` shop enter needs no catalogue
round trip. Prices live in `shop_price` and `shop_option`, one row per price key with its
unit, amount, base and sale price and currency (gold or astro).

## Shop

| C2S | Server |
|-----|--------|
| `0x10` enter | shop screen state |
| `0xB7` buy: category, base key, price key, account name | the price row of that key sets cost and currency, the echoed name is ignored, wallet and grant in one transaction with the wallet row locked, S2C `0xB7` with the new record |
| `0xB8` delete | removes the owned rows of that key, no refund, the driver in use and pets are refused or handled by `InventoryHandler` |
| `0x0112` extend | renews a timed row at its price row |
| `0x98` gift | the target by name and its level, the sender pays, the row is granted to the target at once and logged in `gift_log`, S2C `0x99` when the target is online |
| `0xD0` wallet poll | the astro balance, or the grants waiting for this player first |

A price that moved between the catalogue and the press is refused with the new price. Every
buy, gift, extend and refusal is written to `shop_transaction_log`.

## Gifts

The received and sent lists go out at login. C2S `0x9C` marks a gift read, `0x9B` delete is
echoed so the client removes the row. The item itself was granted at the send.

## Gacha

C2S `0xED` names a gacha coin row the player owns. `GachaHandler::handleRoll` takes one use of
the coin, picks a prize by the weights of the gacha table, grants it in one transaction and
answers S2C `0xED` with the prize record. A table with zero total weight burns the coin with
no prize.

## Garage and inventory

C2S `0x0F` opens the garage, a working server answers the S2C `0x0F` alone.

| C2S | Category | Server |
|-----|----------|--------|
| `0xB9` install or use | 0 character, 1 kart | selects it and resends the equipment set |
| | 2 item | a repair scroll (use type 4 to 7) repairs the selected kart and takes one use, any other item is marked in use |
| | 3 part | writes the part key into the kart or character slot it belongs to |
| | 4 pet | equips the pet |
| `0xBA` remove | same | clears the slot |

The selection pair also lands in the profile blob at +0x4B0 and +0x4B4, see
`docs/packets/PROFILE_BLOB.md`.

## Kart durability

A kart on period mode 3 carries its durability in `owned_kart.period_value`. Every finished
race takes one wear step from the selected kart and resends the kart list. A repair scroll
adds its amount up to the cap. The starter kart is unlimited (migration 045).

## Car craft and room craft

| C2S | Server |
|-----|--------|
| `0x010A` car craft open | part definitions once per session, the preset list, owned part instances, then the ack |
| `0x010B` save | ownership of the kart and every part checked, preset written, S2C `0x010B` |
| `0x0114` rename | owned preset only, name cut to 9 characters, S2C `0x0114` then `0x0124` for that row |
| `0x010E` room craft open | object catalogue and owned objects once per session, then the stage push |
| `0x010F` save | the changed records, every path answers or the client waits forever |

The saved room craft is the decor of the waiting room the player hosts, it rides the tail of
S2C `0x13`.

## Open items

| Item | Why |
|------|-----|
| part wear on C2S `0xCC` | the client lowers the count of a counted part itself, the server does not mirror it |
| gift claim `0x9A` | still routed to the legacy item use, no claim answer is built, the item was granted at the send |
| gift delete `0x9B` | echoed only, the `gift_log` row stays |
| legacy `InventoryHandler` equip, sell and use paths on the old `items` and `accessories` tables | only `0x9A` still reaches one of them |
