# Suspension and teleport

Read in KnC.exe.raw, image base 0x400000, on 2026-09-14. Closes the last open gaps of the physics reverse: the per wheel suspension compression write, the wheel on ground byte write, the fall/respawn teleport, the checkpoint list, and the piece level byte arrays.

## Suspension compression

The tick reads car+0x2AD4, +0x2B20, +0x2B6C, +0x2BB8 once, at `car_physics_tick_local` 0x49D15A (`FLD float ptr [EBX+EBP*1+0x2AD4]`), same instruction shape for the other three at 0x49D1A4, 0x49D213, 0x49D268. Each value is multiplied by the constant at 0x5A6AD8 (bytes `AC C5 27 37`, value ~1.0e-5) and folded into a per car global bump accumulator at game+0x1397320, used later to nudge the wheel matrices.

A literal text search for the offsets 0x2AD4/0x2B20/0x2B6C/0x2BB8 across the whole program (485310 instructions scanned) finds only these four reads, nowhere else. The write is through a computed pointer that never carries the literal 0x2AD4 in its own function. Found by walking the call chain with disassembly, not decompile text, because the decompiler drops a register adjustment:

- `body_step_world` (0x4EC560) is a 2 instruction thunk: `ADD ECX,0x340` then `JMP body_world_step` (0x4EFE90). Called from the tick with ECX = wrapper (car+0x211C, confirmed by `LEA EDI,[EBX+EBP*1+0x211C]` at 0x49CF80, EDI held through the substep loop, `MOV ECX,EDI` at 0x49D133 right before `CALL 0x4EC560`). So `body_world_step`'s real this is wrapper+0x340 = car+0x245C, the wheel sub object, not the wrapper the decompile text implies.
- `body_world_step` calls `body_spring_channel_update` and `body_gear_update` with this passed straight through (`MOV ECX,ESI; CALL 0x4EFA90` at 0x4EFF0B, ESI = car+0x245C). So `body_gear_update`'s this is car+0x245C too.
- `body_gear_update` calls `gear_wheel_force_solve` (0x4EECD0) with ECX still car+0x245C: the immediately preceding call is `gear_rk4_state_copy` (0x4EF3E0), which only reads and writes through ECX and never assigns it, so ECX survives the call unchanged (confirmed by its disassembly, no MOV ECX anywhere in that function).
- `gear_wheel_force_solve` entry does `MOV ESI,ECX` at 0x4EECD5, so ESI = car+0x245C for the whole function. It calls `gear_tire_force_model` (0x4F32E0) four times, this per wheel built with `LEA ECX,[ESI+0x630]` (0x4EF046, wheel 0), `[ESI+0x67C]` (0x4EF140, wheel 1), `[ESI+0x6C8]` (0x4EF23A, wheel 2), `[ESI+0x714]` (0x4EF334, wheel 3). Stride 0x4C, matching the stride between the four car offsets.
- `gear_tire_force_model` writes its result at this+0x48 with `MOV dword ptr [ESI+0x48],EAX` at 0x4F334D, ESI being its own this (`MOV ESI,ECX` at 0x4F32EF).

this(wheel N) + 0x48 = car+0x245C + (0x630 + N*0x4C) + 0x48, which computes to car+0x2AD4, 0x2B20, 0x2B6C, 0x2BB8 for N = 0..3, exact match, no rounding. That resolves the write site: **`gear_tire_force_model` (0x4F32E0), called from `gear_wheel_force_solve` (0x4EECD0), writes the suspension compression float for each wheel at instruction 0x4F334D.**

The value stored, from the decompile at 0x4F32E0: a curve lookup on `|dot|` (through `FUN_004EE5D0`, not traced) minus a dot product offset by this+0xC, times this+0x0 (a per wheel rate, read at 0x4F3334 `FMUL float ptr [ESI]`), plus a bias passed in as the last argument. Reads like `compression = (restLength - currentLength) * springRate + bias`. The exact geometric meaning of the dot product inputs (`FUN_004ED6E0`, `body_vec3_dot`) was not traced further, guess only past this shape.

Correction to RIGID_BODY.md: its wrapper offset table lists several fields in the +0x980 to +0xD80 range (gear outputs, "current gear", per wheel load sum) as wrapper relative. Those attributed through `body_gear_update`/`gear_wheel_force_solve` are wrong by 0x340, because that doc's own text for `body_step_world` says "same this, no reassignment", missing the `ADD ECX,0x340` thunk above. Concretely: **car+0x2AA8 is never touched anywhere in the program** (0 hits for the literal offsets 0x2AA8 and 0x2AAC, program wide search, 485312 instructions scanned). The real current gear lives at car+0x2DE8, proven by `car_gear_shift_up` (0x4990A0, already named, increments `*(car+0x2DE8)` clamped under `*(car+0x2F40)`), `car_gear_shift_down` (0x4990D0, decrements clamped at -1), and `car_input_poll_dispatch` (0x497E40, reads car+0x2DE8 at 0x497EBE, sets the reverse/brake byte car+0x332D when it is negative, at 0x497EC8). car+0x2F40 is the max gear ceiling, same field `car_gear_shift_up` clamps against.

This is the same 4 bytes GROUND_AND_RESPAWN.md calls the "fall/respawn counter", written by `car_gear_clamp` (0x499050) and `car_ground_flag_set` (0x49A920) through the raw car array offset, independent of the wrapper/wheel sub chain (both compute `idx*0xA7260 + 0x2DE8 + base` directly, confirmed by their own decompile). So car+0x2DE8 is really the gear index, and those two functions force the gear toward 0 (ground invalid) or clamp it to 1 (ground just became valid) rather than track a fall counter. **This means car+0x2DE8 is not the mechanism behind the teleport described below; that turned out to be a dead end from the old docs, superseded here.**

## The tyre model, settled, fourth pass 2026-09-14

`gear_tire_force_model` (0x4F32E0, this = wheel scratch at wheel set +0x630 + i*0x4C, 25 stack args) fully read from the disassembly. The scratch holds catalogue 0x128 (spring, at +0x0, 400000 after the setup override at 0x495017, the immediate 0x48C35000 was read as 100000 until the fifth pass of RIGID_BODY.md, with 400000 the port rests at the recorded 0.79), that times 0.1 (+0x4), minus catalogue 0x130 (+0x8), catalogue 0x138 (+0xc), the wheel radius (+0x10), the literal 2 (+0x14) and three curve pointers (+0x18 +0x1c +0x20) bound by `tire_scratch_bind` (0x4F3290). The curves come from `wheel_disc_curves_build` (0x4F3130) per axle, count catalogue 0x118, edge power 0x120, over t = i / (count minus 1) with c = sqrt(1 minus t squared), halfW = width / 2, rim = radius minus halfW:

- +0x18 axis offset, table minus t times halfW
- +0x1c contact distance, table (1 minus t to the power) / c times rim plus halfW, the last entry halfW
- +0x20 clearance, table c times rim plus halfW

`curve_build` (0x4EE540) keeps count, the table, xmin, xmax minus 0.001 (0x5A3BEC) and scale (count minus 1) / span, `curve_eval` (0x4EE5D0) clamps to the range, u = (x minus xmin) times scale, i = trunc(u), t = u minus i, and returns a Catmull Rom with the weights (3t3 minus 5t2 plus 2) / 2, (4t2 minus 3t3 plus t) / 2, (2t2 minus t3 minus t) / 2, (t3 minus t2) / 2 on p0, p1, pm1, p2, the neighbours mirrored at both ends (constants 0x5A32B8, 0x5A3238, 0x5A24EC, 0x5A0054, 0x59F414).

Arguments: hubPos, hubVel, axisWorld, spinRate, the col cell pointer of the wheel (wheel set +0x970 + i*4, the cell the wheel's query block holds, its first 16 bytes A B C D are the plane), outContact, outForce, the seven axle floats at +0x800 or +0x81c, the five shared floats at +0x838, the result block at +0x860 + i*0x14, the two slip states, the two slip derivative outputs, and the bias +0x858 or +0x85c.

The formula, n = (A, B, C), d = axisWorld dot n, ad = |d|:
- compression (written at +0x48, car+0x2AD4 family) = (clearance(ad) minus (hubPos dot n plus D)) times spring plus bias
- load = compression clamped to plus minus (400000 (0x5A83B0) minus min(ad, 0.2) times 1000000 (0x5A8374))
- forward = axis cross n, down = axis cross forward, lateral = forward cross n
- contact = hubPos plus down times contactDist(ad) plus axis times axisOffset(ad) with the sign of d
- when ad is 0.9999 or less (double 0x5A8388): forward and lateral scaled by 1 / sqrt(1 minus ad squared), vFwd = hubVel dot forward, vLat = hubVel dot lateral, else both 0
- vs = vFwd above shared 2 (1.0), minus vFwd below minus 1.0, else (vFwd squared / 1.0 plus 1.0) / 2
- lateral slip derivative = (vLat minus slipLat times vs) / shared 1 (0.2), longitudinal = ((spinRate times radius minus vFwd) minus slipLong times vs) / shared 0 (0.2), the two banks at +0x8b0 and +0x910 integrate these
- load under 1.0: results zero, return 0, the ground byte becomes 1
- else e = exp(minus load times axle 5 (5e-5)), f1 = e times axle 4 (the grip base 3.8 or 4.8), f2 = f1 times axle 6 (1.0), nLat = axle 0 (85947.66) times slipLat / (f2 times load), nLong = axle 1 (85943) times slipLong / (f1 times load), mag = sqrt(nLat2 + nLong2), fLat = axle 0 times slipLat, fLong = axle 1 times slipLong
- mag at least 0.001: x = mag / axle 2 (1.2), q = mag2 / 4, a = (axle 0 times q + axle 1) times nLong, b = (q + 1) times nLat times axle 1, ang = atan(x), ang2 = atan(x minus (x minus ang) times axle 3 (minus 0.2)), m = sin(ang2 times 1.2) / sqrt(a2 + b2), fLong = m a f1 load, fLat = minus m b f2 load
- force = forward times fLong plus lateral times fLat plus n times load
- results: f1 times load, fLat, fLong, mag / (mag + shared 3 (6.0)) times load, and the int 1 when mag is above shared 4 (3.0), return 1

The seven axle floats and five shared floats are the `body_create` immediates, see RIGID_BODY.md fourth pass. The dot product inputs of the earlier reading are settled: the spin axis against the plane normal, and the hub against the plane.

### The tyre frame on a slope, ninth round 2026-09-15

`gear_tire_force_model` 0x4F32E0 disassembled for the bank slide of GHOST_REFERENCE.md. EBX is the axis in world (arg 3), EDI the cell pointer (arg 5), `body_vec3_cross` 0x4ED700 takes the second pushed operand as A and the first pushed as B:

- 0x4F33B8 `PUSH EDI` `PUSH EBX`, out [ESP+0x18], forward = axis cross n
- 0x4F33C3 pushes forward then EBX, out [ESP+0x3c], down = axis cross forward
- 0x4F33D2 `PUSH EDI` then forward, out [ESP+0x24], lateral = forward cross n
- 0x4F3408 `body_vec3_madd2`(out arg 6, hub, down, contact distance, axis, axis offset with the sign of d)
- under 0.9999 both forward and lateral scale by 1 over sqrt(1 minus d squared), vFwd is hub velocity dot forward, vLat hub velocity dot lateral

Forward and lateral lie in the plane of the wheel's own cell (n is the first three floats of the cell, unit length in the Race 01 col, the trace prints its length as 1.0000 on the bank), the tyre frame is not built on the world horizontal. The load is (clearance(|d|) minus (hub dot n plus D)) times the spring plus the bias, `FADD [EDI+0xc]` at 0x4F332D is D. The port has this frame line for line.

## Wheel on ground bytes

car+0x2DF0 to car+0x2DF3 (one byte per wheel) are written by `gear_wheel_force_solve` itself, right after each `gear_tire_force_model` call, this = car+0x245C so ESI+0x994 = car+0x2DF0:

- wheel 0: `MOV byte ptr [ESI+0x994],BL` at 0x4EF068 (BL=0, taken with a prior call to `FUN_004F2060`) or `MOV byte ptr [ESI+0x994],0x1` at 0x4EF070
- wheel 1: 0x4EF162 / 0x4EF16A, wheel 2: 0x4EF25C / 0x4EF264, wheel 3: 0x4EF356 / 0x4EF362

The byte is 1 when `gear_tire_force_model` returns 0 (the shallow branch, `_DAT_0059f480 <= param6` false, no significant load computed) and 0, after calling `FUN_004F2060` first, when it returns 1 (the branch that computes real tire forces). The exact physical meaning of that polarity is not resolved here, guess only; the write site and the condition on the return value are proven by the disassembly above.

`body_wheel_axis_correct` and `body_wheels_on_ground_count` only read this+0x994..0x997 (already established in RIGID_BODY.md), they do not write it.

## The teleport

The old docs' framing of car+0x2DE8 as a fall counter with car+0x2F40 as its floor does not hold, see above. The real mechanism is a separate watchdog and a separate state machine, both already named in Ghidra.

`respawn_crash_recovery_update` (0x4A0970) runs once per tick for the local car. State kept at game+0x1397368 (state, 0/1/2/100), +0x1397370 (progress index along the nearest checkpoint polyline), +0x1397374 (distance to it), +0x1397378/+0x139737C (64 bit ms timestamp of the last direction change):

- State 0: arms through `respawn_checkpoint_nearest_all` (0x489970, renamed this pass), which finds the closest point across all 4 checkpoint lists to the car position (car+0x3244..0x324C). Success sets state 1.
- State 1/2: each tick calls `respawn_checkpoint_nearest_on_list` (0x489890, renamed this pass) against the one list already chosen, comparing the new progress index against the stored one, and takes the nearest point of all four lists again when the car sits more than 80.0 (0x5A32A0) from its list. Only a FALLING index counts, read again on the asm 2026-09-24: state 1 goes to 2 at 0x4A0AD8 only when the index step is under 5 and the new index is below the stored one, state 2 goes to 100 at 0x4A0B89 when the index is still falling more than 0x9C4 (2500) ms after the stamp, a rising index takes state 2 back to 1, an equal index changes nothing. Tracks 63 (list 0 rows 116 to 130) and 42 (list 0 rows 56 to 86) go back to state 1 instead of 100. The battle track 30000000 (`FUN_00487250`) skips the machine. A car that stands still never escalates, the port escalated on an index that did not rise, so a kart parked on the grid through the countdown was teleported ahead the moment the green light let `car_respawn_state_machine` run.
- State 100: if the index resumes increasing, drop back to state 1 (0x4A0AF9-0x4A0B99 area). If it is still falling and 0x7D0 (2000) ms pass (`EBP - lastTime > 0x7D0` test at 0x4A0C0A), commit: state resets to 0, current position is cached into car+0xA7878/0xA787C/0xA7880 (0x4A0C33-0x4A0C4A), car+0x6BC is set to 100 (0x4A0C52), the checkpoint name and timer block car+0x6C0..0x6E0 is zeroed (0x4A0C6F-0x4A0C8C).
- If state stays 100 without committing this tick, and the local car is not finished/spectating (car+0xA7854 is 0 or 1) and no other UI mode is active (game+0x2EBA084 == 0), it calls `FUN_004B23C0(6, 1)` (0x4A0CFA) to enter a recovery camera state. `FUN_004B23C0`'s case 6 has no special body beyond the common state bookkeeping (checked by decompile of 0x4B23C0: only cases 0, 2, 3, 4, 7 do anything past the header), so this is a pure UI/camera flag, not itself a teleport.

`car_respawn_state_machine` (0x4A1420, kept under its existing name, the checkpoint/teleport state machine hiding behind that name) switches on car+0x6BC and drives the actual teleport once it sees 100:

- case 100 (0x4A335C-0x4A3390): starts a screen fade (`FUN_0043ED70(5,-1,1.0)` at 0x4A3370, `FUN_0043D7E0` on object 0xD6E188 mode 2 duration 600.0 at 0x4A337C-0x4A3381), sets car+0x6BC = 0x65 (101) at 0x4A3386.
- case 0x65 (0x4A3395 onward): waits for the fade flag `DAT_00D6E1D0 == 0` (0x4A3395-0x4A339B), then calls `respawn_recovery_point_find` (0x489B40, renamed this pass) with the cached position car+0xA7878.. as the query, on the same checkpoint list object as above. On success it reads a baked table at 0x1AE2224 (stride 0x10, index = (listIndex*400+pointIndex)*0x10, computed at 0x4A33E2-0x4A33EA) and writes: car+0x3244/0x3248/0x324C = table x, z, y+3.0 (bias constant at 0x5A32B8, added at 0x4A3403), car+0x3220 = table yaw, at 0x4A33ED-0x4A3431. It then calls `body_place_and_probe(-car.x, -car.z, car.y, -car.yaw)` at 0x4A346C, ECX = car+0x211C (wrapper, `LEA ECX,[EBP+0x211C]` at 0x4A345F). **This call is the teleport**, it is outside the tick and outside spawn, exactly the code GROUND_AND_RESPAWN.md was looking for. It continues with camera and sound resets, a fade back in, and sets car+0x6BC = 0x66 (102).
- case 0x66 (102): waits for the fade flag again, then sets car+0x6BC = 0 and clears the checkpoint name/timer block, back to idle.

Trigger condition: the car's progress index along the nearest checkpoint polyline keeps falling for two back to back windows of 2500 and 2000 ms (roughly 4 seconds total, gated by a race state check, `FUN_00487230`/`FUN_00487250`, and only active in race states 0xB, 0xF, 0x11). No height test and no key press are involved on the static path found here; recovery is time and progress driven.

Position source: **not** the cached car+0xA7878 position itself, and not a start.ini row. That cached position is only the query key into a baked per track grid table at 0x1AE2224 (400 wide, `respawn_recovery_point_find` walks the same 4 checkpoint lists with an extra validity filter through `FUN_00485970`, not traced). The loader for this table was not located in this pass (xrefs land in `gimmick_pool_update`, `FUN_004BA380`, `FUN_004A93E0`, `car_respawn_state_machine`, `FUN_004D00C0`, all consumers, no obvious fopen/fread site among them), guess that it is baked per track alongside the checkpoint lists.

What it resets: boost/drift state (`FUN_00496B10`, `car_drift_state_set(idx,0)`), the checkpoint name and timer block car+0x6C0..0x6E0, and car+0x6BC itself back to 0 once the sequence completes. It does not touch car+0x2DE8 (gear) or car+0x2F40.

## The checkpoint list

`respawn_checkpoint_nearest_all`, `respawn_checkpoint_nearest_on_list` and `respawn_recovery_point_find` all take the same object, a global at a fixed address, confirmed by `MOV ECX,0x1ADF810` at every call site in `respawn_crash_recovery_update` (0x4A099C, 0x4A0983-0x4A09BC, 0x4A0A84 area) and in `car_respawn_state_machine` (0x4A33CC).

Object layout, read from the three functions' own decompiles:

- +0x2A04, +0x2A08, +0x2A0C, +0x2A10: int counts, one per list, 4 lists
- +0x2A14: base of the point records (`respawn_checkpoint_nearest_all` indexes 8 bytes past this at +0x2A1C with a -2/-1/0 float offset, which is the same base)
- record stride 0x10 bytes (4 floats): offsets 0, 4, 8 read as x, y, z in the distance test (`FUN_0044D950`, not traced, looks like a squared distance), offset 0xC never read by these three functions
- list stride 0x1900 bytes (400 records of 0x10 bytes each, matches the `iVar6 + 400` outer loop step in `respawn_recovery_point_find`)

Not proven to come from start.ini or the .col file in this pass; no loader was reached from these three functions, they only read the object. Given the 4 parallel lists and per point stride, guess it is a set of baked polylines (branch/lane variants of the track's centerline) generated at track load, separate from the height planes documented in GROUND_AND_RESPAWN.md.

## The piece arrays

Task asked to list every xref of piece+0x18 and piece+0x34 (array C and D of the .col piece record, stride 0xC each, counts at piece+0x14 and piece+0x2C). GROUND_AND_RESPAWN.md already traced every function that takes a piece pointer (`world_bsp_set_piece`, `world_bsp_locate_point`, `world_bsp_edge_sweep`, `world_query_surface_name`, `world_locate_piece_by_height`, `world_ground_test_point`, `world_probe_wheel_ground`, `world_probe_wheel_point`, `body_ground_probe_response`, `body_place_and_probe`) and found none of them dereference piece+0x18 or piece+0x34; only the loader `world_load_col_piece` (0x4ECE90) and the destructor `world_col_piece_free` (0x4ECDB0) touch the two pointers, to allocate/free them.

This pass worked deep through the body_*, gear_*, respawn_* and car_respawn_state_machine families (the whole suspension and teleport chain above) and never encountered a piece-relative read of +0x18 or +0x34 either. That is corroborating, not new proof; a direct literal search for "0x18" or "0x34" program wide is not meaningful on its own (those are common small immediates used for unrelated purposes throughout the binary), so the only sound static method is the function-by-function trace GROUND_AND_RESPAWN.md already did. Standing conclusion: **loaded, freed, never read**, purpose unknown, guess only (editor data, an unused triangle index list, or a consumer outside the traced call graph).

## Offset table

| Car offset | Meaning | Proof |
|---|---|---|
| +0x2AD4/+0x2B20/+0x2B6C/+0x2BB8 | per wheel suspension compression, stride 0x4C | write: `gear_tire_force_model` 0x4F32E0 at 0x4F334D. read: `car_physics_tick_local` 0x49D15A/0x49D1A4/0x49D213/0x49D268 |
| +0x2DF0..+0x2DF3 | per wheel on ground byte | write: `gear_wheel_force_solve` 0x4EECD0 at 0x4EF068/70, 0x4EF162/6A, 0x4EF25C/64, 0x4EF356/62 |
| +0x2DE8 | current gear, not a fall counter | `car_gear_shift_up` 0x4990A0, `car_gear_shift_down` 0x4990D0, `car_input_poll_dispatch` 0x497EBE |
| +0x2F40 | max gear ceiling, own writer not found | `car_gear_shift_up` 0x4990AA clamp |
| +0x6BC | teleport phase inside the checkpoint/progress range: 100 fade out, 0x65 reposition, 0x66 fade in, back to 0 | `car_respawn_state_machine` 0x4A1420, case bodies at 0x4A335C, 0x4A3395, common footer 0x4A30E9 area |
| +0xA7878/+0xA787C/+0xA7880 | cached position, query key for the teleport target lookup, not the destination itself | `respawn_crash_recovery_update` 0x4A0970 at 0x4A0C33-0x4A0C4A, read by `car_respawn_state_machine` at 0x4A33B1 |
| wheel sub object (car+0x245C) +0x630..+0x760 | per wheel tire force scratch, this passed to `gear_tire_force_model`, includes the compression output at +0x678/+0x6C4/+0x710/+0x75C (= car+0x2AD4 family) | `gear_wheel_force_solve` 0x4EF046/0x4EF140/0x4EF23A/0x4EF334 |
| game+0x1397368/+0x1397370/+0x1397374/+0x1397378 | crash recovery watchdog: state, progress index, distance, last direction change time (64 bit) | `respawn_crash_recovery_update` 0x4A0970 |
| global object 0x1ADF810 | checkpoint list, 4 lists, counts at +0x2A04.., points at +0x2A14, stride 0x1900 per list, 0x10 per point | callers of `respawn_checkpoint_nearest_all`/`_on_list`/`respawn_recovery_point_find` |
| global table 0x1AE2224 | baked per track grid, stride 0x10 (x, z, y, yaw), teleport destination | `car_respawn_state_machine` 0x4A33ED-0x4A3431 |
| piece+0x18, piece+0x34 | loaded and freed, never read by any traced collision or physics function | `world_load_col_piece` 0x4ECE90, `world_col_piece_free` 0x4ECDB0, see GROUND_AND_RESPAWN.md |

## Constants read (address, bytes, value, used by)

| Address | Bytes (LE) | Value | Used by |
|---|---|---|---|
| 0x5A6AD8 | AC C5 27 37 | ~1.0e-5 | tick, scales the suspension compression read at 0x49D161 |
| 0x5A32D4 | 00 00 80 3E | 0.25 | tick, weights each wheel's compression into the bump accumulator at 0x49D192 |
| 0x5A3720 | 00 00 80 BF | -1.0 | tick, sign gate on the scaled compression at 0x49D16C |
| 0x5A32A0 | 00 00 A0 42 | 80.0 | `respawn_crash_recovery_update`, re-probe distance bound at 0x4A0A49 |
| 0x5A32B8 | 00 00 40 40 | 3.0 | `car_respawn_state_machine`, Y bias added to the teleport table height at 0x4A3403 |
| 0x9C5 (immediate) | - | 2501 | direction change debounce, `respawn_crash_recovery_update` 0x4A0B39 |
| 0x7D0 (immediate) | - | 2000 | commit timer, `respawn_crash_recovery_update` 0x4A0C0A |
| 0x190 (immediate) | - | 400 | grid table row width, `car_respawn_state_machine` 0x4A33E2 |

## Open questions

- The per wheel rate at wheel sub object+0x630 (this[0] in `gear_tire_force_model`, the spring rate multiplier at 0x4F3334) is catalogue 0x128 bound by `tire_scratch_bind`, 400000 after the setup override, settled.
- `FUN_0044EE5D0` (curve lookup used inside `gear_tire_force_model`) and `body_vec3_dot`'s exact inputs there were not traced, so the compression formula's geometric meaning (what the dot product measures) is not fully proven, only its shape.
- The polarity of the wheel on ground byte relative to `gear_tire_force_model`'s return value (1 means "shallow, no load" in the traced branch) is counter to a naive reading, not independently confirmed against ground truth gameplay.
- The loader for the checkpoint list object at 0x1ADF810 and the grid table at 0x1AE2224 was not located; both are read only by every function this pass traced. Whether either comes from start.ini, the .col file or a separate track resource is guess.
- `FUN_00485970` (the extra validity filter `respawn_recovery_point_find` applies) was not decompiled.
- `FUN_004B23C0`'s states 1, 5, 6, 8 and above (beyond 0, 2, 3, 4, 7 which are handled in its decompile) were not resolved, kept unrenamed for that reason.
- `FUN_004B1E30`, called to leave UI state 6, was not decompiled.
- Whether `car_gear_clamp` and `car_ground_flag_set` forcing the gear (car+0x2DE8) toward 0/1 on ground validity change is intentional game design or leftover from an earlier "fall counter" version of the code is guess; only the addresses and the call sites are proven.
