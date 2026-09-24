# World collision

Read in KnC.exe.raw, image base 0x400000, on 2026-09-14. Companion to `docs/reverse/CLIENT_PHYSICS_MAP.md`. That doc maps the tick, this one maps how the car meets the map: the collision file, the BSP that locates a point in it, surface lookup, friction and grip, falling and respawn. Names in brackets are the ones now set in Ghidra, the raw address is the truth. Every function below was renamed in Ghidra with the `world_` prefix, snake_case, except two one-line thunks noted inline (not renamed, per instructions).

The scope list in the task used working labels for three functions that the decompile does not support once you read the bytes: 0x487280/0x487450 are not "two rays", and 0x498800 is not "fall off the track". Both corrections are documented below with the evidence, not asserted from the label.

## The collision file, .col

Strings read at 0x5a6148 and 0x5a6170:

- `./Data/Public/World/%s/%s/track.col` (base piece)
- `./Data/Public/World/%s/%s/track%d.col` (extra pieces, %d = 1..8)

`world_track_init` (0x4875C0, was `FUN_004875C0`) is the track/room initialise entry point, `__thiscall(this, param_2)`. `this` is a per-mode object; the disassembly at the call site inside it (0x48772e, `MOV ECX,0x1adf5e8; CALL world_load_track_pieces`) shows the world/track object that owns the pieces lives at the fixed address 0x1adf5e8. Sequence, all guarded, returns 0 on any failure ("Track initialize fail" is the caller's message, not printed by this function itself):

1. `sprintf("World/%s/%s/Texture/Low"... )` and `sprintf("World/%s/%s/track")`/`"geometry"`, loaded through `FUN_00445250`/`FUN_004450d0` (generic scene resource loader, not decompiled here, out of scope, this is the visual mesh path, separate from collision).
2. `"./Data/Public/World/light"` through `FUN_00443c10`.
3. `world_load_track_pieces(this=0x1adf5e8, folder, name)` at 0x487733, this is the collision loader, see below. Track init aborts (return 0) if it fails.
4. Terrain/object setup calls (`FUN_0048a800` and the 0x48b1xx/0x48axxx/0x48adxx/0x48afxx/0x48b0xx/0x48b2xx block), not decompiled, not collision.
5. Camera, sky (day/night by `DAT_00b2319c`), fog distance (`world_track_init` sets `param_1+0x1b4` from `FUN_00486bf0`, then `FUN_0058b370` with near/far constants 30/500 or computed). None of this touches the .col data.
6. On success sets `this+0x13338=0`, `this+0x13358=1` and returns 1.

Fields read at 0x487626-0x487653: `this+0x1334c` is a "license" record pointer; its +0x30/+0x34/+0x38 are copied into globals `_DAT_005eb6f0`, `_DAT_005eb6f4`, `_DAT_005eb6f8`, and `_DAT_005eb6fc` is hardcoded to 0x43a00000 = 320.0. `_DAT_005eb6f4` is later used in the tick as an engine-force scale (`local_7ac * _DAT_005eb6f4 * _DAT_005a6adc`, CLIENT_PHYSICS_MAP step 12/13 area), a per-track tuning value, not a collision constant. The license record's own layout was not read (guess: physics/engine tuning block per license/track).

### world_load_track_pieces, 0x485580 (was FUN_00485580)

`void __thiscall(this, param_2 folder, param_3 name)`. `this` is the fixed track/world object at 0x1adf5e8 (per the one call site found, in `world_track_init`).

- `this+4`: inline array of up to 9 piece records, 0x38 (56) bytes each. Piece 0 is loaded from `track.col` (`ECX = this+4` at 0x4855c8), pieces 1..8 from `track1.col`..`track8.col` (`ECX = this + count*0x38 + 0x3c`, at 0x4855f9, where `count = this+0x1fc` before the increment, so piece N lives at `this + 4 + N*0x38`).
- `this+0x1fc` (int): number of pieces loaded beyond the base one (0 if only track.col exists), read/written at 0x4855b4 and 0x485606-0x485610.
- `this+0x200` (bool): set to 1 only if at least one `trackN.col` loaded; the base `track.col` alone leaves it 0.
- Loop stops at 8 extra files (`CMP EDI,0x8` at 0x48560d) or on first `world_load_col_piece` failure.

Each call is `world_load_col_piece(piece_this, filename)`.

Cross-reference note (guess, not fully closed): `world_probe_wheel_ground` (0x486570) and its piece-transition path index a *different* base, `&DAT_01adf624`, stride 0x38, bound `DAT_01adf7e4`. `0x1adf624 = 0x1adf5e8 + 0x1fc + 4`... no, recompute: piece array base per `world_load_track_pieces` is `0x1adf5e8+4 = 0x1adf5ec`; `DAT_01adf624 - 0x1adf5ec = 0x38`, exactly one 56-byte slot. So the array `world_probe_wheel_ground` walks by "`#N`" name starts one slot after the array `world_load_track_pieces` fills starting from. Also `this+0x1fc = 0x1adf5e8+0x1fc = 0x1adf7e4`, which is exactly the bound `DAT_01adf7e4` read in `world_probe_wheel_ground`, so the piece *count* field is confirmed shared between the two. Whether piece slot 0 (plain `track.col`) is simply never addressed by the `#N` name path, or the name-index is off by one from the physical slot index, was not resolved by reading further code. Marked as guess.

### world_load_col_piece, 0x4ece90 (was FUN_004ece90)

`bool __thiscall(this, filename)`. Opens the file (`FUN_0044d560`), and reads, in this exact order, 4 header int32 counts directly into scattered struct fields, then allocates and freads each array:

| Struct offset | Field | Source in file |
|---|---|---|
| +0x08 | count A (int32) | 3rd header int read |
| +0x0c | pointer, array A, stride 0x14 (20 bytes) | allocated `count_A * 0x14`, fread `count_A` records of 20 bytes |
| +0x10 | pointer, array B, stride 0x38 (56 bytes) | allocated `count_B * 0x38`, fread `count_B` records of 56 bytes |
| +0x14 | count B (int32) | 2nd header int read |
| +0x18 | pointer, array C, stride 0x0c (12 bytes) | allocated `count_C * 0xc`, fread `count_C` records of 12 bytes |
| +0x1c | count C (int32, reused as count B's twin) | 1st header int read, labelled "count B" above by where it is consumed; the header order is: read1→+0x14, read2→+0x1c, read3→+0x8, read4→+0x2c |
| +0x2c | count D (int32) | 4th header int read |
| +0x34 | pointer, array D, stride 0x0c (12 bytes) | allocated `count_D * 0xc`, fread `count_D` records of 12 bytes |

Re-stated cleanly by read order (this is what a 1:1 loader must replicate): the file is header-first, 4 little-endian int32 counts, then 4 variable-length arrays back to back in this order:

1. int32 `n1` → struct+0x14
2. int32 `n2` → struct+0x1c
3. int32 `n3` → struct+0x8
4. int32 `n4` → struct+0x2c
5. `n1` records of 12 bytes → struct+0x18 (array C above)
6. `n2` records of 56 bytes → struct+0x10 (this is the BSP node / cell array, see below)
7. `n3` records of 20 bytes → struct+0xc (this is the shared edge/plane array, see below)
8. `n4` records of 12 bytes → struct+0x34

The 12-byte records (arrays at +0x18 and +0x34) were not decoded field-by-field; 12 bytes is consistent with either 3 floats (a vertex XYZ) or 3 int32 (a triangle's 3 vertex indices), guess, not read further. The 20-byte array (+0xc) and the 56-byte array (+0x10) are the ones actually walked by the point-location code, decoded below with evidence.

Piece struct size is exactly 0x38 (56) bytes end to end (fields span +0x08 to +0x37), matching the stride `world_load_track_pieces` uses for its inline piece array.

## The BSP: locating a point, and what a "triangle" is

Two arrays inside a piece do the work: the **edge array** (piece+0xc, stride 0x14/20 bytes, fields read at record+4, +8, +0xc as floats A, B, C of a 2D half-plane equation `A*x + B*z + C`) and the **cell array** (piece+0x10, stride 0x38/56 bytes, the same array a piece loads `n2` of). A 16-bit index into the edge array carries its sign in the top bit (`& 0x7fff` for the index, `(short)v < 0` selects the negated form of the plane test), so edges are shared and reused by multiple cells with either orientation, the classic BSP/edge-shared-triangle-soup trick.

### world_bsp_locate_point, 0x4ebfc0 (was FUN_004ebfc0)

`bool __thiscall(query_ctx, x, z, out_edge_ptr, out_t)`. Walks the tree from the **cell array root** (`piece+0x10` field read as `*(int*)(piece+0x10)`, cursor stored at `query_ctx+0xc`) down using 16-bit child links read from the current cell record (`*puVar1` or `puVar1[1]` depending on which side `world_bsp_edge_sweep`/`FUN_004ebdd0` selects, actually here it is `FUN_004ebdd0` doing the classification per step). A child value of `0xffff` means "no piece here" and yields success with the raw (unclamped) point (`return 1`, first branch). Otherwise the index selects a new cursor (`*(uint*)(ctx+0xc) = child*0x38 + piece_cell_array_base`) and the walk repeats, capped at 0x400 (1024) steps (safety break, returns 0 = not found / too deep). On exit it also writes an interpolated point along the last edge crossing into `query_ctx+4/+8` and the edge record pointer into `query_ctx+0xc`.

`query_ctx` here is not the piece, it's a small caller-owned struct (car+0x3608, see `world_probe_wheel_ground` below, or a stack scratch struct for other callers): +4/+8 = current query x/z, +0xc = piece pointer on entry / found-cell or found-edge pointer on exit, +0x10/+0x14/+0x18 = previous saved x/z (copied from +4/+8/+0xc at function entry, `param_1+0x10=param_1+4` etc, restored as the "from" point for the interpolation).

### world_bsp_edge_sweep, 0x4ebdd0 (was FUN_004ebdd0)

`int __thiscall(ctx, x, z, out_t)`. Tests up to 3 edges read from the *cell* record at `ctx+0xc` (offsets +0x10, +0x12, +0x14 inside that 56-byte cell = three 16-bit edge indices, this is the cell's own 3 boundary edges, i.e. a triangle) against the plane equation from the *edge* array at `piece+0xc` (`*(int*)(ctx+0x3c)+0xc`, same array `world_bsp_locate_point` walks). For each edge: if the new point (x,z) is behind the edge (`< _DAT_005a83e8`, read as exactly 0.0 at 0x5a83e8) but the old point (`ctx+4/+8`) is in front, it computes a crossing fraction `t = front/(front-back)` and keeps the edge with the smallest `t < DAT_0059f480` (read as exactly 1.0 at 0x59f480, i.e. the crossing must lie within the segment). Returns the winning edge's signed 16-bit index, or the running `uVar5` (initialised 0xffff = "no crossing"), this is a segment/sweep test from the previous query point to the new one, used to walk out of the current triangle when the point moved. Confirms the piece geometry is triangle-based (3 edges per cell) with plane equations shared through the edge array.

### world_bsp_set_piece, 0x4ebcf0 (was FUN_004ebcf0)

`bool __thiscall(ctx, piece, x, z)`. Sets `ctx+0x3c = piece`. If `piece != 0` and `piece+0x1c` (cell count, `n2` above) is nonzero, scans the cell array (`piece+0x10 + 0x10`, i.e. skips a 16-byte cell header, then 3 edge-index shorts per cell, cell stride 0x1c shorts = 0x38 bytes matching the 56-byte cell record) testing up to 3 edges per cell against the (x,z) point using the same half-plane test as `world_bsp_edge_sweep` (`< _DAT_005a83e8` = 0.0 fails, all 3 must pass). First cell where all 3 edges pass is the containing triangle: `ctx+0xc = cell_ptr`, `ctx+4/+8 = x,z`, returns 1. Returns 0 if `piece==0`, no cells, or none contain the point. This is the plain linear point-in-triangle-soup scan (used as a fallback / initial locate, where `world_bsp_locate_point` is the accelerated tree descent).

### world_query_surface_name, 0x4ec0b0 (was FUN_004ec0b0), and its thunk

`char* __fastcall(query_ctx)`. If `ctx+0xc` (the found cell, set by `world_bsp_set_piece` or `world_bsp_locate_point`) is null, returns null. Otherwise reads a null-terminated ASCII string starting at **cell_ptr + 0x16** (22 bytes into the 56-byte cell record), upper-cases it (`_strupr`) and copies it into `ctx+0x1c`, returns that pointer. **This is the surface material name, stored directly inside the collision cell record**, confirms each triangle/cell embeds its own surface tag as text, not an index into an external table, at offset +0x16 of the 56-byte cell.

`FUN_004ec7e0(wheel_index)` is a one-line thunk to this function with no argument passed through, not renamed (thunk). `FUN_004ec100(x, z)` is a one-line thunk to `world_bsp_locate_point` (also not renamed) that sets the query point.

### body_ground_probe_response, 0x4ec1d0 (was FUN_004ec1d0)

`void __thiscall(query_group, piece)`. Sets `query_group+0xd44 = piece`; if `piece != 0`, calls `world_bsp_set_piece(piece, x, z)` for 4 sub-points read from `query_group+4` stride 0xc (3 floats each, only x/z used). This is the "collision response" step CLIENT_PHYSICS_MAP names at the top of the tick, run once the 4-corner ground probe (`world_probe_wheel_ground`) says the car moved onto a new piece, it re-locates all 4 cached corner points against the new piece.

### world_wheel_query_surface, 0x4a0680 (was FUN_004a0680)

`void __thiscall(game, car_index, wheel_index)`. Computes `car = car_index*0xA7260 + wheel_index*0xC + game`, sets the active query point to `(-car+0x3274, -car+0x3278)` (world_bsp_locate_point via its thunk), then fetches the surface name for `wheel_index` via the `world_query_surface_name` thunk. The x/z negation matches CLIENT_PHYSICS_MAP's "x and y are negated when copied" convention for this car's coordinate system. The source field `car+wheel_index*0xC+0x3274` overlaps the body axis block CLIENT_PHYSICS_MAP lists as "+0x3274 to +0x329C forward, right and up axes", for wheel_index 0..3 this reads 4 different 3-float slots across that range and one slot past it (0x3298..0x32A4, which butts against "+32A4 per-wheel steer angle"). Whether this is really 4 wheel positions co-located with the 3 body axes, or CLIENT_PHYSICS_MAP's axis range needs widening, was not resolved here, guess, flagged for the other doc.

### world_surface_name_to_index, 0x4866d0 (was FUN_004866d0)

`int(char* name)`. Tries 9 hardcoded (XOR/offset-obfuscated, decoded through small helper functions `FUN_00485c80`..`FUN_00486240`, not traced further) name strings against `name` case-insensitively, returning a small enum: 0, 1, 2, 3, then a gap (skips 4 briefly), 6, 7, 4, 5, 8, or -1 if none match. Returns -1 also if `name` is null. This is the mapping from a cell's embedded material text to the 0-8 index used by `world_surface_friction`/`world_surface_air_penalty`/`world_surface_grip`. The 9 decoded name strings were not read out (they're built through obfuscated-byte helper calls); guess only that they correspond to a small fixed surface enum (road/grass/sand/etc).

## Per-wheel ground probe

### world_probe_wheel_ground, 0x486570 (was FUN_00486570)

`bool __thiscall(query_ctx, x, z, unused, car_index, wheel_index)`. Called 4 times per tick (CLIENT_PHYSICS_MAP step 1), once per wheel: `this = car+0x3608` (a ~0xA0-byte per-car query context, sits right before `+0x36A8` active-effect-code from CLIENT_PHYSICS_MAP), and `x,z` = the 2 of the 3 floats at `car + wheel_index*0xC + 0x3274` (confirmed by disassembly of the call site at 0x49c135-0x49c170; `param_5=car_index` from the spilled slot at `[ESP+0x7c0]`, `param_6=wheel_index` = the tick's own 0..3 loop counter).

Body: bails immediately (`return false`) if `this+0x9c == 0` (context not active/track not loaded). Runs an inner 4-iteration loop calling `world_bsp_locate_point` (via `FUN_004ebfc0`, negating x/z again) against a *second* per-car global array at `&DAT_01b1c304 + car_index*0xA7260`, this is the running per-corner footprint check: 4 points, all must land inside a piece (`local_8 == 4`) to continue, else return false. If all 4 land, it fetches the current piece's *name* via `world_wheel_query_surface(car_index, wheel_index)` (`world_query_surface_name`'s output), looks for a `#` in it and `sscanf`s the trailing number as the intended piece index; if that differs from the cached piece index at `this+0x98`, it clears the "current piece valid" flag at `this+0x94`. When the flag is clear and the parsed index is in range (`0 <= idx < DAT_01adf7e4`), it repoints `this+4` at `&DAT_01adf624 + idx*0x38` and calls `world_bsp_set_piece` on it; failure there returns false, success sets `this+0x98=idx`, `this+0x94=1`. Returns true only when the piece pointer at `this+4` actually changed from what it was on entry, i.e. **true means "the car crossed onto a different piece this tick"**, which is exactly when the caller (`body_ground_probe_response`, per CLIENT_PHYSICS_MAP step 1) needs to re-locate the cached corner points against the new piece.

## Surface properties: friction, grip, air penalty

A 9-entry table at 0x5ea880, stride 0x10 (16 bytes) = 4 floats per surface index (0-8, bound-checked `-1 < i < 9` in all three readers below; out of range falls back to a default). Read at 0x5ea880, 144 bytes:

| idx | friction (+0) | air penalty (+4) | grip (+8) | field +0xC (int) |
|---|---|---|---|---|
| 0 | 1.0 | 0.0 | 1.0 | 0 |
| 1 | 1.0 | 0.0 | 0.2 | 0 |
| 2 | 1.0 | 0.01 | 0.2 | 2 |
| 3 | 0.9 | 0.0 | 0.4 | 1 |
| 4 | 0.6 | 0.0 | 0.5 | 3 |
| 5 | 0.4 | -0.001 | 0.3 | 3 |
| 6 | 1.0 | 0.0 | 0.2 | 0 |
| 7 | 0.8 | 0.02 | 0.2 | 2 |
| 8 | 0.9 | 0.01 | 0.4 | 1 |

Field +0xC reads as small integers (0,0,2,1,3,3,0,2,1) when interpreted as int32, not a meaningful float, guess: a secondary category id (sound/particle group), not used by any function in this scope.

### world_surface_friction, 0x486d40 (was FUN_00486d40)

`float(int surface_index)`. In range: `*(float*)(0x5ea880 + index*0x10)`. Out of range: falls back to `_DAT_0059f480` = 1.0 (read at 0x59f480).

### world_surface_air_penalty, 0x486d70 (was FUN_00486d70)

`float(int surface_index)`. In range: `*(float*)(0x5ea884 + index*0x10)` (field +4 of the same table). Out of range: falls back to `DAT_0059f44c` = 0.0 (read at 0x59f44c). Called from the tick once per wheel for every wheel whose ground byte is 0, and the byte 0 means loaded (round five, 0x4EF068, byte 1 is the wheel in the air), accumulating a per tick multiplier that starts at 1.0 and is reduced by `(value + car+0xa798c bonus) * 0.25` (`_DAT_005a32d4`, read at 0x5a32d4) for each loaded wheel. So it is a contact drag charged per wheel on the ground, water 0.01, ice minus 0.001, asphalt 0, the port names it `world_surface_contact_drag`. The earlier reading "charged while the wheel is off the ground" had the byte polarity inverted.

### world_surface_grip, 0x486da0 (was FUN_00486da0)

`float(int surface_index)`. In range: `*(float*)(0x5ea888 + index*0x10)` (field +8). Out of range: falls back to `DAT_0059f44c` = 0.0. Called once per wheel (all 4, not gated on ground/air) to compute `car+0x3728+wheel*4` (the per-wheel grip array CLIENT_PHYSICS_MAP already names) as `(stat14_base + stat14_bonus) * (surface_grip + car+0xa7990 bonus) * 0.2` (`_DAT_005a15ec`, read at 0x5a15ec = 0.2).

### world_wheel_surface_index, 0x4a1310 (was FUN_004a1310)

`int __thiscall(game, car_index, wheel_index)`. Returns -1 immediately if `(&DAT_01397384)[game] == 0` (collision/track not active for this game slot). Otherwise `world_surface_name_to_index(world_wheel_query_surface(car_index, wheel_index))`, the full path from "where is this wheel" to "which of the 9 surface types is it on". Called once per wheel at the top of the friction/grip section (CLIENT_PHYSICS_MAP step 9), result cached per wheel and fed to the friction/grip/penalty readers above.

## The 30-entry zone multiplier table

### world_zone_table_index, 0x4b4b50 (was FUN_004b4b50)

`int __fastcall(this=0x2ebd6a0 fixed)`. Linear-searches a **global**, fixed-address, 16-slot cache at `this+0xdc0`, stride 0xc (3 int32s), comparing the first int32 of each slot to the global `DAT_01a20658` ("current probed id", not traced to its writer, guess: set by whatever last resolved a special zone under the car). Returns the matching slot's index (always 0-15, the internal clamp-to-[0,15]-or-(-1) check is dead code since the loop bound already guarantees it) or -1 if not found in 16 slots.

Caller (tick, 0x49ca40-0x49ca5e): if the result is `0 <= idx < 30` (0x1e), multiplies the accumulated off-ground grip value (`local_7a4`, the same accumulator `world_surface_air_penalty` fed) by `*(float*)(0x5eb718 + idx*4)`. The bound is 30 even though this particular lookup can only ever return 0-15, the other half of the table (indices 16-29) is reachable only from some other, not-in-scope caller.

Read at 0x5eb718, 30 floats (static/default file values, i.e. before any track-specific override, a real track may patch these at runtime, not observed here):

```
idx 0-6:  0.99934, 0.99941, 0.99947, 0.99954, 0.99960, 0.99967, 0.99983
idx 7-15: 1.0 (x9)
idx 16-29: 0.0 (x14)
```

All at or near 1.0 for the populated half, i.e. by default this multiplier is a no-op; whatever mechanism sets `DAT_01a20658` and populates the 16-slot cache at 0x2ebd6a0+0xdc0 at runtime (per-track special zones, most likely) was not located in this pass.

## Out of bounds, stuck, and the car-vs-car push (correcting the working label)

### world_stuck_probe, 0x498590 (was FUN_00498590)

`bool(x, z, y_or_related, unused, y_center)`. Pure spatial search, no timer of its own (the "100 ms window" the task mentions lives in the *caller*, see below). Two nested loops: outer covers `y_center - 90.0` (`_DAT_005a323c`) stepping by 5.0 (`_DAT_005a3238`) up to +50.0 (`DAT_0059f450`, i.e. `local_14` runs -50..+50 in 21 steps of 5); inner covers 0..2.0 (`_DAT_005a24ec`) stepping 0.2 (`_DAT_005a15ec`). For each grid point it calls `world_ground_test_point`; if that returns false (no valid ground found there) it returns **true** immediately (problem found). If ground is found, it also checks the surface name there (`world_last_surface_name`) against `"PUSH"` case-insensitively; a match also returns true immediately. Only after exhausting the whole grid without either condition does it return false ("surroundings are fine"). So **true = a hole or a "PUSH" surface was found nearby; false = no problem**, the caller treats a true return as the start/continuation of a stuck condition, not the other way round.

Caller (tick, 0x49d2c2-0x49d30c): on `world_stuck_probe` returning true, a 3-state timer at `game+0x1397178` (state)/`+0x1397180` (start timestamp)/`+0x1397184` starts; once the elapsed time exceeds 100 ms (literal `100` compared against the millisecond timer difference, 0x49d... region) or wraps to a later state, it sets the stuck flag at `game+0x1397174` and advances the state to 2. This 100 ms figure belongs to the tick, not to `world_stuck_probe` itself.

### world_ground_test_point, 0x4858e0 (was FUN_004858e0)

`bool __thiscall(ctx, x, z)`. Returns 0 immediately if `ctx+0x9c == 0`. Negates x/z, calls `world_bsp_locate_point` twice (both results discarded/reused as the same call, looks redundant in the decompile, not investigated further), and if that reports "no piece" (`cVar1 == 0`), falls back to `world_bsp_set_piece(ctx+4, x, z)`, i.e. tries the fast tree descent first, then the plain linear scan against the currently cached piece. Returns whether either found a containing cell.

### world_last_surface_name, 0x485a80 (was FUN_00485a80)

`char* __fastcall(ctx)`. Returns 0 if `ctx+0x9c == 0`, else forwards to `world_query_surface_name` (the same "read the cell's embedded name and uppercase it" function used everywhere else).

### world_on_track_check, 0x4a0750 (was FUN_004a0750)

`__fastcall(car)`. Reads `car+0x6bc` (an int, likely a race/lap-position or track-distance-along value, not otherwise named in CLIENT_PHYSICS_MAP). Returns a packed byte (low byte 1 = "off track", 0 = "on track", bits 8-31 = the value shifted right 8, apparently just passed through/unused by callers) as: off-track (1) if the value is `< 100` or `> 299`; on-track (0) if it's in `[100,200)`; and a third branch for `[200,300)` returning the shifted value with low byte 0 (on-track). In effect: on-track is `100 <= v < 300`, off-track is `v < 100 or v >= 300`. Caller (tick, CLIENT_PHYSICS_MAP step 6): when this returns off-track, it zeroes the car's accel/left/right input flags and forces brake=1 (`car+0x1c=1, +0x18=0, +0x2c=0`), matches "clears the inputs" from the task. The exact meaning of `car+0x6bc`'s numeric range (a track-progress/segment-position value, not a boolean) is inferred from the range check shape, not read at its writer, guess.

### world_car_push_apart, 0x498800 (was FUN_00498800), corrected from "fall off the track"

`bool __thiscall(game, car_index)`. The decompiled body is unambiguous and does not touch fall/void/respawn state at all:

1. `FUN_00497f80(car_index)` finds another car whose XZ bounding range overlaps this one (scans up to 30 car slots, skipping self and finished/spectating cars, calls `FUN_00497ee0` to confirm real overlap). Returns -1 if none found, and `world_car_push_apart` returns false immediately in that case.
2. If found, computes the separation vector between the two cars' positions (`+0x3244/+3248/+324c`), normalizes it (`thunk_FUN_00502f99`), eases it (`FUN_004ed600` with 0.995ish, `FUN_004ed3f0`), and nudges **both** cars apart along it by 25% (`_DAT_005a6a44`), writing into the other car's `+0x3244/+3248`.
3. Compares a speed value (`+0x3234`, CLIENT_PHYSICS_MAP's "speed") against 30.0 (`_DAT_005a3244`) and 15.0 (`_DAT_005a3230`): above 30 triggers `FUN_004981b0(other_or_self_index, 0)`, above 15 (but not 30) triggers `FUN_004981b0(other_or_self_index, 1)`. `FUN_004981b0` itself (not renamed, out of scope) triggers a one-shot visual/audio effect at a position blended 25% toward a fixed reference point (`DAT_005c83a8/ac/b0`), guarded by "not already playing", a collision impact effect, not a fall/respawn action.

There is no fall, void, or track-height check anywhere in this function. Called unconditionally once per substep in the tick (CLIENT_PHYSICS_MAP step 13, guarded only by `DAT_00b2360c != 0xf && != 0x11`, a race-state check unrelated to collision). The "fall off the track" working label does not hold; this is car-vs-car overlap separation plus a speed-gated impact effect. If a genuine void/fall/height check exists in the client, it was not found at this address in this pass.

### car_gear_clamp, 0x499050 (was FUN_00499050), narrower than "respawn"

`void __thiscall(game, car_index, value)`. `if (value < -1) { car+0x2de8 = -1; return; } clamp = min(value, car+0x2f40); car+0x2de8 = clamp;`. Called from the tick (CLIENT_PHYSICS_MAP step 13/14 boundary) as `car_gear_clamp(game, car_index, 0)`, a constant 0, every substep, whenever `car+0x9d8 == 0` (no valid piece under the car, this is the flag `world_probe_wheel_ground`/`world_bsp_set_piece` leave when a probe fails) or `car+0x9d9 == 1` (an explicit force flag). With `value` always 0 at this call site, the effect is: `car+0x2de8` is reset to 0 each such substep unless `car+0x2f40` already holds a negative value, in which case that negative value wins. This reads as a **per-substep "currently over valid ground" counter/latch**, not a teleport-to-checkpoint action, no position write, no checkpoint lookup happens inside this function. The actual respawn teleport (if the client performs one) is driven by something downstream of `car+0x2de8`/`+0x9d9`, not found in this pass, guess.

## The two "airborne test" tag functions, corrected from "two rays"

These do not cast rays. They decode a 4-character surface tag from two different in-place-constructed buffers and check it against the current wheel surfaces via `car_name_match_count`, gating whether the tick primes its air-time tracking (CLIENT_PHYSICS_MAP's `+0x3716/+0x3718/+0x371c`).

### world_decode_surface_tag_bytes, 0x487280 (was FUN_00487280)

`void __thiscall(dest[5], src)`. `dest[i] = src[i*4] - (i+1)` for i=0..3, `dest[4]=0`. Reads the low byte of 4 consecutive 32-bit stack slots. At the one call site found (tick, ~0x49c5fe), the source bytes are the literal constants 0x53,0x47,0x4a,0x49 ('S','G','J','I'), which decode to **"REGE"** (`0x53-1='R', 0x47-2='E', 0x4a-3='G', 0x49-4='E'`). Given the sibling call decodes to "LAVA" (below) and the string table has a `.../regen.ini` file (world zone init file, 0x5a6718), "REGE" reads as a truncated "REGEN" tag, guess, plausible given the ini file exists, not confirmed by reading a regen-zone consumer.

### world_decode_surface_tag_digits, 0x487450 (was FUN_00487450)

`char*(dest[5], int src[4])`. `dest[i] = (char)(src[i]/37) - (i+1)` for i=0..3 (the `/ -0x6c000000` term in the decompile is 0 for all realistic magnitudes, dead in practice), `dest[4]=0`. At the one call site found, the source ints are 0xb21, 0x9af, 0xcdd, 0x9f9, which decode exactly to **"LAVA"** (`2849/37=77='M'-1='L'`, `2479/37=67='C'-2='A'`, `3293/37=89='Y'-3='V'`, `2553/37=69='E'-4='A'`). High confidence given the clean, meaningful result.

### car_name_match_count, 0x4a1380 (was FUN_004a1380)

`int __thiscall(game, car_index, char* tag)`. Returns 0 immediately if `(&DAT_01397384)[game]==0`. Else, for each of the 4 wheels: sets the query point to the wheel's `+0x3274`-family axis vector (negated, via the `world_bsp_locate_point` thunk) and fetches the surface name (`world_query_surface_name` thunk), `_strcmpi`s it against `tag`, counting matches. Returns the match count (0-4).

Tick usage (CLIENT_PHYSICS_MAP step 8/9 boundary, only entered when `wheels_on_ground_count()==4` and drift state is 0): decode "REGE" via `world_decode_surface_tag_bytes`, count matches via `car_name_match_count`; if any wheel matches, skip (do not re-prime air tracking). Else decode "LAVA", count again; if any wheel matches, also skip. Only if **no** wheel is on a "REGE" or "LAVA" tagged surface does the tick set `car+0x3716=1` (was previously named "airborne flag" in CLIENT_PHYSICS_MAP), `+0x3718=1.0`, `+0x371c = current Y`. Net reading: while fully grounded and not drifting, the tick re-arms its air-time baseline every tick **except** when a wheel sits on a "REGE" (regen-trigger?) or "LAVA" (hazard) surface, on those two special surfaces the air-tracking state is deliberately left alone, presumably so a fall/respawn sequence already in progress over lava is not cancelled just because the collision mesh still reports wheel contact. This reading is evidence-supported (the code shape is exactly "skip priming on these two tags") but the *reason* is inferred, guess.

## Constants read (address, value, 4-byte LE float unless noted)

| Address | Value | Used by |
|---|---|---|
| 0x5ea880..0x5ea90f | 9x(friction,penalty,grip,cat) table | world_surface_friction/air_penalty/grip |
| 0x5eb718..0x5eb793 | 30-float zone multiplier table | world_zone_table_index caller |
| 0x59f480 | 1.0 | default friction; BSP sweep segment-t upper bound |
| 0x59f44c | 0.0 | default air penalty/grip; general zero |
| 0x5a83e8 | 0.0 | BSP half-plane test epsilon (strict >=0 pass) |
| 0x5a32d4 | 0.25 | air-penalty scale; car push-apart blend; car+0x3738 pitch-offset ease |
| 0x5a15ec | 0.2 | grip formula scale; world_stuck_probe inner step |
| 0x5a24ec | 2.0 | world_stuck_probe inner loop bound |
| 0x5a323c | 90.0 | world_stuck_probe outer center offset |
| 0x5a3238 | 5.0 | world_stuck_probe outer step |
| 0x59f450 | 50.0 | world_stuck_probe outer bound (grid runs center-90-50..+50) |
| 0x5a6b08 | 0.92 | read, consumer not traced in this pass (near the fall/wheel-compression block, CLIENT_PHYSICS_MAP territory) |
| 0x5a3244 | 30.0 | world_car_push_apart high-speed effect threshold |
| 0x5a3230 | 15.0 | world_car_push_apart low-speed effect threshold |
| 0x1a20658 (DAT) | runtime | "current probed zone id", consumed by world_zone_table_index, writer not found |
| 0x1adf7e4 (DAT) | runtime (piece count) | bound for world_probe_wheel_ground's piece array walk, and world_load_track_pieces's `this+0x1fc` |
| 0x1adf624 (DAT) | runtime (piece array, offset by one slot from world_load_track_pieces's base) | world_probe_wheel_ground named-piece lookup |
| 0x2ebd6a0 | fixed object address | world_zone_table_index's "this" (16-slot cache at +0xdc0) |
| 0x1adf5e8 | fixed object address | world_load_track_pieces's "this" (piece array at +4) |

## Summary: is it a BSP, a triangle soup, or a heightfield

It is a 2D (XZ-plane) BSP over a triangle soup, per piece. Each piece (56-byte header + 4 arrays, loaded straight from a `.col` file) owns: a shared pool of half-plane edge equations (20-byte records: A,B,C float coefficients of `A*x+B*z+C`, sign carried by the top bit of the 16-bit index that references them) and an array of 56-byte cells, each cell = 3 edge references (its triangle boundary) plus two 16-bit BSP child links for tree descent, plus an embedded, null-terminated, upper-cased-on-read surface name string at cell offset +0x16. Point location either walks the tree from the cell array root (`world_bsp_locate_point`, capped at 1024 steps) for speed, or linear-scans all cells (`world_bsp_set_piece`) as a fallback/initial-set. Height (Y) is not part of this structure as read, none of the functions in this pass read or interpolate a Y/height value from the .col data; only X/Z point-location and the surface name were confirmed. Height must come from elsewhere (guess: the visual "geometry" resource, or an unread field in the 12-byte arrays at piece+0x18/+0x34, or the cell record's untouched bytes 0x00-0x0f/0x18-0x37). This is the most important open gap for a 1:1 port and was not closed in this pass.

## Round four, the two 12 byte arrays and the axis names

The two arrays hold three floats each, read on Cookie 01 track.col with the loader order above.

- Piece +0x34, n4 records, the mesh vertices. 4734 records for 9189 cells and 13932 edges, which is the Euler count of a closed triangle sheet. The two uint16 at edge record +0x10 and +0x12 index this array, their maximum is 4733. Range x minus 645 to 643, y minus 707 to 711, z minus 63 to 86
- Piece +0x18, n1 records, one point per named zone in file order. Cookie 01 has 17, the cell names other than dust and regen are START and CHECK_001 to CHECK_016, 17 names. Point k is the negated x y centroid of zone k with z one to nine units above the cell plane, for example START centroid 74.5 minus 65.7 minus 7.4 against point 0 minus 74.7 64.5 minus 5.7

No function in the exe dereferences either array. Piece 0 lives at the fixed address 0x1ADF5EC so its pointers sit at 0x1ADF604 and 0x1ADF620, both without a single code reference, and the BSP walk and the height plane never touch them. The port names the first `zoneAnchors` in `ColPiece`, the vertex array keeps the member name `unreadD` because `body_get_shape_point` in body.cpp reads its raw records.

Axis names. This file calls the BSP plane XZ. In the car record the two plane coordinates are +0x3244 and +0x3248, x and y, and the height is +0x324C, z. `world probe wheel ground` 0x486570 passes minus p[0] and minus p[1] of each probe point at car +0x3274 to `world bsp locate point`, `body place and probe` receives minus +0x3244, minus +0x3248, +0x324C. The port keeps the parameter names x, z, y of this file, a caller must hand +0x3248 to the z argument and +0x324C to the height argument.

## Round six, read on the bytes 2026-09-15

### The four corner footprint is car 0x3274

The absolute addresses inside the car array resolve against the game base 0x1B19090: DAT_01B19A6A is car+0x9DA, DAT_01BC0950 is car+0xA78C0, and DAT_01B1C304 in `world_probe_wheel_ground` is car+0x3274, the four wheel probe points the tick writes from body 0x2120. All four must land in a cell (`world_bsp_locate_point` on minus x minus y of each) before the piece switch by the `#N` name. The port signature takes the four points and the wheel index.

### world_car_push_apart 0x498800 and its two helpers

`world_car_find_overlap` 0x497F80 and `world_car_overlap_test` 0x497EE0 are the pair search, the overlap needs the two z extents (car+0x2EA8) to cross, the plane distance at or under 10.0 and under 0.8 times the sum of the two y extents (car+0x2EA4, catalogue 0x004). The push scales the self body velocity by 0.994 (0x3F7E76C9 at 0x498880, the earlier 0.9936 was a bad decode) and adds (0.8 dx, 0.8 dy, dz) of the unit delta, nudges the other plane position by 0.8 dx and 0.8 dy times 0.06, and plays the impact effect on the faster car. TICK_HELPERS.md round six has the whole read.

### world_zone_table_index is the standings row

0x4B4B50 is `standings_local_row_index`, DAT_01A20658 is the local player id and the 16 rows at 0x2EBD6A0 plus 0xDC0 are the standings table (ITEM_DROP_TABLE_VERIFIED.md). The 30 floats at 0x5EB718 are a throttle factor per standings row, the first seven a hair under 1.0. Renamed `standings_rank_throttle_multiplier`.

### world_ground_height_at 0x485970

Takes three floats, needs ctx 0x9C and `world_ground_test_point` on the first two, reads the four plane floats of the cell at ctx 0x18, `body_vec3_dot` of (minus x, minus y, z) with A B C, and writes `z minus (dot plus D)` into the third float. Unlike `world_locate_piece_by_height` the C coefficient is live here. The cell planes of Race 01 are unit normals, C sits at 0.88 to 0.94 and D near 200, so `world_locate_piece_by_height` with its zero third component reads C times the height, inside its 5.0 tolerance. The port has both as the client does.

### Edge 0x10 and the regen rows

The four bytes at edge 0x10 the earlier rounds called unread are two uint16 vertex indices into the piece 0x34 array, the body contact fallback and the remote wall slide read them through `body_get_shape_point`. The regen rows `world_load_regen_zones` fills at world 0x1ADF810 plus 0x808 with the count at 0x7FC have no reader anywhere in the exe (no absolute reference to 0x1AE000C or 0x1AE0018 and no register relative read of 0x7FC or 0x808 outside the loader), like the two 12 byte col arrays. The REGE cell tag is what acts, the row meaning cannot be settled from the code.
