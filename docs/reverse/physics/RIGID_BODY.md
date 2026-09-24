# Rigid body layer

Read in KnC.exe.raw, image base 0x400000, on 2026-09-14. This is the layer under `car_physics_tick_local` (0x49C0D0), the second half of docs/reverse/CLIENT_PHYSICS_MAP.md. Every address below was decompiled or disassembled directly; anything not read is marked "guess".

Correction of 2026-09-14, later, checked against the bytes: `body_step_world` adds 0x340 to this before it jumps, and `body_set_mass_friction` does the same at 0x4EC1AC before `body_set_material_pair`, so `body_world_step`, `body_spring_channel_update`, `body_gear_update` and the six material rates live on the wheel sub object car+0x245C, the rates at car+0x2A70, not the wrapper. The wrapper relative offsets attributed through that chain below are off by 0x340, car+0x2AA8 is never touched, the gear lives at car+0x2DE8. In `body_spring_channel_update` channels 0, 1, 4 and 5 decay with 4.0 at 0x5A0054, not 1.0, and `body_ramp_toward` tests target greater than 1.0 at 0x59F480, not greater than 0. `body_update_transform` also takes the wheel sub object as this, the position is R at car+0x2608 times the vector at car+0x25A4 plus T at car+0x25F0, and the accumulator at car+0x25FC is the velocity an RK4 integrator under `body_gear_update` advances T with. See BODY_MOTION.md, SUSPENSION_AND_TELEPORT.md and the port under games/kart/physics/client.

## Seventh pass, 2026-09-15, the six channel rates

The drift recording (GHOST_REFERENCE.md the drift recording, closed, TICK_HELPERS.md round eleven) sent the brake back to the bytes. `body_set_mass_friction` 0x4EC180 does not build `{m,m,f,f,s,s}`. At 0x4EC183 to 0x4EC1B6 the first argument goes to [ESP+0x14] and [ESP], the second to [ESP+0x10] and [ESP+0x04], the third to [ESP+0x08] and, after the `PUSH EAX` of the array pointer, to [ESP+0x10] which is the old [ESP+0x0C]. The local is `{m,f,s,s,f,m}` and `body_set_material_pair` 0x4EEB70 copies it in order to wheel set 0x614 to 0x628. With the setup call 5.0 1.0 car+0x32E8 the rates are accel 5.0, brake 1.0, right 0.4, left 0.4, handbrake 1.0, reverse 5.0. The port ramped the brake channel at 5 per second, ten ticks to the full 20000 and 30000 brake torques, which locks the four wheels in six ticks (the wheel inertia is 9.2 for basic 1) and a locked tyre pushes against its sliding velocity alone, so the port could not steer under the brake. With the brake at 1 per second the torque grows 2000 and 3000 a tick, the wheels roll and the fronts keep their lateral force, the brake samples of the recording read 0.86 units and 2.5 degrees against 1.76 and 11.1 before. The port builds `{a, b, c, c, b, a}` now, `tests/body_test.cpp` checks the six.

## Sixth pass, 2026-09-15, the bank slide candidates on the body side

Read for the slide down the banked back straight of GHOST_REFERENCE.md the fifth run. The slide was the harness teleport (the sixth run there), the body reads below found the port as the bytes say, plus two small divergences in `body_integrate`.

- `body_integrate` 0x4ECAB0 disassembled once more. `MOV byte ptr [ESI+0xd04],BL` with BL 0 at 0x4ECB35 clears the collision flag car+0x2E20 at the top of every integrate, before the wheel loop, and the loop `CMP byte ptr [ESI+0xd04],1` `JZ` at 0x4ECB86 leaves after the first wheel whose locate failed, the later wheels keep the cell of the previous substep. The port cleared the flag at the end of the collision response instead (nothing in the program writes 0x2E20 besides this function, the tick at 0x49D016 only reads it) and located all four wheels. Both are ported now, the flag clear at the top and the break. Each wheel locates through its own query block, `LEA ECX,[EBP-0xc]` with EBP wrapper+0x24c+i*0x40 at 0x4ECB66, and the cell copied to wrapper+0xcb0+i*4 (wheel set +0x970) is query+0xC of that block, `MOV EAX,[EBP]` `MOV [EDI],EAX` at 0x4ECB81, the port's `wheelQuery[i].cell` into `contactPlane[i]`.
- The gravity inside the body. `body_chassis_rk4_begin` 0x4F0A40 resets the force to (v.x times +0x184, v.y times +0x184, v.z times +0x184 plus +0xF0), the gravity is a scalar on world z, no rotation. +0xF0 is written by `body_principal_axes` 0x4F0710 as minus the global 0x2F26D44 times the mass and by `body_apply_durability_scale` 0x4F1A60 as minus value times +0xE8 (the mass) every tick with the tick's 19.6 times the speed curve, 0.4 units per second squared at 205 km per hour, the port has both writers and the reset.
- `body_wheel_axis_correct` 0x4EFC90 with the sign of the gate. `FLD [ESI+0x1cc]` is m 2 2 of R, the world z component of the body z axis, acos of it minus pi over 180 against the double 0.0 with `TEST AH,5` `JNP`, the body runs when the excess is at or over zero. The axis is (m 1 2, minus m 0 2, 0), `FLD [ESI+0x1b4]` `FCHS` for y and `[ESI+0x1c0]` for x, the body z axis crossed with world z, so the correction turns the body up toward world z, not toward the contact normal. The slerp order is D3DX slerp(out +0x9b4, q +0x9b4, target +0x9a4, 0.12) with target the correction times q (`PUSH EDI` q first so the correction is the first operand of `body_quat_multiply`). On the bank the body sits about 3 degrees more upright than the road under this correction (R z z 0.931 against the cell 0.909 at 17 s), which gives the body up push 3.8 units per second squared down the slope out of 22.5, in the client as in the port.
- The tyre frame on a slope, SUSPENSION_AND_TELEPORT.md the ninth round paragraph, forward is axis cross n and lateral forward cross n with n the plane of the wheel's own cell, both in the road plane, the argument order proven from the pushes.

## Fifth pass, 2026-09-15, the three body gaps of the ghost run

Read against the Race 01 ghost of GHOST_REFERENCE.md with the harness and a scratch probe on the body internals. Each item names what the client does, with the address, and what the port changed.

- The tyre spring override is 400000, not 100000. The immediate at 0x495017 and 0x494AC1 is `B8 00 50 C3 48`, `MOV EAX,0x48C35000`, and 0x48C35000 decodes to 400000.0 (100000.0 would be 0x47C35000). The fourth pass wrote the bytes right and the value wrong. `kSetupTireSpring` is 400000 now, the scratch +0x0 of every wheel with it. With that the port rests at 0.79 over the plane (0, 0, 1, minus 0.5434) with both substep pushes on, the recording reads 0.79, the old 100000 gave 0.57. The z of the launch follows the recording too, 1.00 0.90 0.82 0.78 0.77 against 0.97 0.90 0.81 0.78 0.76 at 0.2 s steps.
- The six channel rates. `body_create` writes six 1.0 through `body_set_material_pair` (0x4ECA3E to 0x4ECA6E), then `car_physics_setup` calls `body_set_mass_friction(5.0, 1.0, car+0x32E8)` at 0x49510C with `LEA ECX,[ESI+0x211C]`, the wrapper, the immediates 0x40A00000 and 0x3F800000 at 0x495101 and 0x4950FC, car+0x32E8 is the 0.4 written at 0x494F56. So the accel channel ramps at 5 per second, full 0.2 s after the key (the seventh pass above corrects the other five, the brake at 1, the steer pair at 0.4 until the drift update writes its own rate, the handbrake at 1, the reverse at 5). The port's harness and tick test never made that call, their accel channel ramped at 1 per second and the engine throttle took 1 s to reach 1, that is the whole launch lag. `kSetupChannelRateMass` 5.0 and `kSetupChannelRateFriction` 1.0 are in constants.h, the tick side calls `body_set_mass_friction(car.body, kSetupChannelRateMass, kSetupChannelRateFriction, car.steeringScale)` right after `body_create`. With the call the harness reads 3.1 17.0 31.8 41.9 52.7 61.1 68.9 75.5 79.9 83.9 units per second at 0.2 s steps against 2.2 12.5 27.3 40.4 51.8 60.5 68.4 75.0 79.6 83.6 recorded (the port value is the speed at the sample, the recorded one the mean over the sample), the horizontal gap at 3 s is 5.4 units, it was 37.
- FUN_004EC130 is `body_set_tire_grip(this = wrapper, front, rear)`: it writes wrapper+0xB50 and wrapper+0xB6C, wheel set 0x810 and 0x82C, the fifth float of each axle block, the peak scale of the tyre model (`f1 = exp(minus load times 5e-5) times this`). `car_apply_force_if_valid` (0x499000) forwards the two floats of the tick's friction pair to it every tick, and the pair is (friction of the two wheels plus twice car+0xA7988) times 0.5 times car+0x32DC or car+0x32E0, which are catalogue 0x130 and 0x134, the grip bases, not a wheelbase and a track. On asphalt with friction 1.0 the pair equals the `body_create` values 3.8 and 5.4, on grass 0.9 and in a drift (0.6 on gas, 0.8 off gas on the front pair, 8.0 slow flagged, 0.75 in the rear view) it changes the grip. The port exports `body_set_tire_grip(CarBody&, float, float)`, the tick calls it at the `car_apply_force_if_valid` call site.
- FUN_004A41F0 is `net_motion_queue_clear`, `this+0x28 = 0` on the queue object at car+0x3348 whose count is car+0x3370, the same class as `net_motion_sample_push` and `net_motion_queue_shift` (the pop at 0x49EFD9 takes this = car+0x3348 and writes the popped sample to car+0x3374). `car_ground_flag_set` (0x49A920) calls it at 0x49A942 with `LEA ECX,[ESI+0x3348]` before it clamps the gear to 1. So the side effect of the ground flag is to drop any pending remote motion sample of the car, the remote owner's mailbox, nothing in the body. The port's `car_ground_flag_set` keeps the gear write, the tick side clears `car.remote.mailbox` when it calls it with 1.
- The per wheel visual matrix of the tick (0x49D650 to 0x49DBE8, one D3DX chain per wheel into car+0x3020 plus i times 0x40) takes its translation from car+0x3528 plus i times 0xC, the position of the model node `O_WHEEL01` to `O_WHEEL04` that `car_model_wheel_nodes_read` (0x498DE0, was FUN_00498de0) fetches by name at the kart loadout (0x49125B), z raised by the bump (local minus car+0x3738). The body gives three things only: the spin angle wheel+0x68 (car+0x26C8 stride 0xB8) copied negated to car+0x32A4 plus i times 4, times 0.25 while the wheel's ground byte is 0 (0x49D503 to 0x49D584), the ground byte itself, and the steer average car+0x32E4 for the two front wheels (times 6.0 at 0x5A6A30, times 6.0 again while drifting, clamped to plus minus 0.5236 at 0x5A6AC8 0x5A6ACC), plus a fixed plus minus 0.0873 rad per side (0x3DB2B8C2). The hub points car+0x2120 copied negated to car+0x3274 feed the ground probes and `car_visual_update`, not the wheel nodes. So the tick needs no new body field, `wheel[i].spinAngle`, `wheelOnGround[i]` and `steerAverage` are exported already, the offset is model data.
- The steer response. The ghost input mask is a snapshot at the sample tick (`car_ghost_sample_record` samples the key down state every 10th tick), not a hold over the next 0.2 s. The first press of the race shows 0x90 on the sample at 2.8 s with the status turn bit already set (flags 0x12, car+0xA78E4 written by the key poll) and the yaw already 1.4, and 0x80 with flags 0x10 at 3.0 s, so the key went down before 2.8 s and up before 3.0 s. With the mask applied centred on its sample (five ticks before, five after) the port reads 0.7 7.7 9.0 7.5 degrees at 2.8 3.0 3.2 3.4 s against 1.4 8.5 8.5 7.1 recorded, inside the 1.41 degree yaw byte, with the harness convention (the mask held over the following ten ticks) it reads 0.0 3.9 9.0 8.6, the half sample lag of the second run. The body turns the same total, 9.0 against 8.5, at the same rate. The three sample left turn at 4.2 to 4.8 s turns 33.5 degrees in the port against 32.5 recorded, its recorded peak of 1.35 rad per second, the speed rise 95.8 to 102.4 and the 8.5 degree come back over the next 0.6 s belong to a mini turbo: the ghost flags carry 0x94 0x90 0x90 0x92 on the samples at 4.6 to 5.2 s, the kind 0 boost of 608 ms whose push along yaw minus the smoothed gauge times 1.8 pulls the velocity off the heading and the tyres weathervane the body back. A mini turbo needs a drift, a drift needs slot 5 down or the latch game+0xB0 (0x49AADA), and the mask never shows slot 5 (bit 0x08), so the recording's drift arming is not in the mask. That is for the tick owner, the body is settled on the clean press. Nothing in the body changed for the steer, the yaw damping of `body_wheel_axis_correct` (verified again: `CMP EDX,2` at 0x4EFE31 on the count the two `body_ramp_toward` calls leave alone, 6.0 at 0x4EFE5D), the Euler terms, the Ackermann and the tyre model read the same as the fourth pass.

## Fourth pass, 2026-09-14, the open points of the port

Read again in KnC.exe.raw with the disassembly of every call site. What changed against the text below, each item names the entry it corrects.

- The physics frame is the col frame. `body_place_and_probe` passes the hub x y of car+0x2120 straight into `world_bsp_set_piece` (0x4EBCF0) with no sign change, and `body_integrate` does the same into `world_bsp_locate_point` (0x4EBFC0). The wire position is the negated one (car+0x3244 = -position.x, car+0x3248 = -position.y). The port's world helpers take wire x y and negate inside, so the body passes -hub.x -hub.y to them. With that, `body_create` succeeds on the recorded Race 01 spawn (-321.3, 228.57, 0.98).
- `body_create` (0x4EC950) third and fourth arguments are not wheelbase and track (the tick's car+0x32DC and car+0x32E0 carry the same misnaming, they are the grip bases the friction pair scales). `car_physics_setup` copies car+0x2FD0 and car+0x2FD4 into car+0x32DC and car+0x32E0 first, those are catalogue 0x130 and 0x134, the tyre grip base front 3.8 and rear 4.8, and `body_create` stores them at wrapper+0xb50 and +0xb6c which are wheel set 0x810 and 0x82c, the fifth float of each axle's tyre parameter block. The other immediates of that function are the tyre model constants, see SUSPENSION_AND_TELEPORT.md, the tyre model settled.
- `body_box_feature_select` (0x4ED9C0) is a matrix to quaternion conversion, renamed `body_mat3_to_quat`. The weights are (trace + 1) / 4 and that minus the pair sums, the four jump table targets 0x4EDA87, 0x4EDABD, 0x4EDAF0, 0x4EDB22 are the four Shepperd branches picking w, x, y or z as the largest, then w is made positive and the quaternion normalized. It is not a box feature routine. Callers are `body_set_pose` (0x4F0900, was FUN_004f0900) and the presentation loop of `body_finalize_wheels`.
- `body_get_shape_point` (0x4ECE10) takes the col piece as this. Its +0x34 is the piece's array D (the 12 byte vertices, count at piece+0x2C). `body_wheel_contact_fallback` (0x4EC460) gets the piece from the wheel's query block at wrapper+0x240+i*0x40 offset 0x3c (wrapper+0x27c+i*0x40, the BspQuery piece field) and the two int16 vertex indices from the edge record the failed locate left at wrapper+0xd48+i*4, the edge's bytes 0x10 and 0x12 (the `reserved` field of the port's ColEdge). The normal at wrapper+0xd18 is normalize(pB.y - pA.y, pA.x - pB.x, pA.z - pB.z), the point at wrapper+0xd0c is (-hub.x, -hub.y, hub.z). So array D of a piece is read, by this fallback only.
- `body_wheel_axis_correct` (0x4EFC90) gates its whole body on acos(R[2][2]) minus one degree, the grip ramp included, nothing runs while the body tilts less than a degree. The axis is (R[1][2], -R[0][2], 0), the correction quaternion from that axis and the excess angle is multiplied with the orientation quaternion, the D3DX slerp at the delay slot 0xAFB628 (thunk 0x5059F0, stub 0x505A16) blends 0.12 of the way, then R is rebuilt. The three floats at +0x998 are not grip scales, they are the spin vector rotated to world axes (`body_mat3_transform_vec` with R), scaled by `body_ramp_toward` with 8 minus the count of unloaded wheels on x and y, and on z with 6.0 (0x40C00000) when the count is 2 or less, zeroed when it is 3 or 4, then rotated back with the transpose. The spin is what this function damps.
- `body_wheels_on_ground_count` counts the bytes that are 1, and the byte is 1 when `gear_tire_force_model` returned 0, the no load branch (`XOR EBX,EBX` at 0x4EECE2 then `MOV byte ptr [ESI+0x994],BL` on the loaded path at 0x4EF068, the immediate 1 on the other path at 0x4EF070). The count is the number of wheels without load. The tick agrees: its throttle factor loop subtracts the surface value of `world_surface_air_penalty` (0x486D70) times a quarter for every wheel whose byte is 0 (attic line 447, the loop after the surface indices), a penalty that only makes sense for a wheel in contact, and the crash recovery arms on `count > 2` (0x49C2xx), more than two wheels without load. So byte 0 is a loaded wheel, byte 1 is a wheel in the air, count 4 is airborne and count 0 is four wheels down. The name is kept. The tick port reads it the other way round, that is the tick side to flip.
- `body_wheel_state_reset` (0x4EEAD0) is called from `body_place_and_probe` at 0x4EC2F1 with the position and the yaw quaternion as arguments. It runs `body_state_reset_pose` (0x4F1A80, was FUN_004f1a80) which calls `body_set_pose` (0x4F0900) with the position, the quaternion, the zero vector at 0x2F26D10 twice and the literals 0 and 0, then zeroes the four wheel travels and spins, then `body_steer_front_wheels` (0x4F1BE0, was FUN_004f1be0) with 0, then the engine speed, the channels, the banks and the results as written below.
- `body_set_pose`: R is the quaternion matrix times the transpose of the principal axes at +0x124, the quaternion at +0x1d0 is rebuilt from R, T = pos minus R times the origin offset at +0x148 so that R times offset plus T is the spawn point again, velocity and spin are zero, and the drag coefficients at +0x184 and +0x188 to +0x190 are minus 0 times the mass and the moments, so both drags are zero. The `this+0xEC times this+0x1EC` term of BODY_MOTION.md is inverse mass times the force accumulator, and the velocity times drag part of that force is always zero.
- `body_load_wheel_config` (0x4EE830) starts with `body_geometry_setup` (0x4F2AB0, was FUN_004f2ab0): two boxes from catalogue 0x00 and 0x10 composed with `mass_props_compose` (0x4F0130) at the offset (catalogue 0x20, 0, 0x24), then `body_principal_axes` (0x4F0710) shifts to the mass centre and runs a Numerical Recipes Jacobi (`body_mat3_jacobi_eigen`, 0x4EDBF0, 50 sweeps, threshold 0x5A8360 = 0.2 / 9) on the tensor, the eigenvectors transposed are the matrix at +0x124, the eigenvalues go to +0x154 +0x15c +0x164 with their inverses and the gyroscopic ratios at +0x16c to +0x174, 1 / mass at +0xEC and minus the gravity scale times the mass at +0xF0. The wheel bases at wheel+0x14 are the axle positions of catalogue 0x40 and 0x4c (the right side has y negated) minus the mass centre, in the principal frame. The rear spin axes and their frames are built here, the front ones by `body_steer_front_wheels` every substep with the Ackermann tangents. Then `engine_init` (0x4F3750), the gear copies, `wheel_disc_curves_build` (0x4F3130) per axle, `tire_scratch_bind` (0x4F3290) per wheel, and the traction pair 0.98 times mass times 0.25 which is the bias the tyre model adds to the compression.
- The catalogue is not "kart stat derived numbers", see Catalogue below.
- +0x2F40 is the gear count of the catalogue block and the shift ratios are one array, +0x584 scaled by the final drive, the "two curves" reading of the gear state machine was wrong: down from gear g multiplies the engine speed by ratio[g-2] / ratio[g-1], up by ratio[g] / ratio[g-1], the thresholds are engine speeds, catalogue 0xc4 up and 0xc8 down.
- Wheel objects live in the wheel set at +0x204 stride 0xB8 (car+0x2660), not in the wrapper. The port's earlier +0x218 base is wheel+0x14. Layout: +0x0 width, +0x4 radius, +0x8 spin axis, +0x14 base, +0x20 travel direction, +0x2c mass, +0x30 inertia, +0x34 +0x38 inverses, +0x3c local rotation 3x3, +0x60 travel, +0x64 travel rate, +0x68 spin angle (car+0x26C8, the "steer input" of the old table, it is the wheel rotation the presentation reads), +0x6c spin rate, +0x70 suspension force sum, +0x74 axle torque sum, +0x78 to +0x84 the begin copies, +0x88 to +0xb4 the rk4 samples.
- The tick's car+0x3274 to +0x32A0 are the four hub points of car+0x2120 with x and y negated, copied at 0x49dd2a onward (`car+0x3274 = -car+0x2120`, `+0x3278 = -0x2124`, `+0x327c = +0x2128`, same for the next three), not forward right up axes. The tick's step 1 probes use them as wire x y. The per wheel visual matrix of step 16 starts from R copied off car+0x2608 and the wheel object base at car+0x2674 with a (±1, ±1, 0) sign pair per wheel, wheel 0 (1, 1), 1 (-1, 1), 2 (1, -1), 3 (-1, -1), read at 0x49de40 to 0x49de91. This is for the tick owner, the body port does not touch tick.cpp.

## Catalogue

car+0x2EA0 is the 0x140 byte block of a `.car` file plus its torque curve, nothing in it comes from the 17 stats or the vehicle kind.

- `car_physics_setup` (0x494D50) builds the path "./Data/Car/default.car" from an obfuscated char array (each byte is value / 2 minus index minus 1), extracts it through `gimmick_ini_cache_extract` into `dx8_rlg.dll` and reads it with `catalogue_load_path` (0x4EFF70, was FUN_004eff70) with ECX = car+0x2EA0 at 0x494F01. `car_apply_kart_loadout` (0x490A70) does the same at 0x490D7F with a format string and the kart record name, the per kart file, and falls back to default.car at 0x490E98. Every `Data/Car/*.car` is 416 bytes, the files differ per kart (basic_1 has axle 1.34 / 1.008, dampers 6000, tyre spring 500000, rear grip 5.4).
- `catalogue_read_file` (0x4EFF20): fread 0x140 bytes into the block, then `curve_read_file` (0x4EE6E0) on the block's curve at 0xd8: 16 bytes count, xmin, xmax, scale, then count floats, the table pointer at 0xdc is replaced by the allocation.
- After the load, `car_physics_setup` at 0x495009 to 0x49501A and the loadout at 0x494AB5 to 0x494AC6 write car+0x2FA8 = car+0x2FAC = 6000.0 (0x45BB8000, the dampers) and car+0x2FC8 = car+0x2FCC = 400000.0 (0x48C35000, the tyre springs, read as 100000 until the fifth pass). Then car+0x32DC = car+0x2FD0 and car+0x32E0 = car+0x2FD4 (the tyre grip bases).
- The port: `catalogue_load_car_file` reads a `.car`, `catalogue_apply_setup_overrides` writes the four floats, `catalogue_from_stats(stats, kind, out)` gives default.car with the overrides and ignores both inputs by evidence, the defaults of `SpawnCatalogue` are default.car byte for byte (constants.h, CATALOGUE section, file offsets).

| Offset | car | Field | default.car |
|---|---|---|---|
| 0x00 0x0c | 0x2EA0 | chassis box full extents and mass | 4.0 2.2 0.8, 200 |
| 0x10 0x1c | 0x2EB0 | lower box extents and mass | 1.4 0.8 0.2, 800 |
| 0x20 0x24 | 0x2EC0 | lower box offset x and z | 0.056, -0.54 |
| 0x28 0x30 0x38 | 0x2EC8 | wheel radius width mass, front then rear | 0.48, 0.4, 80 |
| 0x40 0x4c | 0x2EE0 | axle positions front and rear, x forward y half track z | 1.26 1.208 -0.2, -1.26 1.208 -0.2 |
| 0x58 0x64 | 0x2EF8 | axle spin axes, a toe of 0.0349 on x | 0.0349 1 0, -0.0349 1 0 |
| 0x70 0x7c 0x88 | 0x2F10 | travel directions and the steer up | 0 0 1 |
| 0x94 | 0x2F34 | max steer degrees | 16 |
| 0x98 | 0x2F38 | drive mode 0 front 1 rear 2 both | 2 |
| 0x9c | 0x2F3C | clutch | 20 |
| 0xa0 | 0x2F40 | gear count | 6 |
| 0xa4 | 0x2F44 | gear ratios | 3.38 2.05 1.43 1.09 0.87 0.7 |
| 0xbc 0xc0 | 0x2F5C | final drive, reverse ratio | 2.2, -8 |
| 0xc4 0xc8 | 0x2F64 | upshift and downshift engine speed | 837.76, 481.71 |
| 0xcc 0xd0 0xd4 | 0x2F6C | engine drag cap, drag coefficient, inertia | 80, 80, 4 |
| 0xd8 | 0x2F78 | torque curve count pointer xmin xmax scale | 20, 0, 1005.31, 0.0189 |
| 0xec | 0x2F8C | brake coefficient on the spin rate | 3000 |
| 0xf0 0xf4 0xf8 | 0x2F90 | brake torque front, rear, handbrake rear | 20000, 30000, 60000 |
| 0xfc | 0x2F9C | display gear zero threshold on the brake channels | 0.9 |
| 0x100 0x104 | 0x2FA0 | springs front rear | 80000 |
| 0x108 0x10c | 0x2FA8 | dampers front rear, setup writes 6000 | 6800 |
| 0x110 0x114 | 0x2FB0 | anti roll front rear | 6800 |
| 0x118 0x120 | 0x2FB8 | disc curve count and edge power per axle | 20, 16 |
| 0x128 | 0x2FC8 | tyre spring per axle, setup writes 400000 | 800000 |
| 0x130 0x138 | 0x2FD0 | tyre grip base and aux per axle | 3.8 4.8, 3.8 4.8 |
| 0x140 | file only | torque curve count xmin xmax scale then 20 floats | 12269 up to 14000 down to 1.35 |

## The three objects

The car record has stride 0xA7260 (`car_base = car_array + index*0xA7260`). The rigid body layer is not one struct, it is three, all embedded inside the car record:

- **wrapper**, `car+0x211C`. Created once by `body_create` (0x4EC950). This is "the body", every per-substep call (`body_ground_probe_response`, `body_apply_force`, `body_integrate`, `body_step_world`, `body_finalize_wheels`) takes this address as `this`. Proven directly: `car_physics_tick_local` does `LEA ECX,[EBX+0x211c]` / `LEA EDI,[EBX+EBP*1+0x211c]` right before each of these calls (EBX/EBP resolve to `car_base`, confirmed against the decompiler's own `iVar14 = param_2*0xA7260+param_1`). Internal offsets line up exactly with the fields CLIENT_PHYSICS_MAP.md already named from the tick side: wrapper+4 = car+0x2120 (doc: rotation axes), wrapper+0xc4 = car+0x21E0 (doc: position), wrapper+0xd04 = car+0x2E20 (doc: collision flag).
- **wheel/contact sub-object**, `wrapper+0x340` = `car+0x245C`. Populated at setup by `body_load_wheel_config` (0x4EE830) from the catalogue, read every tick by `body_wheel_axis_correct` and `body_wheels_on_ground_count`. Proven directly: `body_finalize_wheels` does `LEA ECX,[ESI+0x340]` before calling `body_wheel_axis_correct`, and `car_physics_tick_local` does `LEA ECX,[EBX+EBP*1+0x245c]` before calling `body_wheels_on_ground_count`, its internal `+0x994..+0x997` then lands exactly on car+0x2DF0..0x2DF3, the doc's wheel-on-ground flags.
- **catalogue / spawn config**, `car+0x2EA0`. A small block of kart-stat-derived numbers (car_physics_setup writes car+0x2FA8, 0x2FAC, 0x2FC8, 0x2FCC into it directly). `body_create` receives its address as a plain stack argument and hands it to `body_load_wheel_config`, which copies/converts it into the wheel sub-object. It is read-only input, not part of the live body.

`body_create`'s own incoming `this` (ECX) could not be read directly at its call site in `car_physics_setup` (ECX is clobbered by an intervening call to the placement-validity check `FUN_00486300`). The `car+0x211C` value is inferred instead from internal consistency: `body_create` computes `EDI = ESI+0x340` and passes it to `body_load_wheel_config`, exactly the same `+0x340` sub-object `body_finalize_wheels` and `car_physics_tick_local` independently use from a confirmed `car+0x211C` base. Treat "body_create's this = car+0x211C" as strongly evidenced but indirect.

## What the body model is

Not a general 6DOF rigid body with an inertia tensor. It is a box chassis positioned every substep from four independent wheel/corner ground probes (raycasts), each spring-damped ("suspension compression"), plus one force accumulator that is a plain vector add (no torque, no angular momentum integration seen). `body_box_feature_select` (0x4ED9C0) picks 1 of 4 box-corner/edge feature handlers from a barycentric-style weight over three box half-extents, this is a closest-feature routine for a box shape, not a mesh or capsule. `body_wheel_contact_fallback` (0x4EC460) falls back to the *last known* box feature pair when a wheel's raycast misses this substep, which only makes sense against a small fixed-topology shape like a box. Orientation (`body_quat_from_axis_angle`, `body_quat_to_matrix`, `body_quat_multiply`) is quaternion-based and gets converted to a 3x3 matrix once per finalize step, then combined with position via `body_update_transform`. Net verdict: a lightweight custom "4-corner raycast vehicle", not a licensed physics engine body, matches the doc's own informal framing.

## Functions, in call order

### 0x4EC180 `body_set_mass_friction`
`void(float mass, float friction, float steerScale)`. No visible `this` argument in its own body; it spreads the 3 floats into a 6-float local `{m,f,s,s,f,m}` (corrected in the seventh pass above, read as `{m,m,f,f,s,s}` before) and calls `body_set_material_pair` on it. Called once from car_physics_setup as `body_set_mass_friction(5.0, 1.0, car[+0x32E8])` right after `body_place_and_probe` returns, with ECX still holding the wrapper pointer from that prior call chain, so the 6 floats land at wrapper+0x614 = car+0x2730 (see `body_set_material_pair`). 5.0 and 1.0 are literal immediates (0x40a00000, 0x3f800000); the third value is car+0x32E8, the doc's "steering scale, 0.4 at setup".

### 0x4EEB70 `body_set_material_pair`
`void __thiscall(this, float in[6])`. Copies `in[0..5]` into `this+0x614..0x628` = car+0x2730..0x2744. This 6-float array doubles as the per-channel decay/ramp *rate* read by `body_spring_channel_update` (see below), `{mass,mass,friction,friction,steerScale,steerScale}` become the six channel rates.

### 0x4EC1D0 `body_ground_probe_response`
`void __thiscall(this=wrapper, int callback)`. Stores `callback` at `this+0xd44` = car+0x2E60. If non-null, loops 4 times over `this+4` (stride 0xC, i.e. the rotation-axis/axle array at car+0x2120) calling `FUN_004ebcf0(callback, entry.x, entry.y)`, a ground/track contact query below the scope boundary (0x4EBCF0 < 0x4EC000, not renamed). Called from `car_physics_tick_local` once per tick (0x49C186), with `callback` = car+0x360C's value, right after a 4-probe pre-check via `FUN_00486570`.

### 0x4EC420 `body_apply_force`
`void __thiscall(this=wrapper, float fx, float fy, float fz)`. Confirmed by direct disassembly (decompile dropped the args): builds `force = (fx,fy,fz)` on the stack with `body_vec3_set`, then calls `body_vec3_add(this+0x4E0, &force)`, i.e. `this+0x4E0 += force`. `this+0x4E0` = car+0x25FC, a 3-float force accumulator not previously documented. Called once per substep from the tick with fx/fy/fz built from car+0x2610 (doc: engine force base), car+0x261C and car+0x2628, scaled by the constants at 0x5EB6F4 (0.6) and 0x5A6ADC (0.16).

### 0x4ED3D0 `body_vec3_set` / 0x4ED3F0 `body_vec3_add` / 0x4ED410 `body_vec3_madd` / 0x4ED600 `body_vec3_scale`
Plain `__thiscall` vec3 helpers, no state: `set(v,x,y,z)`, `add(a,b): a+=b`, `madd(out,base,dir,scale): out=base+dir*scale`, `scale(v,s): v*=s`. Leaves, no callees.

### 0x4EE440 `body_vec3_transform`
`void __thiscall(out, v, mat3x3, translate)`: `out = mat*v + translate` (row-major 3x3). Leaf.

### 0x4ECAB0 `body_integrate`
`void __fastcall(this=wrapper)`. Runs once per substep, after `body_apply_force`:
- `body_update_transform(this+0xc4)`, `this+0xc4` = car+0x21E0, the doc's rigid-body position. This is the single strongest confirmation of the wrapper base.
- `body_update_wheel_transforms(this+4)`, and copies `this+0x4e0..0x4e8` (the force accumulator, car+0x25FC) into `this+0xd0..0xd8` = car+0x21EC..0x21F4 (an accel/force cache next to position, not named in the doc).
- If `this+0xd44` (contact callback, car+0x2E60) is non-zero: loops the 4 wheels. `puVar2` walks `this+4` (rotation axis array, stride 0xC), `puVar5` walks `this+0xcb0` = car+0x2DCC (stride 4, a 4-entry result-pointer array). For each wheel: `FUN_004ebfc0(axis.x, axis.y, puVar5+0x26, this+0xd08)`, the raycast itself (0x4EBFC0, below scope, not renamed); `puVar5+0x26` lands on `this+0xd48` = car+0x2E64, a per-wheel result-pointer slot. On raycast failure (`cVar1==0`): calls `body_wheel_contact_fallback(wheel_index)` and sets `this+0xd04 = 1`, car+0x2E20, exactly the doc's "collision happened this substep" flag. Then `*puVar5 = *puVar4` copies a value from `this+0x24c` (car+0x2368, stride 0x40) into the result-pointer slot.

### 0x4EC460 `body_wheel_contact_fallback`
`void __thiscall(this=wrapper, int wheelIndex)`. Runs only when a wheel's raycast misses. Reads two 16-bit feature indices out of the *previous* raycast result (`*(this+0xd48+i*4)` then `+0x10`/`+0x12` as int16, via `body_get_shape_point`), writes the reconstructed contact point/edge into globals `_DAT_02f26cf8..02f26d0c` (a shared scratch, not car-relative, plausibly a debug/last-contact display value) and into `this+0xd0c..0xd20` = car+0x2E28..0x2E3C. This is the box-shape closest-feature fallback described above.

### 0x4ECE10 `body_get_shape_point`
`void __thiscall(this, out vec3, int index)`: `*(int*)(this+0x34)` is a pointer to a box-feature/vertex table; copies the 3 floats at `table[index*0xC]` into `out`. `this+0x34` is the same field `body_integrate`/`body_place_and_probe` pass to `body_update_wheel_transforms` (car+0x2150), a pointer field, not inline data (guess: points at the box's 4 axle/corner definitions built at spawn).

### 0x4ED9C0 `body_box_feature_select`
`void(vec3 halfExtents)`. Computes `t = (extents.x+extents.y+extents.z+1)*0.25` (0x5A32D4 = 0.25) then three more candidate sums, compares them, and jumps through a 4-entry function table at `PTR_LAB_004edbdc` to pick one of 4 box-feature handlers (face/edge/vertex cases). Ghidra could not recover the jump table's targets ("too many branches"); none of the 4 targets were traced, marked as **guess** beyond "this is a box closest-feature routine".

### 0x4F1B10 `body_update_transform`
`void __fastcall(this)`. Internally (own-relative offsets, not wrapper-relative, this is called with `this=car+0x21E0`, i.e. `wrapper+0xc4`): `body_mat3_multiply(this+0x1ac, this+0x124, this+0x148[implicit third operand not shown])`, then `body_vec3_transform(this+0x148, this+0x1ac, this+0x194)`. Reads/writes a small transform block starting right after position (car+0x238C, 0x2304, 0x2328, 0x2374 by the same arithmetic), a matrix compose, not a `pos += vel*dt` integrator. No evidence of velocity integration was found anywhere in this call chain; position/orientation for the substep look kinematic, driven by the wheel raycasts.

### 0x4F1B50 `body_update_wheel_transforms`
`void __fastcall(this)`. Same matrix-compose pattern as `body_update_transform`, looped 4 times with `iVar1 = this+0x224`, stride 0xB8 per wheel (matches the doc's wheel-object stride). Calls `body_vec3_madd`, `body_vec3_transform`, `body_mat3_multiply` per wheel. Called from `body_integrate` as `body_update_wheel_transforms(this+4)` (car+0x2120, the rotation axis array) and from `body_place_and_probe`/`body_create` the same way.

### 0x4EC950 `body_create`
`bool(this=wrapper[inferred], catalogue*, float wheelbase, float track, float negPosX, float negPosY, float posZ, float negYaw, int handle)`. Confirmed by disassembling the single caller (car_physics_setup, 0x4950df, `RET 0x20` = 8 stack dwords):
```
body_create(car+0x2EA0, car[0x32DC], car[0x32E0], -car[0x3244], -car[0x3248], car[0x324C], -car[0x3220], car[0x360C])
```
Body:
- `body_load_wheel_config(this+0x340, catalogue=car+0x2EA0, 0.98)`, `this+0x340` = car+0x245C.
- Writes hard-coded spring/damper tuning floats into `this+0xb40..0xb94` = car+0x2C5C..0x2CB0 (not renamed as a struct, just a flat block of constants; see the byte values in `body_create`'s own disassembly, e.g. `0x47a7dbd5`, `0x3851b717`, `0x3e4ccccd`, `0x40c00000`, `0x41a00000`).
- `*(this+0xb50) = track`, `*(this+0xb6c) = handle` (a second, earlier cache of `handle` distinct from `this+0xd44`).
- `*(this+0xd44) = handle`, car+0x2E60, same field `body_ground_probe_response` sets every tick.
- `body_place_and_probe(-posX,-posY,posZ,-yaw)` (this passthrough via ECX=ESI).

### 0x4EE830 `body_load_wheel_config`
`int __thiscall(this=car+0x245C, catalogue=car+0x2EA0, float gripScale)`. Reads catalogue fields (+0x98,+0x9c,+0xc4,+0xc8, +0xa0 count, +0xa4[] array scaled by +0xbc, +0xc0, +0xec..+0x114) and writes the wheel sub-object's own copies at `this+0x570..0x5c8` (car+0x29CC..0x2A24). Loops twice building two curve-breakpoint entries at `this+0x788+i*0x14` (car+0x2BE4+) via `FUN_004f3130` (0x4F3130, above the 0x4F2000 scope boundary, not traced). Finishes with `fVar1 = gripScale * this[0xe8] * 0.25` (0x5A32D4) stored twice at `this+0x858`/`0x85c` (car+0x2CB4/0x2CB8), read as a symmetric front/rear (or left/right) traction limit, and zeroes `this+0x998,0x99c,0x9a0` (car+0x2DF4/0x2DF8/0x2DFC), the same fields `body_wheel_axis_correct` updates every tick. Returns 0x14 (error) on any sub-call failure, 0 on success.

### 0x4EC290 `body_place_and_probe`
`bool __thiscall(this=wrapper, float negPosX, float negPosY, float posZ, float negYaw)`. Builds a position vec3 and an "up" vec3(0,0,1) with `body_vec3_set`, a quaternion from `negYaw*0.017453` (0x5A1E88 = pi/180) with `body_quat_from_axis_angle`, converts it with `body_quat_to_matrix` (called `body_quat_multiply`/`eead0` too per the decompile, the full spawn-orientation chain), then calls `body_update_wheel_transforms(this+4, this+0x34)` twice (car+0x2120, car+0x2150). If `this+0xd44` (car+0x2E60, the contact callback set moments earlier by `body_create`) is non-zero, loops the same 4-wheel-axis probe as `body_ground_probe_response`, calling `FUN_004ebcf0`. Returns false if any of the 4 probes fails, this is the spawn-time "drop onto the track" validation.

### 0x4EEAD0 `body_wheel_state_reset`
`void __thiscall(this, a, b)`. Calls `FUN_004f1a80(a,b)` (0x4F1A80, in-scope but not decompiled, not renamed) and `FUN_004f1be0(0)` (0x4F1BE0, same), then zeroes `this+0x550`, `this+0x5cc..0x5e0` (car+0x26E8..0x26FC, the spring-channel state `body_spring_channel_update` maintains), `this+0x8b0..0x91c` (car+0x29CC..0x2A38, part of the gear/rpm state block), and 0x14 dwords starting at `this+0x860` = car+0x297C, the mean front steer tangent (once called yaw rate here) is the very first dword zeroed here. Caller not identified (not seen in any traced call site); presumably part of `body_create`'s chain, called through `body_place_and_probe`'s own callees per the decompile ("`FUN_004eead0`" appears there) though the exact edge was not confirmed by disassembly, **guess**.

### 0x4EC560 `body_step_world`
`void __thiscall(this=wrapper)`. One line: calls `body_world_step()` with the same `this` (fastcall passthrough, no reassignment). Called once per substep from the tick, after `body_integrate`.

### 0x4EFE90 `body_world_step`
`void __thiscall(this=wrapper, dt, float in[6], flag)`. Copies `in[0..5]` into `this+0x5fc..0x610` = car+0x2718..0x272C, calls `body_spring_channel_update(dt)` (this passthrough), then `body_gear_update(dt, this[0x5d4]-this[0x5d8], this[0x5cc], this[0x5d0], this[0x5dc], this[0x5e0], flag)`.

### 0x4EEBB0 `body_spring_channel_update`
`void __thiscall(this=wrapper, float dt)`. Caches the previous values of a 6-float channel array `this+0x5cc..0x5e0` (car+0x26E8..0x26FC) into `this+0x5e4..0x5f8` (car+0x2700..0x2714), then for each of the 6 channels `i`:
- reads `channel[i+0xc]` (= `this+0x5fc+i*4`, the fresh per-substep input `body_world_step` just copied in) as an "active" flag, and `channel[i+0x12]` (= `this+0x614+i*4` = car+0x2730+i*4, the mass/friction/steerScale rate set by `body_set_material_pair`) as that channel's rate.
- if inactive: decays the channel value toward 0 by `rate*dt*constant`, with channels 2 and 3 using a different decay constant (0x5A6A9C = 16.0 or 0x5A0054 = 4.0 depending on index) than channels 0,1,4,5 (0x59F480 = 1.0), and clamps to 0 past a floor that for channels 2/3 is `rate*0.04` (0x5A3EB8).
- if active: ramps the channel up by `dt*rate`, clamped to 1.0, with a floor of `rate*0.04` for channels 2/3.

Given the rates are literally {mass, mass, friction, friction, steerScale, steerScale} and channels 2/3 (the "friction" pair) get distinct floor/decay handling, this reads as a per-axle (front/rear) or per-side grip/load ramp rather than a literal spring, **guess** on the exact physical meaning; the data flow (offsets, rates, clamps) is directly read.

### 0x4EFA90 `body_gear_update`
`void __thiscall(this=wrapper, dt, deltaThrottle, in0, in1, in2, in3, gearFlag)`. A gear-ratio/state machine (`this+0x98c` = car+0x2AA8 holds the current gear, -1 = neutral) that computes a gear-change ratio from `this+0x57c/0x584` arrays (the wheel sub-object's curve breakpoints, i.e. this function reaches back into `car+0x245C+...` through the pointer chain) and writes `this+0x984/0x988` (car+0x2AA0/0x2AA4). It then calls a chain of 12 further functions (`FUN_004effc0`, `FUN_004ef3e0`, `FUN_004eecd0` x4, `FUN_004ef450`, `FUN_004ef550`, `FUN_004ef650`, `FUN_004ef7c0`, plus several above the 0x4F2000 scope boundary) that were not traced, the repeating `curve-eval, then eecd0` pattern strongly suggests a per-gear RPM/pitch curve for the engine sound, i.e. presentation rather than force generation. Left unexpanded; **not fully covered**, noted rather than guessed at.

### 0x4EC570 `body_finalize_wheels`
`void __fastcall(this=wrapper)`. Runs once per tick, after the substep loop closes (not per substep). Confirmed by direct disassembly (`LEA ECX,[ESI+0x340]; CALL body_wheel_axis_correct` at the very first instruction, `ESI=ECX` at entry):
- `body_wheel_axis_correct(this+0x340)`, car+0x245C.
- `this+0xd58..0xd70` (car+0x2E74..0x2E8C) = sign-flipped copy of `this+0x510..0x51c` (car+0x262C..0x2638).
- Loop 4 times over `this+0x140` (car+0x225C, stride 0x40, a per-wheel local matrix block not named in the doc) using per-wheel angle data from `this+0x5ac` (car+0x26C8, stride 0xB8), **this is exactly the doc's "+0x26C8 +0x2780 +0x2838 +0x28F0 wheel steer inputs"**, confirmed to the byte. Builds each wheel's local rotation via the D3DX thunks at 0x504649/0x504A8F/0x503C3B (the doc's matrix thunks).
- RPM ratio: `t = clamp(this[0xd24] * this[0x890], this[0xd28], this[0xd2c])`. `this+0x890` = car+0x29AC, **the doc's rpm source**, confirmed to the byte. Result stored at `this+0xd78` = car+0x2E94.
- Wheel-load ratio: `sum(this[0xbac],this[0xbc0],this[0xbd4],this[0xbe8]) * this[0xd38]`, clamped 0..1 against 0x5A83F8/0x59F480, stored at `this+0xd7c` = car+0x2E98.
- Final lerp: `this+0xd80 = lerp(this[0xd30], this[0xd34], t)`, car+0x2E9C.

### 0x4EFC90 `body_wheel_axis_correct`
`void __thiscall(this=car+0x245C)`. Computes an angle via `_CIacos()` between the wheel's current axis (`this+0x1c0`,`this+0x1b4`) and its reference axis (`this+0x1d0..0x1dc`, a quaternion); if the deviation exceeds 1 degree (0x5A83E8 = 0.0 threshold on `acos_result - 0x5A1E88(pi/180)`), slerps the reference quaternion toward the current one by a fixed 0x3DF5C28F (~0.12) factor via the D3DX thunk at 0x5059F0, and re-derives the rotation matrix (`body_ed740`-equivalent) and translation (`this+0x1ac`, `car+0x2618`). Then recomputes wheel grip scale: reads `this+0x994..0x997` (car+0x2DF0..0x2DF3, the doc's wheel-on-ground flags, byte-identical field to `body_wheels_on_ground_count`'s own reads) into a 0-4 count, computes `f = 8.0(0x5A15F0) - count`, and scales `this+0x998` and `this+0x99c` (car+0x2DF4/0x2DF8) by `body_ramp_toward(value, f)`. If the ground-wheel count is 3 or less it also scales `this+0x9a0` (car+0x2DFC) by `body_ramp_toward(value, 4.0)`, otherwise zeroes it. Called once per tick from `body_finalize_wheels`, not per substep.

### 0x4EF370 `body_ramp_toward`
`float10(float value, float target)`. `absVal = abs(value)` (compared against 0.0 at 0x59F44C); if `absVal < target` and `target > 0`: returns `(1.0/target) * (target - absVal)`, else returns 0.0. A generic normalized-falloff/ease helper, used above to turn "how many wheels are off the ground" into a grip multiplier.

### 0x4EFA50 `body_wheels_on_ground_count`
`char __fastcall(this=car+0x245C)`. `count = (this[0x994]==1) + (this[0x995]==1) + (this[0x996]==1) + (this[0x997]==1)`, i.e. car+0x2DF0..0x2DF3, the doc's wheel-on-ground flags, confirmed by direct disassembly of a call site (`LEA ECX,[EBX+EBP*1+0x245c]` immediately before the call, in `car_physics_tick_local` at 0x49c1ef). Leaf, called 5+ times per tick by the tick itself (crash-recovery gating, airborne detection) and once internally (inlined the same 4-flag pattern) by `body_wheel_axis_correct`.

### 0x4F1A60 `body_apply_durability_scale`
`void __thiscall(this=car+0x245C, float value)`. `_DAT_02f26d44 = value` (shared scratch, not car-relative); `this+0xf0 = -(value * this[0xe8])`, car+0x254C = -(value * car+0x2544, the traction-limit input `body_load_wheel_config` also reads). Called once per substep from the tick's durability section: `car_physics_tick_local` reads `game+0x1396FE0[clampedIndex] * 19.6 (0x5A6A10)` from the 100-entry durability curve and passes it in, this is how kart durability scales down wheel grip.

### Vector/matrix/quaternion leaves
- `body_mat3_multiply` (0x4ED150): 3x3 * 3x3.
- `body_mat3_transform_vec` (0x4ED510) / `body_mat3_transform_vec_transposed` (0x4ED570): `vec * mat3` in the two row/column conventions.
- `body_quat_to_matrix` (0x4ED740): standard quaternion-to-3x3-matrix (`1 - 2(y²+z²)` etc., using 0x59F480 = 1.0).
- `body_quat_from_axis_angle` (0x4ED8E0): half-angle (`angle*0.5`, 0x59F414) sin/cos into a quaternion, after normalizing the axis via `body_vec3_normalize_to` 0x4ED620 (was FUN_004ed620, renamed in the eighth tick round, out and in pointers, returns 0x14 and leaves the output alone when the squared length is under the gate 0x5A3C70, a negative bit pattern that never fires).
- `body_quat_multiply` (0x4ED930): standard quaternion product.

None of these carry car-record state; they are pure math, listed for completeness since they are direct callees inside the 0x4EC000-0x4F2000 range.

## Not fully traced (left as open questions)

- `FUN_004ebcf0` and `FUN_004ebfc0` (the actual ground/track raycast) sit just below the 0x4EC000 boundary, out of the assigned scope, not renamed, not decompiled here.
- `FUN_004f3130`, `FUN_004f3290`, `FUN_004f3750`, `FUN_004f2ab0` (all called from `body_load_wheel_config`) sit above the 0x4F2000 boundary, not traced.
- `body_gear_update`'s 12-function curve-evaluation subtree (`FUN_004effc0`, `FUN_004ef3e0`, `FUN_004eecd0`, `FUN_004ef450`, `FUN_004ef550`, `FUN_004ef650`, `FUN_004ef7c0`, plus more above 0x4F2000) was identified but not expanded, it reads as engine-sound RPM/pitch, not force generation, so it was deprioritized.
- `FUN_004f1a80`, `FUN_004f1be0` (called by `body_wheel_state_reset`) were seen but not decompiled.
- `body_vec3_normalize_to` 0x4ED620 (axis normalize, called by `body_quat_from_axis_angle`) read in the eighth tick round, see TICK_HELPERS.md.
- The exact caller of `body_wheel_state_reset` was not confirmed by disassembly.
- `body_box_feature_select`'s 4-entry jump table (0x4EDBDC) could not be resolved by the decompiler ("too many branches"); the 4 feature handlers themselves are unidentified.
- Body-internal constants at wrapper+0xb40..0xb94 (spring/damper tuning written by `body_create`) were read as raw hex but not decoded to decimal or assigned physical units, see the raw bytes in that function's disassembly.

## Offset table

All offsets below were read directly (decompile or disassembly), not inferred from the doc alone; where they match a CLIENT_PHYSICS_MAP.md field, that is noted.

### Wrapper root, `car+0x211C`

| Wrapper offset | Car offset | Field | Doc match |
|---|---|---|---|
| +0x004..+0x034 | 0x2120..0x2150 | rotation/axle axis array, 4x3 floats | yes, "+0x2120 to +0x2148" |
| +0x0c4 | 0x21E0 | rigid body position | yes, "+0x21E0..+0x21E8" |
| +0x0d0..+0x0d8 | 0x21EC..0x21F4 | force/accel cache (copy of +0x4E0) | no |
| +0x140 (stride 0x40, x4) | 0x225C.. | per-wheel local matrix block | no |
| +0x24c (stride 0x40) | 0x2368 | per-wheel raycast source array | no |
| +0x340 | 0x245C | wheel/contact sub-object (see below) | no (new) |
| +0x4e0 | 0x25FC | force accumulator, 3 floats | no |
| +0x510..+0x51c | 0x262C..0x2638 | reference forward/up vector (quat-ish, 4 floats) | no |
| +0x558 (stride 0xB8, x4) | 0x2674 | wheel object array | yes, "+0x2674 stride 0xB8" |
| +0x5ac (within each wheel obj) | 0x26C8/0x2780/0x2838/0x28F0 | per-wheel steer input | yes, exact |
| +0x5cc..+0x5e0 | 0x26E8..0x26FC | 6-channel ramp/decay state | no |
| +0x5fc..+0x610 | 0x2718..0x272C | latest per-substep 6-float input | no |
| +0x614..+0x628 | 0x2730..0x2744 | mass/friction/steerScale pair cache | no |
| +0x860 | 0x297C | mean front steer tangent, the tick copies it to car+0x32E4 at 0x49D4CD, not a yaw rate | yes, exact |
| +0x890 | 0x29AC | engine rpm source | yes, exact |
| +0x984/+0x988 | 0x2AA0/0x2AA4 | gear-update outputs | no |
| +0x98c/+0x990 | 0x2AA8/0x2AAC | current gear / display gear | no |
| +0xbac/+0xbc0/+0xbd4/+0xbe8 (stride 0x14) | 0x2CC8/0x2CDC/0x2CF0/0x2D04 | per-wheel load sum inputs | no |
| +0xcb0..+0xcbc | 0x2DCC..0x2DD8 | per-wheel raycast result pointers | no |
| +0xd04 | 0x2E20 | collision happened this substep | yes, exact |
| +0xd08 | 0x2E24 | raycast query param | no |
| +0xd18..+0xd20 | 0x2E34..0x2E3C | fallback contact normal | no |
| +0xd24/+0xd28/+0xd2c | 0x2E40/0x2E44/0x2E48 | rpm-ratio clamp range | no |
| +0xd30/+0xd34 | 0x2E4C/0x2E50 | lerp endpoints | no |
| +0xd38 | 0x2E54 | wheel-load scale | no |
| +0xd3c/+0xd40 | 0x2E58/0x2E5C | tolerance band | no |
| +0xd44 | 0x2E60 | contact/track query callback | no (new) |
| +0xd48 (stride 4, x4) | 0x2E64.. | per-wheel result-pointer slot | no |
| +0xd58..+0xd70 | 0x2E74..0x2E8C | sign-flipped copy of +0x510..0x51c | no |
| +0xd78/+0xd7c/+0xd80 | 0x2E94/0x2E98/0x2E9C | rpm ratio / load ratio / final lerp output | no |

### Wheel/contact sub-object, `car+0x245C`

| Sub-object offset | Car offset | Field | Doc match |
|---|---|---|---|
| +0x98/+0x9c | 0x24F4/0x24F8 | catalogue-copied geometry | no |
| +0xa0 | 0x24FC | element count | no |
| +0xa4.. | 0x2500.. | per-element scaled array | no |
| +0xbc/+0xc0/+0xc4/+0xc8 | 0x2518/0x251C/0x2520/0x2524 | catalogue fields | no |
| +0xcc/+0xd0/+0xd4 | 0x2528/0x252C/0x2530 | catalogue vec3 | no |
| +0xe8 | 0x2544 | traction-limit input (also used by body_apply_durability_scale) | no |
| +0xec..+0x118 | 0x2548..0x2574 | curve breakpoints / 2-entry source array | no |
| +0x570..+0x57c | 0x29CC..0x29D8 | copied catalogue geometry | no |
| +0x580 | 0x29DC | count | no |
| +0x584.. | 0x29E0.. | scaled array | no |
| +0x788 (stride 0x14, x2) | 0x2BE4.. | gear curve breakpoints | no |
| +0x858/+0x85c | 0x2CB4/0x2CB8 | traction limit pair | no |
| +0x994..+0x997 | 0x2DF0..0x2DF3 | wheel on ground flags (bytes) | yes, exact |
| +0x998/+0x99c/+0x9a0 | 0x2DF4/0x2DF8/0x2DFC | grip scale outputs | no |
| +0x9a4..+0x9c0 | 0x2E00..0x2E1C | axis-drift correction quaternion scratch | no |

## Constants read (address, hex bytes LE, decoded float)

| Address | Bytes (LE) | Value | Used by |
|---|---|---|---|
| 0x59F414 | 00 00 00 3F | 0.5 | body_finalize_wheels (per-wheel half-track), body_quat_from_axis_angle (half-angle) |
| 0x59F480 | 00 00 80 3F | 1.0 | body_quat_to_matrix, body_finalize_wheels clamp, body_spring_channel_update decay |
| 0x59F44C | 00 00 00 00 | 0.0 | body_ramp_toward zero threshold |
| 0x59F404 | 00 00 20 41 | 10.0 | tick-level airborne speed gate (reads body_wheels_on_ground_count) |
| 0x5A83F8 | 00 00 00 00 | 0.0 | body_finalize_wheels load-ratio clamp |
| 0x5A1648 | 00 00 C8 42 | 100.0 | body_finalize_wheels tolerance band |
| 0x5A32D4 | 00 00 80 3E | 0.25 | body_load_wheel_config traction limit, body_box_feature_select weight |
| 0x5A1E88 | 35 FA 8E 3C | 0.017453 (pi/180) | body_place_and_probe (deg->rad), body_wheel_axis_correct (1 degree threshold) |
| 0x5A83E8 | 00 00 00 00 | 0.0 | body_wheel_axis_correct angle-deviation threshold, body_gear_update throttle check |
| 0x5A2494 | CD CC CC 3D | 0.1 | body_load_wheel_config per-element scale |
| 0x5EB6F0 | CD CC CC 3E | 0.4 | body_finalize_wheels engine-force prep (same value as car+0x32E8 steering scale) |
| 0x5EB6F4 | 9A 99 19 3F | 0.6 | tick substep engine-force prep, multiplied with body_apply_force's fz |
| 0x5A6AD8 | AC C5 27 37 | ~1e-5 | tick, scales suspension compression (car+0x2AD4) right after body_finalize_wheels |
| 0x5A6ADC | 0B D7 23 3E | 0.16 | tick substep engine-force prep |
| 0x5A6AE0 | 0B D7 23 BE | -0.16 | tick substep engine-force prep (symmetric pair with 0x5A6ADC) |
| 0x5A15F0 | 00 00 00 41 | 8.0 | body_wheel_axis_correct grip-scale base (8 - wheelsOnGround) |
| 0x5A6A10 | CD CC 9C 41 | 19.6 | tick durability section, scales durability_curve[i] before body_apply_durability_scale |
| 0x5A323C | 00 00 B4 42 | 90.0 | tick, yaw-rate wrap near body_apply_durability_scale call |
| 0x5A3EB8 | 0A D7 23 3D | 0.04 | body_spring_channel_update floor clamp for channels 2/3 |
| 0x5A0054 | 00 00 80 40 | 4.0 | body_spring_channel_update decay constant (index-dependent) |
| 0x5A6A9C | 00 00 80 41 | 16.0 | body_spring_channel_update decay constant (index-dependent) |

Mass and friction (5.0, 1.0) and the 0.98 grip-scale passed into `body_load_wheel_config`, and the 0.4 steering scale, are instruction immediates (0x40A00000, 0x3F800000, 0x3F7AE148, 0x3ECCCCD), not memory constants, no `read_memory` needed, decoded directly from the disassembly.
