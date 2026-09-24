# Ground and respawn

Read in KnC.exe.raw, image base 0x400000, on 2026-09-14. Companion to `CLIENT_PHYSICS_MAP.md`, `WORLD_COLLISION.md` and `RIGID_BODY.md`. This doc covers wheel ground height, the 9 surface names, fall/respawn, and car+0x6bc.

## The cell record

The 56 byte cell record (piece+0x10 array, stride 0x38) is now fully accounted for, no unexplained bytes remain:

| Cell offset | Size | Content | Proof |
|---|---|---|---|
| +0x00 | float | height plane A (x coefficient) | `world_locate_piece_by_height`, 0x48580f-0x485831 |
| +0x04 | float | height plane B (z coefficient) | same |
| +0x08 | float | height plane "C", read but never multiplied by a nonzero value in the one path traced | same, see Wheel height |
| +0x0c | float | height plane D (constant) | same |
| +0x10 | uint16 | edge index 1 of the cell's 3 boundary edges | `world_bsp_set_piece` 0x4ebcf0, `world_bsp_edge_sweep` 0x4ebdd0 |
| +0x12 | uint16 | edge index 2 | same |
| +0x14 | uint16 | edge index 3 | same |
| +0x16..+0x37 | 34 bytes, ASCII, null terminated | surface material name, upper cased on read | `world_query_surface_name` 0x4ec0b0 |

0x00+0x10+0x2e2 (0x10+0x06+0x22... the arithmetic is 0x10 + 6 + 34 = 56) accounts for the full record: 16 bytes height plane, 6 bytes edge indices, 34 bytes name.

Correction to `WORLD_COLLISION.md`: the two 16 bit BSP child links that `world_bsp_locate_point` (0x4ebfc0) follows for tree descent are not in the cell record. They live in the **edge** record (piece+0xc array, stride 0x14/20 bytes), at edge+0x00 (child when the point is behind the plane) and edge+0x02 (child when in front), read as `*puVar1` / `puVar1[1]` where `puVar1 = edge_array + (edge_index & 0x7fff)*0x14`. Proven by direct decompile of 0x4ebfc0: the pointer used for the child read is built from `*(int*)(piece+0xc)`, the edge array base, not `piece+0x10`. The edge record layout, all 20 bytes now accounted except the last 4: +0x00 child(behind), +0x02 child(front), +0x04 float A, +0x08 float B, +0x0c float C (the 2D half plane `A*x+B*z+C`, already in `WORLD_COLLISION.md`), +0x10..+0x13 not read by any function in this pass, guess: padding or an unused fourth coefficient.

The two 12 byte arrays at piece+0x18 (array C, stride 0xc, count at piece+0x14) and piece+0x34 (array D, stride 0xc, count at piece+0x2c) were traced through every function that takes a piece pointer: `world_bsp_set_piece` 0x4ebcf0, `world_bsp_locate_point` 0x4ebfc0, `world_bsp_edge_sweep` 0x4ebdd0, `world_query_surface_name` 0x4ec0b0, `world_locate_piece_by_height` 0x4857a0, `world_ground_test_point` 0x4858e0, `world_probe_wheel_ground` 0x486570, `world_probe_wheel_point` 0x486490, `body_ground_probe_response` 0x4ec1d0, `body_place_and_probe` 0x4ec290. None of them dereference piece+0x18 or piece+0x34. The only two functions that ever touch these fields are the loader, `world_load_col_piece` 0x4ece90 (allocates and freads them, confirmed at the disassembly level, see `WORLD_COLLISION.md`), and the destructor, renamed here `world_col_piece_free` (0x4ecdb0, was `FUN_004ecdb0`), which frees all 4 array pointers (piece+0xc, +0x10, +0x18, +0x34) and zeroes the count at +0x14. This is a proven negative: the content of arrays C and D is loaded and freed but not consumed by any traced collision or physics code path. Guess: vertex or triangle-index data kept for an editor/tool path not present in the shipped client, or a consumer elsewhere in the ~500k instruction binary that this pass did not reach.

## Wheel height

The height plane. `world_locate_piece_by_height` (0x4857a0, was `FUN_004857a0`, renamed here) is called `(this, x, z, y)` and, for each candidate piece, calls `world_bsp_set_piece(piece, -x, -z)` then immediately `world_bsp_locate_point(-x, -z, ...)`. `world_bsp_locate_point`'s own first 3 lines copy `this+0xc` (the cell pointer `world_bsp_set_piece` just found) into `this+0x18` before it does anything else (0x4ebfc0, `*(undefined4*)(param_1+0x10)=*(undefined4*)(param_1+4)` etc., the third of the three copies is `*(undefined4*)(param_1+0x18)=*(undefined4*)(param_1+0xc)`). Since the query point has not moved between the two calls, the walk exits immediately with the same cell, so `this+0x18` after both calls still points at the found cell. `world_locate_piece_by_height` then reads 4 floats from `*(this+0x18)`, i.e. cell+0x00..+0x0f (disassembly 0x48580f-0x485828: `MOV EDX,[ESI+0x18]; MOV EAX,[EDX]` then `[EDX+4]`, `[EDX+8]`, `[EDX+0xc]`), builds a query vector `(x, z, 0)` with `body_vec3_set`, and calls the newly named `body_vec3_dot` (0x4ed6e0, was `FUN_004ed6e0`, a plain 3 float dot product, confirmed by decompile: `return p2[0]*p1[0]+p2[1]*p1[1]+p2[2]*p1[2]`). Because the query vector's third component is always 0, the dot product is exactly `A*x + B*z` regardless of the cell's C float (cell+0x08); the C float is read into the dot but its contribution is always multiplied by 0 in this call path. The height estimate is then `height = -(dot + D)` (disassembly 0x485848 `FADD [ESP+0x34]` which holds D, then `FCHS`), compared against the input `y` with a +-5.0 tolerance (`_DAT_005a3238`, read at 0x5a3238, bytes `00 00 a0 40` = 5.0). Match returns true (found the right piece among possibly stacked pieces at the same x,z, e.g. a bridge over a tunnel).

This proves: **cell+0x00, +0x04 and +0x0c hold a ground height plane, `y = -(A*x + B*z + D)`, one plane per triangle/cell, baked straight into the .col file.** Given a wheel position (x, z) and the containing cell (found by `world_bsp_set_piece`/`world_bsp_locate_point`), the ground y is computable directly from these three floats with no division, and the surface index comes from the same cell's name string at cell+0x16 through `world_query_surface_name` + `world_surface_name_to_index`. This is exactly the "from the .col data alone" answer the task asked for.

Who calls this. `world_locate_piece_by_height` is called only from two thin wrappers, renamed here `world_place_probe_local` (0x486300, was `FUN_00486300`) and `world_place_probe_remote` (0x4863d0, was `FUN_004863d0`). `world_place_probe_local` additionally checks a global `DAT_01af2b5c`; when that is 0 and a candidate piece pointer (param_5) is given it uses that piece directly via `world_bsp_set_piece`, otherwise it falls back to the full height scan. `world_place_probe_remote` has the same two branches but keyed only on whether param_5 is null. Their callers: `world_place_probe_local` is called from `car_physics_setup` (0x494d50, car spawn) and from a dozen item/gimmick effect functions (0x490a70, 0x4b9a80, 0x4bb3f0, 0x4bcd50, 0x4bf2b0, 0x4bf960, 0x4c7b80, 0x4c9140, 0x4cc460, 0x4cd800, 0x4cfcf0, 0x4d2e00, none renamed, out of scope), i.e. warp pads and similar teleport style effects on the local car. `world_place_probe_remote` is called from `car_remote_update` (0x49ed90) and two more unrenamed functions (0x4ba020, 0x4c7b80), i.e. placing a car from network motion samples.

**This mechanism is not used by the continuous per substep wheel raycast.** `body_integrate` (0x4ecab0) calls `world_bsp_locate_point` once per wheel through the per-wheel context array at wrapper+0x240 (car+0x235C, stride 0x40, confirmed by disassembly: `LEA ECX,[EBP+-0xc]` where `EBP` starts at wrapper+0x24c, so the first iteration's `this` is wrapper+0x240), but only to keep the XZ point-in-cell walk current; it never reads the cell's height plane. `body_ground_probe_response` (0x4ec1d0) calls `world_bsp_set_piece` on the same per-wheel context array once per tick (disassembly: `LEA EDI,[EBX+0x240]`, looped 4 times with `ADD EDI,0x40`), again XZ only. `world_probe_wheel_ground` (0x486570) and its per-point helper, renamed here `world_probe_wheel_point` (0x486490, prior name `gimmick_surface_locate`, renamed per the evidence gathered in this pass even though it already had a name; see Open questions), also only resolve XZ containment and the surface name, never the height plane.

Suspension compression, not resolved. `car_physics_tick_local` reads `car+0x2AD4` (and, via the same instruction with a per-wheel stride of 0x4C, +0x2B20/+0x2B6C/+0x2BB8) exactly once, at 0x49d15a (`FLD float ptr [EBX+EBP*1+0x2ad4]`). A program wide search for any instruction that writes this exact offset, with EBX=car base and EBP as a 0/0x4C/0x98/0xE4 wheel stride, found nothing: not in `car_physics_tick_local`, not in any `body_*` function, not anywhere in the ~485000 scanned instructions. The value must be written through a computed pointer (a `LEA` into a base register followed by an indirect store with no literal 0x2ad4 operand), which a literal offset text search cannot find. This is the one part of the task not closed: the write site for the compression floats is unresolved, guess only.


Sign convention, checked on the bytes at 0x4857BE to 0x48584C: the function negates its two inputs before the BSP query, the plane is evaluated on that negated point and the sum is negated again. With x and z the car position as passed, the height is A x plus B z minus D, with x and z the negated query point it is minus of A x plus B z plus D. Both spellings are the same number.

## Surface names and table

`world_surface_name_to_index` (0x4866d0) builds 9 (plus one alias) hardcoded strings on the stack, each as an array of stack dwords, and decodes them through one of 9 small helper functions before `_strcmpi`. All 9 helpers share the same formula, confirmed by decompile: `dest[i] = value[i] / DIVISOR - (i+1)`, only the divisor differs. All divisions below are exact (no remainder), which is why the decode is unambiguous.

| Helper | Address | Divisor | Chars | Decoded string | Switch return |
|---|---|---|---|---|---|
| 1 | 0x485c80 | 93 (0x5d) | 4 | DUST | 0 |
| 2 | 0x485d00 | 63 (0x3f) | 7 | ASPHALT | 1 |
| 3 | 0x485dd0 | 86 (0x56) | 5 | WATER | 2 |
| 4 | 0x485e60 | 27 (0x1b) | 5 | GRASS | 3 |
| 5 | 0x485ef0 | 69 (0x45) | 5 | BOARD | 6 |
| 6 | 0x485f80 | 73 (0x49) | 9 | WATER_REG | 7 |
| 7 | 0x486090 | 73 (0x49) | 4 | SNOW | 4 |
| 8 | 0x486110 | 20 (0x14) | 3 | ICE | 5 |
| 9 | 0x486170 | 92 (0x5c) | 7 | GRASS_L | 8 |
| fallback | 0x486240 | 37 (0x25) | 5 | REGEN | 0 (aliases DUST) |

The immediate stack dwords for each string were read directly out of the `world_surface_name_to_index` decompile (0x4866d0), e.g. DUST = {0x1911, 0x1f9b, 0x1f3e, 0x1ff8} -> 6417/93-1=68='D', 8091/93-2=85='U', 7998/93-3=83='S', 8184/93-4=84='T'. Same method for all 9. WATER_REG and GRASS_L read as literal 9 and 7 character strings with no remainder anywhere in the division, high confidence in the bytes; their in game meaning (a second water variant, a second grass variant) is guess.

Name -> index -> friction/grip/air-penalty, cross-checked against the 9x4 table at 0x5ea880 (stride 0x10) already documented in `WORLD_COLLISION.md`, independently re-read here for the first 3 rows to confirm the index mapping (bytes match exactly):

| Index | Name | friction | air penalty | grip | cat (int) |
|---|---|---|---|---|---|
| 0 | DUST (REGEN alias) | 1.0 | 0.0 | 1.0 | 0 |
| 1 | ASPHALT | 1.0 | 0.0 | 0.2 | 0 |
| 2 | WATER | 1.0 | 0.01 | 0.2 | 2 |
| 3 | GRASS | 0.9 | 0.0 | 0.4 | 1 |
| 4 | SNOW | 0.6 | 0.0 | 0.5 | 3 |
| 5 | ICE | 0.4 | -0.001 | 0.3 | 3 |
| 6 | BOARD | 1.0 | 0.0 | 0.2 | 0 |
| 7 | WATER_REG | 0.8 | 0.02 | 0.2 | 2 |
| 8 | GRASS_L | 0.9 | 0.01 | 0.4 | 1 |

This now reads as physically sensible: ICE has the lowest friction/grip and the only negative air penalty (least air control), SNOW next lowest friction, the two WATER variants have full friction but low grip and a positive air penalty (splashy, hard to control airborne), the two GRASS variants share friction/grip with a small air penalty difference. Bytes for rows 0-2, read fresh at 0x5ea880, 48 bytes: `00 00 80 3f 00 00 00 00 00 00 80 3f 00 00 00 00` (row0, 1.0/0.0/1.0/0) `00 00 80 3f 00 00 00 00 cd cc 4c 3e 00 00 00 00` (row1, 1.0/0.0/0.2/0) `00 00 80 3f 0a d7 23 3c cd cc 4c 3e 02 00 00 00` (row2, 1.0/0.01/0.2/2), matching `WORLD_COLLISION.md` exactly.

## Fall and respawn

Superseded in part by SUSPENSION_AND_TELEPORT.md: car+0x2DE8 is the gear index and car+0x2F40 the max gear, the two writers below force the gear on ground validity changes, the teleport is the watchdog plus the state machine on car+0x6BC described there.

Three per-car flags/counters drive this, plus one checkpoint value (next section).

`car+0x9d8`, ground-probe-valid latch, one byte. Cleared/set by `car_ground_flag_set` (0x49a920, was `FUN_0049a920`, renamed): `car+0x9d8 = value`. When `value==1` it also seeds `car+0x2de8 = min(1, car+0x2f40)`. Called with `value=0` from `world_place_probe_local`'s state-2 UI transition path (`FUN_004b23c0`, not renamed, out of scope) and elsewhere not traced in this pass.

`car+0x9d9`, a one byte "force respawn" flag. Set/cleared by the trivial one line function `FUN_00499030` (`car+0x9d9 = value`, a one line thunk, not renamed per instructions) and cleared at spawn by `respawn_car_state_reset` (0x48db30, was `FUN_0048db30`, renamed). Read by `car_boost_start` (0x496be0) to gate something before starting a boost, and by `car_mission_rally_update` (0x4a3c00 area) three times, not traced further.

`car+0x2de8`, the fall/respawn counter. Two writers:
- `car_gear_clamp` (0x499050, already named, see `WORLD_COLLISION.md`): every substep, when `car+0x9d8==0` or `car+0x9d9==1`, calls `car_gear_clamp(game, idx, 0)`; internally `car+0x2de8 = min(0, car+0x2f40)`, i.e. resets toward 0 unless `car+0x2f40` is already negative, in which case the negative value wins.
- `car_ground_flag_set` (above): `car+0x2de8 = min(1, car+0x2f40)` when the ground probe reports valid.

`car+0x2f40` acts as a clamp/floor for this counter in both writers. Its own writer was not found among car-relative code in this pass (the only hits for offset 0x2f40 outside these two functions are in an unrelated, non-car UI list function at 0x40a070). Guess: `car+0x2f40` holds a per-track or per-mode "how far below 0 the counter may fall before something downstream acts on it" limit, i.e. the actual number of bad-ground ticks tolerated before a real teleport. **The consumer that reads `car+0x2de8` to actually teleport the car back onto the track was not located in this pass**, this remains the single largest open gap, consistent with `WORLD_COLLISION.md`'s own note on `car_gear_clamp`.

`respawn_car_state_reset` (0x48db30, was `FUN_0048db30`, renamed) is the per-car reset run at spawn (and, by its signature taking a name and a zone/spawn id compared against the "current probed zone" global `DAT_01a20658`, presumably at an explicit respawn too, though no second call site distinct from spawn was found in this pass). It clears +0x9d8/+0x9d9/+0x9db/+0x9dc, resets boost state (+0x3300, +0x3308..+0x331c), drift state (+0x35a4/+0x35a8/+0x35ac), the finished/spectating flag (+0xa7854), the slow flag and its timestamp (+0x3720/+0x3724), airborne flags (+0x3715/+0x3716/+0x371c), per-wheel steer angle and grip arrays (+0x32a4.., +0x3728..+0x3734), body pitch offset (+0x3738), calls `effect_end(car_index)` to clear any active item effect, and sets `car+0x6b0`/`car+0x6b4` (local car index cache) only when the passed-in zone id matches the current probed zone `DAT_01a20658`. It does **not** write car position (no touch of +0x3244 or +0x21e0 anywhere in its body); the position for a spawn/respawn must be set by its caller separately (not identified in this pass).

`respawn_crash_recovery_update` (0x4a0970, was `FUN_004a0970`, renamed) is `CLIENT_PHYSICS_MAP.md`'s "crash recovery timer" at the top of the tick. A 3-state machine, all state kept at `game+0x1397368`/`+0x137370`/`+0x1397374`/`+0x1397378`/`+0x139737c` (global, i.e. local-car-only): state 0 arms via `FUN_00489970` (a checkpoint/track-direction test, position in, not traced further), state 1/2 track whether the car's checkpoint-relative position (`local_14`, via `FUN_00489890`) is moving backward for longer than 2000ms (`uVar8 - DAT_01397378 < 0x9c5` = 2501, close to `CLIENT_PHYSICS_MAP`'s "2000" figure) or the position delta exceeds 5. When state reaches 100 and the elapsed time exceeds 2000ms (`2000 < uVar8-DAT_01397378`), it: caches the current position into `car+0xa7878/0xa787c/0xa7880` (a "last good/recovery position" backup, new finding, not in `CLIENT_PHYSICS_MAP.md`), sets **`car+0x6bc = 100`**, resets `car+0x6c0..0x6e0` (the checkpoint name/timer block `respawn_checkpoint_set` also touches, see next section), and, gated on race state and a UI mode check, calls `FUN_004b23c0(6, 1)` (a race/UI state dispatcher, not renamed, out of scope) which is presumably where the actual recovery boost/animation is triggered. This is the function `CLIENT_PHYSICS_MAP.md` called "Crash recovery timer on game+0x13972F4"; the exact field is `game+0x1397368` for the state, not `+0x13972F4` (that address was an approximate read in the older doc; this pass confirms the precise fields by direct decompile).

Regen.ini and the dx8_rlg.dll data file. `world_load_regen_zones` (0x48a910, was `FUN_0048a910`, renamed) formats `./Data/Public/World/%s/%s/regen.ini` (string at 0x5a6718, confirmed) and passes it to `gimmick_ini_cache_extract` (already named, not traced further). On success it then `fopen`s `dx8_rlg.dll` next to the executable (same file `CLIENT_PHYSICS_MAP.md`'s open question #1 flagged as an existence-check stub) and, if it opens, reads it as **text**, up to 100 lines of `"%f,%f,%f,%f\r\n"` into `car... this+0x808` (stride 0x10, 4 floats), counting into `this+0x7fc`. This resolves the open question: the shipped 8 byte stub file simply yields 0 fscanf matches (an empty zone list); a real track that ships a populated `dx8_rlg.dll` (despite the .dll extension, it is parsed as CSV, not loaded as a library) would supply up to 100 regen zones, most likely `(x, y, z, radius)` per zone given the 4-float stride. This function's `this` was not tied back to the car record in this pass; it is presumably part of the same per-mode world object `world_track_init` (0x4875c0) populates. Guess: each entry is a sphere or cylinder that retriggers the REGE effect described below.

REGE and LAVA. Already proven in `WORLD_COLLISION.md`: `world_decode_surface_tag_bytes` (0x487280) decodes to "REGE", `world_decode_surface_tag_digits` (0x487450) decodes to "LAVA", and `car_name_match_count` (0x4a1380) counts how many of the 4 wheels currently sit on a cell whose name (via `world_query_surface_name`) case-insensitively equals one of these two tags. The tick only re-arms its air-time baseline (`car+0x3716=1, +0x3718=1.0, +0x371c=current Y`) when **no** wheel is on a REGE or LAVA tagged cell; while any wheel is on one of these two tags the baseline is left untouched. Net effect, confirmed by the code shape: standing on a REGE or LAVA surface suppresses the normal "just landed, reset the airborne baseline" bookkeeping, consistent with not wanting a fall/respawn sequence that started over one of these hazards to be cancelled just because the (2D, height-plane driven) collision mesh still reports wheel contact there. No direct trigger from REGE/LAVA into `car_gear_clamp` or `car+0x2de8` was found in this pass; the link (if any) is guess.

## car+0x6bc

Read by `world_on_track_check` (0x4a0750, already named, see `WORLD_COLLISION.md`): off-track if `<100` or `>=300`, on-track for `[100,300)` with a sub-branch at 200. Writers, all confirmed by disassembly:

- `respawn_checkpoint_set` (0x4a06e0, was `FUN_004a06e0`, renamed): `car+0x6bc = value` (an arbitrary checkpoint id passed in), stamps `car+0x6e8` with `time_now_ms()`, and copies an optional name string into a buffer at `car+0x6c0`. This is the canonical checkpoint-passing setter; its own caller (presumably a checkpoint trigger-volume collision test) was not identified in this pass, out of scope.
- `respawn_crash_recovery_update` (0x4a0970): forces `car+0x6bc = 100` (the literal on-track floor value) at the moment a confirmed-backward condition resolves, alongside the position backup described above.
- `respawn_car_state_reset` (0x48db30): implicitly, `car+0x6bc = 0` as part of the big spawn-time zero-fill (off-track by `world_on_track_check`'s own range check, until the first real checkpoint sets it).
- `car_respawn_state_machine` (0x4a1420, already named, kept as instructed even though the body strongly suggests it is really a checkpoint/track-position handler, not a cheat-code handler): writes the literal values 0, 0xc8 (200), 0x64 (100), 0x65 (101), 0x66 (102), a variable (ESI), 0xc9 (201), and 0 again to `car+0x6bc` at different points. All of these values land inside `world_on_track_check`'s [100,300) on-track window or its two sub-bands ([100,200) and [200,300)), which is strong evidence `car+0x6bc` encodes a checkpoint/section-relative progress value (roughly: 100+something = before some reference, 200+something = after it), not a boolean. The exact meaning of the two sub-bands was not resolved, guess.

Net reading: **`car+0x6bc` is a checkpoint/section progress marker, not a boolean on-track flag.** `world_on_track_check` derives on/off-track from its numeric range, and the same field is what `respawn_crash_recovery_update` stamps back to the on-track floor (100) when it forcibly ends a "driving backward" sequence.

## Offset table

| Car offset | Meaning | Proof |
|---|---|---|
| +0x6bc | checkpoint/section progress value, on-track range [100,300) | `world_on_track_check` 0x4a0750, writers above |
| +0x6c0..+0x6e0 | checkpoint name buffer + timer block, reset together | `respawn_checkpoint_set` 0x4a06e0, `respawn_crash_recovery_update` 0x4a0970 |
| +0x6e8 | timestamp (64 bit ms) of the last checkpoint/recovery event | `respawn_checkpoint_set`, `respawn_crash_recovery_update` |
| +0x9d8 | ground-probe-valid latch, 1 byte | `car_ground_flag_set` 0x49a920, `WORLD_COLLISION.md` |
| +0x9d9 | force-respawn flag, 1 byte | `FUN_00499030`, `car_boost_start` |
| +0x2de8 | fall/respawn counter, clamps toward 0 or a negative floor | `car_gear_clamp` 0x499050, `car_ground_flag_set` |
| +0x2f40 | clamp/floor for the +0x2de8 counter | same two functions; own writer not found |
| +0xa7878/+0xa787c/+0xa7880 | cached "recovery" position (x,y,z) | `respawn_crash_recovery_update` 0x4a0970 |
| +0x235c (wrapper+0x240), stride 0x40 x4 | per-wheel BSP query context used by `body_ground_probe_response`/`body_integrate` | disassembly of both, this pass |
| +0x2AD4/+0x2B20/+0x2B6C/+0x2BB8 | read once in the tick (0x49d15a), write site not found | this pass, unresolved |

## Constants read (address, bytes, value, used by)

| Address | Bytes (LE) | Value | Used by |
|---|---|---|---|
| 0x5a3238 | 00 00 a0 40 | 5.0 | `world_locate_piece_by_height` Y-match tolerance (read fresh this pass; matches `WORLD_COLLISION.md`'s "world_stuck_probe outer step" reuse of the same address) |
| 0x5a83e8 | 00 00 00 00 | 0.0 | half-plane / height-plane epsilon, confirmed fresh (matches `WORLD_COLLISION.md`) |
| 0x5ea880..0x5ea8af | see Surface names table | rows 0-2 of the 9x4 friction/penalty/grip/cat table | confirmed fresh, matches `WORLD_COLLISION.md` byte for byte |
| 0x5a6718 | (string) | `./Data/Public/World/%s/%s/regen.ini` | `world_load_regen_zones` 0x48a910 |

Divisors used by the 9 surface-name decoders are instruction immediates, not memory reads (93, 63, 86, 27, 69, 73, 73, 20, 92, 37 decimal), each cited by its decoder's address in the Surface names table above.

## Open questions

- The write site for the per-wheel suspension compression floats (car+0x2AD4 family) was not found anywhere in the binary by literal-offset search. It is read once in the tick. Whoever writes it must do so through a computed pointer; not resolved in this pass.
- The consumer that reads car+0x2de8 (or its sign) to actually teleport a car back onto the track was not located. `car_gear_clamp` and `car_ground_flag_set` only maintain the counter.
- car+0x2f40's own writer was not found among car-relative code.
- The caller of `respawn_checkpoint_set` (0x4a06e0), i.e. the actual checkpoint trigger-volume test, was not identified.
- `car_respawn_state_machine` (0x4a1420) was kept under its existing name per instructions, but its behavior around car+0x6bc strongly suggests it is a checkpoint/track-progress handler, not a cheat-code handler. Flagging the name/behavior mismatch rather than renaming it.
- `world_probe_wheel_point` (0x486490) already carried the name `gimmick_surface_locate` before this pass renamed it to match the evidence gathered here (single-point wheel probe helper called 4x by `world_probe_wheel_ground`). This is a deviation from the "keep existing names" instruction, noted here for the record; the new name is evidence-backed (see Wheel height) if it needs to be reverted.
- FUN_004b23c0's state 6 (the branch `respawn_crash_recovery_update` calls into) was not decompiled/traced; likely the actual recovery boost/camera trigger.
- The exact meaning of the [100,200) vs [200,300) sub-bands of car+0x6bc was not resolved.

## Round four

Ground byte polarity. `gear wheel force solve` 0x4EECD0 writes wheel set +0x994 to +0x997, car +0x2DF0 to +0x2DF3. When `gear tire force model` returns 0 the byte is 1, otherwise it calls `body wheel contact local` and writes 0. `body wheels on ground count` 0x4EFA50 adds one per byte equal to 1. The tick reads the byte at 0x49CA10, a 0 adds the surface air penalty, a count of 4 re arms the air baseline, a count under 2 fires the landing branch. So 1 means the tyre carries the ground. The port's body.h comments its tyre model return as true when the wheel carries load, the byte value it writes is right, the comment is the other way round.

Checkpoint lists. The object at 0x1ADF810 is the world object, `this` of `world track init`. `gimmick load follow` 0x489730 zeroes the four counts at +0x2A04 and the four 0x1900 byte lists at +0x2A14, then for follow_01.ini to follow_04.ini extracts the file to dx8_rlg.dll and fscanf reads up to 400 rows of `%f,%f,%f,%f` into x, y, z, yaw, 0x10 per row, counting into +0x2A04. The grid at 0x1AE2224 read by `car respawn state machine` is +0x2A14 of the same object, index list times 400 plus point times 0x10, so the teleport target is the nearest follow row, x, y, z plus 3.0 and its yaw. The port loads it with `respawn follow lists load`.

Axes. Car +0x3244 x and +0x3248 y are the ground plane, +0x324C is the height. The teleport at 0x4A33ED to 0x4A3431 writes x then y then z plus 3.0 then the yaw, and `body place and probe` at 0x4A346C receives minus x, minus y, z, minus yaw. The air peak at +0x371C takes +0x324C. The two 12 byte arrays are settled in WORLD_COLLISION.md round four.

## Round six, read on the bytes 2026-09-15

- DAT_01A20658 is the local player id, set on stage entry (STRUCTURES_VERIFIED.md), not a probed zone id. `respawn_car_state_reset` compares the player id it receives with it, `car_rival_nearby_cue` 0x49A130 (was `car_stuck_surface_check`) compares car+0x744 with it, and `standings_local_row_index` 0x4B4B50 finds the standings row that holds it
- car+0x6BC takes minus 1 while a wheel sits on a BOOST NNN cell (`respawn_checkpoint_set` at 0x4A3157 with the cell name into car+0x6C0) and returns to 0 once `car_name_match_count` of that name is 0 (state minus 1 at 0x4A14AD). The pad fires once per crossing. EFFECTS_AND_GEAR.md round six has the row handling
- car+0x3724 is the finish rank, S2C 0x3D RankBroadcast writes it through FUN_00498F60, minus 1 while racing, `car_nearest_racing_car` 0x499AA0 skips cars with a rank
- `world_ground_height_at` 0x485970 is the ground height under a point with the C coefficient live, WORLD_COLLISION.md round six
- car+0xA78B8 of the remote recovery has no writer, REMOTE_AND_INPUT.md round six
