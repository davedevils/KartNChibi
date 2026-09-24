# File Formats

Mix of standard and proprietary formats. A reverse engineering pass on 2026-09-14 (KnC.exe, image base 0x400000) settled the collision file layout, the track gimmick csv files, the source of the car stats, and the NIF version facts. Those sections carry the new findings below, function addresses in `sub XXXXXX` form.

---

## Engine Formats (Gamebryo/NetImmerse)

### NIF (`.nif`)

NetImmerse File. 3D models, scenes, scene graphs.

```
Data/Public/Car/Body/     Vehicle bodies
Data/Public/Driver/Body/  Character models
Data/Public/World/        Track geometry
Data/Public/Item/         Item models
Data/Public/Effect/       Visual effects
```

Structure: header with version string, block type strings, object blocks (nodes, meshes, textures), root refs.

Version facts, checked 2026-09-14 against the stock client tree: shipped data is Gamebryo 10.2.0.0, including `Data/Public/World/Light.nif` (header string `Gamebryo File Format, Version 10.2.0.0`). The 8 files under `Data/Public/Car/TestWheel/` are the odd ones out, at 10.0.1.0.

Reader status: the engine's NIF reader (`engine/formats`, see `docs/engine/RENDER_MODULE.md`) handles versions 5.0.0.1 to 30.3.0.5. Against the stock client tree it decodes 2872 of 2880 files, the 8 unread are the TestWheel set above. Every car file it does parse rewrites byte identical (`tools/nif/nif_rewrite`). NiPointLight and NiSpotLight decoders were added to close part of the remaining gap.

Tools: `tools/nif/nif_smoke`, `nif_rewrite`, `nif_export`, `nif_animate` (see `docs/tools/README.md`). NifSkope and PyNIFlib work for manual inspection of individual files.

`Data/Public/World/Light.nif`, 764 bytes, is the one light rig every track loads: `world_track_init` (sub 4875C0) calls `light_file_load` (sub 443C10) with the fixed path `./Data/Public/World/light`. 7 blocks: NiNode "Scene Root", NiZBufferProperty, NiVertexColorProperty, NiNode "Direct01", NiDirectionalLight, NiNode "Direct01.Target", NiAmbientLight. One NiAmbientLight (RGB 200 200 200) and one NiDirectionalLight (RGB 255 246 235) light the whole scene. Lights embedded in car and track NIFs are exporter leftovers (`__MAX_Default_Light`) and are not used at runtime, guess, consistent with the dark render symptom that started this investigation. Full detail in `docs/reverse/CLIENT_SHADING.md`.

### KF (`.kf`)

Keyframe animation files. Status: well-documented.

```
Data/Public/Driver/Body/*/  Character animations
Data/Public/Car/Effect/     Vehicle animations
```

Animation sequences, keyframe data (position, rotation, scale), controller links.

### KFM (`.kfm`)

Keyframe Manager. Links multiple KF files to base NIF.

### DDS (`.dds`)

DirectDraw Surface textures. Standard format.

```
Data/Public/Car/Body/     Vehicle textures
Data/Public/Driver/Body/  Character textures
Data/Public/World/        Environment textures
Data/Public/Image/        UI textures
```

Compression: DXT1 (RGB, 1-bit alpha), DXT3 (RGBA, 4-bit alpha), DXT5 (RGBA, interpolated alpha).

---

## Game-Specific Formats

### CAR (`.car`)

`Data/Car/*.car`, 48 files, 416 bytes each: a 0x140 byte block of floats and ints, then a torque curve (count, xmin, xmax, scale, 20 floats).

Read by the client, corrected on 2026-09-14 night. `car_physics_setup` 0x494D50 builds `./Data/Car/default.car` from an obfuscated char array (no plain string in the exe, which is why the string search of the earlier pass found nothing), extracts it through the ini cache into `dx8_rlg.dll` and reads it with `catalogue_read_file` 0x4EFF20 into the spawn catalogue block car+0x2EA0. `car_apply_kart_loadout` 0x490A70 does the same with the kart record name, `Data/Car/<kart>.car`, and falls back to default.car. After the read the setup writes dampers 6000 and tyre springs 100000 over the file values.

The block is the chassis and drivetrain, nothing in it comes from the 17 stats: chassis box and mass at 0x00, lower box at 0x10, wheel radius width mass at 0x28, axle positions at 0x40, spin axes at 0x58, travel directions at 0x70, max steer 0x94, drive mode 0x98, clutch 0x9c, gear count 0xa0 and ratios 0xa4, final drive 0xbc, shift speeds 0xc4, engine drag and inertia 0xcc, torque curve header 0xd8, brake torques 0xf0, springs 0x100, dampers 0x108, anti roll 0x110, disc curve 0x118, tyre spring 0x128, tyre grips 0x130. The full table with the default.car values is in `docs/reverse/physics/RIGID_BODY.md` under Catalogue, the port reads the file with `catalogue_load_car_file`.

The 17 runtime kart stats (car+0x3440..0x3480) still come over the wire: the S2C 0xC0 KartDefinition packet, handler `stat_catalog_recv_0xc0` (sub 47F4F0), see `docs/packets/PACKET_REGISTRY.md`.

### REP (`.rep`)

Replay/recording files. Partially documented. In `DevClient/` (License_Track_*.rep).

Hypothesis: header (magic, track ID, player count, duration) + timestamped position/rotation frames + input events + item usage.

### PAK (`.dat`)

Packed resource archive. `DevClient/pak001.dat`. Unpacker exists (`KNC Un-Packer.exe`).

Structure: file table header, entries (name, offset, size), compressed/raw data.

### INI Files

| File | Purpose | Encrypted |
|------|---------|-----------|
| `Input.ini` | Keybindings | No |
| `Option2.ini` | Game options | No |
| `Network2.ini` | Server config | Yes (use Network2 Decrypt.exe) |
| `launcher.ini` | Launcher config | No |

---

## Image Formats

- PNG (`.png`): UI elements in `Data/Eng/Image/`
- TGA (`.tga`): alpha-channel textures for effects
- BMP (`.bmp`): legacy textures
- IFL (`.ifl`): animated texture sequence (text file listing frames)

## Audio

WAV (`.wav`): PCM, ~232 sound effects in `Data/Public/Sound/High/`.

---

## Text/Data Formats

### Definition Files (`Define/Eng/`)

| File | Purpose |
|------|---------|
| `def_emotion_*.txt` | Emote definitions |
| `def_quest_index.txt` | Quest ID mapping |
| `def_quest_message.txt` | Quest text |
| `def_title.txt` | Title definitions |
| `def_trans_index.txt` | Translation index |
| `def_trans_message.txt` | Localized text |

Tab-separated or custom delimited.

### Driver Position (`Define/Driver/`)

`driver_pos_*.ini`: character position/offset per vehicle. Standard INI with position vectors.

### Track Gimmick Files (`Data/Public/World/{map}/{track}/`)

Reversed 2026-09-14 (`docs/reverse/physics/EFFECTS_AND_GEAR.md`, `docs/reverse/physics/GROUND_AND_RESPAWN.md`). None of these use INI key=value sections despite the extension: every one is headerless CSV, one record per line, `\r\n` terminated, read with a fixed `fscanf` format.

All are staged the same way before parsing: `gimmick_ini_cache_extract` (sub 48A710) pulls the file's bytes out of the game's packed asset store and rewrites them to a plain disk file literally named `dx8_rlg.dll`, capped at 0xA000 (40960) bytes. The loader then `fopen`s that staged file and `fscanf`s it. Despite the `.dll` name this is plain text, never loaded as a library.

| File | Format | Fields | Max lines | Loader address |
|---|---|---|---|---|
| start.ini | `%f,%f,%f,%f` | x, y, z, heading in degrees, zero drives toward minus x like the 0x40 yaw | 100 | gimmick_load_start, sub 48A800 |
| boost.ini | `%d,%f,%f,%f,%f` | kind (int), 4 floats | 100 | gimmick_load_boost, sub 48AB10 |
| itembox.ini | `%f,%f,%f` | x, y, z | 100 | gimmick_load_itembox, sub 48AC20 |
| itembite.ini | `%f,%f,%f` | x, y, z | 100 | gimmick_load_itembite, sub 48AD20 |
| | | Cookie 01 ships it empty, a track without bites | | |
| itemdrum.ini | `%f,%f,%f,%f` | x, y, z, yaw in degrees, the drum gimmick rows, see Track Gimmick NIFs below | 80 | gimmick_load_itemdrum, sub 48AF20 |
| follow_01.ini .. follow_04.ini | `%f,%f,%f,%f` | 4 floats | 400 per file | gimmick_load_follow, sub 489730 |
| regen.ini | `%f,%f,%f,%f\r\n` | x, y, z, radius (guess) | 100 | world_load_regen_zones, sub 48A910 |

All 6 loaders above (not regen.ini) are called from `world_track_init` (sub 4875C0). `regen.ini` is staged and parsed the same way but by its own loader, `world_load_regen_zones` (sub 48A910), which formats `./Data/Public/World/%s/%s/regen.ini` before the extract/stage step. Field meaning beyond column order is settled for start (heading), for itemdrum (yaw, see Track Gimmick NIFs below) and for boost kind 3 (`docs/reverse/physics/EFFECTS_AND_GEAR.md`), the other boost kinds are a guess.

`minimap.ini` exists alongside these but was not part of this pass, format unverified.

### Track Gimmick NIFs (`Data/Public/World/{map}/{track}/Gimmick/`)

Reversed 2026-09-15 in KnC.exe (image base 0x400000), see `docs/reverse/physics/EFFECTS_AND_GEAR.md`, Gimmick placement. No csv row and no ini names these NIFs. The exe names them per track id in `world_gimmick_load_by_track` (sub 4D4180, called by `world_track_init` right after the ini loaders), a switch on the track record field +4, the id the server sends in 0xC3 (theme id plus the folder suffix minus one, Cookie_01 is 20). Every format string is obfuscated as an int array, char i is `v[i] / D - i - 1` with one divisor per function, and decodes to `World/%s/%s/Gimmick/<name>`, loaded through `nif_object_load` (sub 444A20) as `./Data/<pak>/<name>.nif` then `./Data/Public/<name>.nif`, case insensitive.

| Track id | Folder | Loader | NIFs loaded | Placement |
|---|---|---|---|---|
| 11 | Forest_02 | gimmick_load_forest02_mushman, sub 4DBBE0 | mushman, three copies | exe table 0x5F1AC0, three x y z, yaw 180, scale 0.2, z minus 0.8 |
| 12 | Forest_03 | sub 4DF9E0 | tree_fairy_01 | baked |
| 13 | Forest_04 | sub 4DF460, sub 4DAEC0 | tree_door_01, mole_01 | baked |
| 20 | Cookie_01 | sub 4D4780, sub 4DC420 | Ant_01, Pierrot_01, Pierrot_02 | baked |
| 21 | Cookie_02 | sub 4D59F0 | CookieMan_01 | baked |
| 22 | Cookie_03 | sub 4D4DB0 | Chef_01 | baked |
| 31 | Desert_02 | sub 4DCAF0 | scorpion_01 | baked |
| 40 | Toy_01 | sub 4DEDB0 | toybox_01, toybox_02 | baked |
| 56 | Devil_07 | sub 4D7C40 | lavaman_01 | baked |
| 60 | Snow_01 | sub 4DD150 | Sheep_01 | baked |
| 70 | Palace_01 | sub 4D53E0 | cobra | baked |
| 71 | Palace_02 | sub 4D7610, sub 4E01D0, sub 4D6070 | Glass_01, Glass_02, Turnstile_01, Door_01 | baked |
| 72 | Palace_03 | sub 4E01D0, sub 4D6F90, sub 4D68D0 | Turnstile_01, Frame_01, fountain_01 | baked |
| 80, 82 | Swamp_01, Swamp_03 | gimmick_load_swamp_spider, sub 4DDA90 | swa_spider, six copies | exe tables 0x5F1B18 and 0x5F1B60 (x y z), yaws 0x5F1AE8 and 0x5F1B00, scale 1, z minus 0.8 |
| 20000000 | Rally, no folder shipped | sub 4E0B40 then sub 4DCAF0 | twister, scorpion_01 | baked |

Baked means the NIF is attached to the scene root as it is: `nif_object_attach_scene` (sub 4443F0) hands the root node to the scene with no transform, `nif_object_anim_start` (sub 444370) starts its controllers, and the hit tests read the world translation of the `POS_%02d` nodes inside the NIF (`nif_object_find_node_translate`, sub 444850, `gimmick_ant_hit_test` sub 4D49B0, `gimmick_pierrot_hit_test` sub 4DC680). So the artist placed them in world space inside the NIF. Cookie_01 proves it: `pierrot_01.nif` root interpolator pose is `-228.6 -70.0 1.75`, beside the road between follow rows 13 and 14 of `follow_01.ini` (`-268.0 -87.7` and `-207.8 -85.9`), and its keys hop it over the road every 6.67 s, the same 6666 ms cycle the pierrot update restarts. Any other track id loads nothing and succeeds, so the extra NIFs in the folders (Cookie_03 ant_01, Cookie_04 train, Desert_02 scorpion_02, Palace_02 Bill Pot skewer RevolvingDoor, every Desert_03 Desert_04 Devil_01 Devil_04 Snow_02 Snow_04 Swamp_02 Toy_02 Toy_03 Toy_04 file) never load in this build. A track with a case fails the whole track load when one named NIF is missing.

The mushman and the spiders are the two gimmicks that chase the car, so they keep their pose in the exe: `gimmick_mushman_update` (sub 4DB500) and `gimmick_spider_update` (sub 4DE580) build scale times `D3DXMatrixRotationZ(-yaw)` times translation (x, y, z - 0.8) every tick and set it on the node, the rest pose above is where they wait.

**Drums.** The `itemdrum.ini` rows are the only ini rows that place a gimmick NIF, and the model comes from `Data/Public/Item/ItemDrum/`, not from the track folder. `itemdrum_models_load` (sub 4BF3F0) loads two NIFs per row by the track id: 10..12 `FOR_Gimmick_02_1` and `_2`, 20..22 `cheese_01` and `_02`, 30..32 `de_Gimmick_01` and `_02`, 40..41 `TOY_Gimmick_01` and `_02`, 50..52 `bone_head_01` and `_02`, 60..61 `SN_Gimmick_02_1` and `_2`, every other id (Forest_04, Cookie_04, Desert_04, Toy_03, Toy_04, Devil_04, Devil_07, Snow_03, Snow_04, Palace, Swamp, Race) the desert crate `de_Gimmick_01`. Node scale 1.0 for forest cookie devil snow ids, 1.5 for toy, 2.5 for the rest. `_01` is the standing drum with its `POS_01` hit node, `_02` the smash animation. `itemdrum_place_row` (sub 4BF2B0) snaps z to the col ground under x y (`world_place_probe_local` then `world_ground_height_at`), then `itemdrum_slot_place` (sub 4BEA90) sets translation (x, y, z - 0.2) and rotation `D3DXMatrixRotationZ(-(yaw + 270))` through `nif_object_set_rotation_xyz` (sub 444560, called with 0, 0, yaw + 90 and adding 180 inside), the same clockwise rule as the car yaw with a 270 degree model offset. The loader keeps a three column row: the fscanf returns 3, the loop only stops on EOF, the count still grows, and the yaw slot keeps whatever it held, zero on a fresh process. Race_01, Cookie_02, Cookie_03 and Desert_04 ship such rows. A drum hit at speed above the threshold at 0x5A3298 smashes it (`itemdrum_hit_test` sub 4BED40), a slow car bounces off it.

The `itembite.ini` rows work the same way with `Item/Bite/item04` at each row (sub 4B9B30, sub 4B9A80), one pickup effect `Item/itembox/itembox_02` shared.

---

## Collision (`.COL`)

Track collision: `Data/Public/World/{map}/{track}/track.col` for the base piece, `track1.col` .. `track8.col` for up to 8 extra pieces. Reversed 2026-09-14 (`docs/reverse/physics/WORLD_COLLISION.md`, `docs/reverse/physics/GROUND_AND_RESPAWN.md`), replacing the vertex/face/zone struct this section used to describe, which does not match the bytes.

Loading: `world_track_init` (sub 4875C0) calls `world_load_track_pieces` (sub 485580), which loads `track.col` into piece slot 0 and `track1..8.col` into slots 1-8, stopping at the first missing file or after 8 extra pieces. Each `.col` file is read by `world_load_col_piece` (sub 4ECE90). The in-memory piece descriptor is 56 bytes, one per loaded file, kept in a fixed array.

### File layout

Header, 4 int32 LE counts, then 4 variable length arrays back to back:

| Read order | Count | Records that follow | Record size | Content |
|---|---|---|---|---|
| 1 | n1 | n1 | 12 bytes | zone anchors, 3 floats x y z, one per named zone in file order, not read by the exe |
| 2 | n2 | n2 | 56 bytes | cell array, the BSP/triangle cells |
| 3 | n3 | n3 | 20 bytes | edge array, half plane equations shared by cells |
| 4 | n4 | n4 | 12 bytes | mesh vertices, 3 floats x y z, indexed by the two uint16 at edge +0x10 and +0x12, not read by the exe |

The 4 counts are read first, in the order n1, n2, n3, n4, then the 4 arrays are allocated and freed in that same order. Proven by direct disassembly of `world_load_col_piece` (sub 4ECE90).

### Cell record, 56 bytes

Fully accounted, no unknown bytes remain.

| Offset | Size | Content |
|---|---|---|
| +0x00 | float | height plane A |
| +0x04 | float | height plane B |
| +0x08 | float | height plane C, read but never multiplied by a nonzero value on the one path traced |
| +0x0c | float | height plane D (constant) |
| +0x10 | uint16 | edge index 1 of the cell's 3 boundary edges |
| +0x12 | uint16 | edge index 2 |
| +0x14 | uint16 | edge index 3 |
| +0x16 | 34 bytes, ASCII, null terminated | surface material name, upper cased on read |

Ground height at a query point: `y = -(A*x + B*z + D)`, the cell's own C term drops out because the query vector's third component is always 0. Proven by `world_locate_piece_by_height` (sub 4857A0). Surface name read by `world_query_surface_name` (sub 4EC0B0), from cell+0x16.

### Edge record, 20 bytes

| Offset | Size | Content |
|---|---|---|
| +0x00 | uint16 | BSP child index when the query point is behind the plane |
| +0x02 | uint16 | BSP child index when in front |
| +0x04 | float | half plane A |
| +0x08 | float | half plane B |
| +0x0c | float | half plane C, the 2D equation `A*x + B*z + C` |
| +0x10 | 4 bytes | not read by any traced function, guess: padding or an unused 4th coefficient |

A 16 bit edge index carries its sign in the top bit (`index & 0x7fff`), so one edge is shared by two cells with either orientation.

### Point location and surfaces

- `world_bsp_locate_point` (sub 4EBFC0): walks the tree from the cell array root using the child links in the edge record, capped at 1024 steps. The fast path.
- `world_bsp_set_piece` (sub 4EBCF0): linear scan of every cell, tests all 3 edges per cell. Fallback and initial locate.
- `world_bsp_edge_sweep` (sub 4EBDD0): segment test used to find which edge a moving point crosses between two ticks.
- `world_surface_name_to_index` (sub 4866D0): maps a cell's name string to one of 9 surface indices: DUST (also the REGEN alias), ASPHALT, WATER, GRASS, SNOW, ICE, BOARD, WATER_REG, GRASS_L. Friction, air penalty and grip per surface are read from a fixed table at 0x5EA880 by `world_surface_friction` (sub 486D40), `world_surface_air_penalty` (sub 486D70) and `world_surface_grip` (sub 486DA0).

### Still unknown

Array C (n1 records, 12 bytes) and array D (n4 records, 12 bytes) are allocated and freed by `world_load_col_piece` (sub 4ECE90) and its destructor (sub 4ECDB0) but never dereferenced by any traced collision or physics function, checked across every function that takes a piece pointer. 12 bytes is consistent with 3 floats (a vertex) or 3 int32 (a triangle's vertex indices). Content is unknown, guess only: editor data, an unused triangle index list, or a consumer outside the traced call graph.

---

## Unknown Formats

### OGP (`.ogp`)

File: `clientinfo.ogp`. OGPlanet platform metadata. May not be needed for clone.

---

## File Path Conventions

### Models

```
Vehicle body:       Data/Public/Car/Body/High/{name}/BODY.nif
Vehicle wheels:      Data/Public/Car/Body/High/{name}/WHEEL1.nif .. WHEEL4.nif
Vehicle colour:      Data/Public/Car/Body/High/{name}/BODYCOLOR/{COLOUR}/*.dds
Character body:      Data/Public/Driver/Body/{name}/*.nif
Track geometry:      Data/Public/World/{map}/{track}/track.nif, sky.nif, sky_night.nif
Helper mesh:         Data/Public/World/{map}/{track}/geometry.nif, one placeholder texture Geometry.BMP that ships nowhere, not the visible road. world_track_init loads track.nif into the scene holder at world+8 (0x1ADF818) through scene_holder_load_nif (sub 445250, vtable slot 1 with the texture folder), then geometry.nif into the same holder through scene_holder_load_extra_nif (sub 4450D0, vtable slot 4, no texture folder). scene_holder_query_height (sub 4451A0, vtable slot 5 of the same object, x y in, z out) answers the ground height under dropped items and gimmick objects (sub 4C4790 item drop, sub 4BB900 hazard placer), so the mesh is the pick surface of the scene object, the car physics never touches it, it uses the .col. Whether the renderer draws it was not traced, the class name strings at 0x5A3670 are obfuscated
Track collision:     Data/Public/World/{map}/{track}/track.COL, track1.COL .. track8.COL
Track gimmick data:  Data/Public/World/{map}/{track}/*.ini (see Track Gimmick Files)
Track textures:      Data/Public/World/{map}/Texture/High/*.dds, Texture/Low/*.dds
World light:         Data/Public/World/Light.nif (one file, shared by every track)
Item:                Data/Public/Item/{item}.nif
Effect:              Data/Public/Effect/{effect}.nif
```

The shipped basic kart data has 9 colour folders under `BODYCOLOR/`: BLACK, BLUE, GREEN, ORANGE, PINK, PURPPLE, RED, SILVER, YELLOW (PURPPLE is a typo in the shipped tree, not introduced here). No code path selecting this folder was located in KnC.exe (`docs/reverse/CLIENT_SHADING.md`), the path is confirmed by the files on disk, not by a traced loader.

### UI

```
Buttons:          Data/Eng/Image/Buttons/*.png
Channel select:   Data/Eng/Image/ChannelSelect/*.png
Garage:           Data/Eng/Image/Garage/*.png
Lobby:            Data/Eng/Image/Lobby/*.png
Menu:             Data/Eng/Image/Menu/*.png
Popup:            Data/Eng/Image/Popup/*.png
Room:             Data/Eng/Image/Room/*.png
Shop:             Data/Eng/Image/Shop/*.png
```

### Audio

```
Sound effects:    Data/Public/Sound/High/*.wav
```
