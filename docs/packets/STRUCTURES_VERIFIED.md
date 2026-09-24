# Structures Verified

Field by field reverse of VehicleData ItemData AccessoryData plus the kart
factory customization and upgrade system. Byte level ground truth from the
loaded binary.

## Source of truth

| Fact | Value |
|------|-------|
| Authoritative dump | `DevClient/KnC-new.exe.c` full Hex-Rays same build in Ghidra |
| Do NOT trust for addresses | `DevClient/KnC-ghydra.exe.c` `DevClient/extracted/*.c` |
| Read primitive | `sub_44E910(pkt, dest, N)` reads N bytes 3rd arg is byte count |
| Opcode map | client dispatcher switch at line 219100+ case N = opcode N decimal |
| Car defs on disk | `DevClient/Data/Car/*.car` 416 bytes each raw float struct |
| Factory parts on disk | `DevClient/Data/Public/Car/FactoryCar/<PART>/<Car>/*.nif` |

## Handler map confirmed

| Opcode | Case | Handler | Reads |
|--------|------|---------|-------|
| 0x1B 27 | 27 | sub_478EC0 | int32 count then N x VehicleData 0x2C |
| 0x1C 28 | 28 | sub_478F20 | int32 count then N x ItemData 0x38 |
| 0x1D 29 | 29 | sub_478F80 | int32 count then N x AccessoryData 0x1C |
| 0x1E 30 | 30 | sub_478FE0 | one AccessoryData 0x1C |
| 0x21 33 | 33 | sub_4797C0 | room seat one VehicleData 0x2C one ItemData 0x38 |
| 0x3E 62 | join | sub_479D60 | player join one VehicleData 0x2C one ItemData 0x38 |
| 0x8C 140 | equip | sub_47B9E0 | multi type equip switch 0..7 |
| 0x9D 157 | S2C add | server S_ADD_VEHICLE declares VehicleData 0x2C |
| 0x9E 158 | S2C add | server S_ADD_ITEM declares ItemData 0x38 |

All three list handlers just read raw bytes into a store they do NOT interpret
fields at read time. Field meaning comes from where stored bytes get used.
Single add readers exist too sub_47C320 reads 0x38 into item store sub_47C370
reads 0x1C into accessory store note the prior command map labels for these two
are swapped they are not the vehicle add.

---

# SECTION 1 struct offset maps

## Store mechanics

Each list is a flat array of raw fixed structs plus a count. Two key funcs per
store one keys at offset +0 one keys at offset +4.

| Struct | Insert | Elem size | Max | Count at | Remove +0 | Remove +4 |
|--------|--------|-----------|-----|----------|-----------|-----------|
| VehicleData | sub_44FCB0 | 0x2C 44 | 64 | store+705 | sub_44FD60 | sub_44FDE0 |
| ItemData | sub_44F2D0 | 0x38 56 | 64 | store+897 | sub_44F380 | sub_44F400 |
| AccessoryData | sub_451530 | 0x1C 28 | 64 | store+449 | sub_4515C0 | sub_451640 |

Vehicle inventory store base `unk_844720` item store `unk_845228` accessory
store `unk_847C38`.

## Key finding id vs templateId is SWAPPED vs server

Client treats offset +4 as the TYPE id the thing you look up in the static
catalog. Client treats offset +0 as the unique instance id the inventory key.
Server currently writes templateId at +0 and id at +4 this is the reverse.

Proof for vehicles HIGH confidence:

| Evidence | sub_ | What |
|----------|------|------|
| join lookup | sub_479D60 line 220624 | `sub_450060(catalog, v14[1])` v14[1] = VehicleData+4 |
| catalog match | sub_450060 | matches arg vs catalog elem +0x0C returns 216 byte type record |
| constant type | line 145662 | `sub_450060(dword_1A20B30, 30)` looks up TYPE 30 by constant |
| default type | line 135952 | `sub_450060(dword_1A20B30, dword_5C8054)` global default type id |
| equip dedup | sub_47B9E0 case 0 | `sub_44FDE0(store, v22[1])` dedup by +4 before add |

The catalog `dword_1A20B30` alias `unk_80E680` is a static vehicle TYPE table
loaded once holds mesh physics stat template per kart type. Looking it up by a
per row unique id makes no sense the constant 30 lookup proves the key is a type
id. So VehicleData+4 = templateId is locked.

Same +0 key +4 key dual pattern holds for items sub_44F380 vs sub_44F400 and
accessories sub_4515C0 vs sub_451640 and the item catalog `dword_1A22638`
lookup sub_44F6F0 uses ItemData+4 join line 220625. So the swap is systemic.

## VehicleData 0x2C 44 bytes 11 dwords

| Off | Idx | Type | Field | Conf | Evidence |
|-----|-----|------|-------|------|----------|
| +0x00 | 0 | int32 | uniqueId instance id inventory key | HIGH | sub_44FD60 removes by +0 not used on remote render |
| +0x04 | 1 | int32 | templateId vehicle type catalog key | HIGH | sub_450060 lookup arg join line 220624 constant 30 lookup |
| +0x08 | 2 | int32 | durability | HIGH struct MED name | merged v18[14] repair reads durability |
| +0x0C | 3 | int32 | maxDurability | HIGH struct MED name | merged v18[15] |
| +0x10 | 4 | int32 | stat speed | MED name | merged v18[16] server DB stat_speed |
| +0x14 | 5 | int32 | stat accel | MED name | merged v18[17] |
| +0x18 | 6 | int32 | stat handling | MED name | merged v18[18] |
| +0x1C | 7 | int32 | stat drift | LOW name | NOT merged on join owner side only |
| +0x20 | 8 | int32 | stat boost | LOW name | NOT merged on join |
| +0x24 | 9 | int32 | stat weight | LOW name | NOT merged on join |
| +0x28 | 10 | int32 | stat special | LOW name | NOT merged on join |

Merge proof sub_479D60 lines 220628-220633 copies catalog template into v18
then overrides only v18[14..18] with VehicleData indices 2..6. So on a REMOTE
player join only durability maxDurability and first three stats ride along the
other four stats come from the local catalog or owner side. The 7 stat names
speed accel handling drift boost weight special come from the server catalogue
`vehicle_templates` plus the `owned_kart` upgrade delta of migration 064 they are
inferred the client block copies them.

Structure 2 ids plus 9 value dwords is HIGH confidence. Which value dword is
which stat is server DB derived MED to LOW.

## ItemData 0x38 56 bytes 14 dwords

| Off | Idx | Type | Field | Conf | Evidence |
|-----|-----|------|-------|------|----------|
| +0x00 | 0 | int32 | uniqueId instance id inventory key | HIGH | sub_44F380 removes by +0 |
| +0x04 | 1 | int32 | itemTypeId catalog key | HIGH | sub_44F6F0 lookup arg join line 220625 equip dedup +4 |
| +0x08 | 2 | int32 | quantity | MED | server DB items.quantity in +8 block copied on join |
| +0x0C | 3 | int32 | slot | MED | server DB items.slot |
| +0x10 | 4 | int32 | equipped flag | MED | server DB items.equipped |
| +0x14 | 5 | int32 | expiration unix time | LOW | server assumes reserved 0 on wire |
| +0x18 | 6 | int32 | enhancement | LOW | server writes 0 |
| +0x1C | 7 | int32 | bound flag | LOW | server writes 0 |
| +0x20 | 8 | int32 | reserved | LOW | server writes 0 |
| +0x24 | 9 | int32 | reserved | LOW | server writes 0 |
| +0x28..+0x34 | 10..13 | int32 | reserved 4 dwords | LOW | server writes 0 |

Join copies a 32 byte block from ItemData+0x08 into the runtime item
sub_479D60 line 220640 `qmemcpy(&v19[33], v16, 0x20u)` where v16 sits at
ItemData+0x08. So bytes +0x08..+0x28 8 dwords are the live instance data. The
static item art name effect come from the 320 byte 0x140 template
`dword_1A22638` copied first line 220639. Individual +8 block names are server
DB derived not proven in client.

## AccessoryData 0x1C 28 bytes 7 dwords

| Off | Idx | Type | Field | Conf | Evidence |
|-----|-----|------|-------|------|----------|
| +0x00 | 0 | int32 | uniqueId instance id inventory key | MED | sub_4515C0 removes by +0 like vehicle item |
| +0x04 | 1 | int32 | accessoryTypeId | MED | 0x1E update sub_450E30 keys +4 equip dedup +4 by analogy to vehicle |
| +0x08 | 2 | int32 | slot | MED | server DB accessories.slot |
| +0x0C | 3 | int32 | bonus1 | MED | server DB |
| +0x10 | 4 | int32 | bonus2 | MED | server DB |
| +0x14 | 5 | int32 | bonus3 | MED | server DB |
| +0x18 | 6 | int32 | equipped flag | MED | server DB accessories.equipped |

Accessory +0 +4 order is by analogy to vehicle item both proven no accessory
type catalog lookup found so confidence MED not HIGH. 0x1E single add handler
sub_478FE0 reads one 0x1C then `sub_450E30(store, v5[1])` remove by +4 then
`sub_450D20` add. slot bonus equipped names come from server DB.

## Reconciliation vs server current build

Server serializers `PacketBuilder.cpp` all write templateId first then id.

| Builder | Line | Order written | Verdict |
|---------|------|---------------|---------|
| inventoryVehicles | 603 | templateId id dur maxDur 7 stats | SWAPPED ids client wants id then templateId |
| inventoryItems | 626 | templateId id qty slot equip 0s | SWAPPED ids |
| inventoryAccessories | 649 | templateId id slot 3 bonus equip | SWAPPED ids |
| addVehicle | 1043 | templateId id ... | SWAPPED ids |
| equipVehicle | 1208 | templateId id ... | SWAPPED ids |
| registrationResponse | 190 | templateId id ... | SWAPPED ids |

Impact when server sends templateId at +0 client reads +0 as instance id and
reads the server id value at +4 as the templateId. Client then looks up the
catalog by the wrong number wrong or default kart renders for other players
and for the local equip path. Fields durability stats slot bonus keep their
positions only the first two dwords are swapped. Fix swap the first two
writeInt32 in every vehicle item accessory serializer.

Note PACKET_REGISTRY.md marks 0x3E 0x21 as MATCH, that check is packet
framing and field COUNT not the id vs templateId semantic order.

---

# SECTION 2 kart factory and customization

## What the factory is

Two related systems.

1 Stat garage upgrade repair per vehicle numeric stat grind gold sink. Server
GarageHandler wired.

2 Car factory visual part swap body cover bumper fender tire wing booster
chassis on a set of factory base cars. Client UI heavy server persistence
plumbing present but the customize save opcodes are NOT wired.

## .car file format 416 bytes raw struct dump

`DevClient/Data/Car/*.car` all exactly 416 bytes 104 fields x 4 bytes. NO magic
NO header NO version NO string. Little endian. It is a raw fwrite of a live C++
physics struct. First 16 bytes are floats 4.0 2.2 0.8 200.0 not ascii.

Proof it is a serialized in memory struct field 0xDC holds a STALE HEAP POINTER
`0x00DB____` garbage that varies per save some files share the same stale value
`mini` `tank` `oskart` all `0x00DB5D58` saved from same process layout. So .car
is a designer tool dump not a clean data format. NOTE this contradicts
`docs/formats/FILE_FORMATS.md` lines 56-71 which claim magic plus nif path
string that doc is WRONG there is no string data in .car.

The .car holds engine chassis suspension gearbox tuning it feeds the ENGINE
physics only. Wire stats speed accel handling etc are separate live in the DB
and the network vehicle catalog. Only about 25 of the 104 fields are tuned per
kart the rest is a fixed shared template.

Offset table verified across all 48 car files f float i int ptr pointer.

| Off | Idx | Type | Var | Field | Conf |
|-----|-----|------|-----|-------|------|
| 0x00 0x04 0x08 | 0 1 2 | f | VAR | chassis dims x y z | MED |
| 0x0C..0x1C | 3..7 | f | CONST | body inertia consts 200 1.4 0.8 0.2 800 | LOW |
| 0x20 0x24 | 8 9 | f | VAR | cg offset bike differs | LOW |
| 0x28..0x3C | 10..15 | f | CONST | friction damping 0.48 0.4 80 80 | LOW |
| 0x40 | 16 | f | VAR | wheel +X track half width | HIGH |
| 0x44 | 17 | f | VAR | wheel Z wheelbase | HIGH |
| 0x48 | 18 | f | CONST | wheel Y height -0.2 | HIGH |
| 0x4C 0x50 0x54 | 19 20 21 | f | VAR | wheel mirror -X Z Y | HIGH |
| 0x58 | 22 | f | CONST | 0.034921 = 2.00 deg toe camber | HIGH |
| 0x5C..0x90 | 23..36 | f | CONST | suspension axle direction unit vecs | MED |
| 0x94..0xA0 | 37..40 | f i | CONST | counts 16 2 20 6 | MED |
| 0xA4..0xB8 | 41..46 | f | CONST | 6 speed gear ratios 3.38 2.05 1.43 1.09 0.87 0.70 | HIGH |
| 0xBC | 47 | f | CONST | final drive ratio 2.20 | HIGH |
| 0xC0 | 48 | f | CONST | reverse limit -8.0 | MED |
| 0xC4..0xD4 | 49..53 | f | CONST | engine consts 837.76 481.71 80 80 4 | LOW |
| 0xD8 | 54 | i | CONST | count 20 | MED |
| 0xDC | 55 | ptr | VAR | STALE HEAP POINTER not data | HIGH |
| 0xE0..0xFC | 56..63 | f | CONST | engine aero idle 1005 0.0189 rpm bands 0.9 | MED |
| 0x100 0x104 | 64 65 | f | VAR | engine force max power L R pair | MED |
| 0x108 0x10C | 66 67 | f | VAR | front axle force L R | MED |
| 0x110 0x114 | 68 69 | f | VAR | rear axle force L R | MED |
| 0x118..0x124 | 70..73 | i f | CONST | counts 20 20 16 16 | LOW |
| 0x128 0x12C | 74 75 | f | VAR | suspension spring max force pair | MED |
| 0x130..0x13C | 76..79 | f | VAR | front axle then rear axle grip susp len | MED |
| 0x140..0x14C | 80..83 | f i | CONST | count 20 mirror 1005 0.0189 | LOW |
| 0x150..0x198 | 84..102 | f | CONST | engine torque curve vs rpm 19 pts peak 14000 | HIGH |
| 0x19C | 103 | f | VAR | final steer scalar bikes 1.417 | LOW |

Per vehicle tuned fields chassis dims wheel track wheelbase engine and axle
forces suspension. The whole gearbox 0xA4..0xD8 and the torque curve
0x150..0x198 are IDENTICAL across all karts shared drivetrain. tank widest track
+/-1.42 bike shortest wheelbase Z 0.70.

## .car loader goes through encrypted pak not a plain path

The literal `.car` `Data/Car` `0x1A0` `416` size read strings are ABSENT from
`KnC-new.exe.c` `EngineDLL.dll.c` `KnC-ghydra.exe.c`. The shipping exe reads
kart data from the encrypted archive `pak001.dat` via VFS loader `sub_48A710`
@0x48A710 which pulls a record from archive obj `off_5E02C0` decrypts into
`unk_1AE8B48` and writes a disguised scratch file `dx8_rlg.dll` the caller then
parses. Loose `Data/Car/*.car` are the dev unpacked copies. Many paths are also
runtime decoded from int arrays eg lines 135922-135939 so static grep misses
them. The exact float struct deserializer was not pinned. A same size 0x1A0 x16
object array is built by `sub_4C20A0` but that is the in race entity manager
first dword is a vtable so not the file struct.

## driver seat ini loader

`DevClient/Define/Driver/driver_pos_*.ini` text INI loaded by `sub_48C480`
@0x48C480 via `sprintf("./Define/Driver/driver_pos_%s.ini", name)` then
`GetPrivateProfileStringA` keys x y z per section. This path DOES appear in the
dump unlike .car. It is only the rider seat offset not kart stats.

## Driver seat offset ini separate from .car

`DevClient/Define/Driver/driver_pos_*.ini` plain text seat position per body.

```
[Basic_1]
x = -0.25
y =  0.000
z =  0.559
```

Just where the driver model sits on that kart body not stats.

## Factory folders on disk

Part meshes live under `DevClient/Data/Public/Car/FactoryCar/`.

| Folder | Server column | partType index |
|--------|---------------|----------------|
| BOOSTER | booster | 1 |
| BUMPER | bumper | 2 |
| CHASSIS | chassis | 3 |
| COVER | cover | 4 |
| F_FENDER | front_fender | 5 |
| R_FENDER | rear_fender | 6 |
| TIRES | tires | 7 |
| WING | wing | 8 |
| Texture Effect | body_color | 0 |

Each part folder holds a subfolder per FACTORY BASE CAR the 5 customizable
shells.

```
FactoryCar/BUMPER/Circler/BUMPER.nif
FactoryCar/WING/Firedragon/WING.nif
FactoryCar/TIRES/Squarer/WHEEL1..4.nif
FactoryCar/COVER/Striper/COVER.nif
```

Factory base cars Circler Firedragon Quatzalcuatl Squarer Striper. So
customization is mix and match take Circler bumper plus Firedragon wing etc.
The `partValue` 0..255 indexes which base car mesh fills that slot. UI art in
`Data/Public/Image/CarFactory/` has Factory_Car_Install Factory_Car_Remove
Factory_Car_Parts_Booster Bumper etc buttons plus a FactoryCarName popup.

Named vehicle .car files basic_1..6 bike_01..06 monster tank etc are the fixed
non customizable karts the 5 factory shells are the tunable ones.

## Packet build plumbing

| Func | Addr | Role |
|------|------|------|
| sub_44EBE0 | 0x44EBE0 | init reset packet buffer NOT the opcode writer |
| sub_44ECD0 | 0x44ECD0 | set opcode `[size u16 @+12][opcode u16 @+14][payload]` |
| sub_44E9C0 | 0x44E9C0 | write raw bytes len |
| sub_44EAD0 | 0x44EAD0 | write ascii string strlen+1 |
| sub_44EB00 | 0x44EB00 | write wide string 2*wcslen+2 |
| sub_476B80 | 0x476B80 | enqueue send on net `dword_12124B0` |
| sub_405FE0 | 0x405FE0 | send after confirm popup used by garage actions |

## Real client C2S senders what the client ACTUALLY emits

Verified from the sender funcs not the server guess.

| Action | Opcode | Sender | Payload |
|--------|--------|--------|---------|
| Equip vehicle race | 140 0x8C | inline sub_43AB00 line 175164 | u32 vehicle uniqueId |
| Upgrade vehicle | 156 0x9C | sub_482B00 | u32 vehicle uniqueId |
| Tuning action A | 154 0x9A | sub_4829C0 | u32 vehicle uniqueId |
| Tuning action B | 155 0x9B | sub_482A60 | u32 vehicle uniqueId |
| Accessory tune | 133 0x85 | sub_482740 | u32 accessory uniqueId |
| Garage item action | 185 0xB9 | sub_4842F0 | u32 action u32 key u32 extra |
| Garage variant | 183 184 186 274 | sub_4841D0 sub_4844A0 sub_4843D0 sub_484570 | 2 to 3 u32 plus str |
| Open garage | 15 0x0F | sub_4806C0 | none |
| Enter stage | 24 0x18 | sub_4806F0 | u32 stageId u32 param |

Key sent is always vehicle store element +0 the unique instance id. Upgrade
sender is on the Car Tuning list screen `dword_EF1FF8` key from
`sub_450360(dword_1A5FC80, idx)` elem +0 gated by `!elem[+108]` maxed flag.

## CRITICAL client server opcode mismatch

Only 140 equip and 156 upgrade line up 1 to 1 with the server.

| Server assumes | Reality in client |
|----------------|-------------------|
| C_REPAIR_VEHICLE 0x9D 157 | client NEVER emits 157 repair is opcode 185 action code |
| C_DELETE_ITEM 0x9E 158 | client NEVER emits 158 delete is opcode 185 action code |
| C_GARAGE_VEHICLES 0x1B 27 | client NEVER emits 27 as C2S it is S2C only |

Repair delete install use all funnel through opcode 185 `sub_4842F0(action,
key, extra)` differentiated by the action first field. Garage double click
dispatcher `sub_412BC0` line 146838 resolves the clicked item then picks action
0..4 by tab. Action 2 is the durability repair path shows `MSG_REPAIR_USE`
line 146884. So server must decode opcode 185 with an action switch not treat
157 158 as standalone. Server 0x9D 0x9E are S2C add vehicle add item only.

Upgrade rules server GarageHandler.cpp handleUpgradeVehicle still valid for 156
- statIndex 0..6 maps stat_speed accel handling drift boost weight special
- cost = currentStat+1 x 100 gold per point one stat +1 per call row locked tx
- reply resend vehicle list plus MSG_UPGRADE_SUCCESS

Equip vehicle reply S2C uses S_EQUIP_ITEM 0x8C multi type packet sub_47B9E0
- head int32 result int32 subId then int32 gold int32 cash
- case 0 vehicle 0x2C case 1 item 0x38 case 2 3 4 accessory 0x1C case 5 pet
  0x30 case 6 template 0x84 case 7 8 bytes
- each case dedup by struct +4 then add to store

## Vehicle catalog populated over network not from .car

Catalog store 216 byte 0xD8 54 dword elems count at +1729 max 32 key at elem
+0x0C. Insert `sub_44FED0` @0x44FED0. Get by key `sub_450060` matches elem +0x0C.

The ONLY insert caller is S2C handler `sub_47F390` @0x47F390 line 224409 it
parses a server packet 5 u32 then cstring then 0x14 bytes then two cstrings
then u32 subcount then N x 0x10 subentries and inserts into catalog
`unk_80E680`. The key at elem +0x0C is the 4th u32 read. So the vehicle catalog
name template stats come from the SERVER over the wire. VehicleData+4 is matched
against this +0x0C templateId. This nails VehicleData+4 = templateId HIGH.

The .car files feed only the engine physics not this catalog.

## Car factory customization is CLIENT SIDE saves to factoryCar.ini

The car factory paint and part assembly is a client UI called CarCraft. There
is NO customization C2S opcode. The built car is saved LOCALLY.

| Fact | Evidence |
|------|----------|
| local save file | `fopen("factoryCar.ini","wb")` line 167281 read at 135875 |
| save gate | `MSG_CARFACTORY_SAVE` line 165349 factory stage `dword_B2360C == 18` |
| part mesh path | `"Car/FactoryCar/CHASSIS COVER BOOSTER TIRES F_FENDER R_FENDER BUMPER WING/%s/..."` lines 168143-168488 |
| factory bg art | `CarFactory/Factory_Car_Back.png` `Factory_Car_Top.png` 168760 |
| UI tabs | CarCraft tabs Chassis Tire Cover Booster Front Rear Bumper Wing plus Paint lines 150949-150998 |
| stage init | `Stage CarFactory initialize fail !` 137054 |

So the paint job never leaves the client. `%s` in the part path is the base car
name Circler Firedragon Quatzalcuatl Squarer Striper. The client swaps which
base car mesh fills each slot writes the combo to factoryCar.ini.

## Server customization plumbing exists but is DEAD

Server has customize handlers but NO dispatcher case wires them and NO CMD
constant exists in Protocol.h. They are speculative and never fire.

| Handler | Reads | Wired |
|---------|-------|-------|
| handleEnterCarFactory | none sends 0x16 int32 15 UI state | NO opcode |
| handleCustomizeVehicle | int32 vehicleId int32 partType int32 partValue | NO opcode |
| handleSaveCustomization | int32 vehicleId int32 count then N pair | NO opcode |

vehicle_customization table `server/scripts/004_missing_content.sql`

```
vehicle_id PK
body_color booster bumper chassis cover front_fender rear_fender tires wing
```

9 columns mirror the 9 part slots each INT UNSIGNED default 0 range 0..255.
Since the client persists to factoryCar.ini locally this DB table has no live
data path today. To make the paint job server side authoritative you would add
a real C2S opcode the client does not currently send one.

## Unknowns and next reverse

| Item | Status |
|------|--------|
| .car float struct deserializer | loader goes through encrypted pak sub_48A710 exact float parser not pinned |
| VehicleData stat dword order | client block copies names are server DB order not proven per index |
| ItemData +8 block field split | quantity slot equipped positions server assumed not read by index in client |
| accessory +4 type proof | no accessory type catalog lookup found order is by analogy MED |
| opcode 185 action codes | 0..4 install use remove delete repair need exact action int per garage tab |
| opcode 183 184 186 274 154 155 | garage shop variants payload known purpose still fuzzy |
| factoryCar.ini schema | client saves paint combo locally to factoryCar.ini decode its key layout |

RESOLVED this pass
- client customize sender there is NONE paint saved locally to factoryCar.ini
- .car has no header raw struct dump feeds engine physics only
- vehicle catalog is network populated sub_47F390 confirms VehicleData+4 templateId
- repair delete are opcode 185 action codes not 0x9D 0x9E

---

# Lobby stand and the two selection globals

Read out of Ghidra on `DevClient/KnC.exe` 21 aug 2026. PROVEN unless marked.

## The two globals every model build reads

| Global | Meaning |
|--------|---------|
| `DAT_01A20658` | local player id, set on stage entry |
| `DAT_01A20B18` | selected CHARACTER owned instance id, `owned_character.id` |
| `DAT_01A20B1C` | selected KART owned instance id, `owned_kart.id` |

Writers, complete list from a byte scan of the image:

| Address | Function | Writes |
|---------|----------|--------|
| `0x402B61` `0x402B67` | `sub_4028C0` world stage | 0658 0B18 |
| `0x4111AD` `0x4111B3` `0x4111B9` | `sub_410CB0` from stage setter `sub_404410` | 0658 0B18 0B1C |
| `0x425EDE` `0x425EE4` | `sub_425CB0` | 0B18 0B1C |
| `0x42A90A` `0x42A910` | `sub_42A260` offline branch only | 0B18 0B1C |
| `0x47D5B9` `0x47D5BE` | `sub_47D540` the 0xD9 handler | 0B18 0B1C |

So on the network path the ONLY writer is 0xD9. Send it or the pair stays at
whatever the offline branch left, which is `0` and `1`.

## S2C 0xD9 217 sub_47D540

```
i32   playerId
0x2C  character record   same bytes as an 0x1B row
0x38  kart record        same bytes as an 0x1C row
0x3C  custom car block
```

Total 164 bytes. Body:

```
sub_40CC90(playerId, charRec, kartRec, customCar)     model build
if playerId == DAT_01A20658:
    DAT_01A20B18 = charRec[0x00]
    DAT_01A20B1C = kartRec[0x00]
    DAT_0119F515 = 0
```

The model build runs for every player id. The two globals only move for the
local one, so the record instance ids MUST be the ones 0x1B and 0x1C published.

## Lobby CharInfo panel sub_42A260 and sub_42A050

`sub_42A260` builds the panel. Art binds, the 5th arg is the hit region id.

| Art | x | y | region |
|-----|---|---|--------|
| `CharInfo/Factory_Car_Front_` | 0xDA | 0x204 | 0 |
| `CharInfo/Common_Char_Left_` | 0x44 | 0x170 | 2 |
| `CharInfo/Common_Char_Right_` | 0x168 | 0x170 | 1 |

Guard at the top is `DAT_012124B8 == -1`, the offline test build. That branch
writes `테스트` into `DAT_01A20AEE`, 1000 into `DAT_01A20B10` and `DAT_01A20B14`,
then `DAT_01A20B18 = 0` and `DAT_01A20B1C = 1`. Skipped once connected.

Viewport is `sub_4A5E00(0, 0x3A, 0x6E, 0x152, 0x1CE, 0x438F0000)`, then
`sub_42A050`, and the panel flag at `this+0x7948` is cleared if it returns 0.

`sub_42A050` is the refresh. Every equip ack calls it, see `sub_484B10`.

```
row  = sub_44FE30(DAT_01A20B18)          owned character row
def  = sub_450060(row+0x04)              driver def by base key
copy 0x36 dwords of def   -> this+0x08
row+0x08 .. row+0x18      -> this+0x40 .. this+0x50   five accessory slots

row  = sub_44F450(DAT_01A20B1C)          owned kart row
def  = sub_44F6F0(row+0x04)              kart def by base key
copy 0x50 dwords of def   -> this+0xE0
row+0x08 .. 8 dwords      -> this+0x164                eight skin slots

pet  = first sub_4504E0 entry whose +8 is 1 -> this+0x1AB0  0x2F dwords

sub_4A5ED0(this+0x08, this+0xE0, this+0x1AB0)
sub_4A4E40(this+0x7944)
```

`sub_4A5ED0` is the icon and texture builder, not the 3D mesh. It returns 0 on
the first `sub_444A20` texture load that fails, and then `sub_4A4E40` never
runs. Tier suffix comes from part def `+0x80`:

| Range | Texture |
|-------|---------|
| `< 5` | `Car/FactoryCar/Texture/%s_Basic` |
| `5 .. 0x13` | `_Unique` |
| `0x14 .. 0x40` | `_Epic` |
| `> 0x40` | `_Legend` |

## Skin slot index lives at partDef+0x38

`sub_484B10`, the 0xBA remove handler, resolves the slot from the part def, not
from any category on the wire. `sub_4510C0(key)` then `*(def+0x38)`.

| Slot | Target | Source on the def |
|------|--------|-------------------|
| 0 | owned kart `skin_primary` | def+0x84 |
| 1 | owned kart `skin_secondary` | def+0x88 |
| 8 | owned kart `skin_tertiary` | def+0x8C |
| 2 | owned char `acc_body` | def+0x38 |
| 3 | owned char `acc_face` | def+0x3C |
| 4 | owned char `acc_head` | def+0x40 |
| 5 | owned char `acc_glass` | def+0x44 |
| 6 | owned char `acc_back` | def+0x48 |
| 7 | none, the catalog uses it to hide a row |

`partDef+0x38` is the field `PacketBuilder::partCatalog` writes at +0x38 as
uiCategory in the 0xC2 catalog. The server owns it.

The same handler also proves `DAT_0080E668` is the selected character instance
and `DAT_0080E66C` the selected kart instance, a second pair the garage screen
keeps next to `DAT_01A20B18` and `DAT_01A20B1C`.

## S2C 0xBC 188 sub_484D90 apply equipment set

```
i32   selectedCharacterInstanceId  -> DAT_0080E668
i32   selectedKartInstanceId       -> DAT_0080E66C
i32   count
if count > 0:
    count * 0x30 room craft records, keyed by rec+0x00 through sub_452830
else:
    0x2C character record, patches owned row +0x08 .. +0x18 by rec+0x00
    0x38 kart record,      patches owned row +0x08, 8 dwords, by rec+0x00
```

Count zero is the character plus kart shape. It patches the owned rows in place
so a skin change shows without republishing 0x1B or 0x1C.

---

# Panels that read a struct, three draw routines

Read out of Ghidra 21 aug 2026. The draw routine is the ground truth for which
field the server has to fill, the handler only tells you where it lands.

## Quest list sub_43CCC0

Per row of the panel:

```
state = sub_452110(questIndex)     the 0xFC state row
def   = sub_452240(questIndex)     the 0xFB catalog row
title = sub_4E1B70(def+0x2C)
print L"%d / %d %s"  with  state+0x08, def+0x0C, UNIT_TIMES
```

| Field | Offset | Meaning |
|-------|--------|---------|
| state +0x00 | quest index | key for sub_452110 |
| state +0x04 | state | 0 1 2 |
| state +0x08 | progress | left number |
| def +0x00 | valid flag | zero skips the row and shifts the theme |
| def +0x04 | quest index | key for sub_452240 |
| def +0x08 | theme id | counted by sub_4521C0 |
| def +0x0C | GOAL | right number |
| def +0x14 | reward | detail box lower line, panel y+0x1F7 |
| def +0x18 | reward | detail box upper line, panel y+0x1D4 |
| def +0x2C | title key | ascii, into off_7272E0 |

def+0x0C was written zero for a long time which drew every line as "n / 0".

## User info panel sub_45A4A0

Blob at `this+0x5FB0`. Level at blob+0x1E is read with MOVSX and is a ZERO
BASED index, the full bar test is `level + 1 >= 50`. The icon sheet sub_4425F0
preloads starts at `Icon/lv_icon_001`, so a raw ten renders eleven.

Four numbers reach the screen and nothing else:

| Position | Value |
|----------|-------|
| x+0xEB  y+0xDB | A1+A2 + B1+B2 + C1+C2, clamped at zero |
| x+0x1E8 y+0xDB | A1+A2 |
| x+0xEB  y+0xFC | (C1+C2) / total * 100, drawn "%3.1f%%" |
| x+0x1E8 y+0xFC | B1+B2 |

Only the pair sums are read so the one two split inside a pair is free. The
percent cell is the only rate on the panel so C is wins. Per cell labels are
baked into the panel art and are NOT proven, the sums are.

Name comes from blob+0x04 drawn at x+0x109 y+0x55. The line under it is
sub_451450 on the pendant key, MSG_PENDANT_NOEQUIP on a miss.

## Licence screen stage 14

`licenseScreenAck` 0xA1 builds from what the client already holds, so the
order is fixed. Every 0xC5 definition, then the single 0xA2 progress list,
then the ack. Ack alone builds an empty screen.

Key space is 0..3 rookie, 10..13 amateur, 20..23 pro, four tests per band.
Grade is how many whole bands are done, clamped at three, and 0xA4 carries it
as one byte.

On 0xA3 the two echoes in the payload are client input. license_test_def is
the authority, param_01 is the exp and param_02 the gold the client already
predicted off the definition, so the result must NOT carry a currency tail or
the popup doubles. The item tail is gated on exactly one flag each, anything
else makes the client skip the read and the frame desyncs.

## 0x45 is standings not item use

sub_47A560 reads three int32, player id then position then a third field, and
hands them to sub_4B4B00. Never send an item id on this opcode, the position
slot feeds the rank and the rank picks the drop table line for everyone.
