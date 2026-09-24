# Input and stats

Read in KnC.exe.raw, image base 0x400000, project "Reverse HBO", on 2026-09-14. Covers the local input writer and key bindings, the source of the 17 kart stats and their part bonuses, the per-track license record, and two open tick details (local_7a0, vehicle kind branches). Builds on CLIENT_PHYSICS_MAP.md, REMOTE_AND_INPUT.md, TICK_HELPERS.md and CONSTANTS.md.

## Input flags

`car_input_poll_dispatch` (0x497E40, already named) gates on game+0x1397384 (session flag) and DAT_00b23614, zeroes car+0x332c/0x332d, reads game+0x94 (device index 0-3) and dispatches to one of four sub handlers. After the dispatch it sets car+0x332d (reverse) to 1 when car+0x2de8 is negative.

Device 0: `input_poll_keyboard` (0x497AB0, renamed this pass) is the writer of the flags at **game**+0x18 (accel), +0x1c (brake), +0x20 (steer right, slot 3), +0x24 (steer left, slot 2), +0x2c (stuck). The left and right words of this pass were guessed from the offset order and are corrected in round five below. These are game-relative, no car-index multiply is applied at the store sites (read at 0x497ab0-0x497e19), confirms CLIENT_PHYSICS_MAP's "game object input flags": there is one input state for the whole game object, not one per car slot. Logic, all read from the 0x497AB0 decompile:
- binding 2/3 swap when `car_is_camera_reversed` (0x4BD8D0) is true: normally binding 2 -> +0x24 (the left turn), binding 3 -> +0x20 (the right turn); reversed swaps them.
- binding 0 -> +0x18 (accel), unconditional.
- if car+0x9d4 == 0: binding 1 -> +0x1c (brake); then `input_key_pressed(0x41)` ('A') calls `car_gear_shift_up` (0x4990A0, renamed), else `input_key_pressed(0x5a)` ('Z') calls `car_gear_shift_down` (0x4990D0, renamed). Both step an int at car+0x2de8 (up, capped by car+0x2f40; down, floored at 0), a discrete gear index, not a slot binding. Negative value later flips car+0x332d (see above).
- else (car+0x9d4 != 0): binding 1 held with speed/rpm above threshold sets a "shift into reverse" latch (+0x1c=1,+0x18=0); otherwise it works the +0x2c stuck flag.
- `world_wheel_bump_slot(carIndex)` (already named) >= 0 forces +0x18/+0x1c/+0x2c to 0.
- game+0x28 == 1, or `body_wheels_on_ground_count()` (already named) > 3, forces +0x18 to 0. game+0x28 is a fifth input-adjacent flag not named anywhere else read this pass, guess: a look-back/handbrake-view latch, not confirmed.
- +0x1c==1 also sets car+0x332c (brake latch consumed by the tick's crash recovery block per TICK_HELPERS).

Device 2: `input_poll_device2` (0x497270, renamed) reads a joystick/gamepad-style raw state block (DAT_00e520e0 word axis X, DAT_00e520e8 word axis Y, DAT_00e520ec.. button bytes) and, for slots 0-7, converts axis thresholds and button edges (`FUN_0044b0b0`/`FUN_0044b0c0`, not renamed) into **synthesized key-down state** through `FUN_0044b550(keycode)` (not renamed), the same per-key-code byte array `input_key_down` (0x44B580) reads. `car_input_poll_dispatch` then calls `input_poll_keyboard` right after. That is the join point: the pad path injects virtual key-down events, the keyboard path re-reads the same bindings and produces the actual flags, unaware whether a keystroke was real or synthesized.

Device 1 (`input_poll_device1`, 0x497940, renamed) and device 3 (`input_poll_device3`, 0x4975E0, renamed) are each called alone, with no following call to `input_poll_keyboard`. Device 1 only writes game+0x98/0x9c/0xa0/0xa8 (a steering axis pair) from `FUN_0044b830(1)` and mouse buttons 1/2 (literal VK 1/2), never the +0x18.. flags. Device 3 synthesizes key-down state for slots 0,1,2,3,5 only (same DAT_00e520e0 block as device 2, same `FUN_0044b550` mechanism) but likewise never calls `input_poll_keyboard`. Neither device path is proven to reach the actual flags in this call graph, open question, see below.

Counters 0xBFC3B0-0xBFDA1C: re-checked with `search_instructions` on the literal 0xbfc3b0 operand, same single hit as TICK_HELPERS.md already found, the read gate in `car_physics_tick_local` at 0x49c462 (`CMP dword ptr [0xbfc3b0],ESI`). No writer located this pass either; the twelve offsets are not evenly strided so a computed-address writer (itemType*stride+base) is the likely explanation, unresolved.

## Key bindings

`input_key_binding_lookup` (0x45AF30, already named): `this+0x100A0 + (slot+altIndex*8)*4`, slot 0-7, altIndex -1/0 primary, 1 secondary. `input_key_down` (0x44B580, already named): `this+4+keycode`. `input_key_pressed` (0x44B590, already named): `this+0x104+slot*4 == 1`.

Slot meaning, proven this pass from `input_poll_keyboard` and `car_drift_update` (0x49AA90):

| Slot | Meaning | Evidence |
|---|---|---|
| 0 | accelerate | 0x497ab0: binding(0) -> +0x18 |
| 1 | brake / reverse | 0x497ab0: binding(1) -> +0x1c, gated by car+0x9d4 |
| 2 | left steer (right when camera reversed) | 0x497ab0 binding 2 -> +0x24, 0x49aa90 local_20 = 0x25 VK_LEFT for slot 2, corrected in round five |
| 3 | right steer (left when camera reversed) | 0x497ab0 binding 3 -> +0x20, 0x49aa90 local_20 = 0x27 VK_RIGHT for slot 3, the yaw grows on +0x20 |
| 5 | drift | 0x49aa90: `input_key_binding_lookup(5,-1)` gates the whole drift state machine |
| 4, 6, 7 | not read by 0x497ab0 or 0x49aa90 in this pass | REMOTE_AND_INPUT.md's slot4/7 = item use (from FUN_004AEFA0, not re-checked here) and slot6 = cosmetic guess stand, unconfirmed here |

Item use is not reached through the slot table by anything read this pass; the only newly found hardcoded keys are 'A' (0x41) and 'Z' (0x5A), which drive the gear index above, not items. Look-back camera binding: not found, open question unchanged from REMOTE_AND_INPUT.md.

## The 17 stats

Confirmed source: the **S2C 0xC0 KartDefinition packet**, handler `stat_catalog_recv_0xc0` (0x47F4F0, renamed this pass; docs/packets/PACKET_REGISTRY.md already lists `0x00C0 KartDefinition sub_47F4F0, catalog, PROVEN`). Read order, from the decompile (`FUN_0044e910`/`FUN_0044eb30` calls): int32, int32, int32, 1 byte, int32 x4, three length-prefixed name strings (buffers 33/33/34 bytes), a 32-byte block, **a 68-byte block (17 floats, the stats)**, a 2x8-byte block, then an int32 part count and that many 16-byte part records.

`stat_catalog_store` (0x44F510, renamed): `__thiscall(this=catalog object, src)`. Refuses past index 0x3f (63 entries max). Copies 80 dwords (0x50, 320 bytes, the fixed head of the parsed packet, id through the 2x8-byte block, not the variable part list) into `this+4 + idx*0x140`, idx from `this+0x5004`, then increments the counter. This is the per-kart-type catalog slot.

`car_apply_kart_loadout` (0x490A70, renamed): `__thiscall(game, carIndex, kartVisualId=param_3, param_4=80-dword kart-type record ptr, param_5=15-dword part-loadout array)`. Copies param_4 (320 bytes, same size as one `stat_catalog_store` slot) into **car+0x33A4**. CLIENT_PHYSICS_MAP's stat block at car+0x3440..0x3480 sits at byte offset `0x3440-0x33A4 = 0x9C` (156) inside that copy. My own reading of `stat_catalog_recv_0xc0`'s stack-local layout puts the 68-byte stat block at record-relative 0xA4 (164) instead, the two disagree by 8 bytes, direction not resolved, flagged as guess for the exact wire-relative byte offset. The fact that the stat block travels inside the 0xC0 payload through this exact call chain is proven by the matching sizes (320-byte copy, 68-byte stat sub-block, 17 floats) and is not itself a guess.

.car files: found on disk, e.g. `<client>/Data/Car/Circler.car`, byte-identical to the example given in the task (`00 00 80 40 33 33 13 40 9a 99 99 3f 00 00 48 43 ...`, verified with `od`). `search_strings` for `.car`, `%s\.car`, `Data/Car` and `Car\\` against KnC.exe.raw returned zero matches (checked as both literal and regex). No function in the traced call graph opens a path by that pattern. Conclusion: the running client gets the 17 stats over the wire only (S2C 0xC0); the .car files are very likely server/tool-side source data used to build that catalog, not an asset the client loads itself. This is the strongest answer available from the binary; it is not proof that no code anywhere in the 7612-function image ever opens one.

Stat index naming (car-relative, base = car+0x3440 + i*4, per CLIENT_PHYSICS_MAP's own numbering). Indices 3,4,7,8,9,10,11,12,13,14 are already named. This pass adds:
- **index 5** (car+0x3454, bonus car+0xa794c): `car_boost_start` (0x496BE0, already named) mini-turbo target speed stat, `clamp((stat5+bonus)*0.2+1.0, 1.0, 1.2) * 120.0` km/h.
- **index 2** (car+0x3448): read once, `car_physics_tick_local` at 0x49CDDA (FLD), right after the throttle-jitter reseed block (car+0x6f8), added to car+0xa7940 (index **0**'s bonus, not index 2's own bonus, the mismatch is read directly off the disassembly at 0x49cde1, not assumed) and passed as an FPU argument into `FUN_004ED600` (rigid body layer, out of the scope this pass covers). Meaning not resolved beyond "an input to the body setup call", guess.
- **indices 0, 1, 6, 15, 16**: no read site found anywhere in `car_physics_tick_local`, `car_drift_update`, or the tick-helper functions TICK_HELPERS.md covers. Open question.

## The .car file

Corrected on the fourth pass: the file is read, by `catalogue_read_file` 0x4EFF20 through the ini cache and `dx8_rlg.dll`, into the spawn catalogue car+0x2EA0. It is the chassis and drivetrain block, not the 17 stats, see `RIGID_BODY.md` under Catalogue. The paragraph below is the first pass reading of the bytes and stays as history.

`<client>/Data/Car/Circler.car`, 416 bytes = 104 little-endian floats. First 17: 4.0, 2.3, 1.2, 200.0, 1.4, 0.8, 0.2, 800.0, 0.056, -0.54, 0.48, 0.48, 0.4, 0.4, 80.0, 80.0, 1.6. Bytes 0x150-0x19C ramp smoothly (12269..14000..1951), the same shape as the durability curve's sine ease (CONSTANTS.md), and bytes around 0x40-0x140 look like gear/suspension/engine tuning tables, not a flat 17-float block. If these 17 floats mapped 1:1 onto the runtime stat array in file order, file value 200.0 would land on the index the tick calls "stat 3" (max speed), but that value feeds `clamp(stat3+bonus+1, 1.0, 2.0)`, which only makes sense for a raw value near -1..1, not 200. The field-order correspondence between the .car file and the runtime stat array is therefore **not proven**, guess only; the file is treated here as a richer, design-time kart definition, not a byte image of car+0x3440.

## Bonuses

car+0xA7940..0xA7980 (17 floats) is zeroed at `car_physics_setup` (0x494D50, already named) and again inside `car_apply_kart_loadout` (0x490A70), right before applying parts (`for i=0x11: *puVar=0`, at 0x490b31-0x490b40).

`stat_bonus_add_part` (0x48F710, renamed): `__thiscall(game, carIndex, partId, durability)`. Looks up the part's catalog record through `FUN_0044FBC0(partId)` (not traced further, not renamed), then adds that part's own 17-float bonus block (record+0x78..+0xb8) into car+0xA7940..+0xA7980, one field at a time. The first 10 (indices 0-9, record offsets 0x78-0x9c) are scaled: `partStat * (1 + durability/50)`; the last 7 (indices 10-16, offsets 0xa0-0xb8) are added flat, no durability term.

`car_apply_kart_loadout` is the only caller (confirmed with `get_function_callers`). When the loadout record's 6th dword (param_4[5]) equals 1, it calls `stat_bonus_add_part` seven times with (partId, durability) pairs from param_5[1..14], a 15-dword loadout list (1 header dword + 7 part slots), matching the CarCraft shop's Wing/Bumper/R_Fender/F_Fender/Tire/Booster/Cover tabs (strings already in the binary at 0x5A0AA8 etc.). An 8th part (chassis/body) goes through `FUN_0048F8D0(carIndex, param_5)` separately, not traced.

## The license record

`world_track_init` (0x4875C0, already named) resolves the record with `FUN_004531F0(trackId)` (this = fixed global object at 0x1A45F50, per the disassembly at 0x4875E2 `MOV ECX,0x1a45f50`), stores the pointer at world_track_init's own this+0x1334C (0x4875EE). Record+0x30/+0x34/+0x38 copy straight into DAT_005EB6F0/DAT_005EB6F4/DAT_005EB6F8 at 0x48762C-0x48764D; DAT_005EB6FC is hardcoded to 0x43A00000 = 320.0 at 0x487653, not part of the record.

`FUN_004531F0` (0x4531F0, not renamed, outside the input_/stat_/car_ scope of this pass) is a linear scan of a 140-byte-stride (0x8C) table at 0x1A45F50, id field at entry+4, capacity at this+0x4604; returns 0 if not found.

The record's loader is the **S2C 0xC3 TrackDefinition packet**, handler `FUN_0047F990` (0x47F990; PACKET_REGISTRY.md: `0x00C3 TrackDefinition sub_47F990, catalog, PROVEN`; not renamed, same scope note). Wire order from the decompile: int32 id, int32, int32, a 36-byte name string at record+0xc (the track's folder name, used verbatim in `world_track_init`'s "World/%s/%s/track" sprintf), then int32 fields at +0x30, +0x34, +0x38 (the three floats), +0x3c, +0x40, +0x44, +0x48 (license grade required, matches PROTOCOL.md's "track catalog +72 grade", 0x48 = 72 decimal, confirmed by the same offset used in `FUN_00439630` and `FUN_00410640` as a grade-gate check), +0x4c, +0x50 (lap count, matches PROTOCOL.md's "+80 clamped 1..9", 0x50 = 80 decimal), +0x54, +0x58, +0x5c, +0x60, +0x64, then a final 36-byte display-name string at +0x68 (used for a `"%s_INFO"` UI label). Stored through `FUN_00453020(&record)` (0x453020, not traced), same one-slot-per-call pattern `stat_catalog_store` uses for kart types.

Current baked-in values, `read_memory` on KnC.exe.raw:
- DAT_005EB6F0 = `cd cc cc 3e` = 0.4. Also used by `body_finalize_wheels` (RIGID_BODY.md) as an engine-force prep value, same value as car+0x32E8's steering scale. Guess: a per-track engine/steering setup scale.
- DAT_005EB6F4 = `9a 99 19 3f` = 0.6. Used directly in `car_physics_tick_local` as the engine-force durability-penalty scale, `(1-durability[idx]) * 0.6 * 0.16 * engineForceBase`. Guess: a per-track engine-power scale.
- DAT_005EB6F8 = `00 00 b4 42` = 90.0. Turn-force baseline additive term, `turnForce = fVar1*0.3 + 90.0`. Guess: a per-track baseline turning force.

These three are whatever track record the static image last had loaded, not universal engine constants; the port needs the live per-track values from the 0xC3 payload, not these three numbers specifically.

Our server sends them from `track_catalog` columns `tuning_engine_setup_bits`, `tuning_engine_force_bits` (raw int bits, defaults 1053609165 and 1058642330, that is 0.4 and 0.6) and `tuning_turn_force` (90.0), named by server/scripts/056_column_renames.sql, see server/scripts/013_kart_part_wire.sql and SpawnPackets::trackCatalogEntry. The pre rename names in migration 013 (left unedited, see that file) matched the static image the same way. Migration 013 sets tracks 1 and 2 to 50 and 100 as raw ints, which read as floats near zero, the origin of those two values is not documented, worth a check against a capture. `fall_off_timeout_ms` at record+0x40 is the next field, default 500.

## Tick details

### local_7a0

`local_7a0` is a decompiler-reused stack slot, not one variable. At the top of `car_physics_tick_local` (attic line 137, `local_7a0 = FUN_0044ed50();`) it briefly holds the crash-recovery timestamp (time_now_ms), unrelated to what follows and the likely source of CLIENT_PHYSICS_MAP's "time value" phrasing. By the scaling block near attic lines 916-924, the same stack bytes hold a **float pair** (`local_7a0._0_4_` = x, `local_7a0._4_4_` = y), built a few lines earlier (attic ~349-356) from a per-wheel friction/grip loop:
```
local_7a4[i] = (grip(car+0xa7988)*2 + friction_a + friction_b) * 0.5 * wheelbase_or_track(car+0x32dc / car+0x32e0)
```
for i=1 (x, wheelbase-scaled) and i=2 (y, track-scaled), read directly off the decompile; exact physical meaning (a force or a torque split) not confirmed, guess.

Scaling, immediately after `car_drift_update` (0x49AA90) is called, read off the decompile and cross-checked byte for byte against CONSTANTS.md:
- if drift state != 0 (car+0x35a4): x *= 0.8 (DAT_005A322C, accel released) or 0.6 (DAT_005A164C, accel held), x only, y untouched.
- if slow flag == 1 (car+0x3720): x *= 8.0 and y *= 8.0 (DAT_005A15F0).
- if DAT_02F0DDB0 == 2 (camera mode): x *= 0.75 and y *= 0.75 (DAT_005A6B04).

CLIENT_PHYSICS_MAP's open question called this "scaled by drift and boost pad states". This pass finds drift state, the slow flag and camera mode are what gate the three multiplies at this exact spot; no boost-specific term sits here, boost affects the throttle factor a few lines earlier instead (`local_7a4 *= 0.99 while boosting`, already in CONSTANTS.md).

### Vehicle kinds

car+0x33B4 == 2 and == 5 do **not** appear inside `car_drift_update` (0x49AA90), the full decompile was re-read this pass, no 0x33B4 comparison exists in it. TICK_HELPERS.md's `car_suspension_shake` (0x49A8A0, kind 2 only) and `car_effect_lean_update` (0x49B3D0, kind 2 cornering lean) already cover the drift-adjacent kind-2 path; nothing new found in `car_drift_update` itself.

Inside `car_physics_tick_local` (attic lines ~877 and ~925, wheel-matrix build, step 16 of CLIENT_PHYSICS_MAP):
- kind == 2: the front wheel's cosmetic steer angle is taken directly from **stat 9** (wheel steer angle, car+0x3464+bonus 0xa795c) instead of the generic yaw-rate formula (`yawRate*6.0`, clamped +-30 deg) every other kind uses; the wheel spin torque is taken directly from **stat 8** (wheel spin, car+0x3460+bonus 0xa7958) instead of the generic formula built from `car_suspension_shake`'s return value (itself only nonzero for kind 2 anyway, per TICK_HELPERS).

Tail of the tick (attic lines 1568-1586, matches CLIENT_PHYSICS_MAP step 22 and TICK_HELPERS' `car_visual_update` note):
- kind == 5: an extra part spin accumulator at car+0xa7998 advances by `kmh(car+0x32f4) * DAT_005A699C * time_frame_delta()`, DAT_005A699C = `cd cc 4c 3d` = 0.05 (read this pass), sign flipped when the reverse flag (car+0x332d) is set, wrapped against a period at car+0xa7994, then pushed to the scene graph via `FUN_00444820`/`FUN_004447F0`, a continuously spinning extra part (propeller/spinner), independent of steering.

## Offset table

| Offset | Meaning | Where |
|---|---|---|
| game+0x18/0x1c/0x20/0x24/0x2c | accel/brake/steer right/steer left/stuck | written by `input_poll_keyboard` 0x497AB0 |
| game+0x28 | forces accel off when 1, or wheels-on-ground > 3 | `input_poll_keyboard` 0x497AB0, meaning not confirmed |
| game+0x94 | input device selector 0-3 | `car_input_poll_dispatch` 0x497E40 |
| game+0x98/0x9c/0xa0/0xa4/0xa8 | device 1/2/3 raw analog scratch | `input_poll_device1/2/3` |
| car+0x2de8 | gear index, +/- by 'A'/'Z', floor 0 cap car+0x2f40 | `car_gear_shift_up/down` 0x4990A0/0x4990D0 |
| car+0x33a4..0x34e4 (0x140) | working copy of the car's kart-type catalog record | `car_apply_kart_loadout` 0x490A70 |
| car+0x3440..0x3480 | the 17 base stats, sub-range of the above | same |
| car+0xa7940..0xa7980 | the 17 bonus floats, zeroed then summed per part | `stat_bonus_add_part` 0x48F710 |
| world-object this+0x1334c | resolved per-track/license record pointer | `world_track_init` 0x4875C0 |
| record+0xc / +0x68 | track folder name / display name (36 bytes each) | S2C 0xC3 handler 0x47F990 |
| record+0x30/0x34/0x38 | -> DAT_005EB6F0/F4/F8 | `world_track_init` |
| record+0x48 | license grade required | `world_track_init`, `FUN_00439630`, `FUN_00410640` |
| record+0x50 | lap count | `world_track_init` -> `FUN_004b0da0` |

## Constants read

All read with `read_memory` on KnC.exe.raw, 4 bytes little endian, IEEE-754 float32.

| Address | Bytes | Value | Used by |
|---|---|---|---|
| 0x5EB6F0 | cd cc cc 3e | 0.4 | license record float 1 |
| 0x5EB6F4 | 9a 99 19 3f | 0.6 | license record float 2, engine-force durability scale |
| 0x5EB6F8 | 00 00 b4 42 | 90.0 | license record float 3, turn-force baseline |
| 0x5A699C | cd cc 4c 3d | 0.05 | kind-5 extra-part spin rate |
| 0x5A6B04 | 00 00 40 3f | 0.75 | local_7a0 camera-mode scale |
| 0x5A15F0 | 00 00 00 41 | 8.0 | local_7a0 slow-flag scale |
| 0x5A164C | 9a 99 19 3f | 0.6 | local_7a0 drift/accel-held scale |
| 0x5A322C | cd cc 4c 3f | 0.8 | local_7a0 drift/accel-released scale |

## Open questions

- Stat readers and the wire offset: settled, see "The 17 stats, wire numbering, settled" at the end, only wire 13 has no reader.
- .car file field order against the runtime stat array: settled, unrelated, the file is the catalogue block car+0x2EA0 (`RIGID_BODY.md` Catalogue).
- `input_poll_device1` (0x497940) and `input_poll_device3` (0x4975E0): neither is proven to reach the actual game+0x18.. flags in the call graph read this pass.
- Item counters 0xBFC3B0-0xBFDA1C: writer still not found.
- `FUN_0044FBC0` (part catalog lookup) and `FUN_0048F8D0` (8th/chassis part bonus): not traced.
- `FUN_004531F0` / `FUN_0047F990` / `FUN_00453020` (track/license catalog): left unrenamed, outside this pass's input_/stat_/car_ prefix scope.
- Look-back camera binding slot: still unconfirmed.

## Round four

- The pair at 0x49CDDA is car+0x3448 plus car+0xA7940, in the wire numbering wire 0 base plus wire 0 bonus (the round wrote "stat 2" and a bonus block at +0xA7938, that was the old numbering, the bonus block is car+0xA7940 plus 4 times the wire index), one plus the sum times 0.01 (0x5A05E0) clamped 1.0 to 1.01 (0x5A6AE4) is the per tick velocity gain `body_vec3_scale` applies on car+0x25FC together with the throttle factor
- game+0x28 is the stopped flag. The tick clears it at 0x49CA05 and sets it at 0x49CD6A when the car coasts airborne with nothing pressed under speed 1, and at 0x49D120 when car+0x9D9 forces a respawn. `input_poll_keyboard` clears the accel flag while it is 1
- The car item inventory is at car+0x348C, four 16 byte slots from +0x3490, the count at +0x34D0, `item_slot_lookup` 0x451D30, EFFECTS_AND_GEAR.md round four
- The stuck flag block of `car_drift_update` reads the flags at game+0x20 and game+0x24 directly at 0x49B270. Game+0x20 turns the yaw up by dt times 300 times car+0x2A78, game+0x24 turns it down by dt times 300 times car+0x2A7C. With the camera not reversed slot 3 writes game+0x20 and slot 2 writes game+0x24

## Round five

- Slot 3 on game+0x20 is the right turn and slot 2 on game+0x24 the left turn, proven four ways: the drift update tags them 0x27 VK RIGHT and 0x25 VK LEFT, the stuck block turns the yaw up on game+0x20, the ghost recording turns the yaw up (toward wire plus y, the right side of a car driving toward minus x in a right handed z up frame) on the 0x90 sample which is slot 3, and the body geometry turns the car toward the wheel 1 side on channel 2 which is game+0x20. REMOTE_AND_INPUT.md round five has the addresses. The port's `InputFlags` are `steerRight` for game+0x20 and `steerLeft` for game+0x24
- `body_wheels_on_ground_count` returns the number of wheels in the air, so the `input_poll_keyboard` clause "count over three forces accel off" means no accel with all four wheels in the air, the port flag is `wheels_over_three_in_air`
- car+0x32E4 is not a yaw rate, it is the copy at 0x49D4CD of car+0x297C which is wheel set +0x520, the mean of the two front Ackermann tangents `body_steer_front_wheels` writes, the lateral push of the tick reads it airborne
- `car_apply_engine_force` 0x4968F0 only calls `body_apply_force` while game+0x1397384 is set, the same flag that gates `input_poll_keyboard`, so during a race both run

## Round six, read on the bytes 2026-09-15

### input_poll_keyboard 0x497AB0, the brake key on the race path

car+0x9D4 is 1 for the whole race (car_physics_setup sets it), so the `car+0x9d4 != 0` branch is the normal path and the `== 0` branch with the A and Z gear keys is the debug path. On the race path the slot 1 key held:

- car+0x32FC read as an int is not negative (the sign bit of the gear mirror clear), speed car+0x3234 at or over 1.0 and rpm car+0x32F8 over 1000: game+0x2C 0, game+0x1C 1, game+0x18 0, a brake
- otherwise game+0x2C 1, game+0x18 0, game+0x1C 0, a reverse, game+0x2C is the sixth body channel

Not held: game+0x2C 0 and game+0x1C 0. So the "stuck" flag is the reverse input of the client, the port names it so in the comment.

`input_key_pressed` 0x44B590 is indexed by raw key code, `this plus 0x104 plus code times 4`, the gear keys pass 0x41 and 0x5A, FUN_00401080 passes 0x56, the tick passes 0x31 0x32 0x33, the drift update passes the code of slot 0. The port array holds 256 ints.

The rest of the function: with `world_theme_is_special_row` the slot 4 key held starts a kind 5 boost unless DAT_02EB4828 is set, and releases a running kind 5 boost through `car_boost_clear`. Race mode 9 with no accel no reverse and neither steer key scales car+0x25FC by 0.85 (0x3F59999A). With car+0x3520 not negative the turn state car+0xA78E4 is 1 on game+0x24, 2 on game+0x20 alone, else 0, and FUN_0048B460 gets 3 in reverse over 10 kmh, 4 with a boost, 1 or 2 turning over 10 kmh, 0 otherwise. Correction 2026-09-15: FUN_0048B460 is `driver_clip_play`, the value is the driver body clip id, EFFECTS_AND_GEAR.md Driver clips. DAT_02EB0650 latches 1 once the accel key is seen, a HUD flag.

### input_poll_device2 0x497270, the pad

The pad object is 0xE52048: 0xE520E0 the x axis word, 0xE520E8 the y axis word, 0xE520EC the 128 held bytes (FUN_0044B0B0 reads `this plus 0xA4 plus index`), 0xE52048 plus 0x1A4 the 128 pressed ints (FUN_0044B0C0). FUN_0044B0E0 true resets game+0x98 to 0xA8. The steer axis game+0x98 is `(axis x minus 0x8000) times 1 over 32768` (0x5A6A28). The pedal is 0 with the secondary binding of slot 0 held, 65536 (0x5A6A24) with slot 1 held, 32768 otherwise, negated by game+0xA8, split into game+0xA0 and game+0x9C by sign. Ten edge flags at DAT_02EB0440 and 128 pressed latches at game+0xAC with a 100 ms release at game+0x2B0. Then the axis under minus 0.2 (0x5A6A20) synthesises the slot 2 key and over 0.2 (0x5A15EC) the slot 3 key, swapped with the camera reversed, held pad buttons of the secondary bindings synthesise slots 0 1 5 6, pressed ones slots 4 and 7, all through FUN_0044B550 which writes the held byte of the primary key code. `input_poll_keyboard` then reads the same bytes.

### Device 1 and device 3 never reach the flags

`car_input_poll_dispatch` 0x497E40 calls `input_poll_device1` alone for game+0x94 equal 1 and `input_poll_device3` alone for 3. Device 1 (mouse) integrates the mouse x from FUN_0044B830 into game+0x98 (gain 1 over 256 at 0x5A6A2C, decay 1 over 32 at 0x5A32D0 when still), maps mouse button 1 to the pedal and button 2 to game+0xA8, steps the debug gear keys, and writes nothing else. A program wide search finds no reader of game+0x98 outside the three device polls, so the mouse steering is dead in this build. Device 3 (the dev pad) reads both axes and buttons 0 and 1 (drift and reverse), synthesises the slot 0 1 2 3 5 keys from them and maps buttons 2 to 8 to debug spawn calls on DAT_02EC24E0. Its synthesised keys reach the direct readers of `input_key_down` (the drift update slots 2 3 5, the coast check slot 0, the ghost mask) and never game+0x18 to 0x2C. Settled, the port keeps both paths and reports `handled` false.

### The stats and the pets

The 0xC0 record `tail_pair0` and `tail_pair1` at record 0x130 and 0x138 are two (ability id, value) pairs `catalogue_ability_check` 0x4B8460 reads, the parts hold theirs at record 0xBC and the driver accessories at 0x84. The in race pet effects read the owned pet container 0x1A69708, see TICK_HELPERS.md round six, the port keeps it in `stats.h`.

## Round seven, the catalogue fields on the bytes 2026-09-15

Read with the container method: each S2C handler stores into one global container, every reader of a record offset was found through the lookup functions and their callers, plus the cached record pointers. Full tables in docs/packets/opcodes/0x00C0.md, 0x00C1.md, 0x00C2.md, 0x00C3.md, 0x00C5.md, 0x00C6.md, 0x00BF.md.

- The 0xC0 record offset 0xA4 is the stat block, settled. car+0x33A4 holds the whole record so rec+0xA4 is car+0x3448, not car+0x3440 (corrected below, the round seven text said 0x3440 and carried the two slot shift into the column names of migration 057), the record copy in `car_apply_kart_loadout` is 0x50 dwords from record 0x00.
- `garage_stat_bars_compute` 0x428AB0 reads the record by wire offset: bar one wire 0 plus 1, bar two wire 2, bar three wire 8 plus 10 plus 11, bar four wire 3, each normalised over the whole catalogue. The column names of migration 057 mixed that with the port numbering (stat02_accel_scale to stat14_grip sat two too high), migration 059 renames them to the wire truth, see the settled section below.
- The second value of each loadout pair `car_apply_kart_loadout` hands to `stat_bonus_add_part` is the part GRADE from the 0x3E custom block (owned part instance +0x80, the CAR_PART_BASIC to LEGEND ladder in FUN_0042FE20), not a durability. The scale is 1 + grade div 50 with integer division. The kart durability is the owned kart record +0x30 when +0x2C is 3, 0 to 500, drawn by `kart_durability_bar_draw` 0x42AD20, it never enters the stat sum. The word durability in the Bonuses section above and in stats.h should read grade.
- The 17 part bonus floats live in the 0x0108 CarCraftPartDefinition record at +0x78 (`part_def_record_lookup` 0x44FBC0, stride 0x120), the 0xC2 KartPartDefinition record has no stat block, its dwords are required level, restrict target, equip slot and restrict key.
- 0xC0 rec+0x10 is the vehicle kind, copied to car+0x33B4 (the kind 2 and kind 5 branches of the tick) and turned into the wheel count car+0x9E0 by `vehicle_kind_to_wheel_count` 0x49A2A0: 2 gives 4, 3 gives 6, 4 gives 0, anything else 4. chibikart sends 2 on every kart.
- The three track floats 0x30 0x34 0x38 keep their names. 0x54 and 0x58 are fog near and far, `world_fog_near_get` 0x486BC0 and `world_fog_far_get` 0x486BF0, the fog setter 0x58B370 is a RET 0x10 stub in this build, but `world_track_init` also feeds 0x58 to `camera_far_clip_set` 0x43E520 so fog_far is the camera far clip. 0x5C 0x60 0x64 are the lens flare sun position (`lens_flare_init` 0x4D2A30, z below zero turns it off). 0x44 is the difficulty star count, 0x48 the required license class against byte 0x1A20B08, 0x4C hides the track in room game mode 0 and 1. 0x3C has no reader.
- The license record 0xC5 mirrors the same tail: 0x4C fog near, 0x50 fog far and far clip, 0x54 0x58 0x5C lens flare, 0x3C 0x40 reward item type and key.

## The 17 stats, wire numbering, settled

Read on the bytes 2026-09-15. Every stat index in the repo is now the WIRE index, 0 to 16, the column order of the 0xC0 record block at rec+0xA4. The older sections of this file, CONSTANTS.md, TICK_HELPERS.md and CLIENT_PHYSICS_MAP.md counted from car+0x3440, two ahead of the wire, their numbers in the rounds above are the old ones and are corrected here.

The chain, three copies with no reordering:

- `stat_catalog_recv_0xc0` 0x47F4F0 reads the wire into a 0x140 stack struct in wire order, the 17 floats as one 0x44 byte block at struct 0xA4
- `stat_catalog_store` 0x44F510 copies 0x50 dwords from struct 0x00 into the container 0x01A22638 (stride 0x140, entries from +4)
- `car_apply_kart_loadout` 0x490A70 copies the whole entry to car+0x33A4 (`LEA EDI,[EBP+0x33A4]; MOV ECX,0x50; MOVSD.REP` at 0x490AF0 to 0x490AFD, EBX the record argument), then zeros 17 floats at car+0xA7940 (0x490B31) and with model scheme 1 runs `stat_bonus_add_part` 0x48F710 seven times, which adds part record 0x78+4k into car+0xA7940+4k

So wire float k sits at car+0x3448+4k and its part bonus at car+0xA7940+4k, and every tick read pairs those two (the FLD of car+0x3448+4k is followed by the FADD of car+0xA7940+4k at each site below). car+0x3440 and car+0x3444 are not stats: they are record 0x9C and 0xA0, the skin keys 6 and 7 of the eight dword block at rec+0x84 (the item keys of use type 1 and 2, see docs/packets/opcodes/0x00C0.md). The "bonus block at car+0xA7938" of round four was the old numbering applied to the bonus side, no instruction in the image reads a float at car+0xA7938, and car+0xA793C is a byte flag (`car_apply_kart_loadout` 0x492F9A, FUN_0048D9A0 0x48DAD2).

Read sites, every instruction in the image with a car+0x3448 to car+0x3488 operand (`search_instructions` on each offset, plus the absolute form `[EAX+0x1B1C510]` with EAX = car index times 0xA7260, the car base is 0x1B19090). The functions FUN_004B34B0, FUN_004B3420, FUN_004B3720, FUN_004B3780 and FUN_004B3950 also carry 0x3454 to 0x346C operands, on another object (EBX or EDI base, byte and dword moves, no float), not the car:

| Wire | Car | Bonus | Meaning | Readers |
|---|---|---|---|---|
| 0 | 0x3448 | 0xA7940 | body setup input, the per tick velocity gain `1 + x * 0.01 (0x5A05E0)` clamped 1 to 1.01 (0x5A6AE4) on car+0x25FC | `car_physics_tick_local` 0x49CDDA, bonus 0x49CDE1 |
| 1 | 0x344C | 0xA7944 | max speed, `clamp(x + 1, 1, 2) * 320 (0x5EB6FC)` km per hour | `car_physics_tick_local` 0x49CA95, bonus 0x49CA9C |
| 2 | 0x3450 | 0xA7948 | steering gain, `clamp(x * 3 + 1, 1, 4)` on the wheel set max steer and the channel 2 and 3 rates | `car_drift_update` 0x49B1B2, bonus 0x49B1B9 |
| 3 | 0x3454 | 0xA794C | mini turbo target, `clamp(x * 0.2 + 1, 1, 1.2) * 120` km per hour, and the kind 0 duration `clamp(x + 1, 1, 2) * 400` ms | `car_boost_start` 0x496D2B, bonus 0x496D3B; `car_boost_update` 0x497065, bonus 0x49706B |
| 4 | 0x3458 | 0xA7950 | boost lean lift, `clamp(x * 30, 0, 30)` degrees of nose lift under a boost with no drift, visual only | `car_effect_lean_update` 0x49B4C0, bonus 0x49B4B9; `car_visual_update` 0x48EBC8, bonus 0x48EBC2 |
| 5 | 0x345C | 0xA7954 | turn force, `x + 1` clamped 1 to 2, over 2 snaps to 1.6 (0x5A68CC) in a drift else 1 | `car_physics_tick_local` 0x49CB45, bonus 0x49CB4C |
| 6 | 0x3460 | 0xA7958 | wheel spin, the kind 2 wheel spin torque and the lean lift scale | `car_physics_tick_local` 0x49D8D6 and 0x49D922 (bonus 0x49D8CD, 0x49D91B); `car_effect_lean_update` 0x49B54B, bonus 0x49B544; `car_visual_update` 0x48EC2B 0x48EF17 0x48EF61 |
| 7 | 0x3464 | 0xA795C | wheel steer angle, the kind 2 front wheel angle and the side lean scale | `car_physics_tick_local` 0x49D78F and 0x49D8F5 (bonus 0x49D797, 0x49D8FF); `car_effect_lean_update` 0x49B528, bonus 0x49B520; `car_visual_update` 0x48EC60 0x48EE41 0x48EF46 |
| 8 | 0x3468 | 0xA7960 | drift charge rate, `clamp(x * 0.5 + 0.3, 0.3, 0.8)` gauge per tick | `car_drift_update` 0x49AC11, bonus 0x49AC17 |
| 9 | 0x346C | 0xA7964 | drift steer, `clamp(x * 0.6 + 1.2, 1.2, 1.8)`, the drift slip scale and the spin add | `car_physics_tick_local` 0x49D3C9, bonus 0x49D3D0; `car_drift_update` 0x49B24D, bonus 0x49B253; `car_visual_update` 0x48E86E, bonus 0x48E868 |
| 10 | 0x3470 | 0xA7968 | mini turbo threshold, `clamp(1 - x * 0.8, 0.2, 1) * 10 (0x5EB704)` gauge | `car_drift_update` 0x49AEF5, bonus 0x49AEFB |
| 11 | 0x3474 | 0xA796C | mini turbo hold, `clamp(1 - x * 0.8, 0.2, 1) * 800 (0x5EB708)` ms | `car_drift_update` 0x49AF47, bonus 0x49AF4D |
| 12 | 0x3478 | 0xA7970 | grip, per wheel `x * (surface grip + car+0xA7990) * 0.2 (0x5A15EC)` into car+0x3728 | `car_physics_tick_local` 0x49C9D6, bonus 0x49C9E0 |
| 13 | 0x347C | 0xA7974 | no reader, only `stat_bonus_add_part` writes the bonus | none |
| 14 | 0x3480 | 0xA7978 | chase camera distance, 9.0 on the shipped rows, no bonus read | `camera_update` 0x43F529 and 0x43F882 (`[EAX+0x1B1C510]`) |
| 15 | 0x3484 | 0xA797C | chase camera pitch, 37 degrees shipped | `camera_update` 0x43F50A and 0x43F869 (`[EAX+0x1B1C514]`) |
| 16 | 0x3488 | 0xA7980 | look point height, 3.5 shipped | `camera_update` 0x43F510 and 0x43F863 (`[EAX+0x1B1C518]`) |

Wire 4 has readers, the boost wheelie of the lean update, so the "no reader slots" are 13 alone on the car side. The part grade scale of `stat_bonus_add_part` covers wire 0 to 9 (`1 + grade div 50`), wire 10 to 16 add flat. The recorded RageZone row `0.52 0.52 0.52 0.52 0.30 0.52 0.30 0.30 0.52 0.52 0.52 0.70 0.52 0.0 9.0 37.0 3.5` therefore reads: steering gain 0.52 (gain 2.56, not the 0.30 and 1.9 the old numbering gave), turn force 0.52, wheel spin and wheel steer angle 0.30, drift steer 0.52, mini turbo hold 0.70, grip 0.52, and the camera 9.0 37 3.5.

### The garage bars, wire inputs

`garage_stat_bars_compute` 0x428AB0 (this = the garage panel, param_2 = the 0xC0 record, param_3 = the seven part keys) zeros 17 floats at this+0x31C and sums the equipped parts into them with `garage_stat_bonus_add_part` 0x4286E0 (part record 0x78+4k times `1 + grade div 50` for k under 10, flat above, the same shape as 0x48F710, so this+0x31C+4k is the bonus of wire k). Then per bar, min and max over the whole catalogue through `kart_catalog_by_index`:

```
fraction = (max - min > 0) ? ((value + bonus - min) / (max - min)) * 0.2 (0x5A15EC) : 0, capped at 0.3 (0x5A1650)
bar      = (fraction + 0.6 (0x5A164C)) * 100 (0x5A1648), over 100 gives 100, under 40 (0x5A1644) gives 40
```

| Bar | this | Wire inputs | Label on Common_Car_Info_Back.png | Drawn by `FUN_00429040` at |
|---|---|---|---|---|
| one | 0x300 | rec+0xA4 + rec+0xA8, wire 0 + wire 1, bonus this+0x31C + this+0x320 | Speed | FUN_00428440, x+0x4C y+6, top left |
| two | 0x308 | rec+0xAC, wire 2, bonus this+0x324 | Handling | FUN_004284D0, x+0x4C y+0x1C, bottom left |
| three | 0x310 | rec+0xC4 + rec+0xCC + rec+0xD0, wire 8 + 10 + 11, bonus this+0x33C + 0x344 + 0x348 | Drift | FUN_00428560, x+0x15A y+6, top right |
| four | 0x318 | rec+0xB0, wire 3, bonus this+0x328 | Booster | FUN_004285F0, x+0x15A y+0x1C, bottom right |

Each drawer eases the shown value (this+0x2FC, 0x304, 0x30C, 0x314) toward the target by 8.0 (0x5A15F0) a frame and draws `trunc(value)` one pixel columns capped at 0x83 = 131, so a bar of 80 is 80 pixels wide. The shop uses the same shape on the parts: `part_stat_bars_compute` 0x427C30 (renamed this pass) takes a 0x108 part record and reads its 0x78 block with the same wire indices (0x78 + 0x7C, 0x80, 0x98 + 0xA0 + 0xA4, 0x84) against the kart catalogue min and max, fraction capped at 0.2 then times 152 (0x5A15E8) capped 152, and `stat_bars_compute_block_0x20` 0x428040 does the same on a record whose 17 floats sit at +0x20 (called from FUN_0045E110 with a record whose +4 is a part key). `shop_tile_draw_karts` 0x419720 draws no bars, only the ability pairs.

Our side: `client/net/Catalog.cpp` `statBars` already uses the wire indices, the server `PartStatPackets::garageBars` and the `kart_catalog` columns are renamed by migration 059 (`stat00_body_setup` to `stat16_camera_height`), the port `KartStatIndex` in `games/kart/physics/client/stats.h` is the wire index and every read goes through `stat_total(stats, KartStatIndex::X)`.
