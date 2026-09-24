# CONTENT AUTHORING

Operator guide. How to add a character, a kart, a part, a map to the KnC emu.

Target client: `DevClient/KnC.exe` 2014 build. All offsets from `DevClient/KnC-new.exe.c`.

Scope: what to put in the DB, what must exist on disk, what breaks when it does not.

## 1. HOW CONTENT REACHES THE CLIENT

Client holds no hardcoded content. Every character, kart, part, track, theme comes
from a definition packet burst at session start.

Recommended burst order:

```
1. 0xBE   clear all catalogs
2. 0xC4   theme rows
3. 0xC3   track rows
4. 0xBF   character rows
5. 0xC2   skin rows
6. 0xC0   kart rows
7. 0x108  part rows
8. 0xC5   license rows    (ids 10 11 12 13 MANDATORY)
9. 0x87   mission rows
10. 0x1B 0x1C 0x1D 0x1E   owned instances
```

Only two ordering rules are hard:

| rule | consequence of breaking it |
|------|----------------------------|
| every catalog before every owned instance | unchecked deref crashes on the first garage or race build |
| whole burst before the client opens a list UI | list builds against a half filled catalog |

The rest of the order is convenience. Each catalog is an independent container.

Never send `0xBE` while a player sits in lobby, room picker or ghost menu.
That wipes the catalogs under the UI and several lookup sites deref NULL.

### Catalog opcode map

| op | dec | handler | catalog | stride | cap | key off |
|----|-----|---------|---------|--------|-----|---------|
| 0xBE | 190 | sub_478B50 | clear 20 containers | | | |
| 0xBF | 191 | sub_47F390 | character | 0xD8 | 32 | +0x0C |
| 0xC0 | 192 | sub_47F4F0 | kart | 0x140 | 64 | +0x08 |
| 0xC1 | 193 | sub_47F6B0 | accessory item | 0xC4 | 64 | +0x08 |
| 0xC2 | 194 | sub_47F800 | skin BodySet | 0xDC | 512 | +0x08 |
| 0xC3 | 195 | sub_47F990 | track | 0x8C | 128 | +0x04 |
| 0xC4 | 196 | sub_478C40 | theme | 0x4C | 16 | +0x04 |
| 0xC5 | 197 | sub_47FAE0 | license test | 0xA4 | | +0x04 |
| 0x87 | 135 | sub_47DC10 | mission | 0xBC | 20 | |
| 0x108 | 264 | sub_480210 | part | 0x120 | 256 | +0x08 |

Owned instance opcodes:

| op | dec | handler | container | stride | cap | note |
|----|-----|---------|-----------|--------|-----|------|
| 0x1B | 27 | sub_478EC0 | 0x01A56BD0 | 0x2C | 64 | u32 count then records |
| 0x1C | 28 | sub_478F20 | 0x01A576D8 | 0x38 | 64 | u32 count, race path reads this one |
| 0x1D | 29 | sub_478F80 | 0x01A5A0E8 | 0x1C | 64 | u32 count, NOT cleared by 0xBE |
| 0x1E | 30 | sub_478FE0 | 0x01A584E0 | 0x1C | 256 | ONE record, NO count, upsert by +0x04 |

`0x1D` and `0x1E` are two different tables. Writing `0x1E` does not update `0x1D`.

`0x87` mission is also one record per packet with no count prefix.

### Rules that apply to every catalog row

| rule | why |
|------|-----|
| String field width is hard | sub_44EB30 copies until NUL with no clamp, longer name smashes the handler stack |
| Loc key must be under 32 chars | sub_4E1B70 row key column is 32 bytes, longer key can never match |
| Owned instance must reference a published catalog id | unchecked deref, instant access violation |
| Send catalogs before owned instances | same reason |
| Enabled flag 0 hides the row | filter runs before every list build |

## 2. ASSET RESOLUTION

Models, `sub_444A20`:

```
./Data/<lang>/<rel>.nif       first
./Data/Public/<rel>.nif       fallback
```

Images, `sub_441450` reached through `sub_441720` which prepends `Image/`:

```
./Data/<lang>/Image/<rel>
./Data/Public/Image/<rel>
./Define/<lang>/Image/<rel>
```

`<lang>` is `Eng` in this install. Returns 0 on total miss.

Windows filesystem is case insensitive and the client relies on it. Shipped files
mix `geometry.nif` with `Geometry.nif`, `track.col` with `track.COL`, `light.nif`
with `Light.nif`, and the icon dir `Image/parts` with `Image/Parts`. All resolve.
Do not port this content tree to a case sensitive filesystem.

### Localization

| file | encoding | records |
|------|----------|---------|
| `Define/Eng/def_trans_index.txt` | 8 bit ASCII, NO BOM, CRLF | 1466 |
| `Define/Eng/def_trans_message.txt` | UTF 16LE with FF FE BOM, CRLF | 1466 |

Record 0 is a header. Index record 0 is `INDEX`. Message record 0 is `Eng`.
Records pair positionally after that.

Emit the index file as ANSI. `sub_4E1B70` matches with `strcmpi` on 8 bit chars.
UTF 16 index never matches.

Miss is soft. Client displays the raw key text.

## 3. RECIPE: ADD A CHARACTER

### 3.1 Server only, reuse a shipped body

Ten driver bodies ship with `body.nif` and `body.kfm`:

```
Cosmo Monster Moriko Mummy Prince Princess Pumpkin Witch Wolf Yuk
```

Insert one row in `character_def`. Wire order of `0xBF`, exactly:

| # | type | rec off | field | width |
|---|------|---------|-------|-------|
| 1 | i32 | +0x00 | enabled flag | |
| 2 | i32 | +0x04 | UNPROVEN, send 0 | |
| 3 | i32 | +0x08 | UNPROVEN, send 0 | |
| 4 | i32 | +0x0C | CHARACTER ID, primary key | |
| 5 | i32 | +0x10 | UNPROVEN, send 0 | |
| 6 | cstr | +0x14 | model folder name | 36 |
| 7 | 20 raw bytes | +0x38 | five i32 default skin ids | |
| 8 | cstr | +0x4C | loc key | 33 |
| 9 | cstr | +0x6D | loc key 2 | 35 |
| 10 | u32 | | price entry count | |
| 11 | count x 16 bytes | +0x90 | price entries | |

The five default skin ids at +0x38 map to attach nodes in this order:

| index | rec off | node |
|-------|---------|------|
| 0 | +0x38 | O_BODY |
| 1 | +0x3C | O_FACE |
| 2 | +0x40 | O_HEAD |
| 3 | +0x44 | O_GLASS |
| 4 | +0x48 | O_BACK |

Node name table lives at `KnC.exe` file offset 0x1EA910, stride 64:

```
0 O_PAINT  1 O_NAME  2 O_BODY  3 O_FACE
4 O_HEAD   5 O_GLASS 6 O_BACK  7 O_GRAB_R
```

Character id drives the voice bank. `sub_48CBB0` switches on +0x0C with cases:

```
10 20 30 40 50 60 70 80
```

Any other id loses two voice clips and nothing else. `sub_448890` guards with
`a3 > 0xE2` unsigned so the default of `-1` is rejected cleanly. No crash.

### 3.2 Character asset paths

| purpose | path template | source |
|---------|---------------|--------|
| body | `Driver/Body/High/%s/body` | charRec+0x14 |
| body room variant | `Driver/Body/High/%s/body_room` | when dword_B2360C == 9 |
| facial | `Driver/Facial/%s/%s.dds` | charRec+0x14, expression name |
| accessory high | `Driver/Body/High/%s/BodySet/%s` | charRec+0x14, skinRec+0x10 |
| accessory low | `Driver/Body/Low/%s/BodySet/%s` | preload only, result discarded |
| seat offsets | `./Define/Driver/driver_pos_%s.ini` | charRec+0x14 |
| shop icon | `Image/parts/driver_%s_%02d.png` | %02d loops 1 2 3 4 |

Expression names live at `KnC.exe` file offset 0x1EAB50, stride 128. First ten
are walked by the facial preloader and every result is discarded:

```
MT_IMMO_IDLE   MT_IMMO_SMILE   MT_IMMO_CRY   MT_IMMO_ANGRY  MT_IMMO_DING
MT_IMMO_KIDDING MT_IMMO_CUTE   MT_IMMO_LOVE  MT_IMMO_SLEEP  MT_IMMO_SNEER
```

`driver_pos_<name>.ini` is read with `GetPrivateProfileStringA`. Section name is
the kart chassis name from kartRec+0x20. Keys are `x` `y` `z`. Missing section
makes `sub_48C480` return 0 and the driver sits at the kart origin.

All ten shipped ini files carry sections for all five FactoryCar chassis.

### 3.3 Default skin ids are dangerous

`sub_48CBB0` checks the result of `sub_48C9C0`. That ANDs five `sub_48C680` calls.
`sub_48C680` returns `sub_443D10` which returns 0 when a non NULL path fails on disk.

| default skin id | result |
|-----------------|--------|
| 0 or absent from 0xC2 catalog | node left empty, driver spawns fine |
| present in 0xC2, mesh file missing on disk | WHOLE DRIVER SPAWN ABORTS |

So a dangling id is safe. A published id pointing at a missing file is fatal.

Only assign a default skin whose `.nif` exists under that exact character folder.

### 3.4 Shipped BodySet inventory

418 distinct mesh basenames across the ten characters.

| character | BodySet nif | common_char_ nif | Facial dir | Body/Low dir | icons 01..04 |
|-----------|-------------|------------------|------------|--------------|--------------|
| Cosmo | 50 | 47 | ABSENT | ABSENT | 01 02 03 06, no 04 |
| Monster | 105 | 50 | yes | yes | full |
| Moriko | 51 | 48 | ABSENT | ABSENT | 01 02 only |
| Mummy | 93 | 50 | yes | yes | full |
| Prince | 88 | 51 | yes | yes | full |
| Princess | 105 | 51 | yes | yes | full |
| Pumpkin | 106 | 51 | yes | yes | full |
| Witch | 104 | 51 | yes | yes | full |
| Wolf | 85 | 50 | yes | yes | full |
| Yuk | 81 | 50 | yes | yes | full |

`Data/Public/Driver/Body/Low` also holds a folder named `Yok`. Dead typo, no
character folder matches it.

51 distinct `common_char_*` names exist. Only 47 are present in all ten folders.
Prince, Princess, Pumpkin, Witch have all 51. Never assign a "universal" common
accessory as a DEFAULT skin on Cosmo or Moriko without checking the file.

Mesh name tokens, useful to pick the right node:

| token | distinct meshes | node |
|-------|-----------------|------|
| `_char_body_` | 138 | O_BODY |
| `_char_head_` | 184 | O_HEAD |
| `_char_face_` | 10 | O_FACE |
| `_char_glass_` | 42 | O_GLASS |
| `_char_back_` | 39 | O_BACK |
| `_char_hat_` | 1 | O_HEAD |

Four names break the scheme and must be classified by hand:

```
monster_char_bady_015   mummy_char_hair_001
princess_char_cap_010   pumpkin_char_body015
```

### 3.5 Loc keys for characters

Shipped keys are `DRIVER_<NAME>_TITLE` and `DRIVER_<NAME>_INFO`.

`DRIVER_COSMO_TITLE` `DRIVER_COSMO_INFO` `DRIVER_MORIKO_TITLE` `DRIVER_MORIKO_INFO`
exist but their message values are the key strings themselves. Those two render
as raw text until you fix `def_trans_message.txt`.

### 3.6 Brand new character needs a launcher

Files that must ship for a body that is not one of the ten:

```
./Data/Public/Driver/Body/High/<Name>/body.nif
./Data/Public/Driver/Body/High/<Name>/body.kfm
./Data/Public/Driver/Body/High/<Name>/body_O_MAIN_MT_*.kf     (18 to 65 clips)
./Data/Public/Driver/Body/High/<Name>/BODYSET/*.nif
./Data/Public/Driver/Body/High/<Name>/BODYSET/*.dds
./Data/Public/Driver/Facial/<Name>/MT_IMMO_*.dds              (optional)
./Data/Public/Driver/Body/Low/<Name>/BodySet/*.nif            (optional)
./Define/Driver/driver_pos_<name>.ini                         (5 chassis sections)
./Data/Public/Image/Parts/driver_<Name>_01..04.png
```

## 4. RECIPE: ADD A KART

### 4.1 Only one scheme exists

`sub_430420` always builds `Car/FactoryCar/CHASSIS/%s/body` from kartRec+0x20.
There is no `.car` loader and no `Car/Body/` loader in this binary.

`DevClient/Data/Car/*.car` and `Data/Public/Car/Body/{High,Low}` are DEAD assets
from an older client. Ignore them.

Five chassis ship:

```
Circler  Firedragon  Quatzalcuatl  Squarer  Striper
```

Each carries every slot folder plus five texture sets:

```
Car/FactoryCar/CHASSIS/<Name>/BODY.nif
Car/FactoryCar/COVER/<Name>/COVER.nif
Car/FactoryCar/BOOSTER/<Name>/BOOSTER.nif  turbo.nif
Car/FactoryCar/BUMPER/<Name>/BUMPER.nif
Car/FactoryCar/WING/<Name>/WING.nif
Car/FactoryCar/TIRES/<Name>/WHEEL1..4.nif
Car/FactoryCar/F_FENDER/<Name>/F_FENDER01..02.nif
Car/FactoryCar/R_FENDER/<Name>/R_FENDER01..02.nif
Car/FactoryCar/Effect/<Name>/turbo.nif
Car/FactoryCar/Texture/<Name>
Car/FactoryCar/Texture/<Name>_Basic
Car/FactoryCar/Texture/<Name>_Unique
Car/FactoryCar/Texture/<Name>_Epic
Car/FactoryCar/Texture/<Name>_Legend
```

Roughly 40 legacy `kart_*.png` icons exist for Tank, TOILET, Mini, Bike_01..06,
FR_01..05, Basic_1..6 and more. They have no FactoryCar geometry. Publishing them
gives a shop entry whose model load fails.

### 4.2 Kart wire format

`0xC0` wire order. Note the two 8 byte reads come BEFORE the price count even
though they land at a HIGHER record offset:

| # | type | rec off | field | width |
|---|------|---------|-------|-------|
| 1 | i32 | +0x00 | flag, shop filters on nonzero | |
| 2 | i32 | +0x04 | UNPROVEN, send 0 | |
| 3 | i32 | +0x08 | KART ID, primary key | |
| 4 | u8 | +0x0C | one byte on wire, four in record | |
| 5 | i32 | +0x10 | UNPROVEN, send 0 | |
| 6 | i32 | +0x14 | SCHEME, send 1 for FactoryCar | |
| 7 | i32 | +0x18 | UNPROVEN, send 0 | |
| 8 | i32 | +0x1C | UNPROVEN, send 0 | |
| 9 | cstr | +0x20 | CHASSIS NAME | 33 |
| 10 | cstr | +0x41 | loc key | 33 |
| 11 | cstr | +0x62 | loc key 2 | 34 |
| 12 | 32 raw bytes | +0x84 | eight i32 DEFAULT PART IDS | |
| 13 | 68 raw bytes | +0xA4 | stat block, 17 i32 | |
| 14 | 8 raw bytes | +0x130 | UNPROVEN, send 0 | |
| 15 | 8 raw bytes | +0x138 | UNPROVEN, send 0 | |
| 16 | u32 | | price entry count | |
| 17 | count x 16 bytes | +0xE8 | price entries | |

Record total 0x140. Cap 64 rows.

The eight ids at +0x84 are copied verbatim into a new owned kart 0x1C record.
Every one of them must exist in the `0x108` part catalog published in the same
burst. A missing one crashes `sub_430420`.

### 4.3 Texture tier

Tier ladder, signed compare, verified in ASM at 0x43058c and 0x4306e1:

| grade value | texture suffix |
|-------------|----------------|
| negative | `_Basic` |
| 0 to 4 | `_Basic` |
| 5 to 19 | `_Unique` |
| 20 to 64 | `_Epic` |
| 65 and up | `_Legend` |

Chassis tier uses the AVERAGE of the fitted parts grade at ownedPart+0x80.
Zero sum or zero count forces `_Basic`.

Every one of the seven part models re run the SAME ladder on its OWN +0x80.
So one build can mix tiers per piece.

### 4.4 Kart failure modes

| condition | result |
|-----------|--------|
| chassis name has no `CHASSIS/<name>/BODY.nif` | soft in garage preview, `Car initialize fail !` box on race entry |
| owned kart references an unpublished 0xC0 id | ACCESS VIOLATION in sub_430420 |
| owned part references an unpublished 0x108 id | ACCESS VIOLATION in sub_430420 |
| icon png missing | nothing drawn, no crash |
| loc key missing | raw key text shown |

Three of the five `sub_430420` call sites discard the return value. The kart just
renders with missing pieces there. Two propagate it into the race init path.

## 5. RECIPE: ADD A PART

### 5.1 Parts are keyed by chassis name

`partRec+0x14` is used TWICE. Once as the folder under `Car/FactoryCar/<SLOT>/`
and once as the texture set prefix. It MUST equal one of the five chassis names.

`sub_48F8D0` compares `kartRec+0x20` against `partRec+0x14` with `strcmp`.
Case sensitive. Mismatch means the part is rejected as incompatible.

There are 5 chassis x 7 slots = 35 distinct part meshes. Nothing more. Variety
comes from the four grade tiers and the stat block.

### 5.2 Part wire format

| # | type | rec off | field | width |
|---|------|---------|-------|-------|
| 1 | i32 | +0x00 | flag | |
| 2 | i32 | +0x04 | UNPROVEN, send 0 | |
| 3 | i32 | +0x08 | PART ID, primary key | |
| 4 | i32 | +0x0C | SLOT TYPE 0 to 6 | |
| 5 | i32 | +0x10 | UNPROVEN, send 0 | |
| 6 | cstr | +0x14 | CHASSIS NAME | 33 |
| 7 | cstr | +0x35 | loc key | 33 |
| 8 | cstr | +0x56 | loc key 2 | 34 |
| 9 | 68 raw bytes | +0x78 | stat block, 17 i32 | |
| 10 | 8 raw bytes | +0xBC | UNPROVEN, send 0 | |
| 11 | 8 raw bytes | +0xC4 | UNPROVEN, send 0 | |
| 12 | 12 raw bytes | +0xCC | UNPROVEN, send 0 | |
| 13 | u32 | | price entry count | |
| 14 | count x 16 bytes | +0xD8 | price entries | |

Record total 0x120. Cap 256 rows.

### 5.3 Slot table

| slot | name | model path | files |
|------|------|------------|-------|
| 0 | cover | `Car/FactoryCar/COVER/%s/COVER` | 1 |
| 1 | booster | `Car/FactoryCar/BOOSTER/%s/BOOSTER` | 1 |
| 2 | tires | `Car/FactoryCar/TIRES/%s/WHEEL%d` | 4, %d is 1 to 4 |
| 3 | f_fender | `Car/FactoryCar/F_FENDER/%s/F_FENDER%02d` | 2, %02d is 01 02 |
| 4 | r_fender | `Car/FactoryCar/R_FENDER/%s/R_FENDER%02d` | 2 |
| 5 | bumper | `Car/FactoryCar/BUMPER/%s/BUMPER` | 1 |
| 6 | wing | `Car/FactoryCar/WING/%s/WING` | 1 |

Icons: `Image/parts/<name>_<slot>_<B|U|E|L>_%02d.png` with %02d 1 to 2.
Example `Image/Parts/Quatzalcuatl_booster_E_01.png`. 386 such files ship.

### 5.4 Part id convention already used by the shipped loc keys

The shipped `def_trans_index.txt` uses `(slot + 1) * 1000 + chassisIndex`:

| slot | key family | id range |
|------|-----------|----------|
| 0 cover | `COVER_<id>_TITLE` | 1000 to 1004 |
| 1 booster | `BOOSTER_<id>_TITLE` | 2000 to 2004 |
| 2 tires | `TIRES_<id>_TITLE` | 3000 to 3004 |
| 3 f_fender | `F_FENDER_<id>_TITLE` | 4000 to 4004 |
| 4 r_fender | `R_FENDER_<id>_TITLE` | 5000 to 5004 |
| 5 bumper | `BUMPER_<id>_TITLE` | 6000 to 6004 |
| 6 wing | `WING_<id>_TITLE` | 7000 to 7004 |

Chassis index 0 to 4 by the shipped display names:

| index | chassis loc key | display name | folder |
|-------|-----------------|--------------|--------|
| 0 | `CAR_CHASSIS_01_TITLE` | Red Dragon Chassis | Firedragon |
| 1 | `CAR_CHASSIS_02_TITLE` | Speed Star Racer Chassis | UNPROVEN |
| 2 | `CAR_CHASSIS_03_TITLE` | Whirlwind Chassis | UNPROVEN |
| 3 | `CAR_CHASSIS_04_TITLE` | Electric Eel Chassis | Quatzalcuatl UNPROVEN |
| 4 | `CAR_CHASSIS_05_TITLE` | RS 78 Buggy Chassis | UNPROVEN |

Only index 0 to Firedragon is safe by name. The other four map to Circler,
Squarer, Striper in an order not proven anywhere. Verify in game before locking
the ids, or ship your own loc keys and skip the convention.

### 5.5 A new part mesh is really a new chassis

Slot folders are fixed and keyed by chassis name. You cannot add an eighth slot
and you cannot add a mesh under an existing chassis. A new part mesh means a new
`Car/FactoryCar/<SLOT>/<Name>/` set for all seven slots plus five texture sets
plus a `[<Name>]` section in all ten `driver_pos_*.ini` files.

## 6. RECIPE: ADD A MAP

### 6.1 Theme first

`0xC4` wire format. Total 0x4C, cap 16 rows:

| # | type | rec off | field | width |
|---|------|---------|-------|-------|
| 1 | i32 | +0x00 | enabled flag | |
| 2 | i32 | +0x04 | THEME ID, primary key | |
| 3 | cstr | +0x08 | THEME FOLDER NAME | 33 |
| 4 | cstr | +0x29 | loc key | 35 |

Three theme ids are sentinels:

| theme id | ghost strip | make room theme strip | make room track list | extra |
|----------|-------------|-----------------------|----------------------|-------|
| 10000000 | hidden | shown | shown | |
| 20000000 | hidden | hidden | its tracks DROPPED | |
| 30000000 | hidden | shown | shown | becomes the initially selected theme |

Theme 30000000 is also the theme scanned by `sub_453140` for the game mode 4
default track. Its record +0x04 is tested at about 15 sites in race and HUD code.
Do not reuse these three ids for ordinary content.

Ghost strip honours the theme enabled flag. Make room theme strip IGNORES it.

Make room theme strip does NOT drop a theme whose `thema_*.png` is missing. The
tile is blank and still selectable.

### 6.2 Track row

`0xC3` wire format. Total 0x8C exactly. Cap 128 rows.

There is NO price count and NO price entries on this packet. Stop after the
second string. Appending anything desyncs the reader for the rest of the stream.

| # | type | rec off | field | width |
|---|------|---------|-------|-------|
| 1 | i32 | +0x00 | enabled flag | |
| 2 | i32 | +0x04 | TRACK ID, primary key and thumbnail key | |
| 3 | i32 | +0x08 | THEME ID, FK into 0xC4 | |
| 4 | cstr | +0x0C | MAP FOLDER NAME | 36 |
| 5 | i32 | +0x30 | to dword_5EB6F0, fog or ambient UNPROVEN | |
| 6 | i32 | +0x34 | to dword_5EB6F4, UNPROVEN | |
| 7 | i32 | +0x38 | to dword_5EB6F8, UNPROVEN | |
| 8 | i32 | +0x3C | UNPROVEN, send 0 | |
| 9 | i32 | +0x40 | UNPROVEN, send 0 | |
| 10 | i32 | +0x44 | DIFFICULTY, see below | |
| 11 | i32 | +0x48 | REQUIRED LICENSE, vs signed byte_1A20B08 | |
| 12 | i32 | +0x4C | MODE GATE, see below | |
| 13 | i32 | +0x50 | to sub_4B0DA0, BGM UNPROVEN | |
| 14 | i32 | +0x54 | UNPROVEN, send 0 | |
| 15 | i32 | +0x58 | UNPROVEN, send 0 | |
| 16 | i32 | +0x5C | UNPROVEN, send 0 | |
| 17 | i32 | +0x60 | UNPROVEN, send 0 | |
| 18 | i32 | +0x64 | UNPROVEN, send 0 | |
| 19 | cstr | +0x68 | loc key | 36 |

`dword_5EB6FC` is hardcoded to 1134559232, which is 350.0f. Not server settable.

DIFFICULTY is a 6 star scale with a +1 offset. The draw loop is
`do { draw filled } while (v12 <= rec[0x44]);` then pads empties while `v12 < 6`.

| field value | stars drawn |
|-------------|-------------|
| negative | 6 empty |
| 0 | 1 filled, 5 empty |
| 1 | 2 filled, 4 empty |
| 4 | 5 filled, 1 empty |
| 5 | 6 filled, 0 empty |
| above 5 | more than 6 filled, no pad, overflows the strip |

Use 0 to 5. Do not use 1 to 5 thinking it is 1 to 5 stars.

MODE GATE: the make room track list requires
`dword_BCE210 > 1 || rec[0x4C] <= 0`. So a track with a positive +0x4C is hidden
in the low mode values 0 and 1. Exact meaning of `dword_BCE210` is UNPROVEN.

### 6.3 Track loc key convention traps you

Client shows `translate(key)` for the name and `translate(key + "_INFO")` for the
description. The shipped keys ALREADY end in `_INFO`.

```
key to publish       : TRACK_COOKIE_01_INFO
name row in def_trans: TRACK_COOKIE_01_INFO       -> "Cream Town"
info row in def_trans: TRACK_COOKIE_01_INFO_INFO  -> "Easy track,\n..."
```

Publishing `TRACK_COOKIE_01` gives raw key text in BOTH slots.

Theme keys are `THEME_<NAME>_INFO` with the same doubling.

### 6.4 The thumbnail gate is DECISIVE

Make room track picker inserts a track into the selectable array ONLY IF
`Image/Popup/SelectTrack/track_<mapfolder>.png` loaded:

```c
sprintf(Buffer, "Popup/SelectTrack/track_%s.png", (const char *)(v9 + 12));
if ( sub_441720(..., Buffer, a2) ) { insert; ++count; }
```

| thumbnail | world folder | make room | ghost mode | entering |
|-----------|--------------|-----------|------------|----------|
| present | present | listed | listed | plays |
| MISSING | present | SILENTLY DROPPED | listed | plays |
| present | MISSING | listed | listed | `Track initialize fail !` box |
| missing | missing | dropped | listed | `Track initialize fail !` box |

Ghost mode lists every enabled track with no thumbnail check and no mode gate.

Make room per theme cap is 55 tracks. Theme index cap is 12.

### 6.5 Mandatory disk set for a track

`sub_4875C0(worldObj, trackId)` returns 0 on ANY of these. All three callers then
show a `MessageBoxA` titled `Chibi Kart` with text `Track initialize fail !` and
abort the race entry. There is no fallback map.

| # | requirement | loader |
|---|-------------|--------|
| 1 | 0xC3 row for the track id | sub_4531F0 |
| 2 | 0xC4 row for the track theme id | sub_452FB0 |
| 3 | `World/<theme>/<map>/track.nif` | sub_445250 |
| 4 | `World/<theme>/<map>/geometry.nif` | sub_4450D0 |
| 5 | `./Data/Public/World/light.nif` | sub_443C10 |
| 6 | `./Data/Public/World/<theme>/<map>/track.col` | sub_485580 |
| 7 | `./Data/Public/World/<theme>/<map>/start.ini` with >= 1 row | sub_48A800 |
| 8 | whatever `sub_4D4180` needs, UNPROVEN | sub_4D4180 |
| 9 | `World/<theme>/<map>/sky_night.nif` OR `sky.nif` | sub_445250 |

Texture search dir is `World/<theme>/Texture/Low`.

`sky_night.nif` is tried first only when `dword_B2319C == 1`, then falls back to
`sky.nif`. Shipping only `sky.nif` is fine.

Genuinely optional, return value discarded:

```
camera.nif   track1.COL .. track8.COL   regen.ini   boost.ini
camera.ini   minimap.ini  minimap.nif   warp.ini    itembox.ini
itembite.ini itemdrum.ini itemball.ini  itemMoney.ini  follow_%02d.ini
```

`Cookie_02` and `Cookie_04` ship with no `regen.ini` and load fine.

### 6.6 World ini formats

Read through the pak VFS, decompressed to a scratch file named `dx8_rlg.dll` in
the client dir, then parsed with `fscanf`. Max size 40960 bytes.

| file | format | rows | required |
|------|--------|------|----------|
| `start.ini` | `%f,%f,%f,%f\r\n` grid x y z yaw | max 100, min 1 | YES |
| `regen.ini` | `%f,%f,%f,%f\r\n` | max 100 | no |
| `minimap.ini` | `%f,%f,%f\r\n` | 1 | no |

Plain 8 bit ASCII, CRLF. Example first row of `Cookie_01/start.ini`:

```
-78.3,43.442,-8.521,0.00
```

### 6.7 Shipped racing maps that are complete

35 map folders carry track, geometry, sky, track.col, start.ini:

| theme folder | map folders |
|--------------|-------------|
| Cookie | Cookie_01 Cookie_02 Cookie_03 Cookie_04 |
| Desert | Desert_01 Desert_02 Desert_03 Desert_04 |
| Devil | Devil_01 Devil_02 Devil_03 Devil_04 Devil_07 |
| Forest | Forest_01 Forest_02 Forest_03 Forest_04 |
| Palace | Palace_01 Palace_02 Palace_03 Palace_04 Palace_05 |
| Race | Race_01 Race_02 |
| Snow | Snow_01 Snow_02 Snow_03 Snow_04 |
| Swamp | Swamp_01 Swamp_02 Swamp_03 |
| Toy | Toy_01 Toy_02 Toy_03 Toy_04 |

All 35 have a `track_<map>.png` thumbnail.

33 of the 35 have a `TRACK_<MAP>_INFO` loc key. `TRACK_PALACE_04_INFO` and
`TRACK_DEVIL_07_INFO` are ABSENT and those two render raw key text.

Theme thumbnails that ship, 12 total:

```
thema_cookie  thema_desert  thema_devil  thema_forest  thema_Palace
Thema_Race    thema_snow    thema_swamp  thema_toy
thema_battle  thema_lava    thema_random
```

`battle`, `lava`, `random` have no World folder. Theme only, no tracks.

### 6.8 TRAP thumbnails, do not publish

13 `track_*.png` files ship with NO matching World folder:

```
Devil_05  Devil_06  Swamp_04  Swamp_05  battle_01  beach_06  lava_01
bonus_2   bonus_3   bonus_4   bonus_5   bonus_6    random
```

Publish a 0xC3 row for any of these and the track appears, is selectable, then
throws `Track initialize fail !` on entry.

### 6.9 License and Mission maps are NOT reachable through 0xC3

License worlds are built from the license test id, not from a folder name:

| license id | path |
|------------|------|
| below 10 | `World/License/License_%.2d/track` with index `id / 10 + 1` |
| 10 11 13 | `World/License/License_%.2d/nif/...` |
| 12 | `World/License/License_02/nif_jump/...` |
| 20 23 | `World/License/License_%.2d/01/...` |
| 21 22 | `World/License/License_%.2d/02/...` |
| default | `World/License/License_%.2d/...` flat |

Shipped: `License_01` flat, `License_02/{nif,nif_jump}`, `License_03/{01,02}`.

Mission worlds come from the `0x87` catalog. Folder name is at record +0x38 and
feeds `World/Mission/%s/{track,geometry,sky,minimap}`.
Shipped: `Mission_01` to `Mission_05`, all five complete.

## 6B. ACCESSORY ITEM CATALOG 0xC1

Not one of the four recipes but it rides the same burst. Included so a server
author does not desync the stream guessing at it.

`0xC1` wire format. Data ends at +0xC0. `sub_450750` copies 0xC4 so the last
4 bytes of every record are uninitialised stack. Cap 64 rows, key at +0x08.

| # | type | rec off | field | width |
|---|------|---------|-------|-------|
| 1 | i32 | +0x00 | flag | |
| 2 | i32 | +0x04 | UNPROVEN, send 0 | |
| 3 | i32 | +0x08 | ITEM ID, primary key | |
| 4 | i32 | +0x0C | UNPROVEN, send 0 | |
| 5 | i32 | +0x10 | UNPROVEN, send 0 | |
| 6 | cstr | +0x14 | icon name | 33 |
| 7 | cstr | +0x35 | loc key | 33 |
| 8 | cstr | +0x56 | loc key 2 | 34 |
| 9 | u32 | | price entry count | |
| 10 | count x 16 bytes | +0x78 | price entries | |

Only known asset path is the icon `Image/Parts/item_%s_%02d.png` from +0x14.
No model path found. UNPROVEN whether these are race items or wearables.

## 7. CRASH LIST

Every one of these is an unchecked NULL deref that access violates. Prevent them
in the DB with a foreign key or a join time filter.

| site | trigger | address |
|------|---------|---------|
| sub_430420 chassis | owned kart +0x04 has no 0xC0 row, reads from 0x20 | 0x0043046e |
| sub_430420 cover | owned part +0x04 has no 0x108 row, reads from 0x14 | 0x004306c5 |
| sub_430420 booster | same | 0x004307a5 |
| sub_430420 tires | same | 0x00430891 |
| sub_430420 f_fender | same | 0x00430a00 |
| sub_430420 r_fender | same | 0x00430b11 |
| sub_430420 bumper | same | 0x00430c18 |
| sub_430420 wing | same | 0x00430cfe |
| license menu | no 0xC5 row for id 10 11 12 13, reads +0x30 and +0x34 | 154219 154681 155214 155758 |
| kart lookup | unguarded `sub_44F6F0(...) + 65` | 166418 |
| kart lookup | unguarded `sub_44F6F0(...) + 8` | 167287 |
| track lookup | `sub_4531F0(...) + 72` after a mid UI 0xBE wipe | 174044 203646 217183 217663 |

Hardcoded ids the client expects on some bootstrap paths:

```
character 20  character 30
kart 11  kart 40  kart 600
license 10 11 12 13
theme 30000000  (game mode 4 default track lookup)
```

## 8. FAILURE MODE SUMMARY

| what is missing | severity | symptom |
|-----------------|----------|---------|
| track thumbnail png | silent | track dropped from make room, still in ghost |
| theme thumbnail png | cosmetic | blank tile, still selectable |
| any of the 9 mandatory world files | HARD | `Track initialize fail !` modal, race aborts |
| loc key row | cosmetic | raw key text shown |
| catalog icon png | cosmetic | nothing drawn |
| skin id absent from 0xC2 catalog | soft | node left empty |
| skin present in catalog, mesh file missing | HARD | driver spawn aborts |
| chassis BODY.nif | mixed | soft in garage, `Car initialize fail !` on race entry |
| part mesh file | soft | that piece missing on the kart |
| 0xC5 row for id 10 11 12 13 | CRASH | license menu deref NULL |
| owned instance pointing at unpublished catalog id | CRASH | access violation |
| name longer than the field width | CRASH | handler stack smash |

## 9. WHAT NEEDS NO CLIENT PATCH

All of this is DB rows plus the existing opcodes. No launcher, no new files.

| content | pool | cap |
|---------|------|-----|
| characters | 10 shipped bodies | 32 rows |
| skins and accessories | 418 shipped BodySet meshes | 512 rows |
| karts | 5 chassis, many priced or stat variants each | 64 rows |
| parts | 5 chassis x 7 slots = 35 meshes, 4 grade tiers | 256 rows |
| tracks | 35 complete maps | 128 rows |
| themes | 9 with tracks, 3 thumbnail only | 16 rows |
| missions | Mission_01 to Mission_05 | 20 rows |

Skins are by far the largest free content pool.

## 10. WHAT FORCES A LAUNCHER

| goal | files that must ship |
|------|----------------------|
| new character | body nif kfm, ~40 kf clips, BodySet, Facial dds, driver_pos ini, 4 icons |
| new chassis | 9 slot folders, 5 texture sets, 2 kart icons, 28 part icons, section in all 10 driver_pos ini |
| new part mesh | impossible alone, it is a new chassis |
| new track | 9 mandatory world files, Texture/Low, `track_<map>.png` MANDATORY |
| new theme | World folder, Texture/Low, `thema_<theme>.png` |
| any display name | new rows in `def_trans_index.txt` AND `def_trans_message.txt` |

## 11. CHECKLISTS

### Add a character

```
[ ] model folder is one of the 10 shipped, or all launcher files present
[ ] character id in {10 20 30 40 50 60 70 80} for voice clips
[ ] model folder string under 36 chars
[ ] loc key under 32 chars and present in def_trans_index.txt
[ ] each default skin id is 0, or its mesh nif exists in THAT char BODYSET
[ ] Image/Parts/driver_<Name>_01..04.png all present, or accept blanks
[ ] Define/Driver/driver_pos_<name>.ini has a section per chassis
[ ] row count including this one is <= 32
```

### Add a kart

```
[ ] chassis name is exactly one of Circler Firedragon Quatzalcuatl Squarer Striper
[ ] scheme field at +0x14 is 1
[ ] all 8 default part ids exist in the 0x108 burst
[ ] chassis name string under 33 chars
[ ] wire order: two 8 byte blocks BEFORE the price count
[ ] row count <= 64
```

### Add a part

```
[ ] part name strcmp equals a kart chassis name, case sensitive
[ ] slot type in 0..6
[ ] the model file for that slot and chassis exists
[ ] loc key follows (slot + 1) * 1000 + chassisIndex or ships its own row
[ ] row count <= 256
```

### Add a map

```
[ ] a 0xC4 row exists for the track theme id, else the world load hard fails
[ ] theme id is not 10000000 20000000 30000000 unless you want the sentinel
[ ] map folder string under 36 chars
[ ] Image/Popup/SelectTrack/track_<map>.png EXISTS
[ ] World/<theme>/<map>/track.nif geometry.nif sky.nif all present
[ ] World/<theme>/<map>/track.col present
[ ] World/<theme>/<map>/start.ini present with at least one grid row
[ ] World/<theme>/Texture/Low present
[ ] Data/Public/World/light.nif present
[ ] loc key ends in _INFO and a sibling key ending _INFO_INFO exists
[ ] difficulty is 0..5, remembering 0 draws 1 star
[ ] mode gate at +0x4C is 0 unless you want the track hidden in modes 0 and 1
[ ] track rows <= 128, tracks per theme <= 55
[ ] theme rows <= 16, but only the first 12 theme indices get a strip slot
```

## 12. DB TABLES

Content definition tables live in a migration. Column widths are the client field
widths minus the NUL, so a full length value cannot smash the handler stack.

| table | feeds | cap enforced by |
|-------|-------|-----------------|
| `character_def` + `character_def_price` | 0xBF | 32 rows |
| `kart_def` + `kart_def_price` | 0xC0 | 64 rows |
| `part_def` + `part_def_price` | 0x108 | 256 rows |
| `skin_def` + `skin_def_price` | 0xC2 | 512 rows |
| `theme_def` | 0xC4 | 16 rows |
| `track_def` | 0xC3 | 128 rows |

`mission_def` and `license_test_def` already exist in `007_emu_buildout.sql`.

Existing `maps`, `drivers` and `vehicle_templates` tables hold 2010 era invented
content. `vehicle_templates.name` points at `Data/Car/*.car`, which this client
never loads. Do not feed the catalog opcodes from those tables.

`099_default_admin.sql` seeds account admin password admin gm level 2, a local default, change it on any exposed server.
