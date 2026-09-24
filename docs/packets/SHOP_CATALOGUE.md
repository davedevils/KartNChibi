# Shop and catalogues

What the stock client knows, what the real server sold, what our server sells, and where every
price comes from. Audit of 2026-09-23.

Sources:

- the client, `KnC.exe.raw` in Ghidra (image base 0x400000) and the text tables and assets of
  `<client>` (`Define/Eng/def_trans_index.txt` with `def_trans_message.txt`, `Data/Public`)
- the real server, the captured login burst `server/data/reference_login_burst.bin` (733 frames,
  every catalogue row and the whole 0x00C6 price table)
- our server, the package image `knc-server:20260919a` as the gate runs of 2026-09-23 recorded it
  (`tests/server/data/package_carcraft_rx.bin` and `package_roomcraft_rx.bin`, 867 frames each) and the
  package database, read only
- `docs/reverse/ARCHIVE_SOURCES.md`, the web archive pass. The burst is not silent on any price, so no
  row below comes from the archive. The archive confirms four burst numbers through the chibikart.gg forum
  post: Gold Coin 2500 gold permanent, Astro coin 1000 Astro permanent, a kart for 7 days 9000 gold, an
  accessory for 7 days 2500 gold. The 2008 GOA price lists it found (Nitros and Teks, 1 week 1 month 1 year)
  belong to another build, this client only knows days, uses, durability and permanent.

## 1. The price rows and the currency

The client never picks a currency, the server charges. The detail panel `FUN_0045E2E0` resolves each option
through `price_row_lookup` 0x451E00, prints `price_row_draw_detail` 0x45D4A0 (the sale when above 0 else the
base) and at 0x45E36B draws `this+0x138` when the sale is above 0, `this+0xA0` else. The panel init
`FUN_0045CEF0` loads `Popup/ShopItem/UI_Shop_iteminfo_buy_Peri.png` (the word Gold) into `+0xA0` at
0x45CFB6 and `UI_Shop_iteminfo_buy_Cash.png` (the word Astro) into `+0x138` at 0x45CFCF.
So a sale price is an Astro price and a base price is a gold price. The gacha box says the same:
tab 0 is "ASTRO Crazy Lotto" and rolls the Gacha Coin, tab 1 is "GOLD Crazy Lotto" and rolls the Gold Coin.

The 23 rows of the burst, one tier per shop category:

| key | unit | amount | base | sale | the client prints | sells |
|---|---|---|---|---|---|---|
| 1001 to 1007 | 0 permanent | 0 | 0 | 3000 | 3000 Astro permanent | every permanent option |
| 2001 | 1 days | 1 | 1500 | 0 | 1500 gold 1 day | driver |
| 2002 | 1 days | 1 | 1800 | 0 | 1800 gold 1 day | kart |
| 2003 | 1 days | 1 | 500 | 0 | 500 gold 1 day | part the driver wears, slot 2 to 6 |
| 2004 | 1 days | 1 | 400 | 0 | 400 gold 1 day | kart part, paint plate antenna, slot 0 1 8 |
| 2005 | 1 days | 1 | 2500 | 0 | 2500 gold 1 day | pet |
| 2006 | 1 days | 1 | 300 | 0 | 300 gold 1 day | room object |
| 2007 | 1 days | 1 | 1000 | 0 | 1000 gold 1 day | car craft part |
| 3001 to 3007 | 1 days | 7 | 7500 9000 2500 2000 12500 1500 5000 | 0 | gold 7 days | the same seven |
| 4001 | 0 permanent | 0 | 0 | 1000 | 1000 Astro permanent | Gacha Coin 2000 |
| 4002 | 0 permanent | 0 | 2500 | 0 | 2500 gold permanent | Gold Coin 2001 |

Tier n is `2000+n` one day, `3000+n` seven days, `1000+n` permanent, always in that option order:
driver 1, kart 2, worn part 3, kart part 4, pet 5, room object 6, car craft part 7 (`util/ShopRules.h`
`burstPriceTier`, proven on every sold row of the burst by `ShopBurstPrices.EverySoldRowSitsOnItsCategoryTier`).

Ours before this change: 39 rows with our own numbers (1001 to 1006 as gold permanents of 3000 to 24000,
other day and week prices), the coin packs 4001 to 4006 on unit 2, the repair scrolls 4101 4102 on unit 3,
a three day tier 5001 to 5005, and 90001 90002 1001001 1010001 1012001 on three disabled karts. The charge
followed a `currency` column that was 0 everywhere, so a sale row was billed in gold while the client
printed Astro.

After migration 068 and the server of this change: the 23 burst rows key by key, plus 4101 4102 and the
five rows of the disabled karts, 30 rows. The coin packs and the three day tier are gone.
`ShopPackets::loadPrice` sets the currency from the sale (`ShopPackets::currencyOf`, `priceIsAstro`), the
column is ignored and 068 rewrites it to the same rule for the log.

## 2. Every catalogue

| catalogue | the client knows | the real server | ours before | ours after 068 |
|---|---|---|---|---|
| drivers 0x00BF | 10 bodies under `Driver/Body/High`, 11 `DRIVER_*_TITLE` (Malice has text only) | 10 rows, 8 sold on tier 1, Cosmo and Moriko hidden | 10 rows sold on tier 4 | 10 sold on tier 1 |
| karts 0x00C0 | 46 `CAR_*_TITLE`, 45 body folders, 5 chassis under `Car/FactoryCar/CHASSIS` | 46 rows, 41 sold on tier 2, the 5 factory chassis hidden | 46 sold on tier 1 | 46 sold on tier 2 |
| items 0x00C1 | 11 `ITEM_*` keys: 1000 slot exchange, 2000 Gacha Coin, 2001 Gold Coin, 3000 to 3003 repair kits, 4001 to 4003 oils, 5000 third slot, and a horn icon | 2 sold, 2000 on 4001 and 2001 on 4002 | 30 rows: 1000 with no price, the coins on the packs, 3000 3001, 25 race items with no icon and no text | 4 sold: the coins on the burst rows, 3000 and 3001 (our repair scrolls, now on `ITEM_3000_TITLE` and `ITEM_3001_TITLE`) |
| parts 0x00C2 | 406 `PART_<key>_TITLE`, icons under `Image/Parts`, models under `Car/Parts` `Car/Item` and `BODYSET` | 473 rows, 367 sold, worn on tier 3, kart side on tier 4 | 493 rows: 458 skins (427 sold on tier 1 or 2) and 35 car craft twins | 459 rows, 428 sold on tier 3 and 4, no twins |
| pets 0x0103 | 4 `PET_*_TITLE`, 4 bodies | 4 sold on tier 5, keys 5001 to 5004 | 4 sold, keys 10 to 40, tier 5 with zero option words and a three day row | 4 sold on tier 5 |
| room objects 0x010C | 11 sky, 13 floor, 4 back object, 32 object, 11 effect folders under `World/Room` | 71 rows, 10 sold (SKY02 to SKY04 and 7 effects), 31 carry the tier | 71 sold on tier 6 | 71 sold on tier 6 |
| car craft 0x0108 | 35 titles `COVER_1000` to `WING_7004`, 5 chassis x 7 slots | 35 rows, none enabled, car craft was off | 35 sold on tier 7 | 35 sold on tier 7 |
| pendants 0x0119 | 13 icons `Icon/pendant_NN`, `PENDANT_01` to `13` | not in the burst, chibikart sends 13 rows with 13 hidden | 13 rows, 13 hidden | the same, earned never sold, see section 5 |

Keys differ where the emulator picked its own: drivers 5 to 14 (burst 2 6 10 to 80), karts 10010 to 13010
(burst 1 to 48), kart side parts 9001 to 9329 (burst 1000 to 1233), pets 10 to 40 (burst 5001 to 5004), room
objects 1001 to 5011 (burst 8000 to 8070), car craft parts 1000 to 7004 (burst 7000 to 7136). The client
resolves every key through its own container, so a key is free as long as every owned row and every preset
names a key of the same burst. The price keys are not free, they are the burst keys above.

### What the real server did not sell

- drivers: Cosmo and Moriko (hidden rows). Malice (`DRIVER_DOLL_TITLE`) has no body at all.
- karts: the 5 factory chassis Circler Firedragon Quatzalcuatl Squarer Striper (hidden).
- items: 1000 slot exchange, 3000 to 3003 repair kits, 4001 to 4003 GOLD UP EXP UP Total UP oils,
  5000 third slot, the horn. None has a row in the burst.
- parts: 106 hidden rows: 76 worn parts and 10 faces (among them the 30 default costume parts of the 10
  drivers and the special keys 19300 to 23004), the PURPPLE paint, 8 plates (NAMEBOX_NORMAL 001 002 004
  011 to 014) and 11 antennas.
- room objects: 61 of 71 rows.
- car craft: all 35 parts.

What ours sells that the real server did not, kept as the emulator rule: Cosmo and Moriko, the 5 factory
chassis, the repair scrolls 3000 3001, 62 part rows (45 worn, PURPPLE, 7 plates, 9 antennas), 61 room
objects, the 35 car craft parts.

What the real server sold that ours lacked: the War Flag antenna `ant_19` (burst key 1219), migration 068
adds it as 9329 on tier 4. The Emerald Tiara `princess_char_cap_014` (burst key 14213, sold) stays out: the
client ships its icon but no nif under `BODYSET`, `FUN_00443D10` then fails, `FUN_0048C680` returns 0 and the
driver build `FUN_004A5ED0` stops at the accessory.

### The Item tab after migration 071

The official server video (`www.youtube.com/watch?v=ViGZzEWl5sE`) sells exactly three items on the
Item tab, so 071 does the same:

| tile | key | icon | options |
|---|---|---|---|
| 100% Repair Kit | 3001 `ITEM_3001_TITLE` | `repair_full`, the tile art says "30% GOLD" | 4102 unit 3 durability 500 for 1400 gold, the full bar |
| Gold Coin | 2001 `ITEM_2001_TITLE` | `gacha_coin2` | 4002 2500 gold permanent, the burst row |
| Slot Exchange | 1000 `ITEM_1000_TITLE` | `slotchanger` | 4201 50 uses 50 gold, 4202 105 uses 100 gold, 4203 220 uses 200 gold |

The Gacha Coin 2000 and the 50% kit 3000 are hidden, owned copies keep working. The rows go out from the
highest key down so the tiles read like the video. The use rule of the Slot Exchange is on
[0x00CB](opcodes/0x00CB.md): one use per Alt swap in a race, the char panel shows its icon at 0x40 0xAD
while uses are left. The repair kit price is ours, the video does not show it.

The char panel of the video, `FUN_00429990` and `FUN_0042AB70`:

- the top icon `Common_Char_Slot` at 71 179 (the round arrows) takes the Slot Exchange icon at 0x40 0xAD when
  the owned 1000 row has uses (0x429B78);
- the icon under it, `Common_Char_Item` at 71 236, takes an oil at 0x40 0xE5 when an owned row of 4001 4002 or
  4003 has uses and its in use flag +0x18 is 1 (`FUN_004505C0`); none of ours has an 0x00C1 row, the video tab
  does not sell them either, so that icon stays empty here;
- the wrench bar under the kart is `kart_durability_bar_draw` 0x42AD20 at 0x43 0x204, drawn only when the
  selected owned kart is period mode 3; our karts are mode 0 (migration 045), so the bar is missing on the
  package. The docker proof with the kart put on mode 3 and 250 shows the bar on both the package and the new
  server, the server sends what the client needs, giving every kart a durability is a rule change left open.

## 3. The Gold Coin and the gacha

The popup `FUN_004576B0` builds two tabs, `Gacha_tab_C` at +0x289 +0x56 and `Gacha_tab_P` at +0x289 +0x106.
The mouse handler `FUN_00456830` writes the tab index at +0x1AA44 (0x456904), the frame `FUN_004571D0` looks up
the owned item row of base key 2000 for tab 0 and 0x7D1 (2001) for tab 1 (0x45730F to 0x45733B) and draws its
count +0x10 when +0x14 is set. Play `FUN_00456910` calls `FUN_004830C0`, which sends C2S 0x00ED with the
0x1C row only when +0x14 is set and +0x10 is above 0. The Gold Coin tab is live, the old note that called it
a dead tab was wrong.

A coin row must be period mode 2 with a count, `GachaPetPackets::rollRequestMatchesTicket` checks that too.
The burst sells both coins on a unit 0 permanent row, so a plain permanent grant gave a mode 0 row with count
0 that the popup can never spend. `ShopHandler` `periodAfterPurchase` now turns any buy of 2000 or 2001 into
mode 2 plus one coin (plus the amount on a unit 2 row), `coinPeriodAfterPurchase` in `util/ShopRules.h`.

Proofs:

- tests `ShopCoinGrant.TheGoldCoinBoughtThenRolled` (the burst 4002 buy lands a mode 2 row with 1 coin, the
  client gate passes on its bytes, the C2S 0x00ED of that row parses and matches, the roll leaves 0),
  `ShopCoinGrant.AModeZeroCoinIsRefused`, `ShopCoinGrant.TheChargeFollowsTheClientIcon`.
- live on the package image with the clone, 2026-09-23, account hltest: the shop Item tab tile 2 bought the
  Gold Coin (`0x00B7` category 2 key 2001 price 4004, 3000 gold, the ack row count 5 mode 2), the gacha box
  gold tab (click 905 458) then START sent `0x00ED` with instance 10 key 2001 mode 2 count 5 and the server
  answered `0x00ED` category 0 driver 12 rare with the row echoed at count 4. The package still sells the coin
  on our old row 4004, the burst row 4002 at 2500 needs migration 068.
- `python tools/release_gate.py --only gacha` passes but proves less than its README says: `gacha.txt` buys
  tile 2, which is the Gold Coin, then presses Play on tab 0 whose Gacha Coin count is 0, so no 0x00ED leaves.
  The gold tab run above is the scratch copy of `gacha.txt` with one extra click on the gold tab. The script
  lives in `tools/`, left for its owner.

## 4. Car craft, room craft and pendant records the client dereferences

Checked field by field against the exe readers by `tests/server/test_client_reader_rules.cpp`, which decodes
a recorded stream with the client reader rules and sorts every hit into crash (null deref, frame smash,
desync), dropped (the client drops or never draws the row) and cosmetic. The reference burst passes every
rule, the rules read the client right. The package capture of the car craft run and of the room craft run
carries one crash per 0x0107, the 11 char `Factory Car` name the name plate click overruns (rule added
2026-09-23 after the user hit it, see opcodes/0x0107.md), and one dropped, the built slot on a catalogue kart.
Before that rule the captures showed three cosmetic hits that the previous change fixed:

- 5 duplicate 0x00C2 keys, 2000 to 2004, the car craft twin rows over five head parts (below)
- 36 rows that claim to sell on option key 0, the 35 twins and the slot exchange 1000 (068 hides 1000)
- 70 ability pairs drawn as `0%`, two on each 0x0108 part (below)

### Stage 18, what must be buffered before 0x010A

| record | reader | rule | ours |
|---|---|---|---|
| 0x00C6 price rows | 0x478CB0 | every sold option key resolves | yes |
| 0x00C0 kart rows | 0x47F4F0, cap 64 | the preset kart base key resolves, `FUN_00430420` does `LEA EBP,[EAX+0x20]` on the lookup at 0x430463 with no null test | the preset kart is now always a published enabled template (`CarCraftHandler::buildView`) |
| 0x00C0 factory row | model scheme +0x14 = 1 | model name is one of the 5 chassis folders, `Car/FactoryCar/CHASSIS/%s/body` and `Texture/%s_<tier>` | yes |
| 0x0108 part defs | 0x480210, cap 256, stride 0x120 | strings 32 32 33, category 0 to 6, model is a chassis folder, 1 to 4 price rows, the first resolves | yes |
| 0x0108 ability block +0xBC | `carcraft_part_ability_pairs_draw` 0x42B7A0 | id outside 0 to 25 or the tile draws a `0%` icon | FIXED, the image sends 0 0 0 0, the builder now sends -1 0 -1 0 like the burst |
| 0x0108 tail triple +0xCC | `car_apply_kart_loadout` 0x490A70 copies the tires row into car+0xA7988 | +0xD4 adds to the wheel grip | 0 0 0 like the burst |
| 0x0109 part instances | 0x47E4C0, 132 bytes, cap 256 | key resolves in 0x0108, `FUN_00430420` hands `part_def_record_lookup` 0x44FBC0 plus 0x14 to sprintf with no null test (0x4306C5) | instances with no def are dropped in `buildView` |
| 0x0109 grade +0x80 | 0x430420 and 0x490A70 | tier by grade: under 5 Basic, 5 to 19 Unique, 20 to 64 Epic, 65 up Legend, the chassis takes the average | any int is safe |
| 0x0107 presets | 0x47E400, cap 5 | count 1 or more, name NUL within 12 bytes, a built slot (1) names a kart of the 0x001C list that is a factory chassis, every slot 0 or an instance of the slot category, the tire slot set or Save shows MSG_UNSUPPORT, an empty slot (0) carries kart 0 and no part | `presetList` never sends 0, the empty slot is the default, `buildView` empties a built slot with no chassis or tire |
| 0x0107 0x0114 0x0124 name | the name plate click `sub_432B20` 0x4330D8, the rename dialog `sub_455C20` | 9 chars at most, `MultiByteToWideChar` cch 20 into a `WCHAR[10]` stack cell, the cookie follows | FIXED, the image sent `Factory Car` (11) and the click died on "Buffer overrun detected!", now cut to 9 and the default slot has no name |
| 0x001C owned karts | 0x478F20, cap 64 | base key resolves in 0x00C0, a catalogue kart paint resolves in 0x00C2 or `FUN_004A5ED0` returns at 0x4A5FD1 | yes |

The textures `Car/FactoryCar/Texture/<model>_Basic`, `_Unique`, `_Epic`, `_Legend` and `Texture/<model>` exist for
all 5 chassis, the 7 slot folders too, a missing nif makes `nif_object_load` fail and the build return 0 with
no crash.

The C2S 0x010B save and the S2C 0x010B answer are on [0x010B](opcodes/0x010B.md), the rename on
[0x0114](opcodes/0x0114.md).

### The car craft twin rows in 0x00C2

Our login burst shipped a 0x00C2 twin of each 0x0108 part on the same key 1000 to 7004, slot 7. No reader
needs them: the factory branch of `car_apply_kart_loadout` reads its parts from 0x0108 through the loadout
pairs, and 0x00C2 only for skin slot 2 (the antenna, 0x4A823C and the obfuscated path in 0x490A70, both
guarded), and `FUN_004A5ED0` the same. But keys 2000 to 2004 of the twins are the keys of
`common_char_head_001` to `005`, so `part_catalog_lookup` found the twin first and those five head parts
could not render. The twins are gone and the factory karts carry the stock paint and plate as default skins
like every burst kart row, so no default skin names a 0x0108 key any more.

### Room craft

| record | reader | rule | ours |
|---|---|---|---|
| 0x010C objects | 0x47FF70, cap 256 | strings 32 32 33, category 0 to 4, max placeable 1 or more, 1 or more price slots, the first resolves on a sold row | yes, a def with no price ships one inert zero slot |
| 0x010C props | `FUN_00488300` renders category 3 only for keys 4001 to 4032 (0xFA1 to 0xFC0) | a prop outside that range never renders | ours are 4001 to 4032, the burst props 8028 to 8059 could never render |
| 0x010D owned | 0x47D9A0, 48 bytes, cap 256 | an active row of category 0 1 2 4 must resolve in 0x010C, `FUN_00488300` hands the lookup plus 0x18 to sprintf with no null test | yes |
| 0x010E | 0x47D9D0 | no payload | yes |
| 0x010F ack | 0x47DA00 | only instance ids the client holds, a miss stores through address 0xC | yes |

### Pendant rows

0x0119 cap 64, strings 32 32 33, a hidden row last (`FUN_0046EDB0` counts hidden rows in its hit test).
0x011A one row per owned key, instance id unique. Both pass on the capture and on the builders.

## 5. Pendants

The client ships 13 pendants, `Icon/pendant_01` to `13` with `_00`, `_01` and `_s`, and `PENDANT_01` to
`13` in the text table. The box draws the visible rows, the stock box shows the owned ones with their icon
(the M, the 100 star, the level trophies) and a question mark on the others. Ours carries all 13 in
`pendant_def`, 13 hidden like chibikart. None is sold.

| key | icon | title | earn rule | granted by |
|---|---|---|---|---|
| 01 | M logo | Training Master | pass the tutorial, the rookie licence | 0x00A3 reward of licence key 2, catch up on login grade 1 or more |
| 02 to 05 | Rosie Chai Porki Dim Dim | the four pet pendants | finish quest level 5 10 15 20 | first clear of quest 5 10 15 20, the tail of 0x00F8 kind 3 case 7 (0x47E1F1), catch up at login and on the menu, see `systems/scenario.md` |
| 06 07 | 100 and 1000 star | Beginner Steering, Steering of Glory | race 100 and 1000 times | race end and login |
| 08 to 12 | trophies | Rookie Breaker to Racing Star | level 10 20 30 40 50 | level up after a race, a mission, a licence and a quest, and login |
| 13 | Mission Chapter 1 | hidden | clear the five missions of chapter 1 | first clear of the fifth, on 0x011B after the 0x008C board, and login |

Fixes: 02 to 05 and 13 had no source. 01 also came from the mission 2 item reward and from the blanket grant
of migration 037, 068 drops both and takes 01 back from characters with no licence grade. The level pendants
waited for the next race after a licence or quest level up, the licence and quest handlers now push them at
once. All grants run through `ProgressionHandler::grantEarnedPendants`, which also takes hidden rows now.
The pets stay ungated (`required_pendant_key` 0) like the captured real server, the client would refuse a
pet buy with MSG_NOT_CONDITION without the pendant.

## 6. The number plate

There is no plate text and no numbered plate in this build. The plate is one of 21 models
`Car/Parts/NAMEBOX_NORMAL` and `NAMEBOX_001` to `020`, picked like any kart part.

The "B3T4" plate of the official server video (`www.youtube.com/watch?v=ViGZzEWl5sE`) is one of them:
`Car/Parts/NAMEBOX_009.nif` names the texture `Car/Parts/betav.dds`, 256 by 128, a yellow plate with
"B3T4" painted in red and black between two stars. The real burst sells it as key 1109
(`PART_1109_TITLE`, visible, tier 4 options 2004 3004 1004), ours as 9109 on the Car tab, Number Plate
sub tab. The shipped text table is two lines off there: `PART_1108_TITLE` reads "BETA Plate" and
`PART_1109_TITLE` reads "UR2SLO", the text of `NAMEBOX_010` whose `betav01.dds` and `betav02.dds` spell
"UR2SLO". Wearing B3T4 is buying 9109 and installing it, nothing new on the server. Checked again in the
exe on 2026-09-23: the only text renderers are the GDI font atlas (`CreateFontA`, `TextOutW`) and the
sprite fonts, `O_NAME` (0x5EA950) is only an attach node name, no string names `NAMEBOX`, `betav` or
`M_NAME`, and neither the owned kart record (0x38 bytes, plate key at +0x0C) nor the profile carries a
text field for a plate:

- `M_NAME.ifl` is a texture flip list, 10 frames of `betav01.dds` then 11 of `betav02.dds`, the animated look
  of `NAMEBOX_010` (its nif names `betav01.dds` and `betav02.dds`). `M_NAME00` to `02.bmp` are loose textures no
  nif names. No reader in the exe touches `M_NAME`.
- `O_NAME` is the attach node, the second 64 byte cell of the node name table at 0x5EA910 (`O_PAINT`
  `O_NAME` `O_BODY`), indexed by the equip slot in `FUN_0048C680`. No code draws text into a plate texture.
- the shop Car tab sub tab `Shop/Common_Car_Tab_Number` and the garage tab list the 0x00C2 rows of equip slot 1
  (`shop_part_tab_filter` 0x418C80, `part_slot_tab_check` 0x415370).
- C2S 0x00B9 category 3 on a slot 1 part writes owned kart +0x0C, C2S 0x00BA puts the kart default +0x88 back.
  The room and race build read +0x0C through the 0x38 kart blob.

The real server sold 13 plates on tier 4 and hid `NAMEBOX_NORMAL` (key 1100, slot 1, the default of every
kart row) and 001 002 004 011 to 014. Ours sells 20. Our stock plate 9100 sat on slot 7, out of every tab and
out of the 0xB9 slot switch, 068 puts it on slot 1 like the burst, still hidden and never sold, so an owned
copy can be installed again. The server side of the plate is complete: the rows, the buy on tier 4, the owned
part row, the install into `owned_kart.skin_secondary` and the default 9100 on every new kart.

## 7. Tests and the image

`tests/server/test_mission_curve_items.cpp` `ItemTab071` checks the three Slot Exchange rows, the hidden
rows and the full repair kit of 071, `SlotExchange` the use count rule.

Tests: `tests/server/test_quest_mode.cpp` (the quest records, the result tails, the lock rule and the 069 seeds),
`tests/server/test_shop_catalogue.cpp` (the burst price table, the Astro rows, the coin options, the tier
of every sold row, the charge currency, the coin grant and the Gold Coin roll, the 068 seeds),
`tests/server/test_client_reader_rules.cpp` (the reader rules on the reference burst, on both package captures
and on the builders), and in `test_pendant_mission_creation.cpp` the quest and mission chapter pendant rules and
the 01 cleanup of 068.

Migration 068 was applied on a scratch MariaDB 11.1 loaded with a read only dump of the package shop tables:
30 price rows, drivers on 2001 3001 1001, karts on 2002 3002 1002, 370 worn parts on tier 3 and 58 kart parts
(9 paints, 20 plates, 29 antennas with the War Flag) on tier 4, pets on tier 5, the room and car craft option
words set, 4 items for sale, the plate 9100 on slot 1, mission 2 with no item, and a second apply left the
same rows. The package database itself was only read.

Reach the package only with a new image and migration 068 applied:

- the Astro charge of a sale price (`ShopPackets`, `ShopHandler`)
- the coin grant as a count (`ShopHandler`)
- the 0x0108 ability block (`CustomCarPackets::partDef`)
- no car craft twins in 0x00C2 and the stock default skins on factory karts (`GameServerLoginBurst.cpp`)
- the published kart rule of a preset kart (`CarCraftHandler::buildView`)
- the empty factory slot, the 9 char preset name and the craft rule (`CustomCarPackets`, `CarCraftHandler`) and
  migration 070, which empties the built slots that hold no factory chassis or tire, cuts the names and drops
  `uq_preset_kart`
- the pendant grants of 02 to 05 and 13, the live level pendants after a licence and a quest
- the quest mode (`ScenarioHandler`, `ScenarioPackets`, the login frames and the 0x011C and 0x00F5 routes) and
  migration 069, the 20 quests and their rival ghost rows
- migration 068: the burst price rows and tiers, the coin options, the hidden race items and slot changer, the
  repair scroll text keys, the War Flag, the stock plate on slot 1, the removed coin packs and three day
  rows, the 01 cleanup

Scripts of `tools/client_scripts` whose texts name a price will read other numbers after 068: `buy.txt` 500 to
400 gold, `pets.txt` 1800 to 2500, `paint.txt` 500 to 400, `gacha.txt` 3000 to 2500 for the Gold Coin, and the
Gacha Coin costs 1000 Astro. Their expects look at screens only.
