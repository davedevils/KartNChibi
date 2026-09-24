# Client physics map

Read in KnC.exe.raw, image base 0x400000, on 2026-09-14. This is the ground for the 1:1 port of the kart physics. Names in brackets are the ones given in the Ghidra project, the raw address is the truth.

The detail lives in `docs/reverse/physics/`, one file per layer

| File | Covers |
|---|---|
| `physics/CONSTANTS.md` | Every float constant the tick reads, the surface table, the durability curve |
| `physics/RIGID_BODY.md` | The body layer 0x4EC180 to 0x4F1B50, three objects inside the car record, offsets, the four wheel probe model |
| `physics/WORLD_COLLISION.md` | The .col file, the XZ BSP, surface names, friction and grip, on track check, car push apart |
| `physics/TICK_HELPERS.md` | Every helper the tick and the drift update call, boosts, effects, collision response, crash recovery, math and time |
| `physics/GHOST_REFERENCE.md` | the real client measured on a ghost recording, launch curve, steering response, input mask, the harness that compares the port to it |
| `physics/REMOTE_AND_INPUT.md` | The remote car mover, the 0x40 motion mailbox, key bindings, ghost replay and record |
| `physics/GROUND_AND_RESPAWN.md` | The height plane in the collision cell, the nine surface names, the respawn flags and counters, regen zones |
| `physics/INPUT_AND_STATS.md` | The input flag writer, key bindings, the 17 stats from S2C 0xC0, part bonuses, the 0xC3 track record, local 7a0, vehicle kinds |
| `physics/EFFECTS_AND_GEAR.md` | Effect codes 100 to 1100, the gimmick csv files and their hits, the RK4 gear and tyre model behind the rpm |
| `physics/SUSPENSION_AND_TELEPORT.md` | The suspension compression write, the wheel on ground bytes, the gear index, the respawn teleport and its checkpoint lists |
| `physics/BODY_MOTION.md` | How the car moves, the velocity at 0x25FC, the RK4 chassis integrator inside the gear update, the transform block, the once per tick orientation |
| `CLIENT_CAMERA.md` | The chase camera 0x43F040 on the row tail floats 14 15 16, the field of view by speed, the driver seat 0x48B800, the wheel and body matrices, the record tail and the stat numbering |

## Where the tick runs

| Address | Name given | Role |
|---|---|---|
| `FUN 00495330` | cars frame update | Loops the 30 car slots, stride 0xA7260. The local car, index at game+0x6B0, goes to the physics tick. Every other car goes to the remote update. Per car it also runs animation, effects and the pilot path when +0x132 of the car is set |
| `FUN 0049C0D0` | car physics tick local | The whole local simulation for one frame, 1596 lines decompiled. Detailed below |
| `FUN 0049AA90` | car drift update | Drift and mini turbo state machine, called once per tick |
| `FUN 0049ED90` | car remote update | Moves a remote car from its motion samples, no physics. See REMOTE_AND_INPUT.md |
| `FUN 00494D50` | car physics setup | Spawns a car. Zeroes the 17 bonus floats at +0xA7940, loads the body, builds the rigid body, then runs one tick. Reads `dx8_rlg.dll` next to the exe through `FUN 004EFF70` first, the file is the staging copy of the last csv extracted from the packed store, the shipped stub is empty |
| `FUN 00495C30` | car effect apply | Item and gimmick effects on a car, codes 100 to 1100, sets +0x36A8 and refuses to stack |
| `FUN 00496BE0` | car boost start | Starts a boost, second argument is the kind, 0 mini turbo, 1 to 7 items and pads, 6 cancels |
| `FUN 0049BEB0` | motion send | Builds and sends C2S 0x40 from the car state, stages 9 and 0xB only |
| `FUN 004EFA50` | body wheels on ground count | Returns how many of the four wheels are in the air, byte 1 at +0x2DF0, round five |

## The car record

One car is 0xA7260 bytes, base is game object plus index times stride. Offsets used by the tick, in the order the tick touches them. RIGID_BODY.md holds the body internals between +0x211C and +0x2E9C.

| Offset | Meaning |
|---|---|
| +0x0018 +0x001C +0x0020 +0x0024 +0x002C | input flags, accel, brake, steer right (slot 3), steer left (slot 2), stuck, read as ints, written by the device handlers under car input poll dispatch, the sense is proved in round five |
| +0x0030 | fixed physics step, seconds |
| +0x0034 | frame time, seconds. Substeps are ceil of frame time over fixed step |
| +0x06B0 | index of the local car |
| +0x06BC | track progress value, 0 while driving, the respawn phase once the wrong way watchdog sets it to 100, then 0x65 reposition, 0x66 fade in, `world on track check` forces the brake for 100 to 299 |
| +0x06F8 | random jitter factor on the throttle, reseeded every tick |
| +0x0740 | slot occupied byte, +0x743 active byte, +0x744 player or ghost id |
| +0x09D8 +0x09D9 | over valid ground flag, force respawn flag |
| +0x09E0 | count of attached scene nodes, stride 0x9C from +0x0A80 |
| +0x211C | rigid body wrapper, see RIGID_BODY.md |
| +0x2120 to +0x2148 | rotation axes of the rigid body, copied with sign flips into +0x3274 to +0x329C |
| +0x21E0 +0x21E4 +0x21E8 | rigid body position, x and y are negated when copied to +0x3244 +0x3248 +0x324C |
| +0x245C | wheel and contact sub object, see RIGID_BODY.md |
| +0x25FC | velocity, three floats, `body apply force` adds to it, the chassis RK4 integrates it, see BODY_MOTION.md |
| +0x2608 | the body R 3 by 3, +0x2610 is its element 0 2, the substep push follows column 2 |
| +0x2674 stride 0xB8 | the four wheel objects |
| +0x26C8 +0x2780 +0x2838 +0x28F0 | wheel steer inputs per wheel |
| +0x2974 +0x2A78 +0x2A7C | steering gains written by the drift update |
| +0x297C | wheel set +0x520, the mean front steer tangent of `body steer front wheels`, copied to +0x32E4 at 0x49D4CD |
| +0x29AC | engine rpm source, scaled into +0x32F8 and clamped 500 to 10000 |
| +0x2AD4 +0x2B20 +0x2B6C +0x2BB8 | per wheel suspension compression, written by `gear tire force model` 0x4F32E0 |
| +0x2DE8 | current gear, -1 reverse, shifted by `car gear shift up` 0x4990A0 and `car gear shift down` 0x4990D0, forced to 0 or 1 by the ground validity through `car gear clamp` 0x499050 and `car ground flag set` 0x49A920 |
| +0x2F40 | max gear |
| +0x2DF0 to +0x2DF3 | per wheel bytes, 0 loaded, 1 in the air, written by `gear wheel force solve` 0x4EECD0 |
| +0x2E20 | a collision happened this substep |
| +0x2E34 +0x2E38 | collision normal used by the substep collision response |
| +0x2E60 | ground query callback, set at spawn, set again every tick |
| +0x2EA0 | spawn catalogue block, read only, feeds the wheel config |
| +0x2FE0 | car world matrix, 4x4 floats |
| +0x3018 | accumulated body roll |
| +0x3020 stride 0x40 | the four wheel matrices |
| +0x3220 +0x3224 +0x3228 | yaw, pitch, roll in degrees |
| +0x322C | yaw velocity |
| +0x3230 | yaw of the previous substep, restored on collision |
| +0x3234 | speed, units per second |
| +0x3238 +0x323C +0x3240 | velocity |
| +0x3244 +0x3248 +0x324C | position |
| +0x3250 to +0x3258 | acceleration |
| +0x325C to +0x3270 | previous position and velocity, restored on collision |
| +0x3274 stride 0xC | the four wheel probe points, body +0x2120 with x and y negated |
| +0x32A4 stride 4 | per wheel steer angle |
| +0x32DC +0x32E0 | wheel base and track, set at setup |
| +0x32E4 | copy of +0x297C, the mean front steer tangent, the airborne lateral push reads its sign |
| +0x32E8 | steering scale, 0.4 at setup, also the third body material rate |
| +0x32F4 | speed in km per hour, +0x3234 times the constant at 0x5A69A8, zero when negative |
| +0x32F8 | rpm |
| +0x3300 +0x3304 +0x3308 +0x330C +0x3310 | boost state 0 to 3, kind, decay strength, target km per hour, start time |
| +0x332D | reverse or brake flag, one byte |
| +0x3334 +0x3338 | lap checkpoint counter and bearing to the next checkpoint |
| +0x3374 to +0x3398 | motion sample mailbox of a remote car, see REMOTE_AND_INPUT.md |
| +0x33B4 | vehicle kind, 2 and 5 take other steering and spin paths |
| +0x3440 to +0x3480 | the 17 base stats of the kart, the 0xC0 catalogue float block |
| +0x3520 | part slot type of the car, feeds the sound calls |
| +0x35A4 | drift state, 0 none, 1 left, 2 right |
| +0x35A8 | drift gauge, signed |
| +0x35AC | smoothed drift gauge, feeds steering and wheel spin |
| +0x35B0 | car transform built each tick, 16 floats |
| +0x35F0 | mini turbo stage, 0 to 3 |
| +0x35F8 | mini turbo timestamp, 64 bit |
| +0x3600 | gauge threshold count |
| +0x3608 | BSP query context of the wheel probes |
| +0x36A8 | active effect code, 0 when none |
| +0x36AC | steering loss or wobble while spun, thundered or hived |
| +0x36C8 to +0x36D8 | effect 300 snapshot, height and saved pose |
| +0x36DC | lean and wobble state 0 to 9 of the effect lean update |
| +0x3715 | draft active flag, a car ahead in the cone |
| +0x3716 +0x3718 +0x371C | air baseline armed flag, air time scale, peak height |
| +0x3720 | slow flag |
| +0x3728 stride 4 | per wheel grip |
| +0x3738 | body pitch offset |
| +0x373C +0x3740 | landed flag and timestamp |
| +0x3754 stride 0x1C | ghost sample ring, 2400 samples, index at +0xA7858 write and +0xA785C read |
| +0xA7854 | 1 finished, 2 spectating, the tick returns early on 2 |
| +0xA7878 to +0xA7880 | position cached by the wrong way watchdog, the query key of the respawn point |
| +0xA78E4 | turn state 0 to 2, cosmetic lean |
| +0xA7938 to +0xA797C | the 17 bonus floats added to the base stats, zeroed at setup, filled by parts, index 2 at +0xA7940 |
| +0xA7984 to +0xA7998 | pet and spin extras, +0xA7994 spin period, +0xA7998 spin accumulator |

Stat index to use in the tick, base plus bonus every time

| Index | Base | Bonus | Used for |
|---|---|---|---|
| 3 | +0x344C | +0xA7944 | max speed, plus one, clamped, times the constant at 0x5EB6FC |
| 4 | +0x3450 | +0xA7948 | steering gain |
| 5 | +0x3454 | +0xA794C | mini turbo target speed, 120 to 144 km per hour |
| 7 | +0x345C | +0xA7954 | turn force |
| 8 | +0x3460 | +0xA7958 | wheel spin |
| 9 | +0x3464 | +0xA795C | wheel steer angle |
| 10 | +0x3468 | +0xA7960 | drift charge rate |
| 11 | +0x346C | +0xA7964 | drift steer |
| 12 | +0x3470 | +0xA7968 | mini turbo threshold |
| 13 | +0x3474 | +0xA796C | mini turbo hold time |
| 14 | +0x3478 | +0xA7970 | grip |

## What one tick does, in order

1. Four ground probes around the car, `world probe wheel ground` 0x486570, then `body ground probe response` 0x4EC1D0
2. Crash recovery timer on game+0x13972F4, states 1 2 3 with 800 to 2001, 1000, 200 and 1000 ms windows, `car boost start` kind 0 when the key is held in the last window. TICK_HELPERS.md
3. Start boost at the green, race state 4 or 5 and one of three keys down
4. Finished or spectating returns early after the remote update and the anim, `car ghost sample record` when finished, `car ghost sample apply` and `car visual update` when spectating
5. Input poll `car input poll dispatch` 0x497E40, gated in race state 0xD by the twelve counters 0xBFC3B0 to 0xBFDA1C. The device handlers write the input flags
6. `world on track check` 0x4A0750 clears the input flags and forces the brake when +0x6BC is inside 100 to 299, the respawn sequence, it returns 1 outside
7. Substep count, ceil of frame time over the fixed step
8. Airborne flag. With four wheels in the air and no drift, the tick arms +0x3716 +0x3718 +0x371C unless a wheel sits on a REGE or LAVA tagged surface, `world decode surface tag bytes` 0x487280 and `car name match count` 0x4A1380. Landing effect and sound on the drop from the peak once fewer than two wheels are in the air
9. Per wheel surface `world wheel surface index` 0x4A1310, friction 0x486D40, grip 0x486DA0, contact drag 0x486D70 (was air penalty, charged per loaded wheel), surface table at 0x5EB718 indexed by `world zone table index` 0x4B4B50. WORLD_COLLISION.md
10. Drift update `car drift update` 0x49AA90
11. Throttle factor: max speed cap, turn force, draft factor `car draft factor` 0x4998F0 which is slipstream behind a car ahead not a drift bonus, boost states, slow flag, no input decel, random jitter
12. Speed curve at game+0x1396FE0, 100 entries, two indices, km per hour minus 30 times a half for the gravity scale `body apply durability scale` 0x4F1A60, the raw km per hour for the down push term, skipped while an effect runs, TICK_HELPERS.md round five
13. Substep loop: `car apply engine force` 0x4968F0 and `body apply force` 0x4EC420, `body integrate` 0x4ECAB0, collision gate `car decode id5` 0x49BE20 with `car node name is`, `car substep collision response` 0x498960 restores the pose and pushes a bounce impulse, position copied from the body with x and y negated, `world car push apart` 0x498800 separates two cars, `car gear clamp` 0x499050 counts substeps over valid ground, `body step world` 0x4EC560 with the substep time
14. Suspension and wheel spin, stuck detection `world stuck probe` 0x498590 with a 100 ms window, `body finalize wheels` 0x4EC570 once per tick
15. Car matrix from position and angles, D3DX matrix calls through the thunks at 0x504521, 0x5045B5, 0x504649, 0x5046EB, 0x50478E, 0x504A59, 0x503C3B
16. Wheel matrices with steer, spin and a random bump when a wheel is off the ground
17. Velocity, acceleration, speed and km per hour, axes copied from the body
18. Yaw, pitch and roll from the unit square corners through the body R at +0x2608, `math atan2 deg` 0x44E240, `math wrap angle signed180` 0x44DD00
19. Motion send `motion send 0x40` 0x49BEB0 in stages 9 and 0xB
20. Scene node transforms `car node set transform` 0x444720 for the body, the driver and the parts
21. Camera or shake sums at +0x3320, then `car boost update` 0x496E50, `car respawn state machine` 0x4A1420, `car mission rally update` 0x4A3BD0, `FUN 0046F7A0`, `car boost speed gate` 0x49A390, `car stuck surface check` 0x49A130, `car effect lean update` 0x49B3D0
22. Vehicle kind 5 spins its extra part

## Drift update, FUN 0049AA90

- Keys through `input key binding lookup` 0x45AF30 and `input key down` 0x44B580, slot 2 tags 0x25 left and slot 3 tags 0x27 right, `car is camera reversed` 0x4BD8D0 swaps them when the camera is reversed
- Gauge rate is stat 10 plus bonus, times a constant, clamped 0.3 to 0.8, quartered while `car is slowed` 0x4C31D0 says the car is slowed
- State 0 to 1 or 2 when a stick is held and speed is above the constant at 0x5A3298
- Gauge grows at the rate, capped at 0x5EB700, decays back at 0x5A6AA0 when the stick is released, state drops to 0 through `car drift state set` 0x49A9E0 when the gauge crosses zero, which also fires the mini turbo boost when the threshold count passed 1
- Mini turbo: stage 1 when the gauge passes stat 12 times 0x5EB704 and the hold time passed stat 13 times 0x5EB708, stage 2 on release, stage 3 when the accelerator is pressed within 0x5EB70C ms, then `car boost start` kind 0. Pet key 0x14 gives a 3 percent chance of kind 1 instead, that is Chai
- Effects 500 600 900 cancel the drift
- Steering gains at +0x2974 +0x2A78 +0x2A7C from stat 4 and the gauge, drift steer from stat 11 into `body vec3 set` 0x4ED3D0, the yaw snapped through `car body set yaw` 0x49A970
- Stuck flag at game+0x1397174 turns the car with the gauge

## Rigid body layer

Not a six degree rigid body. A box chassis placed every substep from four wheel probes, one force accumulator with no torque, quaternion orientation converted once per finalize. `body create` 0x4EC950 at setup, `body set mass friction` 0x4EC180 with 5.0, 1.0 and the steering scale, the six values become the decay rates of the six spring channels. Full offsets and every function in RIGID_BODY.md.

## World collision

A track is `track.col` plus up to eight `trackN.col` pieces under `Data/Public/World/<map>/<track>/`. Each piece is a 2D XZ BSP over triangles, 56 byte cells with three shared half plane edges, two child links and the surface name embedded at cell+0x16. The name maps to nine surface indices, friction, grip and air penalty come from the 9 row table at 0x5EA880. WORLD_COLLISION.md has the loader, the walk and the tables.

## Constants

Every value in CONSTANTS.md, read by address. The contiguous block 0x5EB6F4 to 0x5EB714 is filled at track init from the license record. Durability curve at game+0x1396FE0, 100 floats, 1 minus sin of i times pi over 200. Surface table at 0x5EB718, 30 floats.

## Settled in round two

- Ground height is the plane in the first 16 bytes of the collision cell, y = -(A x + B z + D), `world locate piece by height` 0x4857A0. The nine surface names are DUST, ASPHALT, WATER, GRASS, SNOW, ICE, BOARD, WATER_REG, GRASS_L, REGEN is an alias of DUST
- The 17 base stats come from S2C 0xC0 through `stat catalog store` 0x44F510 and `car apply kart loadout` 0x490A70, the bonuses from `stat bonus add part` 0x48F710. The `Data/Car/*.car` files are not read by the exe
- The license record is the S2C 0xC3 track record, its floats at +0x30 +0x34 +0x38 fill 0x5EB6F0 0x5EB6F4 0x5EB6F8, our server sends them from `track_catalog`
- Input flags are one shared block written by `input poll keyboard` 0x497AB0, the pad path synthesises key downs into the same array
- `dx8_rlg.dll` is a staging file, every gimmick csv and regen.ini is extracted from the packed store into it and read back, the stub ships empty
- Effect codes, gimmick files and hits, the RK4 gear model that only feeds the rpm and the wheel spin, in EFFECTS_AND_GEAR.md
- local_7a0 is a reused stack slot, the float pair is the per wheel friction and grip sum, vehicle kinds 2 and 5 in INPUT_AND_STATS.md

## Settled in round three

- The suspension compression is written per wheel by `gear tire force model` 0x4F32E0 under `gear wheel force solve` 0x4EECD0, which also writes the wheel on ground bytes. `body step world` 0x4EC560 adds 0x340 to this before the jump, so the whole gear subtree runs on the wheel sub object car+0x245C
- +0x2DE8 is the gear index, not a fall counter, +0x2F40 the max gear
- The respawn: `respawn crash recovery update` 0x4A0970 watches the progress along the nearest checkpoint polyline, two windows of 2000 ms without progress set +0x6BC to 100, then `car respawn state machine` 0x4A1420 fades out, looks the cached position up in the baked grid at 0x1AE2224 through the checkpoint lists at 0x1ADF810, writes the position plus 3.0 in height and the yaw, calls `body place and probe` 0x4EC290, fades in
- The two 12 byte arrays of a collision piece are loaded, freed and never read
- How the car moves, BODY_MOTION.md: `body update transform` 0x4F1B10 runs on the wheel sub object, position is R times v plus T, the accumulator at 0x25FC is the velocity, `body chassis integrate k1` to `k4` under `body gear update` advance T with a k1 2k2 2k3 k4 combine, R is rebuilt once per tick by `body wheel axis correct`

## Settled in round four

- Axes. Car +0x3244 is x, +0x3248 is y, both ground axes, +0x324C is the height. `world probe wheel ground` 0x486570 feeds the BSP with the first two floats of each probe point at +0x3274, `car respawn state machine` calls `body place and probe` with minus +0x3244, minus +0x3248, +0x324C plus 3, minus yaw at 0x4A343D to 0x4A346C, the air peak at +0x371C takes +0x324C. The tick never calls `world locate piece by height` 0x4857A0, the port's height snap is a stand in
- 0x1ADF810 is the world object, the `this` of `world track init`. Its +0x2A04 holds four counts and +0x2A14 the four point lists, stride 0x1900, 0x10 per point. `gimmick load follow` 0x489730 fills them from `follow_01.ini` to `follow_04.ini`, up to 400 rows of `%f,%f,%f,%f` as x, y, z, yaw. The grid at 0x1AE2224 is 0x1ADF810 plus 0x2A14, the same array, the respawn teleports to a follow row
- The wheel byte at +0x2DF0 is 1 when `gear tire force model` returned 0 (no load, the wheel is in the air) and 0 when it took the `body wheel contact local` fold (loaded), `body wheels on ground count` 0x4EFA50 counts the ones, the wheels in the air. The round four reading of the tick side, 0 as air, was wrong, round five
- The two 12 byte arrays of a collision piece hold floats. Piece +0x34 is the mesh vertex list, the edge record ends index it. Piece +0x18 holds one point per named zone in file order, START then CHECK_001 to CHECK_016 on Cookie 01, in the negated frame the BSP queries use. Nothing in the exe reads either array
- `FUN 0059029C` is the CRT float to int truncation, named `crt ftol trunc`. The tick feeds it the capped km per hour minus 30 times a half, the collision response feeds it the impact heading folded to 0 90 over 90 times 100. The 100 float table at game+0x1396FE0 is a speed curve and an impact angle curve, not a durability tier
- The 17 bonus floats start at +0xA7938, index 3 sits at +0xA7944 as the stat table says, +0xA7940 is index 2
- Car +0x2610 is not an engine force, it is element 0 2 of the 3 by 3 at +0x2608, the body R. Each substep the tick pushes the world z by the tuning at 0x5EB6F0 times minus 0.16 and pushes minus the third column of R scaled by one minus the speed curve times 0x5EB6F4 times 0.16. The throttle factor of step 11 scales the velocity at +0x25FC once a tick. The drive itself comes from the gear solver under `body step world`, TICK_HELPERS.md round four
- Step 18 needs no wheel heights. The four corners of the unit square go through R at +0x2608, the yaw follows their heading by a quarter, pitch and roll follow the front rear and side height differences by a sixteenth
- The ghost ring records every 10 ticks and wraps at 2400, in race mode 0xD every tick and 24000, the memory holds 24000 entries. The input mask bit order is in REMOTE_AND_INPUT.md
- `geometry.nif` is loaded into the same scene holder as `track.nif` through the second load method, the holder answers height queries for dropped items, FILE_FORMATS.md

## Settled in round five

The recorded Race 01 ghost drives the port, GHOST_REFERENCE.md the second run, TICK_HELPERS.md round five, REMOTE_AND_INPUT.md and INPUT_AND_STATS.md round five.

- Steer sense. Slot 3 lands on game+0x20 and channel 2, slot 2 on game+0x24 and channel 3, the steer is channel 2 minus channel 3, the swing of `body geometry setup` 0x4F2AB0 is up cross base at 0x4F2E50 so a positive channel 2 turns the car toward the wheel 1 side and the wire yaw grows. Game+0x20 is the right turn, the drift update tags it VK RIGHT, the recording turns the yaw up on it. The port body had the cross reversed, one line
- The steer channels ramp by dt times the rate car+0x2A78 0x2A7C (0.76 for basic 1) while held and decay 16 times the rate per second once up, the drift update writes them every tick, the port matched
- The ground byte is 0 loaded and 1 in the air, the count is the wheels in the air, every tick read listed in TICK_HELPERS.md round five, the surface value of 0x486D70 is a contact drag per loaded wheel, renamed `world surface contact drag`
- The ghost sample position is car+0x3244 0x3248 0x324C as three dwords, the recorded z is the body origin car+0x21E8. The ftol inputs of the yaw byte and the two nibbles are settled, TICK_HELPERS.md
- The down push term of step 12 indexes the speed curve by the raw km per hour, not minus 30 times a half, so it saturates at 99 km per hour, the recorded z settles by 0.8 s the same way
- car+0x32E4 is the mean front steer tangent copied from car+0x297C, not a yaw rate
- `world on track check` forces the brake for +0x6BC in 100 to 299, the respawn sequence, 0 is free driving
- `car apply engine force` runs only with game+0x1397384 set, the same flag as the key poll
- With the basic_1 catalogue and its grips the port follows the recorded first turn within 6 degrees to 4.4 s, one 0.2 s press turns 9.0 against 8.5 recorded, the launch and the z rest depth stay with the body

## Settled in round six

The marker sweep of the tick side, every `TODO open` taken to the bytes, TICK_HELPERS.md round six and the round six sections of EFFECTS_AND_GEAR.md, INPUT_AND_STATS.md, REMOTE_AND_INPUT.md, WORLD_COLLISION.md, GROUND_AND_RESPAWN.md, GHOST_REFERENCE.md the third run.

- The launch. `car physics setup` calls `body set mass friction(5, 1, car+0x32E8)` at 0x49510C right after `body create`, the six channel rates, the harness and the tick test make the call now and the port sits on the recorded launch, 52.7 at 1 s and 83.9 at 2 s against 51.8 and 83.6, the gap at 3 s is 5 units
- `car boost push` 0x496B40 goes through `math dir from heading pitch` 0x44DD50, D3DX rotation z then rotation axis, the result is (sin h cos p, cos h cos p, sin p) with h the heading minus 90, the forward of the wire yaw pitched by car+0x3224, z is a tenth of the strength. The port had the push sideways
- The refusal gate of `car boost start` is `world theme is special row` alone, no camera flag exists in that function. The "part catalogue" bonuses are the equipped pet, `pet owned record lookup` 0x4504E0 on the container 0x1A69708 and `pet key to kind` 0x451490, kind 0x15 adds 0.04 decay, 0x16 adds 500 ms, 0x14 rolls the 3 percent Chai chance
- `car boost clear` 0x496930 is the reset helper of every round, one line, car+0x3300 to 0
- The drift `body vec3 add` this is car+0x263C, the spin of the wheel set, a z spin per tick. Car+0x6BC is 0 while driving, 100 to 201 in the respawn sequence, minus 1 while a wheel sits on a boost pad cell. A cell named FLY under wheel 0 disarms the drift
- `car node name is` 0x4A1350 compares the col cell name under a wheel, FUN_004A0680 is `world wheel query surface`, the collision gate reads the cell under wheel 2, the boost pad scan the four cells
- The effects. 200 ends on at most one wheel in the air (`car wheels in air count` 0x48D730), 300 counts down car+0x36AC and reads the elapsed before the add, 400 ends on the hive table 0x2F077F0 state 0x69 (`effect hive hit lookup` 0x4CF020), 600 and 900 end on `gimmick pool slot lookup` 0x4B9FE0 of the pools 0x2EFC818 and 0x2ECDA38, 0x44C ends on DAT_00D6E1D0, 700 and 1000 scale every tick, every apply clears the boost
- The carry pool `gimmick pool update` 0x4C7ED0 grabs the car with 600, carries it 3000 ms (6000 in licence test 1, plus 1000 remote, plus 2000 with ability 3) along the follow polyline, drops it on the next follow row plus 5 and kicks it 40 along its yaw through `car push along yaw` 0x497160, then a release boost on the slot 0 key inside 200 to 1000 ms. The 40 is a push strength
- The boost pad detector is `car respawn state machine` state 0, a BOOST NNN cell under a wheel takes row NNN minus 1 of boost.ini, kind 3 is a launch pad through `car launch pad kick` 0x497190 (yaw, push, up kick times 1.6 and its decay, boost kind 2), the other kinds `car boost start`, car+0x6BC holds minus 1 until the wheels leave the cell. The engine kick of game+0x1397188 is that launch
- The crash recovery key is the raw slot 0 key. The green light keys 1 2 3 at 0x49C3B0 are debug boosts of kind 0 1 2, not the accel flag. The draft factor literals are 0x5EB710 150 and 0x5EB714 30
- DAT_01A20658 is the local player id. `car stuck surface check` is `car rival nearby cue` 0x49A130, a voice cue every 3 s when a racing car (`car nearest racing car` 0x499AA0, finish rank car+0x3724 negative) sits inside 30 in the 70 to 290 bearing window. `world zone table index` is `standings local row index` 0x4B4B50 on the standings table 0x2EBD6A0, the 30 floats at 0x5EB718 are a throttle factor per row
- The licence test 0x17 counts the collisions at 0x498DB2, DAT_00BFDAC4 is the test id and DAT_00BFDABC the count
- `input key pressed` is indexed by raw key code. The brake key on the race path (car+0x9D4 set) brakes over 1.0 speed and 1000 rpm with the gear mirror not negative and reverses through game+0x2C otherwise. The pad axis is (x minus 0x8000) over 32768 with plus minus 0.2 steer thresholds. Device 1 and 3 never write the six flags, proved at 0x497E40 and by the absence of any game+0x98 reader
- FUN_004A41F0 is `net motion queue clear`, the count car+0x3370 to 0, `car ground flag set` calls it too. DAT_00B23154 and 0xB23168 are a dead knob with no writer. The remote ground snap needs the probe, `world ground height at` 0x485970 with the C coefficient live, the PUSH fan and the wall slide are ported. The status decode gate is car+0x3598 against DAT_005C8320, both 0. The gauge decode adds one unit. The watchdog threshold is the S2C 0x6A delay through `net remote watchdog arm` 0x49E8A0. Car+0xA78B8 has no writer so recovery state 2 lasts one tick. The coast sample and the 0x40 send encode the yaw byte and the two nibbles as the ghost recorder does
- The footprint of `world probe wheel ground` is car+0x3274 (game base 0x1B19090). `world car push apart` 0x498800 with `world car find overlap` 0x497F80 and `world car overlap test` 0x497EE0, the self velocity times 0.994 plus 0.8 dx 0.8 dy dz, the other nudge 0.06, ported in `world car push apart tick`
- The wheel matrices take the model node positions car+0x3528, the spin car+0x32A4 from the body spin angle, the front steer from car+0x32E4 times 6, the bump and the pitch offset car+0x3738 from the tyre peaks
- `car apply force if valid` 0x499000 is `body set tire grip` 0x4EC130 with the friction pair every tick, car+0x32DC 0x32E0 are the catalogue grips 0x130 0x134
- The regen rows have no reader, the edge 0x10 holds the two end vertex indices, the start ini rows are x y z heading, TICK_HELPERS.md and WORLD_COLLISION.md round six

## Settled in round seven

The turn after the boost pad, GHOST_REFERENCE.md the fourth run and TICK_HELPERS.md round seven.

- No drift in the RageZone recording, the flags never carry 0x100 or 0x200, the gauge nibble is 7 on all 665 samples and slot 5 is never in the mask, so `car drift update` ran with state 0 and its unwinding block took the zero gauge branch every tick, which resets the state and never turns the yaw. The six constants of the block read as the port has them
- The kind 0 boost of 4.4 to 5.2 s is the boost.ini kind 0 pad BOOST_001, the port's own pad scan finds it at 4.52 s once the harness loads boost.ini, `car ghost sample record` writes 0x80 for car+0x3304 equal 0 whatever started it
- The flags bits 0x02 and 0x04 are car+0xA78E4, the steer keys at the poll of the tick before the record, a second reading 20 ms before the mask, it pins a right tap at 5.0 to 5.19 s the mask never shows and the release of the 3.6 s press to 3.79 s. With it the port yaw sits inside 3.7 degrees of the recording through 9 s and the run drives the hairpin
- `body wheel axis correct` 0x4EFC90 is a quadratic yaw damper, spin z times (6 minus spin z) over 6 per tick when the principal frame leans a degree, it leans 1.6 at speed so the damper always runs, it is what holds the held turn near 1.05 rad per second, the port has it
- The tyre model 0x4F32E0 and the `body create` immediates match the port line by line, the steer channel and the world step too
- `math wrap angle open360` 0x44DB60 is the coarse pre wrap of the signed fold, 64 steps of 360 each way into the open interval, ported with `math wrap angle 360` as the same capped loops

## Settled in round eight

The speed through the long turns, GHOST_REFERENCE.md the fifth run and TICK_HELPERS.md round eight.

- The client keeps its speed through a held turn with nothing the port lacks. On the recorded line the port loses the same speed per sample as the recording through the 9 to 11 s hairpin (90.5 88.1 90.4 89.7 86.8 against 90.4 88.9 89.8 90.3 88.1). The 3 units per sample of the earlier resync was the harness, its teleport went through `body place and probe` and `body wheel state reset` zeroed the wheel spins, the engine speed, the steer channels and the slip states every 0.2 s, so every sample started with slipping tyres. The free run lost the speed on GRASS_L, friction 0.9 and a contact drag of 0.01 per loaded wheel (one percent of the velocity per tick with four wheels down), because a lateral offset of 18 units put it inside the hairpin
- The tyre model 0x4F32E0 read once more against the port, the combined slip is the client's: nLat is 85947.66 times the lateral slip over (f2 times load), nLong the same on the longitudinal slip, the force follows sin(1.2 atan) on the magnitude and splits by a and b, the longitudinal force does shrink under lateral slip and the client does that too
- The two substep pushes are both inside the substep loop, 0x49CF90 to 0x49D13E with `DEC [ESP 0x28]` and `JNZ 0x49CF90`, the world push (0, 0, tuning 0x5EB6F0 times minus 0.16) and the R column push at 0x49D00A, 14 times a tick at 20 ms
- The steer gains of `car drift update` at 0x49B1F9 read again, +0x2974 is +0x2F34 times pi over 180 times the two scales (1 and 1 with no drift), +0x2A78 +0x2A7C is clamp((stat 4 plus bonus) times 3 plus 1, 1, 4) times +0x32E8, 0.76 for basic 1, the port has both
- +0x2F34 has no runtime writer. The program wide search finds 13 instructions with the literal, 12 are `CMP [EAX 0x2F34], EBP` in FUN_0040A070 on another object and the last is the `FLD` of the drift update, so the value is the catalogue load, 16 degrees for every kart file
- +0xA7988 +0xA798C +0xA7990 are the fields 0xCC 0xD0 0xD4 of the part def record of loadout slot 1, `car apply kart loadout` looks the slot up through `part def record lookup` 0x44FBC0 (was FUN_0044FBC0) on the container 0x1A7932C at 0x490C24 and writes the three at 0x490C38 to 0x490C50, zero with no part, so the friction pair on asphalt is the catalogue 3.8 and 5.4
- The throttle jitter is game+0x6F8, one per game, `FMUL [EBP 0x6F8]` at 0x49CD97 with EBP the game base, the port keeps it on the local car
- `body wheel axis correct` disassembled once more, EBX is R, EBP the spin, EDI the scratch at +0x998, the last `body mat3 transform vec transposed` writes the spin, the damper is not a dead store. `body quat from axis angle` 0x4ED8E0 normalises the axis through `body vec3 normalize to` 0x4ED620 (was FUN_004ED620) whose gate 0x5A3C70 is a negative bit pattern that never rejects
- `car boost push` 0x496B40 named, `car boost start effect` 0x496960 (was FUN_00496960) is the scene node flash of the boost start, cosmetic. The kind 0 push is a velocity add of 6.0 per tick while the km per hour sits under the target, the target is the start km per hour plus 10, the port's mean speed through the pad boosts sits within 0.6 of the recording
- The recorded yaw byte is the quarter smoothed car+0x3220, read again at 0x49E02E, `FMUL 0x5A32D4` then `FADD [ESI]`, so the R heading leads it by four ticks of the rate and the harness resync keeps that lead across the teleport

## Settled in round nine

The bank slide, GHOST_REFERENCE.md the sixth run, TICK_HELPERS.md round nine, RIGID_BODY.md sixth pass, SUSPENSION_AND_TELEPORT.md the tyre frame.

- The two substep pushes read with the FPU chain: the world push is (0, 0, 0x5EB6F0 times minus 0.16) through `car apply engine force`, which negates x and y only, into `body apply force`, a plain add on the velocity. The second push at 0x49CFB3 loads car+0x2628 0x261C 0x2610, the third column of R, scales by the curve term times 0x5EB6F4 times 0.16 and negates all three, so it runs along minus the body up. Both as the port has them, the two other readings were run in the harness and are worse
- The body gravity +0xF0 is a scalar added on world z at every stage reset (`body chassis rk4 begin` 0x4F0A40), written by `body principal axes` and by `body apply durability scale` 0x4F1A60 with the tick's 19.6 times the speed curve, 0.4 units per second squared at 205 km per hour
- The tyre frame is built on the plane of the wheel's own cell (query+0xC of the wheel's query block, copied to wheel set +0x970 in `body integrate`), forward is axis cross n and lateral forward cross n, the argument order read from the pushes of 0x4F33B8 to 0x4F33DC, not the world horizontal
- `body wheel axis correct` turns the body up toward world z, the gate is acos of R z z minus one degree, the axis R z crossed with world z, the slerp order D3DX slerp(q, correction times q, 0.12). On the bank the body sits 3 degrees more upright than the road, in the client as in the port
- `body integrate` 0x4ECAB0 clears the collision flag at 0x4ECB35 before the wheel loop and leaves the loop at 0x4ECB86 after the first wheel whose locate failed, the port cleared the flag at the end of the collision response (no write of car+0x2E20 exists there) and located all four wheels, both ported
- The slide was the harness. The resync teleport posed the body flat with a yaw only quaternion, on the bank that unloaded the four wheels for two ticks every sample and dropped the car onto the road, and it headed the port at the truncated yaw byte, 0.7 degrees low on average, 1.5 units per second of crab at 120 on a 183 degree heading. The teleport keeps the roll and pitch now and the yaw goes to the byte centre, the race reads 0.329 mean 1.979 max (0.322 and 1.140 before the finish), the bank 0.397 with the vertical minus 0.025, the plain straight 0.23 to 0.29 per sample like the flat
- The turn response measured tick by tick on the 3.0 s press, the channel ramp 0.0152 per tick, the spin z 0.105 to 0.910 rad per second over the ten ticks of the press and 0.739 to 0.028 over the seven after it, the yaw 1.16 8.10 8.96 7.95 against the bytes 1.4 8.5 8.5 7.1 whose real values sit up to 1.41 above

## Settled in round ten and eleven

The drift, TICK_HELPERS.md round ten and eleven, GHOST_REFERENCE.md what a drift recording must show, the drift recording and the drift recording, closed, RIGID_BODY.md seventh pass.

- `car drift update` 0x49AA90 read whole as one machine, every line with its address in the round ten table, `car drift state set` takes the tick clock, `car body set yaw` writes R as Rz(360 minus yaw) Rx(pitch) times the transposed principal axes, the step 15 slip turn car+0x35B0 turns the model by the drift steer times the smoothed gauge, `car effect lean update` never writes the roll car+0x3228
- the drift recording of 2026-09-15 (37 drifts driven by the pilot) put the port inside the yaw byte on the drift samples once the state and the gauge come from the flags, 0.52 units and 1.4 degrees a sample over the race
- the mini turbo window is the unwinding of the gauge, 2.4 a tick, the stage dies with the state at 0x49AA13 and 0x49AE24, the gas rising edge must land inside it, the port fires on the 6 client runs and on none of the 20 with a brake between
- the kind 0 push is 6 units a tick through a plain velocity add on wheel set 0x1A0 until the km per hour passes the target (132.5 or the current plus 10 for basic 1), the sample means match, 77.4 and 84.1 against 77.8 and 84.2
- `body set mass friction` 0x4EC180 spreads its three floats as {a, b, c, c, b, a} so the brake channel rate is 1.0 and the reverse 5.0, not 5.0 and 0.4, the port locked the four wheels ten ticks after the brake key and could not steer under the brake, with the rate 1.0 the brake samples read 0.86 units and 2.5 degrees against 1.76 and 11.1
- the friction pair in a drift scales the front alone (0.8 off the gas, 0.6 on it) into wheel set 0x810, the rear 0x82C keeps the catalogue, the two step 11 drift clauses read as ported, the drift entry sheds the same speed as the client in a free slice

## Open questions

- Which theme makes `world theme is special row` 0x487230 true, it compares the theme record field 4 with 0x1312D00, a BSS row
- The bank slide, closed in round nine, it was the harness teleport and not a term. The two substep pushes, the body gravity, the tyre frame and the upright of the tilt correction match the bytes, with the lean kept across the teleport and the yaw at the byte centre the bank reads 0.397 per sample against 0.322 over the race
- The turn response shape. Checked the channel ramp and decay 0x4EEBB0, the Ackermann swing 0x4F1BE0, the yaw damper and the tyre model on the bytes again, all match. On the 3.0 s press the port turns 6.94 degrees inside the sample where the recording turns 7.1, the port sits 0.25 and 0.37 degrees under the byte floor on the two build up samples and inside the byte on the decay, over the race a sample under a steer key costs 0.06 units more than one without (0.363 against 0.307 with the resync). The difference is not located and sits inside the 1.41 degree yaw byte and the one tick edge window, it may not exist
- The release response. In the sample after the drift key goes up the port unwinds 1 to 3 degrees more than the client on a flat release (12.4 s minus 6.3 against minus 5.6, 128.2 s minus 8.2 against minus 4.2) and turns 1.5 to 3 more under the opposite steer in the next sample, the path matches. The gains at 0x49B0F1 to 0x49B249 and the unwinding 0x49ADD3 to 0x49AEDC match the bytes, the interval carries the left key up, the right key down and the drift key up, three edges the harness places one after the other inside one tick windows, not located beyond that
- The resync in a drift. Each sample puts the velocity back on the recorded chord while the tyre slip states of a sliding car stay, the drift samples read a unit or two under the client on the sample mean that a free slice from the sample before the key does not show
- The meaning of the regen ini rows, loaded and never read, and of the WARP NNN cells beyond their index parse
- The FUN_004ADBF0 reset on the object 0x2EB3B30 at the end of the collision response, a cosmetic state, not ported
- The camera reference of `car impact effect play` at 0x5C83A8 and the scene node parameters of the boost fade, host side
