# Effects and gear

Read in KnC.exe.raw, image base 0x400000, on 2026-09-14. Covers car_effect_apply/car_effect_update (item and gimmick effects on the local car), the track gimmick ini files and their hit detection, and body_gear_update's gear and wheel-spin subtree. Extends docs/reverse/physics/TICK_HELPERS.md and docs/reverse/physics/RIGID_BODY.md, does not repeat their entries.

## Effect codes

All codes are read at car+0x36A8 (`car_effect_apply`, 0x495C30). One code active at a time: apply refuses when car+0x36A8 != 0. On success every code snapshots yaw and position into car+0x36CC/0x36D0/0x36D4/0x36D8 first, then does its own setup. Every code except 800 and 1100 also plays the driver damage clip (`driver_clip_play(car+0x3520, 6)`, 0x48B460, read as a sound in this round, see Driver clips below), calls the reset helper `FUN_00496930`, and cancels drift (`car_drift_state_set(carIndex, 0)`) the instant it is applied. Name column is a guess from the ini/sound-file strings found (bomb, thunder, turtle, big turtle, shield, hive), not proven per code; mechanics are proven from the bytes.

| Code | Hex | Name (guess) | Triggered by | Speed scale (car+0x25FC/0x2600, per frame) | Ends when |
|---|---|---|---|---|---|
| 100 | 0x64 | spin, guess | car_effect_apply(100) | x0.967 (0x5A6A00) | car+0x36AC ramp (+1000/s, 0x5A3BF4) exceeds 540 (0x5A69FC) |
| 200 | 0xC8 | spin recovery, guess | car_effect_apply(200) | x0.98 (0x5A6A04) | 36AC != 0 and `FUN_0048D730` durability <= 1 |
| 300 | 0x12C | hit / spin-out, guess (bomb or shell impact) | car_effect_apply(300) | x0.0 (25FC/2600 zeroed) | car+0x36C8 quadratic term < 0 |
| 400 | 0x190 | hive, guess | car_effect_apply(400) | x0.88 (0x5A69E8) | `FUN_004CF020` durability check returns < 0 (36AC clamps at 540, does not end the effect by itself) |
| 500 | 0x1F4 | resting hazard (banana/shell), guess | car_effect_apply(400 or 500), or auto via effect_hazard_hit_lookup on table 0x2F08E10 | x0.93 (0x5A69EC) | effect_hazard_hit_lookup(carIndex) on the same table returns -1 |
| 600 | 0x258 | itembox grab-lock, guess | car_effect_apply(600), or auto via gimmick_pool_update state 0 | none | `FUN_004B9FE0` returns < 0 |
| 700 | 0x2BC | itembite bump, guess | auto via world_wheel_bump_slot on table 0x2EF3000 | x0.94 (0x5A69E4), once | world_wheel_bump_slot on the same table returns -1 |
| 800 | 0x320 | shield, guess (passive) | car_effect_apply(800) | none, never touched again | never inside car_effect_update; an outside system must clear it |
| 900 | 0x384 | unclear, guess | car_effect_apply(900) | none | `FUN_004B9FE0` returns < 0 (same check as 600) |
| 1000 | 0x3E8 | itemdrum bump, guess | auto via world_wheel_bump_slot on table 0x2EF48E8 | x0.88 (0x5A69E8), once | world_wheel_bump_slot on the same table returns -1 |
| 1100 | 0x44C | scripted/warp, guess | car_effect_apply(1100) | none | DAT_00D6E1D0 (cutscene-busy flag) reads 0 |

Codes not in this list (all other integers 0-1100) are rejected by car_effect_apply and left unhandled by car_effect_update: since car+0x36A8 is set before the code is validated, an unrecognized code would still leave 36A8 non-zero (proven from the byte order in car_effect_apply, not observed triggered).

### Per code detail

**100.** car+0x36AC (steering loss, per CLIENT_PHYSICS_MAP) ramps up by `time_frame_delta()*1000` (0x5A3BF4) every frame while the code is active, no clamp assignment, only an end check: once 36AC > 540 (0x5A69FC) the effect ends (`effect_end`). Read from disassembly at 0x4962DD-0x496328 (FCOMP/FNSTSW/TEST AH,0x41 pair confirms "greater than" ends it, not "reaches").

**200.** 36AC decays by `time_frame_delta()*200` (0x59FF80) every frame. `FUN_0048D730(carIndex)` (a durability/position check, not traced) is called every frame regardless. The effect ends only when 36AC is not exactly 0.0 and the durability result is <= 1; while 36AC == 0.0 (first frame or exact zero) or durability > 1, it continues untouched. Disassembly at 0x496262-0x4962DA.

**300.** The heaviest effect. 25FC/2600 (local-frame velocity feeding the body) are zeroed outright (x0.0, 0x59F44C). car+0x2604 accumulates `time_frame_delta()*200000` (0x5A69E0) in car_effect_apply, then is scaled by roughly x1.005 (0x5A69F8) every frame after in car_effect_update. car+0x36C8 = `36BC * (car+14000) - (car+14000)^2 * 29.4` (0x5A69F4), where car+14000 (a byte offset past the struct, i.e. `car_base+0x36B4` region reused, read directly as `iVar3+14000`) accumulates `time_frame_delta()` every frame; 36BC starts at 34.0 (0x42080000, apply time). 36AC recovers at 150/s (0x5A69F0). On apply: input flags at car+0x18..0x2C are zeroed, `car_gear_clamp(carIndex, 1)` is called (also every frame while active, in update), and if this is the local car a cancel-boost is issued (`car_boost_start(carIndex, 6, 0)`). Apply also sets the crash-recovery state machine (game+0x13972F4 = 1, game+0x13972F8 = time_now_ms(), see TICK_HELPERS). Ends when 36C8 < 0.0. Disassembly at 0x49632D-0x4963FA.

**400.** 25FC/2600 x0.88 (0x5A69E8). 36AC ramps up the same way as code 100 (+1000/s), but instead of ending on overflow it clamps to 540.0 when it exceeds the 540 cap; the effect itself only ends via a separate durability call `FUN_004CF020(carIndex)` returning negative. Disassembly at 0x496453-0x4964AA.

**500.** 25FC/2600 x0.93 (0x5A69EC). Shares its apply path with 400 (`param_3==400 || param_3==500`). Ends when `effect_hazard_hit_lookup(carIndex)` (table base 0x2F08E10) returns -1, i.e. the car is no longer inside an active slot of that 16-entry table. The same lookup, on the same table, is also what starts the effect in the first place, from car_effect_update's own idle dispatch (see Gimmick hits below).

**600.** No speed scaling at all, in apply or update. Only a reset+drift-stop and a wait on `FUN_004B9FE0(carIndex)` until it returns negative, then `effect_end`. Matches an item-box "grab lock": the car is held (no throttle scaling shown here, though input is not explicitly cleared either) while gimmick_pool_update's own state machine runs the pickup animation/timing. See Gimmick hits.

**700 / 1000.** Both are one-shot: 25FC/2600 are multiplied once by 0.94 (700) or 0.88 (1000) at the moment they are applied (car_effect_update's opening block, not a per-frame loop), then the code just waits for world_wheel_bump_slot to say the car has left its hit slot on the matching table. 1000 additionally spawns a particle/scene marker via `FUN_004981B0(carIndex, 1)` when applied.

**800.** Snapshot only. No sound, no reset, no drift-stop, never referenced again anywhere in car_effect_update. Whatever clears it is outside this function; best match for a passive, externally-managed flag such as a shield.

**900.** Mechanically identical to 600 (no scaling, ends on the same `FUN_004B9FE0` check) but reached by a different apply path (no world/gimmick trigger found in this pass). car_drift_update also treats 500/600/900 as drift-cancelling codes (TICK_HELPERS).

**1100 (0x44C).** Plays sound 6 and calls `FUN_0043D7E0()` (camera pan, the same helper car_respawn_state_machine's scripted teleport uses) instead of reset/drift-stop. Ends purely when DAT_00D6E1D0 (a cutscene-busy flag reused across the file) reads 0. Reads as a scripted/warp sequence rather than a combat item.

car_effect_lean_update (0x49B3D0, documented in TICK_HELPERS) is the only other consumer of car+0x36A8: it special-cases 200 and 300 to build a roll-only wobble from 36AC, every other code (including the ones above) falls to the ordinary cornering lean.

## Gimmick files

Every gimmick ini is loaded the same way, confirmed from six loaders (`gimmick_load_start` 0x48A800, `gimmick_load_boost` 0x48AB10, `gimmick_load_itembox` 0x48AC20, `gimmick_load_itembite` 0x48AD20, `gimmick_load_itemdrum` 0x48AF20, `gimmick_load_follow` 0x489730), all called from `world_track_init` (0x4875C0):

1. `sprintf("./Data/Public/World/%s/%s/<name>.ini", map, track)`.
2. `gimmick_ini_cache_extract(path)` (0x48A710): reads the file's bytes out of the game's packed asset store (`FUN_0044D500`/`FUN_0044D560`/`FUN_0044D6B0`/`FUN_0044D650`, an internal VFS reader, not traced), then re-writes those exact bytes to plain disk as `<DAT_00B23290>/dx8_rlg.dll` (`fopen` "wb" + `fwrite`). Fails (returns 0) if the entry is missing or bigger than 0xA000 (40960) bytes.
3. The loader then does `sprintf("%s/%s", DAT_00B23290, "dx8_rlg.dll")`, `fopen`s that staged file "rb", and `fscanf`s plain comma-separated lines from it, format specific to the gimmick type. None of these files are read by name; every one is staged through the same on-disk filename one at a time. This is a different use of the "dx8_rlg.dll" name than the 8-byte stub CLIENT_PHYSICS_MAP found gating car_physics_setup; both use the same filename trick, not proven to be the same file instance.

None of the "ini" files use key=value sections; despite the extension they are headerless CSV, one record per line, `\r\n` terminated.

| File | fscanf format | Fields | Max lines | Loader | Count field | Data base |
|---|---|---|---|---|---|---|
| boost.ini | `%d,%f,%f,%f,%f` | kind(int), 4 floats | 100 | gimmick_load_boost | this+0x1164 | this+0x1168, stride 0x14 |
| itembox.ini | `%f,%f,%f` | position (3 floats) | 100 | gimmick_load_itembox | this+0x1938 | this+0x193C, stride 0xC |
| itembite.ini | `%f,%f,%f` | position (3 floats) | 100 | gimmick_load_itembite | this+0x1DEC | this+0x1DF0, stride 0xC |
| itemdrum.ini | `%f,%f,%f,%f` | position + 1 float | 80 (0x50) | gimmick_load_itemdrum | this+0x240C | this+0x2410, stride 0x10 |
| follow_%02d.ini (01-04) | `%f,%f,%f,%f` | 4 floats | 400 per file | gimmick_load_follow | this+0x2A04 + i*4 | this+0x2A18 + i*0x1900, stride 0x10 |
| start.ini | `%f,%f,%f,%f` | position + heading in degrees, zero drives toward minus x, same rule as the 0x40 yaw, checked on the Race 01 ghost | 100 | gimmick_load_start | this+0x1B8 | this+0x1BC, stride 0x10 |

`this` above is the world/track object `world_track_init` receives, not the car record. Field meaning beyond field order is settled for start (heading), for itemdrum (yaw, see Gimmick placement below) and for boost kind 3 (the boost pad detector below), the other boost kinds are a guess.

## Gimmick placement

Read on 2026-09-15. The track `Gimmick/*.nif` files are named by the exe, not by any ini. `world_track_init` calls `world_gimmick_load_by_track` 0x4D4180 (was FUN_004D4180, this 0x5F1950) after the ini loaders, a switch on the track record +4 (the 0xC3 track id). Each arm is one loader per gimmick with the path format obfuscated as an int array (char i is `v[i] / D - i - 1`, D per function, decoded with the divisor read from the helper), all of them `World/%s/%s/Gimmick/<name>` with `%02d` from one: 11 mushman (0x4DBBE0), 12 tree_fairy 01 (0x4DF9E0), 13 tree_door 01 (0x4DF460) and mole 01 (0x4DAEC0), 20 Ant 01 (0x4D4780) and Pierrot 01 02 (0x4DC420), 21 CookieMan 01 (0x4D59F0), 22 Chef 01 (0x4D4DB0), 31 scorpion 01 (0x4DCAF0), 40 toybox 01 02 (0x4DEDB0), 56 lavaman 01 (0x4D7C40), 60 Sheep 01 (0x4DD150), 70 cobra (0x4D53E0), 71 Glass 01 02 (0x4D7610) Turnstile 01 (0x4E01D0) Door 01 (0x4D6070), 72 Turnstile 01, Frame 01 (0x4D6F90), fountain 01 (0x4D68D0), 80 and 82 swa_spider (0x4DDA90), 20000000 twister (0x4E0B40) then scorpion. Every other id falls through with success and loads nothing. The full table with the placement column is in `docs/formats/FILE_FORMATS.md`, Track Gimmick NIFs.

Each loader calls `nif_object_load` 0x444A20 (was FUN_00444A20, `./Data/<pak>/%s.nif` then `./Data/Public/%s.nif`), `nif_object_attach_scene` 0x4443F0 (hands the root to the scene through `scene_attach_nif_root` 0x43D790 with no transform), `nif_object_anim_start` 0x444370 and a random `model_anim_enable` 0x444740, then fills the slot (stride 0xB8 from this+0x10, +0x9C state, +0xA0 car minus one, +0xB0 the load time). No translation, no rotation, no scale is ever set on these, the NIF sits where the artist baked it, that is the placement. The hit tests confirm it: `gimmick_ant_hit_test` 0x4D49B0 and `gimmick_pierrot_hit_test` 0x4DC680 build the node name `POS_%02d` (same obfuscation, divisor 76 and 81), take its world translation from the NIF (`gimmick_ant_node_translate` 0x4D4570 through vtable +0x48 find by name, `nif_object_find_node_translate` 0x444850 walking the root children, +0x8C is the world translate) and compare it with the car position under 8.0 (0x5A15F0) for the ant, under 12.0 (0x5A32CC) for the pierrot, both dispatched per car by `world_gimmick_hit_dispatch` 0x4D3950 (kinds 0 ant, 3 pierrot into `FUN_004982D0`). `world_gimmick_update_all` 0x4D3770 ticks them, the ant restarts its 90 s path every 89999 ms and the pierrot its hop every 6666 ms (`nif_object_anim_reset` 0x444820), the slot state 1 set by a hit clears after 3000 ms. Pierrot 01 of Cookie 01 is the proof on the bytes: its root interpolator pose is `-228.6 -70.0 1.75`, beside the road between follow rows 13 and 14 of follow_01.ini, and the keys hop it over the road and back on a 6.67 s loop.

Two gimmicks chase the car and so keep their rest pose in the exe. `gimmick_load_forest02_mushman` 0x4DBBE0 loads three mushman copies at `gimmick_mushman_positions` 0x5F1AC0 (467.2 -195.4 15.4, 325.7 -283.9 16.8, 541.3 -83.9 14.4), yaw 180, and `gimmick_mushman_update` 0x4DB500 builds `D3DXMatrixScaling(0.2)` times `D3DXMatrixRotationZ(-yaw)` times `D3DXMatrixTranslation(x, y, z - 0.8)` every tick into `car_node_set_transform` 0x444720 (vtable +0x1C of the EngineDLL node object). State 0 waits for a car within 18.0 (0x41900000) through FUN_00499B50 (the spider within 10.0 through FUN_00499BF0), state 1 eases the pose a quarter (0.25 at 0x5A32D4) toward the car each tick and freezes the local car (FUN_0043EAD0 with 20000.0), after 3000 ms it runs a random shake, then after 800 steps and 3000 ms more it goes back to its table pose. `gimmick_load_swamp_spider` 0x4DDA90 does the same with six spiders, scale 1, positions `gimmick_spider_positions_swamp01` 0x5F1B18 and `_swamp03` 0x5F1B60, yaws `gimmick_spider_yaw_swamp01` 0x5F1AE8 (270 270 320 320 330 330) and `_swamp03` 0x5F1B00 (270 270 320 250 330 220), update `gimmick_spider_update` 0x4DE580 with the same state machine and a 4000 ms restart. Both also load three png textures from the Gimmick folder first (mush leaf01 leaf02, spider01 spider01_1 spider01_2 through FUN_00441450).

The rotation sign, settled on the car: `car_visual_update` 0x48E6A0 builds the car node matrix as `D3DXMatrixRotationZ(-(car+0x3220))` times the translation, the car nose sits on minus x in the body NIF, and the wire proves the car at yaw A drives toward (-cos A, sin A), so the EngineDLL takes the D3DX matrix as it is and RotationZ of minus A turns a node clockwise by A seen from above. In bx terms `bx::mtxRotateZ(+A)` is that matrix byte for byte, the map viewer uses it for the start markers, the ghost car, the drums and the two table gimmicks.

**Drums and bites are ini placed gimmicks from the Item folder.** `itemdrum_models_load` 0x4BF3F0 (was FUN_004BF3F0, this 0x2EDA010, called from FUN_004B8820 at 0x4B886C, a failure aborts that caller) walks the itemdrum rows (count 0x1AE1C1C, rows 0x1AE1C20 stride 0x10) and loads two `Item/itemdrum/` NIFs per row by track id: 10..12 FOR_Gimmick_02_1 and _2, 20..22 cheese_01 and _02, 30..32 de_Gimmick_01 and _02, 40..41 TOY_Gimmick_01 and _02, 50..52 bone_head_01 and _02, 60..61 SN_Gimmick_02_1 and _2, default de_Gimmick; `nif_object_set_scale` 0x444670 gets 1.0 for the forest cookie devil snow ids, 1.5 (0x3FC00000) for toy, 2.5 (0x40200000) for the rest. `itemdrum_place_row` 0x4BF2B0 probes the col under x y (`world_place_probe_local`, `world_ground_height_at`) and `itemdrum_slot_place` 0x4BEA90 takes the first free slot (stride 0x158), `nif_object_set_rotation_xyz` 0x444560 with (0, 0, yaw + 90.0 at 0x5A323C) which builds RotX(-b) RotY(a) RotZ(-(c + 180.0 at 0x5A32A8)), so RotationZ of minus (yaw + 270), and `nif_object_set_translate` 0x444620 with (x, y, z - 0.2 at 0x5A15EC), on both models, then anim start on _01 and anim stop on _02. Row column 4 is therefore the drum yaw in degrees on the car rule with a 270 degree model offset. `gimmick_load_itemdrum` 0x48AF20 only stops its fscanf loop on EOF, a three column row returns 3, still counts, and leaves the yaw slot as it was (zero on a fresh process), Race_01 and Cookie_02 ship such rows. `itemdrum_hit_test` 0x4BED40 scans the rows for the car within 10.0 (0x59F404) and the sweep of `math_point_along_heading` under 5.0 (0x5A3238): above speed 20.0 (0x5A3298) the drum smashes (slot state 1, `FUN_004BEC80` sound, `FUN_00482BA0`), below it the car is pushed back (`car_boost_push` with the angle from the drum plus 270 at 0x5A6A50) and its velocity halved (0.5 at 0x59F414). The bites do the same with `Item/Bite/item04` per row (`itembite_models_load` 0x4B9B30, `itembite_place_row` 0x4B9A80, z minus 0.4 at 0x5A3294) and one `Item/itembox/itembox_02` effect model.

## Gimmick hits

**itembox.** `gimmick_pool_update` (0x4C7ED0, "world item manager" from the task brief) loops up to 8 live slots, stride roughly 0x250 bytes, drawn from the up to 100 positions itembox.ini defines (this-side pooling: only a handful are ever "live" near the player at once, guess). Each active slot (byte at slot-0xC != 0) with a car already assigned (dword at slot-0xB >= 0) runs a small state machine held in the slot itself, encoded as denormal float bit patterns 0.0/1/2/200/201 (confirmed by decoding 1.4013e-45=1, 2.8026e-45=2, 2.8026e-43=200, 2.81661e-43=201). State 0 calls `car_effect_apply(carIndex, 600)` (the grab-lock, see above) and advances to state 1. States 1/2 run pickup timing windows (3000/6000 ms depending on race mode, plus a durability-scaled extension via `FUN_004B8580`) and a "recover key pressed in the window" check that calls `car_boost_start`. States 200/201 are a separate scripted sequence (freeze input `FUN_0043ED70`, pan camera `FUN_0043D7E0`, reposition via `body_place_and_probe`, warp via `FUN_00497160`) gated by the slot's stored id matching `DAT_01B19744`, structurally the same shape as car_respawn_state_machine's own 100/200 scripted-teleport states (TICK_HELPERS); whether the two share code or just a pattern was not resolved. Every active slot also calls `world_probe_wheel_point` (0x486490, wraps `world_bsp_locate_point`/`world_query_surface_name`) each tick to track which BSP surface piece the slot's stored position sits over; this is a surface/ground lookup for the slot's own position, not a car-distance test.

**itembite (700) and itemdrum (1000).** Both are detected the same way: car_effect_update's idle dispatch (car+0x36A8==0) calls `world_wheel_bump_slot(carIndex)` twice with two different table bases loaded into ECX at the call site (0x2EF3000 for itembite, disassembly at 0x496139; 0x2EF48E8 for itemdrum, at 0x4961AC). world_wheel_bump_slot (0x4C37D0) scans 16 fixed-stride (0xB8-byte) entries per table, matching when the byte at entry-8 == 1 (active) and the int at entry+0 == 0 and the int at entry-4 == the car index; a match starts the corresponding effect. The same tables and the same lookup are reused every frame while the effect is active, to know when the car has left the slot (see the Effect codes table).

**500 (resting hazard).** Same shape, one table (0x2F08E10, `effect_hazard_hit_lookup`/0x4CFCB0), 16 entries stride 0x428 bytes, matching when the byte at entry-0x38 == 1, the int at entry-0x30 == car index, and the int at entry+0 (a countdown, unit not determined) is between 0x66 (102) and 299.

**Collision test itself.** None of the three tables above are written anywhere this pass traced (`get_xrefs_to` on the reader functions and on the two literal table addresses found only the readers listed above, no writer). The actual radius/box/distance test that flips a table entry's active byte and assigns a car index was not located; open question, see below. No boost-pad hit detector (the live per-tick scan of boost.ini's entries against the car, calling `car_boost_start` with the ini's `kind` field) was located either; only the loader and the ini format are proven.

## Gear and rpm

`body_gear_update` (0x4EFA90, `this`=wrapper=car+0x211C, already covered structurally in RIGID_BODY.md) has two parts.

**Gear selection**, at the top of the function, only when `param_8==1` (a "throttling" flag from the caller `body_world_step`): this+0x98C is the current gear (-1 = neutral). If neutral, engages gear 1 once the throttle input passes a floor (`_DAT_005A83E8`, read as 0.0). If already in gear, compares the current ratio this+0x550 against this+0x578 (a threshold) to decide up or down shift, evaluated against two per-gear curve arrays this+0x57C[] (bounded by this+0x550, downshift table) and this+0x584[] (bounded by this+0x580, the gear count, upshift table); the new ratio is `curve[newGear] / curve[oldGear]` multiplied into this+0x550. This is the transmission model referenced in RIGID_BODY.md's `body_load_wheel_config` (the two curve-breakpoint arrays it builds at car+0x2BE4+). this+0x990 (car+0x2AAC, "display gear") only tracks this+0x98C while at least one wheel speed input (param_5 or param_6) is below this+0x5B0, otherwise it is forced to 0 (neutral display) even though the real gear (98C) keeps running.

this+0x984/0x988 (car+0x2AA0/0x2AA4) are then set from the wheel-pair inputs param_5/param_6 scaled by this+0x5A4/0x5A8/0x5AC (per-wheel gain constants from the catalogue), independent of gear.

**Wheel-spin integration**, the rest of the function, is a textbook RK4 (Runge-Kutta 4th order) solve, run once per call to body_world_step (once per physics substep):

- `gear_rk4_step_coeffs` (0x4EFFC0) writes the step sizes into shared scratch: dt, dt*0.5 (0x59F414), dt*0.25 (0x5A32D4), dt/6 (0x5A8378, decoded to ~0.1667).
- `gear_wheel_force_solve` (0x4EECD0, formerly the "12-function subtree" RIGID_BODY.md left unexpanded) is the derivative evaluation, called exactly 4 times per body_gear_update call (k1..k4). Each call reads a per-wheel timestep from `gear_wheel_solver_dt` (0x4F1DD0, a 3-mode selector on this+0x51C, blending two internal dt sources with 0.2/0.3, 0x5A15EC/0x5A1650, in mode 2), clamps a target wheel accel against this+0x984/0x988, and for all 4 wheels calls `gear_tire_force_model` (0x4F32E0) with per-wheel geometry (this+0x800-0x848 for wheels 0/1, this+0x81C-0x848 for wheels 2/3) and the gear ratio (this+0x858/0x85C, the traction-limit pair RIGID_BODY.md already found). `gear_tire_force_model` is a friction-circle slip model (uses `fpatan`/`fsin` on a longitudinal/lateral slip ratio derived from wheel spin vs ground speed) and writes a 5-float result per wheel (2 force components, a load ratio, a "slip active" flag) into this+0x860/0x874/0x888/0x89C, 0x14 bytes apart. **This overlaps the byte range RIGID_BODY.md's `body_wheel_state_reset` zeroes at this+0x860 (car+0x297C), the same address CLIENT_PHYSICS_MAP independently identified as the tick's own "yaw rate" field (copied to car+0x32E4 every frame).** Whether the tick's yaw-rate read happens before or after this per-substep scratch write was not resolved; flagged in Open questions, not asserted as a conflict.
- `gear_rk4_state_copy` (0x4EF3E0), `gear_rk4_stage2_blend` (0x4EF450), `gear_rk4_stage3_blend` (0x4EF550) and `gear_rk4_stage4_blend` (0x4EF650) build the RK4 midpoint/endpoint states (y0+dt/2*k1, y0+dt/2*k2, y0+dt*k3) between each `gear_wheel_force_solve` call, all four wheels at once, all using the dt*0.5 scratch from `gear_rk4_step_coeffs`.
- `gear_rk4_combine` (0x4EF7C0) folds the 4 stage results per wheel as `(2*mid + k1 + k4) * dt/6 + previous`, clamped to +-1,000,000.0 (0x49742400 / 0xC9742400, confirmed exact by decode) as a runaway-integrator guard, and writes the final per-wheel spin state back to this+0x8B0/0x910/0x8B4/0x914/0x8B8/0x918/0x8BC/0x91C.

Between each pair of RK stages, body_gear_update also calls four further helpers per stage (`FUN_004F2140`/`FUN_004F37A0`, `FUN_004F2240`/`FUN_004F37B0`, `FUN_004F24D0`/`FUN_004F3800`, `FUN_004F2760`/`FUN_004F3850`, `FUN_004F29F0`/`FUN_004F38A0`, all above the 0x4F2000 boundary) that were not decompiled in this pass; by call position they sit alongside the blend/solve pairs and are almost certainly part of the same RK4 loop (state snapshot or a second integrated quantity), not renamed.

**Where rpm actually goes.** The confirmed rpm source is wrapper+0x890 (car+0x29AC, exact per RIGID_BODY.md), which is outside the 0x8B0-0x97C range the RK4 solve above touches; its writer was not found inside body_gear_update's subtree in this pass, open question. What IS proven (RIGID_BODY.md, `body_finalize_wheels`) is that this rpm source feeds a clamp-and-lerp (wrapper+0xD78/0xD7C/0xD80 = car+0x2E94/0x2E98/0x2E9C) once per tick, and separately the tick itself scales the same car+0x29AC value into car+0x32F8 (clamped 500-10000) for the HUD/engine sound. No path from body_gear_update's RK4 wheel-spin output back into car+0x25FC/0x2600 (the linear force accumulator) or into car+0x3234 (speed) was found. **Net: gear and rpm in this build drive presentation (HUD rpm readout, engine sound pitch, wheel spin animation) and the wheel-spin/tire-slip numbers, not the car's forward motion; the motion-relevant "engine force" is the separate, much simpler `car+0x2610`-based computation the tick itself does before the substep loop (CLIENT_PHYSICS_MAP step 12-13).**

## Offset table

| Offset | Base | Field |
|---|---|---|
| car+0x36A8 | car | active effect code |
| car+0x36AC | car | steering loss / wobble amplitude, per-effect ramp |
| car+0x36B4, 0x36B8, 0x36C0, 0x36C4 | car | zeroed by effect_end, meaning not traced beyond "cleared on end" |
| car+0x36BC | car | code-300 quadratic coefficient input, starts at 34.0 |
| car+0x36C8 | car | code-300 end-condition value |
| car+0x36CC/0x36D0/0x36D4/0x36D8 | car | yaw+position snapshot taken by every effect on apply |
| car+14000 (0x36B0 family) | car | code-300 elapsed-time accumulator |
| car+0x2604 | car | code-300 secondary accumulator |
| car+0x25FC/0x2600 | car | local-frame velocity pair scaled by every effect's speed multiplier |
| car+0x3520 | car | sound/part-slot source used for the effect-apply sound |
| world/track object this+0x1164/0x1938/0x1DEC/0x240C/0x1B8/0x2A04+i*4 | track | per-gimmick-file entry counts (boost/itembox/itembite/itemdrum/start/follow) |
| world/track object this+0x1168/0x193C/0x1DF0/0x2410/0x1BC/0x2A18+i*0x1900 | track | per-gimmick-file data arrays |
| DAT_00B23290 | global | staging directory for gimmick_ini_cache_extract, resolved per ini load |
| wrapper+0x550/0x578/0x57C[]/0x580/0x584[] | wrapper (car+0x211C) | gear ratio state and up/down-shift curve tables |
| wrapper+0x5A0/0x5A4/0x5A8/0x5AC/0x5B0 | wrapper | per-wheel gain and neutral-display threshold |
| wrapper+0x800-0x848 | wrapper | wheel 0/1 tire-model geometry input to gear_tire_force_model |
| wrapper+0x81C-0x848 | wrapper | wheel 2/3 tire-model geometry input |
| wrapper+0x858/0x85C | wrapper | traction-limit pair (RIGID_BODY.md), also gear ratio input here |
| wrapper+0x860/0x874/0x888/0x89C | wrapper | per-wheel 5-float tire force result, overlaps car+0x297C "yaw rate" |
| wrapper+0x8B0-0x97C | wrapper | RK4 wheel-spin state (k-values, position/velocity per wheel) |
| wrapper+0x980 | wrapper | per-stage h/torque value threaded through FUN_004F37xx |
| wrapper+0x984/0x988 | wrapper | gear-scaled target wheel accel (RIGID_BODY.md) |
| wrapper+0x98C/0x990 | wrapper | current gear / display gear (RIGID_BODY.md) |
| wrapper+0x890 | wrapper | rpm source (RIGID_BODY.md), not written by this subtree |

## Constants read (address, bytes LE, value, used by)

| Address | Bytes (LE) | Value | Used by |
|---|---|---|---|
| 0x5A69E0 | 00 50 43 48 | 200000.0 | effect 300 apply, car+0x2604 accumulator |
| 0x5A69E4 | D7 A3 70 3F | 0.94 | effect 700/1000 speed scale |
| 0x5A69E8 | AE 47 61 3F | 0.88 | effect 400/1000 speed scale |
| 0x5A69EC | A0 1A 6F 3F | 0.93 | effect 500 speed scale |
| 0x5A69F0 | 00 00 96 43 | 150.0 | effect 300, 36AC recovery rate |
| 0x5A69F4 | 34 33 EB 41 | ~29.4 | effect 300, 36C8 quadratic term |
| 0x5A69F8 | D7 A3 80 3F | ~1.005 | effect 300 update, 2604 multiplier |
| 0x5A69FC | 00 00 87 44 | 540.0 | effect 100 end cap, effect 400 clamp |
| 0x5A6A00 | D9 CE 77 3F | ~0.967 | effect 100 speed scale |
| 0x5A6A04 | 48 E1 7A 3F | ~0.98 | effect 200 speed scale |
| 0x5A3BF4 | 00 00 7A 44 | 1000.0 | effect 100/400, 36AC ramp rate |
| 0x59FF80 | 00 00 48 43 | 200.0 | effect 200, 36AC decay rate |
| 0x42080000 | immediate | 34.0 | effect 300 apply, 36BC seed |
| 0x5A8378 | AB AA 2A 3E | ~0.1667 (1/6) | gear_rk4_step_coeffs, RK4 final weight |
| 0x5A8370 / 0x5A8374 | 00 24 74 C9 / 00 24 74 49 | -1,000,000.0 / +1,000,000.0 | gear_rk4_combine clamp bounds |
| 0x49742400 / 0xC9742400 | immediate | 1,000,000.0 / -1,000,000.0 | gear_rk4_combine, out-of-range snap |
| 0x5A3BEC | 6F 12 83 3A | ~0.001 | gear_tire_force_model, minimum slip speed gate |
| 0x5A15EC / 0x5A1650 | (TICK_HELPERS) | 0.2 / 0.3 | gear_wheel_solver_dt mode-2 blend |

## Open questions

- The writer of the three 16-entry hit tables (0x2F08E10 for effect 500, 0x2EF3000 for itembite/700, 0x2EF48E8 for itemdrum/1000) was not located: no radius, box, or distance constant was found for the actual car-vs-gimmick collision test, only the per-frame scanners that read the tables once an entry is already active. Same open-write-xref situation TICK_HELPERS already flagged for the 0xBFC3B0-0xBFDA1C counters, likely the same runtime-indexed pattern.
- The live boost-pad detector (a per-tick scan of the loaded boost.ini entries against the car, calling car_boost_start with the ini's kind field) was not located; only the loader and file format are proven.
- gimmick_pool_update's state 200/201 scripted-teleport branch and car_respawn_state_machine's own 100/200 states (TICK_HELPERS) share a near-identical shape (freeze input, pan camera, warp); whether they call into shared code or just repeat a pattern used elsewhere in the file was not resolved.
- Effect codes 800 and 900's real names/triggers are guesses; no apply-side trigger for 900 was found in this pass (car_drift_update and car_effect_update both treat it as a known code, but nothing was seen calling car_effect_apply(carIndex, 900)).
- wrapper+0x860 (car+0x297C) is written both as gear_wheel_force_solve's wheel-0 tire-force output and read/copied by the tick as "yaw rate" (CLIENT_PHYSICS_MAP). Both reads are individually proven; which one is live at the point the tick copies it out was not resolved, flagged rather than asserted as a bug.
- The five FUN_004F2xxx/FUN_004F37xx/FUN_004F38xx helper pairs called between each RK4 stage inside body_gear_update were not decompiled; by position they belong to the same wheel-spin solve.
- Field meaning beyond column order in boost.ini (the int "kind"), itemdrum.ini and start.ini's 4th float was not resolved. follow_NN.ini is settled, x y z yaw, the respawn teleports to a row, GROUND_AND_RESPAWN.md round four.
- world_probe_wheel_point's 0x42200000 (40.0) argument to FUN_00497160 inside gimmick_pool_update was read but its role (search radius vs. warp distance) was not confirmed.

## Round four

The item slot penalty. `item_slot_lookup` 0x451D30 takes an inventory object and an index, returns object plus 4 plus index times 0x10 when the index is under the count at object +0x44, else 0. The tick calls it at 0x49CA6B on car+0x348C with index 0, so slot 0 sits at car+0x3490 and the count at car+0x34D0, four records fit. Record +4 is the kind, record +8 the count. Kind 3 with a count under 1 multiplies the throttle factor by 0.978 (0x5A6B00), a held item of kind 3 with nothing left costs engine.

## Round six, read on the bytes 2026-09-15

### car_effect_apply 0x495C30, code by code

The code lands in car+0x36A8 and car+0x36AC goes to 0 before the switch, an unknown code stays set. Every code snapshots yaw and position. Then:

- 100 and 700, sound 6, `car_boost_clear` 0x496930, `car_drift_state_set(0)`
- 200, the same then the spin seed, car+0x36C0 0x36C4 0x36C8 0x36B4 0x36B8 zero, 36BC 34.0, the elapsed car+0x36B0 zero, car+0x2604 plus dt times 200000
- 300, the same as 200 plus `car_gear_clamp(1)`, the six flags game+0x18 to 0x2C zeroed (0x28 included), the local car starts the cancel boost kind 6, the crash recovery state 1 with the time
- 400 and 500, sound 6, boost clear, drift clear
- 600 and 900, boost clear and drift clear with no sound
- 800, the snapshot only
- 1000, `car_impact_effect_play(1)` then sound 6, boost clear, drift clear
- 0x44C, sound 6 and the camera pan FUN_0043D7E0, no boost clear, no drift clear

`effect_end` 0x495BE0 zeroes car+0x36C0 0x36C4 0x36C8 0x36A8 0x36AC and the elapsed, it leaves 36BC and 2604 alone.

### car_effect_update 0x4960A0, the corrections

- the idle dispatch starts 500, 700 and 1000 inline (snapshot, code, 36AC zero, sound 6, boost clear, drift clear, 1000 adds the impact effect), each check only while the code is still 0
- 200 ends when car+0x36AC is not exactly 0 and `car_wheels_in_air_count` 0x48D730 (was FUN_0048D730, it is `body_wheels_on_ground_count` on the wheel set) is at most 1, the "durability check" was the wheels in the air
- 300 computes car+0x36C8 from the elapsed before this tick adds dt, and car+0x36AC goes down by dt times 300 (0x5A69F0 reads 300, the subtraction at 0x49638A), car+0x2604 times 1.005 every tick, game+0x28 is zeroed with the five flags
- 400 ends when `effect_hive_hit_lookup` 0x4CF020 (was FUN_004CF020) finds no slot: the table at 0x2F077F0 (ECX at 0x4964AB), 16 entries stride 0x160, byte at entry minus 8 equal 1, int at entry plus 0 the car index, int at entry plus 4 equal 0x69
- 600 ends when `gimmick_pool_slot_lookup` 0x4B9FE0 (was FUN_004B9FE0) on the carry pool 0x2EFC818 (ECX at 0x4964CD) finds no slot: 8 slots stride 0x250, byte at base minus 4 equal 1, int at base plus 0 the car index, int at base plus 0x30 under 200 the slot state
- 900 is the same lookup on a second pool of the same shape at 0x2ECDA38 (ECX at 0x496512), the item use dispatch FUN_004AEFA0 writes that one
- 700 and 1000 play sound 6 and scale car+0x25FC 0x2600 every tick, not once
- 0x44C ends when DAT_00D6E1D0 reads 0, the cutscene busy flag, port field `cutsceneBusy`

### The carry pool, gimmick_pool_update 0x4C7ED0

Not an item box. Each live slot (byte at base minus 0x30, car at base minus 0x2C) carries its car along the follow polyline:

- state 0, `car_effect_apply(600)`, a particle at the car, the timestamps at base 0x1F0 and 0x1F8, state 1, the local car input freezes (FUN_0043ED70 7)
- state 1, after 600 ms state 100
- state 100, the window is 3000 ms, 6000 in licence test 1 (DAT_00BFDAC4 equal 1 in mode 0xD), plus 1000 for a remote car, plus 2000 when `car_has_ability` 0x4B8580 (was FUN_004B8580) answers ability 3, the local car pressing the slot 0 key drops early, so does licence counter 0xBFC510 at 4 or more. Inside the window `respawn_follow_advance_point` 0x489D50 (was FUN_00489D50) steps the follow point when the slot is within 15 or receding, the slot target advances 3.6 (0x40666666) along the bearing to the point, the slot eases a tenth toward its target, `world_ground_height_at` 0x485970 sets the z, off the mesh the slot snaps to the nearest follow row, the car position and yaw take the slot through FUN_004EC220 (body set position). After 1300 ms the land effect plays once. Past the window state 200
- state 200, state 201 and the release boost arms, the input freeze again (FUN_0043ED70 1), `respawn_checkpoint_nearest_all` then `respawn_follow_next_point` 0x489860 (was FUN_00489860) picks the next row, `body_place_and_probe` there with z plus 5.0 (0x5A3238) and the yaw toward it, then `car_push_along_yaw(40.0)` (0x42200000 at 0x4C8640), the 40 units are the drop kick strength, neither a radius nor a warp distance
- state 201, the slot 0 key pressed inside 200 to 1000 ms gives a kind 0 boost once, after 1300 ms FUN_004BA300 frees the slot

`car_has_ability` walks the catalogue records of the driver accessories (FUN_004510C0, pairs at record 0x84), the kart (FUN_0044F6F0, pairs at record 0x130, the `tail_pair0` and `tail_pair1` of the 0xC0 record) and the parts (FUN_0044FBC0, pairs at record 0xBC), each holding two (ability id, value) pairs, `catalogue_ability_check` 0x4B8460 rolls them through FUN_004B82A0. The port answers it with the `carHasAbility` hook.

### The boost pad detector, found

`car_respawn_state_machine` 0x4A1420 state 0 reads the cell name under each wheel (FUN_004A0680 is `world_wheel_query_surface`) and skips the scan while an effect 500 600 900 runs. `world_cell_warp_index` 0x486CA0 (was FUN_00486CA0) parses `WARP_%03d`, `world_cell_boost_pad_index` 0x486CF0 (was FUN_00486CF0) parses `BOOST_%03d`, both minus one. A BOOST cell at 0x4A3157 calls `respawn_checkpoint_set(car, minus 1, name)`, car+0x6BC goes to minus 1 with the cell name at car+0x6C0, and takes row N minus 1 of boost.ini at 0x1AE0978 stride 0x14: kind 3 drops the drift and the gauge and calls `car_launch_pad_kick` 0x497190 with the four row floats (yaw, push, kick value, kick decay) and boost kind 2, any other kind calls `car_boost_start(kind)`. State minus 1 (0x4A14AD) waits until `car_name_match_count` of the name at car+0x6C0 is 0, then car+0x6BC returns to 0. So a pad fires once per crossing. The boost.ini floats are settled for kind 3: yaw in degrees, the push strength, the up kick times 1.6 and its per tick decay. The port scans in `car_respawn_state_machine` case 0 and minus 1.

### The pet effects

The 0x15 and 0x16 "part catalogue types" of the earlier rounds are the equipped pet kinds, TICK_HELPERS.md round six.

## Driver clips

Read on 2026-09-15 in KnC.exe.raw. The "sound 6" of the rounds above is not a sound, FUN_0048B460 is `driver_clip_play` and 6 is the MT_DAMAGE clip of the driver body. Same for the "engine sound state" of INPUT_AND_STATS.md and TICK_HELPERS.md, that state is the driver clip id.

The driver body is not one of the car models. It lives in the driver manager at 0x1AF2BB0, thirty records of 0x7C0 from manager+8. `driver_manager_load_driver` 0x48CBB0 (was FUN_0048CBB0) loads `Driver/Body/High/<name>/body` and its `body.kfm` through `model_load_kfm` 0x443DF0 (`body_room.kfm` in mode 9), then the five accessory models. Record+0x10 is the car index, record+0xFC the clip playing (minus one at load), record+0x100 and +0x104 two voice sounds picked by the char id, record+0xEC/+0xF0/+0xF4 the seat from driver pos. The car points at its driver with car+0x3520. `driver_place_on_car` 0x48B800 (was FUN_0048B800) seats it every frame with a small sway.

`driver_clip_play` 0x48B460 (driverIndex, clipId) sets the KFM sequence id on the driver model and on the pet model through `model_set_target_clip` 0x443F20 (was FUN_00443F20, the EngineDLL kfm interface, vtable+0xC). It refuses while record+0xFC is in 5..19, a one shot still playing. Ids 9 and 7 play the voice at +0x100, ids 6 5 10 the voice at +0x104, both at the car position. The ids are the `body_Anim.h` enum beside the KFM:

| id | sequence | KF | length | who plays it |
|---|---|---|---|---|
| 0 | MT_IDLE | body_O_MAIN_MT_IDLE.kf | 0.433 s | the rest state |
| 1 | MT_DRIFT_LEFT_LOOP | MT_ITEM_DRIFT_LEFT_LOOP.kf | 0.2 s | steer left |
| 2 | MT_DRIFT_RIGHT_LOOP | MT_ITEM_DRIFT_RIGHT_LOOP.kf, sequence 1 of the 2 in the file | 0.2 s | steer right |
| 3 | MT_BACK_LEFT | body_O_MAIN_MT_BACK_LEFT.kf | 1.333 s | reverse |
| 4 | MT_TURBO | body_O_MAIN_MT_TURBO.kf | 0.133 s | boost |
| 5 | MT_CRASH_2 | body_O_MAIN_MT_CRASH_2.kf | 0.433 s | `car_substep_collision_response` 0x498C9F |
| 6 | MT_DAMAGE | body_O_MAIN_MT_DAMAGE.kf | 2.0 s | `car_effect_apply` 0x495CCA 0x495DBA 0x496011 0x496077, `car_effect_update` 0x49610A 0x49617D 0x49620A 0x496539 0x49658E, `car_respawn_state_machine` 0x4A3300 |
| 7 | MT_ITEM_GET | body_O_MAIN_MT_ITEM_GET.kf | 0.833 s | item pickup FUN_004AECD0 0x4AEDB8, `car_mission_rally_update` 0x4A3D3E |
| 8 | MT_ITEM_EMPTY | body_O_MAIN_MT_ITEM_EMPTY.kf | 1.2 s | item use with nothing FUN_004AEFA0 0x4AFA73 |
| 9 | MT_WIN | body_O_MAIN_MT_WIN.kf | 2.0 s | `drivers_play_result_clip` 0x48C420 (was FUN_0048C420), FUN_004B6FD0 not negative |
| 10 | MT_LOSE | body_O_MAIN_MT_LOSE.kf | 2.067 s | same, FUN_004B6FD0 negative |

Every sequence loops in its KF (cycle 0). The end of a cycle fires the engine callback (event kind 3, posted as window message 0xFB8 by the callback object at 0x444130 and friends, registered for ids 0..19 by `model_register_clip_end_callbacks` 0x4440B0) and `driver_clip_end_of_sequence` 0x48B610 (was FUN_0048B610) sends ids 5 6 7 8 and 11..19 back to idle, ids 0 1 2 3 4 9 10 keep looping. The KFM transitions are all DefaultNonSync, the file default is a 0.1 s blend, the port cuts hard.

The state rule has two writers:

- Local car, `input_poll_keyboard` 0x497D9A to 0x497E14, once a frame after the keys. Turn state car+0xA78E4 = 1 with game+0x24 (steer left), 2 with game+0x20 alone (steer right), else 0. Then clip 3 when game+0x2C (stuck or reverse latch) and car+0x3234 (speed, units a second) > 10.0 (0x59F404), no call at all when stuck under 10, else 4 when car+0x3300 (boost active) != 0, else speed > 10.0 and steer left gives 1, steer right 2, else 0.
- Remote and ghost cars, `car_visual_update` 0x48F515 to 0x48F568, skipped for the local car (car+0x744 == DAT_01A20658) and while DAT_01AE8AE4 is set. Clip 3 when car+0x332D (reverse flag), else 4 when car+0x3300 != 0, else car+0xA78E4 when speed > 3.0 (0x5A32B8) and it is 1 or 2, else 0.

The drift state car+0x35A4 never picks a driver clip, the "drift" loops follow the steer side. Ghost samples carry the turn state in bits 2 and 1 of the flag word (`car_ghost_sample_apply` 0x4A0268), the reverse flag in bit 5, the boost in bits 7 and 6, so a ghost uses the remote rule. `driver_face_by_clip` 0x48B770 (was FUN_0048B770) swaps the M_FACE texture by the clip playing (FUN_0048D3B0 turns char id plus clip into a face index), not ported. `model_anim_enable` 0x444740 (was FUN_00444740) is the bool that starts or stops the actor manager of any model, not a clip pick.

Port: `ghost_driver_sequence` and `ghost_driver_clip` in `tools/track_scene/ghost_car.h`, played by the map viewer.
