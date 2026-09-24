# Body motion

Read in KnC.exe.raw, image base 0x400000, on 2026-09-14. Closes the one gap RIGID_BODY.md left open: where position (car+0x21E0) actually changes value from one substep to the next.

## Correction to RIGID_BODY.md

RIGID_BODY.md read `body_integrate`'s decompile text and concluded `body_update_transform`'s `this` is wrapper+0xc4 (car+0x21E0, the position itself). That is wrong, the same class of error SUSPENSION_AND_TELEPORT.md already found in `body_step_world`. Disassembly of the call site, not the decompiler's call text, is the proof.

`body_integrate` (0x4ECAB0), disassembled:

```
004ecab8: LEA EAX,[ESI + 0xdc]     ; EAX = wrapper+0xdc = car+0x21F8
004ecabe: PUSH EAX                  ; pushed first (far stack arg)
004ecabf: LEA ECX,[ESI + 0xc4]     ; ECX = wrapper+0xc4 = car+0x21E0 (position)
004ecac5: LEA EDI,[ESI + 0x340]    ; EDI = wrapper+0x340 = car+0x245C (wheel/contact sub object)
004ecacb: PUSH ECX                  ; pushed second (near stack arg)
004ecacc: MOV ECX,EDI               ; bytes 8B CF, confirmed by read_memory
004ecace: CALL 0x004f1b10           ; body_update_transform
```

`ECX` is loaded with car+0x21E0 only to build the `PUSH`, then overwritten with `EDI` (car+0x245C) right before the call. The decompiler shows `body_update_transform(param_1+0xc4, param_1+0xdc)` because it only knows the two stack args, it drops the register `this`. The real call is `body_update_transform(this=car+0x245C, stack0=car+0x21F8, stack1=car+0x21E0)`.

The same is true of `body_update_wheel_transforms` two lines later (0x4ecad3-0x4ecadd): `MOV ECX,EDI` again, same `EDI=car+0x245C`. Both functions run on the wheel/contact sub object, not on the position block and not on the wrapper root.

Consequence: every offset the task brief assumed was wrapper-root-relative (wrapper+0x124 = car+0x2240 etc) is off by 0x340, the same error already flagged for `body_gear_update`. The real fields are wrapper+0x340+0x124 = car+0x2580, not car+0x2240. See the offset table.

## The transform block

Both functions read/write a small block inside the wheel/contact sub object (car+0x245C), at the fixed offsets 0x124, 0x148, 0x194, 0x1AC. Call this `R` (0x1AC), `Blk` (0x124), `v` (0x148), `T` (0x194).

### body_update_transform (0x4F1B10)

Disassembled fully (18 instructions). `this=ECX=car+0x245C` on entry, `stack0=car+0x21F8`, `stack1=car+0x21E0` (position).

Step 1, `body_mat3_multiply` (0x4ED150), `CALL` at 0x4f1b26:
```
ECX (out)     = stack0 = car+0x21F8
[ESP+4] (A)   = this+0x1AC = car+0x2608  (R)
[ESP+8] (B)   = this+0x124 = car+0x2580  (Blk)
```
`body_mat3_multiply(out,A,B)` computes `out = A * B`, standard row-major 3x3 (proven by its own disassembly, 0x4ED150-0x4ED232, 9 FLD/FMUL/FADDP/FSTP groups, `RET 0x8`). So `car+0x21F8..0x221B (9 floats) = R * Blk`. No further reader of car+0x21F8 was found anywhere in the program (0 hits searching "0x21f8"), this output is a byproduct, not consumed elsewhere in the traced call graph.

Step 2, `body_vec3_transform` (0x4EE440), `CALL` at 0x4f1b3e:
```
ECX (out)         = stack1 = car+0x21E0  (position)
[ESP+4] (v)       = this+0x148 = car+0x25A4
[ESP+8] (mat)     = this+0x1AC = car+0x2608  (R)
[ESP+0xc] (add)   = this+0x194 = car+0x25F0  (T)
```
`body_vec3_transform(out,v,mat,add)` computes `out = mat * v + add`, proven by its own decompile (`*param_1 = *param_2**param_3 + param_3[2]*param_2[2] + param_3[1]*param_2[1] + *param_4`, row 0 of a row-major 3x3 times a column vector, plus the add vector).

**Result: position (car+0x21E0/0x21E4/0x21E8) = R (car+0x2608, 3x3) * v (car+0x25A4, vec3) + T (car+0x25F0, vec3).**

`R` and `T` are matrix and translation, both hold the full magnitude needed to place the chassis. `v` is a small local vector (never seen larger than the rest of the block). None of the three is a raw position by itself, `T` is the closest thing to one.

### body_update_wheel_transforms (0x4F1B50)

Disassembled fully (33 instructions), `RET 0x8`. Same `this=car+0x245C` (proven at its call site, 0x4ecadb `MOV ECX,EDI`). Two stack args: `stack0=car+0x2120` (rotation/axle axis array, near/last-pushed) and `stack1=car+0x2150` (a 4x 3x3-matrix array, far/first-pushed, ends exactly at car+0x21E0, the position field: `0x2150 + 4*0x24 = 0x21E0`).

Loop 4 times, wheel object `i` at car+0x2674+i*0xB8 (`wrapper+0x558`, RIGID_BODY.md's wheel object array), stride 0xB8:
```
local        = body_vec3_madd(base=wheel_i+0x0, dir=wheel_i+0xC, scale=*(wheel_i+0x4C))
axis_i (out) = stack0 + i*0xC   = car+0x2120 + i*0xC
axis_i       = R (this+0x1AC) * local + T (this+0x194)
wheelmat_i (out) = stack1 + i*0x24 = car+0x2150 + i*0x24
wheelmat_i   = body_mat3_multiply(A=R, B=wheel_i+0x28)
```
So the 4 wheel axis probe points (car+0x2120..0x214C, fed to the ground raycasts) and the 4 wheel local matrices (car+0x2150..0x21E0) are placed with the *same* `R`/`T` as the chassis position. All of it moves together.

### Which fields are velocity, acceleration, or position

- `R` (car+0x2608, this+0x1AC): orientation, a 3x3 matrix. Not touched per substep. Rebuilt once per **tick** by `body_wheel_axis_correct` (0x4EFC90, called from `body_finalize_wheels`, after the substep loop), via `body_quat_to_matrix(this+0x1AC)` at instruction 0x4efd9b. Its source is the reference quaternion at this+0x1D0 (car+0x262C), slerped 12% (immediate `0x3DF5C28F`) toward the wheel-axis deviation each tick. Kinematic input, not a velocity.
- `T` (car+0x25F0, this+0x194): translation. This is the field that moves the car. It is a **position**, and it is the target of a genuine RK4 integration every substep, see below.
- `v` (car+0x25A4, this+0x148) and `Blk` (car+0x2580, this+0x124): a local vector and a matrix. No writer was found anywhere in the program (0 hits for absolute offsets 0x25a4 and 0x2580, and the relative offsets 0x148/0x124 restricted to the body_*/gear_* address range, 0x4EC000-0x4F5000, show only reads). Best supported reading: spawn-time constants (set once by `body_create`/`body_place_and_probe`/`body_wheel_state_reset`, never revisited), a fixed local arm rotated by `R`. Not proven, marked guess.

## Position writers

Exhaustive `search_instructions` across the whole program (485,313-485,314 instructions scanned each pass):

| Pattern searched | Hits | Where |
|---|---|---|
| absolute `0x21e0` | 1 | `car_physics_tick_local` 0x49d0a5, a **read** (`FLD`), copies position out to car+0x3244 with sign flip |
| absolute `0x21f8` | 0 | none |
| absolute `0x2608` | 1 | `car_physics_tick_local` 0x49dd8a, a **read** (`LEA` then `REP MOVSD`, copies R into a stack scratch for the post-loop wheel-matrix build) |
| absolute `0x25a4`, `0x2580` | 0 | none |
| absolute `0x25f0` | 0 | none (T is only ever reached through a register base, see below) |

Only two writers of position (car+0x21E0/0x21E4/0x21E8) exist in the traced call graph:

1. `body_update_transform` (0x4F1B10), every substep, via the `R*v+T` formula above, this=car+0x245C.
2. `body_vec3_set` inside `body_place_and_probe` (0x4EC290), a direct assignment, called once at spawn (from `body_create`) and once at teleport (from `car_respawn_state_machine`, 0x4A346C). Not per substep.

There is no third writer. Because `v` and `Blk` are not proven to change, `T` is where the actual substep-to-substep displacement has to live, and it does.

## The force accumulator

`body_apply_force` (0x4EC420, this=wrapper=car+0x211C) adds `(fx,fy,fz)` into wrapper+0x4E0 = car+0x25FC/0x2600/0x2604 via `body_vec3_add`. Called once per substep from the tick, `fx/fy/fz` built from car+0x2610 (engine force base), car+0x261C, car+0x2628, times 0.6 (0x5EB6F4) and 0.16 (0x5A6ADC/0x5A6AE0) and a per-tick jitter value, confirmed at 0x49cfb3-0x49d00a.

Full-program search on `0x25fc` (25 hits): only 5 are proven car-relative (`EBX+EBP*1+0x25fc` / a confirmed `this`), the rest match the same small literal on unrelated structs and are not claimed here.

| Writer | Address | What it does |
|---|---|---|
| `body_apply_force` | 0x4EC420 | `car+0x25FC += (fx,fy,fz)`, main per-substep input |
| `body_chassis_integrate_k1..k4` | 0x4F0B40, 0x4F0F40, 0x4F12B0, 0x4F1620 | clamps then advances car+0x25FC as the RK4 velocity state, see below |
| `car_physics_tick_local` | 0x49ce25-0x49ce2f | `body_vec3_scale(car+0x25FC, jitterFactor)`, once per **tick**, before the substep loop |
| `car_substep_collision_response` | 0x498b5f-0x498b68 | `body_vec3_scale(car+0x25FC, ~0.94 or a durability-curve value)`, bounce damping on collision |
| `world_car_push_apart` | 0x49887a | address of car+0x25FC computed (`LEA EBP,[EDI+0x25fc]`), not traced further, guess: adds a separation impulse |

**The key finding: car+0x25FC is not only a force accumulator. this+0x1A0 relative to car+0x245C is the exact same memory** (`car+0x245C + 0x1A0 = car+0x25FC`, confirmed by hex arithmetic). `body_chassis_integrate_k1` reads it, clamps it to +-120 (x, constants 0x5A8390/0x5A6A14), +-150 (y, same constants reused) and +-60 (z, 0x5A6A90/0x5A6A68), and treats the clamped value as **velocity**. So the same 12 bytes are a force accumulator to `body_apply_force` and a velocity state to the RK4 integrator, consistent with an implicit mass of 1.

`body_integrate` copies the accumulator into wrapper+0xd0..0xd8 = car+0x21EC..0x21F4 (a snapshot next to position, already noted by RIGID_BODY.md, not itself a further writer) right after `body_update_transform` and before the ground probes.

### The RK4 chain that integrates T

`body_gear_update` (0x4EFA90) is called once per substep (through `body_step_world` -> `body_world_step`, this=car+0x245C throughout, the same +0x340 thunk SUSPENSION_AND_TELEPORT.md found). Its decompile shows 4 stages, each pairing a wheel-spin RK4 step (`gear_rk4_stageN_blend` + `gear_wheel_force_solve`, already named, already ported) with a **second, separate RK4 step for the chassis** that was not previously traced:

```
gear_rk4_step_coeffs(dt)                          ; writes the shared dt scratch, see Constants
body_chassis_rk4_stage1()  -> body_chassis_integrate_k1(this)
body_chassis_rk4_stage2()  -> body_chassis_integrate_k2(this)
body_chassis_rk4_stage3()  -> body_chassis_integrate_k3(this)
body_chassis_rk4_stage4()  -> body_chassis_integrate_k4(this)
```

`this` is passed by simple register passthrough (`body_chassis_rk4_stage1` at 0x4F2240 opens with `MOV ESI,ECX; CALL body_chassis_integrate_k1`), so `this=car+0x245C` for the whole chain, same object as `body_update_transform`.

`body_chassis_integrate_k1` (0x4F0B40), decompiled fully:
- reads this+0x1A0/0x1A4/0x1A8 (car+0x25FC family, the accumulator), clamps it, stores the clamped copy at this+0x58/0x5c/0x60
- **`T += dtFraction * velocityClamped`**: `this+0x194 = 0x02f26d4c * this[0x58] + this[0x194]` (and the y/z pair at 0x198/0x19c), i.e. `car+0x25F0 += halfDt * v`
- integrates the reference quaternion at this+0x1D0 (car+0x262C, the same field `body_wheel_axis_correct` slerps once per tick) from an angular-velocity-like term at this+0x64/0x68/0x6c, renormalizing when the squared magnitude drifts past a threshold (constants 0x5A8388/0x5A8380)
- advances velocity itself: `this+0x1A0 += this+0x70` (an "acceleration" built from `dt * this[0xEC] * this[0x1EC]`, a drag-like term, this+0xEC role not resolved beyond "a per-body coefficient", guess: mass or force-scale, the field RIGID_BODY.md's own offset table left as "curve breakpoints")

`body_chassis_integrate_k2` and `k3` (0x4F0F40, 0x4F12B0) mirror `k1` at the same offsets (confirmed: both show `FSTP float ptr [ESI+0x194]` writes, found by the 0x194-operand search restricted to the body address range).

`body_chassis_integrate_k4` (0x4F1620) is the **combine** step, confirmed by decompile:
```
param_1[0x65] = (param_1[0x2e] + param_1[0x22] + param_1[0x2e] + param_1[0x22] + param_1[0x68] + param_1[0x16]) * DAT_02f26d54 + *param_1
```
(`param_1` here is typed `float*` by the decompiler, index 0x65 * 4 = byte 0x194, same `T` field). That is exactly the classic RK4 weighted sum `(k1 + 2*k2 + 2*k3 + k4) * dt/6 + state0`, `DAT_02f26d54` being the sixth-dt constant `gear_rk4_step_coeffs` writes. The same pattern combines the y and z components, and the quaternion.

This is the missing velocity integration RIGID_BODY.md could not find: it exists, but it sits inside `body_gear_update`'s call tree, above the 0x4F2000 address boundary that RIGID_BODY.md explicitly stopped tracing at ("FUN_004f3130... sit above the 0x4F2000 boundary, not traced"). `body_chassis_rk4_stage1..4` and their 4 `body_chassis_integrate_k1..k4` callees live at 0x4F0B40-0x4F2AF0-ish, inside that unscoped range.

## The substep in order

One call to `car_apply_engine_force` / `body_apply_force` / `body_integrate` / (conditionally) `car_substep_collision_response` / `world_car_push_apart` / `car_gear_clamp` / `body_step_world`, repeated `substeps` times (`car_physics_tick_local`, loop body at 0x49cf90-0x49d13e):

1. **Engine force prep.** Read car+0x2610 (engine base), 0x261C, 0x2628, scale by 0.6 (0x5EB6F4) and 0.16 (0x5A6ADC/-0x5A6AE0) and a per-substep jitter value. `car_apply_engine_force` (0x4968F0) is a thin wrapper: if game+0x1397384[carIndex] is set, it calls `body_apply_force(-x,-y,z)` itself (this=wrapper, computed via `idx*0xA7260+0x211c`); it is not the missing integrator, it is one more caller of `body_apply_force`.
2. **body_apply_force(wrapper, fx, fy, fz)**, CALL 0x4ec420 at 0x49d00a. `car+0x25FC/0x2600/0x2604 += (fx,fy,fz)`.
3. **body_integrate(wrapper)**, CALL 0x4ecab0 at 0x49d011.
   - `body_update_transform(this=car+0x245C, ...)`: position = R*v+T, using the R and T left over from the *end of the previous* substep (T not yet advanced this substep).
   - `body_update_wheel_transforms(this=car+0x245C, ...)`: 4 wheel axis probe points and 4 wheel local matrices, same R/T.
   - accumulator (car+0x25FC) copied into car+0x21EC (snapshot).
   - 4 ground probes (`world_bsp_locate_point`/0x4EBFC0, below the scope boundary). Miss triggers `body_wheel_contact_fallback` and sets car+0x2E20 = 1.
4. If car+0x2E20 == 1: `car_decode_id5` + `car_node_name_is` gate, and unless the touched node is the "REGEN" one, `car_substep_collision_response` (0x498960) runs. It restores the previous pose and scales the accumulator down (bounce damping, 0x498b68).
5. **Position copy-out**, 0x49d0a5-0x49d0cc: `car+0x3244 = -position.x`, `car+0x3248 = -position.y`, `car+0x324C = +position.z` (z unchanged, x/y negated, confirmed byte for byte, matches the existing port).
6. `world_car_push_apart` if race state allows (0x498800).
7. `car_gear_clamp(0)` if off valid ground or force-respawn is set (0x499050).
8. **body_step_world(wrapper, substepDt, 1)**, CALL 0x4ec560 at 0x49d135. Thunk adds 0x340, jumps to `body_world_step` (this=car+0x245C):
   - `body_spring_channel_update`
   - `body_gear_update(this, dt, ...)`: gear shift state machine, then 4 interleaved RK4 stages (wheel-spin RK4 + chassis RK4, see above). This is where **T (car+0x25F0) actually advances**, 4 times, ending with the k1+2k2+2k3+k4 combine.
9. Loop back to 1 for the remaining substeps.

Once per **tick**, after the loop (0x49d144):

10. `body_finalize_wheels(wrapper)`, CALL 0x4ec570. `body_wheel_axis_correct` rebuilds R (car+0x2608) from the reference quaternion (car+0x262C), slerped 12% toward the wheel-axis deviation. This is the only writer of R found anywhere.

## Offset table

All offsets below are relative to the wheel/contact sub object, car+0x245C (= wrapper+0x340). Where the task brief assumed wrapper-root-relative offsets (car+0x2240/0x2264/0x22B0/0x22C8), those are wrong by 0x340, corrected here.

| Offset (from car+0x245C) | Car address | Field | Written by |
|---|---|---|---|
| +0x124 | car+0x2580 | `Blk`, matrix operand B of the first mat3_multiply | not found, guess: spawn constant |
| +0x148 | car+0x25A4 | `v`, local vector added through R | not found, guess: spawn constant |
| +0x194..+0x19C | car+0x25F0 | `T`, translation, **= position source** | `body_chassis_integrate_k1..k4` every substep, `body_place_and_probe` at spawn/teleport |
| +0x1A0..+0x1A8 | car+0x25FC | velocity state, **same memory as the force accumulator** | `body_apply_force`, `body_chassis_integrate_k1..k4`, `car_substep_collision_response`, tick pre-loop jitter scale |
| +0x1AC..+0x1CF | car+0x2608 | `R`, orientation matrix, 3x3 | `body_wheel_axis_correct`, once per tick |
| +0x1D0..+0x1DC | car+0x262C | reference quaternion | `body_wheel_axis_correct` (slerp, once/tick), `body_chassis_integrate_k1..k4` (RK4 integration, every substep) |
| +0xEC | car+0x2548 | coefficient in the chassis acceleration term | not resolved, guess: mass/force-scale |
| +0x58..+0x84, +0x184..+0x200 | car+0x24B4.. | RK4 scratch (per-stage k values, drag coefficients) | `body_chassis_integrate_k1..k4` |
| +0x218/+0x224/+0x264 (per wheel, stride 0xB8) | car+0x2674.. | wheel object base/dir/scale, read by `body_update_wheel_transforms` | `body_create`/`body_load_wheel_config` at setup, guess for per-substep changes |
| wrapper+0xdc | car+0x21F8 | byproduct of the first mat3_multiply (R*Blk), no reader found | `body_update_transform` |

## Constants read (address, bytes, value, used by)

| Address | Bytes (LE) | Value | Used by |
|---|---|---|---|
| 0x3DF5C28F | immediate | ~0.12 | `body_wheel_axis_correct`, slerp factor toward the wheel-axis deviation |
| 0x5A8390 | 00 00 F0 C2 | -120.0 | `body_chassis_integrate_k1`, velocity.x low clamp |
| 0x5A6A14 | 00 00 F0 42 | 120.0 | `body_chassis_integrate_k1`, velocity.x/y high clamp |
| 0x5A6A90 | 00 00 70 C2 | -60.0 | `body_chassis_integrate_k1`, velocity.z low clamp |
| 0x5A6A68 | 00 00 70 42 | 60.0 | `body_chassis_integrate_k1`, velocity.z high clamp |
| 0x5A8388 | 1E A7 E8 48 | large (not fully decoded) | quaternion renormalize gate, low bound |
| 0x5A8380 | 71 AC 8B DB | negative bit pattern (not fully decoded) | quaternion renormalize gate, high bound, guess this pair is read as a wider type than 4 bytes |
| 0x02F26D48 | runtime | dt (full, unscaled) | `gear_rk4_step_coeffs`, shared scratch, not a car field |
| 0x02F26D4C | runtime | dt * 0.5 | `body_chassis_integrate_k1..k4`, T and quaternion integration step |
| 0x02F26D50 | runtime | dt * 0.25 | same family |
| 0x02F26D54 | runtime | dt * (1/6) | `body_chassis_integrate_k4`, final RK4 combine weight |
| 0x5EB6F4 | 9A 99 19 3F | 0.6 | tick, engine force prep before `body_apply_force` |
| 0x5A6ADC / 0x5A6AE0 | 0B D7 23 3E / 0x...BE | 0.16 / -0.16 | tick, engine force prep |
| game+0x1397384[idx] | per car, runtime | bool | `car_apply_engine_force`, gates whether it calls `body_apply_force` at all |
| game+0x1397188 | per car, runtime, 1 byte | bool | not the engine force magnitude. A one-byte flag set/cleared by `FUN_00496B10`, `FUN_00497190`, `FUN_00498030` and cleared by `car_physics_tick_local` itself at 0x49c573 (checked at 0x49c530), read 4 times by `car_respawn_state_machine`. Not part of the fx/fy/fz formula, which uses only car+0x2610/0x261C/0x2628. Guess: an engine-restart or kick latch tied to respawn/boost presentation, not a physics input. |

## Settled in the fourth pass, 2026-09-14

Every function below was disassembled at its call sites, the stack args recovered by hand. All offsets are from the wheel set car+0x245C.

### The frame and the leaves

`body_mat3_transform_vec` (0x4ED510) is M times v, each row dotted with v. `body_mat3_transform_vec_transposed` (0x4ED570) is M transposed times v. The first port had the two swapped in meaning, fixed in math_helpers. `body_vec3_transform` (0x4EE440) is M v plus T, `body_vec3_world_to_local` (0x4EE4A0) is M transposed times (w minus origin). R at +0x1ac maps the principal frame to world. FUN_004ed700 is the cross product a times b (renamed `body_vec3_cross`), FUN_004ed440 adds b times s in place (`body_vec3_madd_inplace`), FUN_004ed4c0 is a minus b (`body_vec3_sub`), FUN_004ed470 is base plus b times sb plus c times sc (`body_vec3_madd2`), FUN_004ed5d0 is v times s into out (`body_vec3_scaled`), FUN_004ed4f0 negates (`body_vec3_negate`), FUN_004ed680 normalizes (`body_vec3_normalize`, its reject gate 0x5A3C70 is a large negative float so it never rejects), FUN_004ee410 returns the quaternion norm squared (`body_quat_norm_sq`), FUN_004ed800 is the quaternion to matrix with 2 over that norm as the scale (`body_quat_to_matrix_scaled`), FUN_004ed270 and FUN_004ed240 are the transpose copy and the transpose in place, FUN_004ed100 fills a 3x3 from nine floats.

### Point 1, the wheel frame and the contact

`body_wheel_frame_transform(this, i, outHubPos, outHubVel, outAxisWorld, outSpinRate)` (0x4F1FA0, RET 0x14):
- arm = wheel.base + wheel.dir times wheel.travel (wheel+0x14, +0x20, +0x60)
- hubPos = R arm + T
- hubVel = R (spin times arm + dir times travelRate) + velocity, spin is +0x1e0, travelRate wheel+0x64
- axisWorld = R times wheel.axis (wheel+0x8)
- spinRate = wheel+0x6c

`body_wheel_contact_local(this, i, worldPoint, worldForce)` (0x4F2060, RET 0xC) folds the tyre result back:
- pointLocal = R transposed (worldPoint minus T), forceLocal = R transposed worldForce
- lever = pointLocal minus arm, torque = lever cross forceLocal
- wheel+0x74 (axle torque sum) += torque dot wheel.axis, wheel+0x70 (suspension force sum) += forceLocal dot wheel.dir
- `body_apply_local_force` (0x4F19B0, was FUN_004f19b0): +0x1f8 (torque about the origin, principal frame) += pointLocal cross forceLocal, +0x1ec (world force) += R forceLocal

The contact quantity is settled in SUSPENSION_AND_TELEPORT.md: worldPoint is the contact point of a disc against the col cell plane, worldForce is the tyre force in world.

### Point 2, the k1 fields

+0xEC is 1 / mass, written by `body_principal_axes` (0x4F0710). +0x1ec to +0x1f4 is the world force sum, +0x1f8 to +0x200 the torque sum in the principal frame. At the end of every stage the force is reset to velocity times +0x184 plus (0, 0, +0xF0) and the torque to spin times +0x188 +0x18c +0x190. +0x184 and the three angular ones are written by `body_set_pose` (0x4F0900) as minus 0 times the mass and the moments, so the drag is zero and the reset force is gravity alone. +0xF0 is minus the gravity scale times the mass, the gravity scale is the global 0x2F26D44 that `body_gravity_scale_set` (0x4EFFB0) stores at spawn (0.98) and `body_apply_durability_scale` writes every substep (19.6 times the durability curve).

The quaternion term: +0x64 to +0x6c is the spin sample of the stage, +0x24 to +0x30 the quaternion at the begin of the step (q0), and the update is q += factor times (q0 times (0, spin)) with the product in the Hamilton order q0 first, factor a quarter dt in stages 1 and 2, half dt in stage 3, a sixth dt on the weighted spin sum (k1 + 2k2 + 2k3 + k4) / 2 in stage 4. The renormalize gate is a double pair, 0x5A8388 = 0.9999 and 0x5A8380 = 1.0001, then q is scaled by (n + 1) / (2n) and n by the square of that. After it R at +0x1ac is rebuilt from q by `body_quat_to_matrix_scaled` in every stage of every substep, the "R rebuilt once a tick" statement above is wrong, `body_wheel_axis_correct` is only one more writer.

The angular acceleration is the Euler equation with the principal moments: dspin.x = (torque.x / Ix + spin.y spin.z (Iy minus Iz) / Ix) times factor, the inverses at +0x158 +0x160 +0x168 and the ratios at +0x16c +0x170 +0x174.

### Point 3, the k2 and k3 samples

`this+0x22` in the decompile is a float index, byte +0x88, the velocity sample of stage 2, copied from +0x1a0 after stage 1 advanced it by k1. +0x94 is the spin sample of stage 2, +0xb8 and +0xc4 the stage 3 samples. So the port reuses the live velocity as before, and now advances it the client way first: stage 1 v += k1, stage 2 v = v0 + k2, stage 3 v = v0 + k3, stage 4 v = v0 + (k1 + 2k2 + k3 + k4) / 3 where each k already carries its dt fraction (half, half, full, half), v0 is the clamped velocity at +0x58, the combine weight 1 / 3 is 0x5A6B2C. Stages 2 to 4 start from T0 at +0x0 and q0 at +0x24, stage 1 adds to the live values which equal them at that point.

### Point 4, the six functions around the stages

- FUN_004f2140, `body_wheel_rk4_begin`: calls `body_chassis_rk4_begin` (0x4F0A40, was FUN_004f0a40) which stores T0 q0 R0 v0 spin0 and the first force and torque reset, then per wheel copies travel, travel rate, spin angle and spin rate into wheel+0x78 to +0x84 and zeroes the two sums at +0x70 +0x74.
- FUN_004f37a0, `engine_rk4_begin(engine, throttle)`: engine+0x30 = engine+0x2c (the previous engine speed), engine+0x24 = throttle. The engine object is wheel set +0x524, its speed +0x550 is car+0x29AC, the rpm source of the tick.
- FUN_004f37b0 `engine_rk4_stage1`, FUN_004f3800 `engine_rk4_stage2`, FUN_004f3850 `engine_rk4_stage3`, FUN_004f38a0 `engine_rk4_combine`, all with the clutch load +0x980 as argument: k = ((curve(speed) times throttle minus min(dragCoef times speed, dragCap)) minus load) times invInertia times the dt fraction, curve is `curve_eval` (0x4EE5D0) on the torque table at engine+0xc. Stage 1 keeps speed0 at +0x34, stages 2 and 3 restart from it, the combine is speed0 + (k1 + 2k2 + k3 + k4) / 3.
- The steer at the top of `body_gear_update` is FUN_004f1be0, `body_steer_front_wheels(steer)`: t = tan(minus steer times max steer radians), left = t / (1 minus t times ackermann), right = t / (1 plus t times ackermann), ackermann = half track / wheelbase at +0x514, each front axis = normalize(base axis +0x4e4 or +0x4f0 plus swing +0x4fc or +0x508 times the tangent times 1.5 at 0x5A6A48), then the wheel frame matrix is rebuilt from the axis. Not presentation, the tyre model dots the spin axis with the ground normal.
- The stage wrappers `body_chassis_rk4_stage1` to `4` (0x4F2240 0x4F24D0 0x4F2760 0x4F29F0) are not passthroughs. After the chassis stage each one steps the four wheel travels and spin angles with the same rk4 pattern on the two sums: acc = invMass times suspension sum times fraction, spinAcc = invInertia times axle torque sum times fraction, samples kept at wheel+0x88 onward, the sums zeroed, and stage 4 combines with a sixth dt for the positions and 1 / 3 for the rates, then wraps the spin angle with fmod by the double 6.2831855 at 0x5A83A8.

### The substep order inside body gear update, settled

steer, shift, display gear and brake torques, `gear_rk4_step_coeffs`, `body_wheel_rk4_begin`, `engine_rk4_begin`, then four times: bank blend, `gear_wheel_force_solve`, chassis stage, engine stage, then `gear_rk4_combine`.

`gear_wheel_force_solve` (0x4EECD0): `driven_wheel_spin_rate` (0x4F1DD0, was gear_wheel_solver_dt, mean spin of the driven axle by +0x51c), clutch load +0x980 = (engine speed minus wheel spin times ratio) times clutch +0x574, the drive torque load times ratio split over the driven wheels by `gear_drive_torque_apply` (0x4F1E40, half each, a quarter each in mode 2), brakes minus clamp(spin times +0x5a0, plus minus +0x984 or +0x988) into the axle sums, springs dampers and anti roll into the suspension sums, then per wheel the frame transform, the tyre model and on load the contact fold.

### The fixed step, and the launch measured at it

game+0x30, the fixed physics step the tick divides the frame by (`ceil(frame / step)` at 0x49C503), is written once by the cars manager init `cars_manager_init` (0x4955B0, was FUN_004955b0) right after the durability curve: `[ESI+0x30] = 0x3AC49BA6` = 0.0015 s at 0x495607, and game+0x34 = `time_frame_delta()`. At 60 frames per second that is 12 substeps of 1.39 ms. The step is not a detail: the tyre slip states relax with the time constant 0.2 / speed (shared 0 and 1 over vs), an explicit RK4 on that is stable only while step times speed over 0.2 stays under 2.8, so at a 1/60 step the slip states diverge above about 33 units per second and the port goes NaN a little later (seen in the tick test at 86 km/h, the trap hit wheel 2 with both banks at the 1e6 clamp). At the client step the same run stays clean past 118 units per second.

Race 01 spawn, accelerator held from 0, 12 substeps, body layer alone, no tick force (`kartclient_body_test`): speed 3.8 at 0.4 s, 11 at 0.6, 19.5 at 0.8, 29 at 1.0, 37.5 at 1.2, 46.5 at 1.4, 54 at 1.6, 62 at 1.8, 69.5 at 2.0, 81.5 at 2.4, 98 at 3.0, engine speed 830 in gear 1 at 1.0 s, gear 2 at 1.2, 3 at 1.6, 4 at 2.2, clutch load 3500 to 7000, wheel torque 26000 in gear 1 down to 15000 in gear 4, longitudinal tyre force 45000 at 1 s, 35000 at 2 s, 26000 at 3 s, peak grip sum 72000. The recording (accelerator from 0.4 s) has 27 at 0.6, 40 at 0.8, 52 at 1.0, 60.5 at 1.2, 68 at 1.4, 75 at 1.6, 80 at 1.8, 84 at 2.0, 89.5 at 2.4, 94 at 3.0. Once rolling both gain about 43 units per second squared at 1 s, the recording keeps its lead from the first 0.4 s where the port hops: the port's hubs start 0.236 over the plane (0, 0, 1, minus 0.5434) with a 0.48 rest clearance so the tyres push the body up to 1.25 before it settles at 1.12, while the recorded z sinks from 0.98 to 0.78. The client body sees the same plane and the same clearance so it would hop the same way. The tick owner then read `car_ghost_sample_record` 0x49FAD0: the sample z is car+0x324C, the body origin, no other field. The shape of the recorded z comes from the tick's down push at 0x49CF48, indexed by the raw capped km per hour and saturating at 99, which the port now runs; the rest depth the port settles at (0.57 against 0.79 recorded) is the tyre compression under that push and stays open on the body side.

Steer, same run, left channel held from 3.0 s at 100 units per second with the default channel rate 1.0: heading 2.1 degrees at 0.1 s, 7.9 at 0.2, 12.9 at 0.3, 16.5 at 0.4, spin z 0.8 rad per second, front axis y minus 0.14, heading 0.00 on the whole straight before it. The recording turns 8.5 degrees in the first 0.2 s.

Fifth pass, 2026-09-15, the same body alone run with the two setup facts of RIGID_BODY.md fifth pass, the tyre spring override 400000 and the channel rates of `body_set_mass_friction` at 0x49510C (5 5 1 1 0.4 0.4 as read then, 5 1 0.4 0.4 1 5 on the bytes since the seventh body pass, the accel rate that this run used is the same): speed 2.1 at 0.4 s, 8.1 at 0.6, 21.3 at 0.8, 29.9 at 1.0, 37.9 at 1.2, 46.2 at 1.4, 54.4 at 1.6, 62.5 at 1.8, 70.4 at 2.0, 82.4 at 2.4, 98.9 at 3.0, the engine at 791 in gear 1 by 0.4 s. The body alone still hops at the spawn (z 1.68 at 0.2 s, all four wheels unloaded for 0.4 s) because the hubs start 0.236 over the plane with a 0.48 clearance and there is no tick push here, under the tick's two pushes the harness sinks like the recording (1.00 0.90 0.82 0.78 0.77 against 0.97 0.90 0.81 0.78 0.76) and reaches 52.7 at 1 s against 51.8. The launch lag of the second run was the accel channel ramping at 1 per second instead of 5, not the engine start, the clutch or the gear.

## Open questions

- `world_car_push_apart` (0x498800) and `car_effect_update`'s 8 read modify write pairs on car+0x25FC: both touch the velocity but were not traced past the address computation.
- What the ghost sample records as z, see the fixed step section.
- The body alone launched the Race 01 spawn to 78.7 units per second in 3 s in the fourth pass, 98.9 in the fifth with the channel rates of `car_physics_setup`, the recording is at 94 with the tick's velocity scale on top. The rest height is settled by the 400000 tyre spring, see RIGID_BODY.md fifth pass.

## Functions renamed this pass

`body_vec3_world_to_local` (0x4EE4A0, was `FUN_004ee4a0`), `body_chassis_integrate_k1..k4` (0x4F0B40/0x4F0F40/0x4F12B0/0x4F1620), `body_chassis_rk4_stage1..4` (0x4F2240/0x4F24D0/0x4F2760/0x4F29F0), `body_wheel_frame_transform` (0x4F1FA0), `body_wheel_contact_local` (0x4F2060). All renamed through `rename_function_by_address`, not dry run.
