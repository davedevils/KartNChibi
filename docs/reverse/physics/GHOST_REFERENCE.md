# Ghost reference, the real client measured

A ghost is a recording the stock client uploads after a race, one 28 byte sample every 0.2 s (ten physics ticks of 20 ms): position, yaw byte, status flags, drift and rpm nibbles, input mask. It is the only capture of the real physics we have with the inputs next to the outputs, so it is the acceptance test of the port in `games/kart/physics/client`.

## The recording

Two recordings, the RageZone run below with no drift is the reference for the free run and the launch curve, the DaveDebile run of the last section holds the drifts.

RageZone on Race 01 (track 90, map 80), kart basic 1 (template 10010, no part bonus), 665 samples, 131.9 s, three laps, uploaded 2026-09-11 14:23 to the local package server. Pulled from `ghost_record` and `ghost_replay_chunk` with the docker exec recipe in `docs/tools/README.md` (replay section) and saved as a `KCGR` file.

What the client had when it drove:

- 17 stat floats from the 0xC0 record: `0.52 0.52 0.52 0.52 0.30 0.52 0.30 0.30 0.52 0.52 0.52 0.70 0.52 0.0 9.0 37.0 3.5`. The login burst of that day read `vehicle_templates`, which has no stat columns, so every value is the server default, checked in the server source of commit bcf7cbe4. In the wire numbering settled on 2026-09-15 (INPUT_AND_STATS.md "The 17 stats, wire numbering, settled", wire k is car+0x3448 plus 4 k) the row reads: wire 0 body setup 0.52, wire 1 max speed 0.52, wire 2 steering gain 0.52, wire 3 mini turbo target 0.52, wire 4 boost lean lift 0.30, wire 5 turn force 0.52, wire 6 wheel spin 0.30, wire 7 wheel steer angle 0.30, wire 8 drift charge rate 0.52, wire 9 drift steer 0.52, wire 10 mini turbo threshold 0.52, wire 11 mini turbo hold 0.70, wire 12 grip 0.52, wire 13 no reader 0.0, wire 14 15 16 the camera 9.0 37 3.5. The runs up to the sixth fed the port two places off (the steering gain took wire 4, 0.30, the grip took wire 14, 9.0, the mini turbo hold took wire 13, 0.0), the seventh run below has the numbers with the right feed
- track record 0xC3 of Race 01, the three tuning floats at 0x30 0x34 0x38 are `0.4 0.6 90.0` (`track_catalog` columns `tuning_engine_setup_bits`, `tuning_engine_force_bits`, `tuning_turn_force`)
- spawn on start row 5 of `start.ini`, first sample at 0.003 units of it, yaw byte 0

## Frame

Wire space, z up, right handed. The first sample is `-321.31 228.57 0.98`, x and y span hundreds over the lap, z stays between 0.8 and 25. Yaw byte times 360 over 255, forward is `(-cos A, sin A)`, so yaw 0 drives toward minus x, checked on every straight of the three laps (motion angle equals 180 minus A within 5 degrees).

## Launch curve, the acceleration target

Accelerate held from the green light, flat road, no steering, no boost. Speed is the distance between two samples over 0.2 s, in world units per second.

| time s | speed | time s | speed |
|---|---|---|---|
| 0.2 | 2.2 | 2.0 | 83.6 |
| 0.4 | 12.5 | 2.4 | 89.5 |
| 0.6 | 27.3 | 3.0 | 93.7 |
| 0.8 | 40.4 | 3.6 | 95.5 |
| 1.0 | 51.8 | 4.6 | 98.4 |
| 1.4 | 68.4 | 5.0 | 102.4 |
| 1.6 | 75.0 | 6.0 | 98.6 |

Top speed over the whole race 126.4 at sample 580 while a boost flag is up. Cruise on a straight without boost sits around 100.

## Steering response

Over the whole race, at speeds above 60, one sample with the steer slot on bit 4 held moves the yaw by +7.8 degrees on average (46 samples), with the steer slot on bit 5 held by -10.0 degrees (129 samples, drift samples included). The first steer press of the race at 3.0 s and 94 units per second turns the yaw from 0 to 8.5 in one sample and the car starts moving in y the next sample.

## Input mask

Values seen: `0x00` (19 samples, after the finish), `0x80` (468, accelerate), `0x90` (46), `0xa0` (129), `0x10` (2), `0x20` (1). Bit 7 is slot 0 accelerate, bit 4 and bit 5 are the two steer slots, the order is 7 minus the slot index. The port recorder must write the same order, see `car_ghost_sample_record`.

## Status flags

`0x10` on 259 samples is the plain driving value, `0x12` and `0x14` add a turn side, `0x50` and `0x54` carry the boost bit of a kind other than 0, `0x90` and `0x94` the boost bit of kind 0, as `GhostPlayback` decodes them. `car_ghost_sample_record` 0x49FAD0 writes 0x80 when car+0x3304 is 0 and 0x40 otherwise, a kind 0 boost is a mini turbo or a boost.ini kind 0 pad, in this recording it is always the pad (fourth run). The turn side bits 0x02 slot 3 and 0x04 slot 2 are car+0xA78E4 written by `input_poll_keyboard` at the poll of the tick before the record, a second key reading 20 ms before the mask. Bits 0x100 0x200 (drift state) never appear and the gauge nibble is 7 (gauge 0) on all 665 samples, the player never drifted and slot 5 (mask bit 3) is never down.

## The harness

`tools/replay/ghost_compare.cpp`, target `ghost_compare`, built by the root solution and by the standalone kart project:

```
release\ghost_compare.exe <track folder> <ghost file> [--seconds N] [--resync] [--resync-every N] [--resync-until T] [--green] [--trace] [--trace-from T] [--centre] [--flags] [--edges] [--car path] [--kind K] [--engine-force F] [--stats 17 floats] [--tuning a b c] [--mask-lsb]
```

It loads the track col, the start rows and the boost.ini rows (so the port's own pad detector fires on the BOOST NNN cells), the `.car` catalogue (`--car`, default `Data/Car/basic_1.car` four levels up from the track folder, with the setup overrides and the catalogue grips 0x130 0x134), spawns the port on the first sample with the stats above, applies the recorded input mask for ten ticks per sample and prints per sample the recorded and the port position, yaw, speed and the gap. The yaw gap comes twice, the port yaw against the recorded byte and the byte the recorder would write for the port (wrap 360, times 255 over 360, truncated, 0x49FB69) against the recorded byte, the second is the honest one since the recorded value is a truncated byte. `--resync` puts the port back on the recorded sample after every compare so the per sample error shows without drift, since the fifth run it moves the pose only, the wheel spins, the engine speed, the steer channels and the slip states stay, the velocity is the central difference of the recorded positions. Since the sixth run the teleport keeps the roll and pitch of the body (R turns about world z by the yaw delta, T moves) and the yaw goes to the centre of the recorded byte, the truncated byte sits 0 to 1.41 degrees under the real yaw. `--resync-every N` teleports every N samples only. `--trace` prints per tick the steer channel, the front steer tangent, the spin z, the yaw, the motion direction, the four tyre lateral forces, the two axle peaks, the four lateral slip states, the four compressions, the R z z term, the roll and pitch spin, the z and the count of wheels in the air, then a `drive` line (boost state, target, the step 11 factor, gear, engine speed, clutch load, the four longitudinal forces, peaks, slips and spins), a `terms` line (roll, pitch, the surface index, friction and contact drag under each wheel), four `plane` lines (the cell plane under each hub, the hub height over it, the travel), a `force` line (the world tyre force sum and the velocity) and a `head` line (the R heading against the smoothed yaw). `--centre` and `--flags` are described under the third and fourth run, `--edges` under the fifth. A line `boost kind K started by the port` or `pad cell NAME under a wheel` marks a boost the port started on its own.

## What the first run found, 2026-09-14

- the port's tick handed `posZ` as the col ground axis and `posY` as the height hint (`tick.cpp` line 329) while the col wants x, y ground, z height, so the on track check failed on the real spawn and forced the brake for one sample
- `body_create` failed on the real spawn because the body layer queried the col with its physics space coordinates (minus x, minus y) instead of the wire ones
- the port reached its 120 units per second cap on the first tick with the placeholder catalogue and an engine force base of 1400, against the 52 units per second the client shows after one second
- the steer input did nothing on one side and jumped 60 degrees in a sample on the other

These are the open points handed to the body and the tick owners, this file is the target they close against.

## The second run, 2026-09-14, the tick side closed against the recording

What the client does, read on the bytes:

- The sample position is car+0x3244 0x3248 0x324C copied as three dwords by `car_ghost_sample_record` 0x49FAD0, the yaw byte is car+0x3220 wrapped to 360 and truncated, there is no other position field in the sample. So the recorded z is the body origin car+0x21E8 that the tick copies at 0x49D0A5, the point `body_set_pose` placed on the spawn, R times the offset at wheel set +0x148 plus T
- The steer sense. `input_poll_keyboard` 0x497AB0 writes slot 3 to game+0x20 and slot 2 to game+0x24 with the camera not reversed, the tick pushes the pointer game+0x18 into `body_step_world` at 0x49D131 so channel 2 is game+0x20 and channel 3 is game+0x24, `body_world_step` 0x4EFE90 hands channel 2 minus channel 3 to `body_steer_front_wheels` 0x4F1BE0, and `body_geometry_setup` 0x4F2AB0 builds the steer swing as up cross base at 0x4F2E50 (the base axis is pushed first so it is the second argument of `body_vec3_cross` 0x4ED700 which is A cross B). With that swing a positive channel 2 tilts the wheel 0 axis toward plus x, the wheels roll toward physics minus y, the wheel 1 side, the heading turns about minus z and the wire yaw grows. The drift update tags slot 3 with 0x27 VK RIGHT and slot 2 with 0x25 VK LEFT, the stuck block turns the yaw up on game+0x20, the recording turns the yaw up on the 0x90 sample. All four agree, slot 3 is the right turn, slot 2 the left turn, the port names them `steerRight` and `steerLeft`
- The steer channel. The drift update writes car+0x2974 as catalogue 0x94 times pi over 180 times the two scales and car+0x2A78 0x2A7C as clamp((wire stat 2 plus bonus) times 3 plus 1, 1, 4) times car+0x32E8 times the scales, 1.02 for basic 1 (0.76 in the runs up to the sixth, which read wire 4). The channel ramps by dt times the rate while the key is down, 0.15 after 0.2 s, and decays 16 times the rate per second once it is up, `body_spring_channel_update` 0x4EEBB0, zero one tick after the release
- The down push term at 0x49CF48 truncates the raw capped km per hour, clamped 0 to 99, not the minus 30 times a half index of 0x49CEEC, so one minus the speed curve saturates at 99 km per hour. The recorded z sinks from 0.98 to 0.78 between 0.2 and 0.8 s and stays flat after, the same saturation
- `world_on_track_check` 0x4A0750 returns 1 for car+0x6BC under 100 or over 299, the tick forces the brake on 0, so 100 to 299 is the respawn teleport sequence with the brake forced and 0 is free driving. The first harness run started the car at 100 and ran a fake respawn for three ticks
- `car_apply_engine_force` 0x4968F0 only reaches `body_apply_force` while game+0x1397384 is set, the same flag that gates the key poll

What changed in the port: the body swing cross order (one line in body.cpp, the only body edit), the down push index, the ground count naming (the count is the wheels in the air, byte 0 is a loaded wheel, the tick charges the surface contact drag per loaded wheel, was called the air penalty), car+0x32E4 takes the wheel set steer average at 0x49D4C0, the harness and the tick test spawn on `Data/Car/basic_1.car` with the setup overrides and the catalogue grips 0x130 0x134 instead of the placeholder 2.4 and 1.5 (the placeholder rear grip under the front one made the port spin out on every press), track progress 0, session flag 1, `--trace` prints the channels the rates the spin and the yaw per tick, `--car` picks the file.

The first 12 s, 20 ms ticks, ten per sample:

| t | mask | rec x y z yaw | port x y z yaw | gap horiz vert yaw | rec speed | port speed |
|---|---|---|---|---|---|---|
| 1.0 | 0x80 | -348.14 228.58 0.76 0.0 | -332.34 228.57 0.60 0.0 | 15.80 -0.16 0.0 | 51.8 | 33.9 |
| 2.0 | 0x80 | -421.57 228.60 0.79 0.0 | -389.60 228.57 0.56 0.0 | 31.97 -0.23 0.0 | 83.6 | 76.0 |
| 2.8 | 0x80 | -493.73 228.67 0.79 1.4 | -456.65 228.57 0.57 0.0 | 37.08 -0.22 -1.4 | 92.9 | 89.7 |
| 3.0 | 0x90 | -512.45 229.63 0.79 8.5 | -474.79 228.78 0.58 3.9 | 37.67 -0.22 -4.6 | 93.7 | 91.4 |
| 3.2 | 0x80 | -531.19 231.76 0.79 8.5 | -493.13 230.06 0.58 9.0 | 38.10 -0.22 0.5 | 94.3 | 92.4 |
| 3.4 | 0x80 | -550.02 234.36 0.79 7.1 | -511.61 232.24 0.58 8.7 | 38.47 -0.22 1.6 | 95.0 | 93.5 |
| 3.6 | 0xa0 | -569.00 236.56 0.79 2.8 | -530.27 234.53 0.58 4.1 | 38.79 -0.21 1.3 | 95.5 | 94.3 |
| 3.8 | 0x90 | -588.08 238.43 0.79 8.5 | -549.11 236.01 0.58 2.7 | 39.04 -0.22 -5.8 | 95.8 | 94.8 |
| 4.0 | 0x80 | -606.96 241.54 0.79 14.1 | -568.05 237.70 0.58 8.2 | 39.10 -0.21 -6.0 | 95.7 | 95.3 |
| 4.2 | 0x80 | -625.67 245.70 0.79 12.7 | -587.00 239.99 0.58 8.6 | 39.08 -0.21 -4.2 | 95.8 | 95.7 |
| 4.4 | 0xa0 | -644.49 249.32 0.79 2.8 | -606.02 242.35 0.58 4.2 | 39.10 -0.21 1.4 | 95.8 | 95.9 |
| 4.6 | 0xa0 | -664.15 250.27 0.82 347.3 | -625.05 243.27 0.58 352.1 | 39.72 -0.25 4.8 | 98.4 | 94.6 |
| 4.8 | 0xa0 | -684.45 247.71 0.80 340.2 | -643.54 241.38 0.57 338.9 | 41.39 -0.23 -1.4 | 102.3 | 91.7 |
| 5.0 | 0x80 | -704.39 243.11 0.81 343.1 | -661.06 236.53 0.57 333.7 | 43.83 -0.24 -9.3 | 102.4 | 90.9 |
| 5.4 | 0x80 | -744.13 233.36 0.80 348.7 | -695.43 223.15 0.58 337.9 | 49.76 -0.22 -10.8 | 101.4 | 93.4 |
| 6.0 | 0x80 | -802.63 221.99 0.79 348.7 | -748.15 202.00 0.58 338.2 | 58.03 -0.21 -10.5 | 98.6 | 95.5 |
| 6.6 | 0xa0 | -860.13 210.52 0.79 341.6 | -799.85 181.11 0.58 334.3 | 67.07 -0.21 -7.3 | 97.3 | 86.2 |
| 7.0 | 0x80 | -897.25 199.52 0.79 341.6 | -829.06 166.05 0.59 329.7 | 75.97 -0.21 -12.0 | 96.8 | 79.4 |
| 7.4 | 0x80 | -934.08 187.63 0.79 338.8 | -856.03 150.75 0.59 330.3 | 86.32 -0.20 -8.5 | 96.7 | 76.1 |
| 7.6 | 0xa0 | -951.90 180.50 0.79 327.5 | -865.88 145.03 0.59 325.7 | 93.05 -0.20 -1.9 | 96.0 | 15.4 |
| 8.0 | 0xa0 | -981.56 159.20 0.78 304.9 | -866.38 144.29 0.81 302.1 | 116.13 0.03 -2.9 | 90.1 | 1.3 |

Read of the table:

- The first turn is followed. One 0.2 s press on slot 3 turns the port by 9.0 degrees (3.9 at the end of the press, the rest over the next 0.16 s as the spin decays), the recording by 8.5 inside the sample. The port lags the client by about half a sample, its spin builds over the press where the client's yaw is complete at the sample. Under a held key the port's yaw rate saturates at 1.0 rad per second at 95 units per second, the recording reaches 1.35, so the three sample right turn at 4.4 to 4.8 leaves the port 9 to 11 degrees past the recording and the recording comes back 8.5 degrees on its own over the next 0.6 s where the port comes back 4.5. That offset carries the port off the road edge at 7.6 s while the recording turns into the second straight at 9 s. The tick side is as the bytes say, the response shape is the tyre model's, for the body owner
- The launch lags by 0.3 s and 16 units at 1 s, 32 at 2 s, the body engine of BODY_MOTION.md, not the tick
- z. The port sinks with the same shape and the same timing as the recording now that the push term saturates, but to 0.57 against 0.79 over the same plane at 0.544. With the tick's constant push at 0x49CF90 removed the port rests at 0.79 to 0.83, with both pushes it rests 0.22 lower. The pushes are read as the bytes say, so the tyre compression under the client's load is the open point, for the body owner, the sample z itself is settled


## The third run, 2026-09-15, the channel rates and the two replay modes

The body side found the one missing setup call, `body_set_mass_friction(5.0, 1.0, car+0x32E8)` at 0x49510C right after `body_create`, it sets the six channel rates so the throttle channel fills in 0.2 s. The harness and the tick test make the call now. The launch lag is gone.

Two replay aids joined the harness:

- `--centre`, the recorded mask is a snapshot at the sample tick, not a hold over the next ten ticks, so the mask of sample i drives ticks 10i minus 5 to 10i plus 4
- `--flags`, the status flags of the sample arm the drift state, the gauge from the nibble, the mini turbo stage and the boost kind of the port at the sample tick, since slot 5 is never in the mask the 4.4 to 5.4 s turn carries a mini turbo (flags 0x94 0x90 0x90 0x92 on samples 23 to 26, kind 0) the mask alone cannot reproduce

Plain run, the first 6 s:

| t | mask | rec x y z yaw | port x y z yaw | gap horiz vert yaw | rec speed | port speed |
|---|---|---|---|---|---|---|
| 0.2 | 0x80 | -321.74 228.57 0.97 0.0 | -321.47 228.57 1.00 0.0 | 0.27 0.03 0.0 | 2.2 | 3.1 |
| 0.4 | 0x80 | -324.24 228.57 0.90 0.0 | -323.53 228.57 0.90 0.0 | 0.72 0.00 0.0 | 12.5 | 17.0 |
| 0.6 | 0x80 | -329.71 228.57 0.81 0.0 | -328.60 228.57 0.82 0.0 | 1.11 0.01 0.0 | 27.3 | 31.8 |
| 0.8 | 0x80 | -337.78 228.57 0.78 0.0 | -336.11 228.57 0.78 0.0 | 1.67 0.01 0.0 | 40.4 | 41.9 |
| 1.0 | 0x80 | -348.14 228.58 0.76 0.0 | -345.67 228.57 0.77 0.0 | 2.46 0.00 0.0 | 51.8 | 52.7 |
| 1.4 | 0x80 | -373.91 228.58 0.78 0.0 | -370.27 228.57 0.77 0.0 | 3.65 0.00 0.0 | 68.4 | 68.9 |
| 2.0 | 0x80 | -421.57 228.60 0.79 0.0 | -416.83 228.57 0.79 0.0 | 4.73 0.00 0.0 | 83.6 | 83.9 |
| 2.6 | 0x80 | -475.14 228.61 0.79 0.0 | -469.86 228.57 0.79 0.0 | 5.29 0.00 0.0 | 91.5 | 91.6 |
| 3.0 | 0x90 | -512.45 229.63 0.79 8.5 | -507.06 228.78 0.79 3.9 | 5.45 0.00 -4.5 | 93.7 | 94.0 |
| 3.2 | 0x80 | -531.19 231.76 0.79 8.5 | -525.86 230.08 0.79 9.0 | 5.59 0.00 0.6 | 94.3 | 94.4 |
| 3.4 | 0x80 | -550.02 234.36 0.79 7.1 | -544.68 232.29 0.79 8.6 | 5.73 0.00 1.6 | 95.0 | 95.1 |
| 3.6 | 0xa0 | -569.00 236.56 0.79 2.8 | -563.62 234.60 0.79 4.0 | 5.73 0.00 1.2 | 95.5 | 95.5 |
| 3.8 | 0x90 | -588.08 238.43 0.79 8.5 | -582.68 236.07 0.79 2.6 | 5.89 0.00 -5.8 | 95.8 | 95.8 |
| 4.0 | 0x80 | -606.96 241.54 0.79 14.1 | -601.79 237.75 0.79 8.1 | 6.41 0.00 -6.0 | 95.7 | 96.0 |
| 4.4 | 0xa0 | -644.49 249.32 0.79 2.8 | -640.02 242.38 0.79 4.1 | 8.25 0.00 1.3 | 95.8 | 96.4 |
| 4.8 | 0xa0 | -684.45 247.71 0.80 340.2 | -677.74 241.32 0.79 338.8 | 9.27 -0.01 -1.4 | 102.3 | 92.1 |
| 5.0 | 0x80 | -704.39 243.11 0.81 343.1 | -695.31 236.39 0.79 333.7 | 11.30 -0.03 -9.4 | 102.4 | 91.3 |
| 5.4 | 0x80 | -744.13 233.36 0.80 348.7 | -729.82 222.90 0.79 337.9 | 17.73 0.00 -10.8 | 101.4 | 93.8 |
| 6.0 | 0x80 | -802.63 221.99 0.79 348.7 | -782.71 201.67 0.79 338.2 | 28.45 0.00 -10.6 | 98.6 | 95.8 |

Mean horizontal gap over 30 samples 7.78, max 28.45. The speed sits on the recording from 0.8 s on, the z rests at 0.79 on the recorded 0.79, the first turn is inside 6 degrees, the gap at 3 s is 5.4 units.

With `--centre --flags`:

| t | mask | rec yaw | port yaw | gap horiz | gap yaw | rec speed | port speed |
|---|---|---|---|---|---|---|---|
| 2.8 | 0x80 | 1.4 | 0.7 | 5.39 | -0.7 | 92.9 | 93.0 |
| 3.0 | 0x90 | 8.5 | 7.7 | 5.44 | -0.8 | 93.7 | 93.8 |
| 3.2 | 0x80 | 8.5 | 9.0 | 5.48 | 0.6 | 94.3 | 94.4 |
| 3.4 | 0x80 | 7.1 | 7.5 | 5.52 | 0.4 | 95.0 | 95.1 |
| 3.6 | 0xa0 | 2.8 | 0.8 | 5.60 | -2.0 | 95.5 | 95.4 |
| 3.8 | 0x90 | 8.5 | 6.4 | 5.72 | -2.1 | 95.8 | 95.8 |
| 4.0 | 0x80 | 14.1 | 8.5 | 5.94 | -5.6 | 95.7 | 96.1 |
| 4.4 | 0xa0 | 2.8 | 358.6 | 7.71 | -4.3 | 95.8 | 95.9 |
| 4.6 | 0xa0 | 347.3 | 345.2 | 8.97 | -2.1 | 98.4 | 103.6 |
| 4.8 | 0xa0 | 340.2 | 333.2 | 10.34 | -7.0 | 102.3 | 101.9 |
| 5.0 | 0x80 | 343.1 | 334.4 | 12.50 | -8.7 | 102.4 | 103.4 |
| 6.0 | 0x80 | 348.7 | 337.9 | 29.96 | -10.8 | 98.6 | 98.2 |

The first turn reads 0.7 7.7 9.0 7.5 against 1.4 8.5 8.5 7.1 recorded, the mini turbo of 4.6 s lifts the port to 103.6 where the recording reads 98.4 to 102.4. The held right turn at 4.4 to 4.8 still overshoots by 7 to 11 degrees, the steer response shape of the tyre model, for the body owner.

## The fourth run, 2026-09-15, the turn after the boost pad

The gap of the third run was read as a drift exit or a tyre limit. Neither, the recording settles it on its own bytes:

- No drift in this recording. Over the 665 samples the status flags never carry 0x100 or 0x200 (car+0x35A4 states 1 and 2, written at 0x49FD07 and 0x49FD31), the gauge nibble is 7 on every sample (gauge 0, the nibble is (gauge plus 45) times 15 over 90 truncated) and the mask never has bit 3 (slot 5, the drift key). So `car_drift_update` 0x49AA90 ran with state 0 the whole race, its unwinding block (0x49ADD3) took the zero gauge branch every tick (0x49ADEA to 0x49AE46, gauge plus 2.4 goes over 0, gauge state and stage to 0 at 0x49AE18 to 0x49AE24) and never reached `car_body_set_yaw`, the mini turbo stages never armed. The six constants of that block read 10.0 at 0x59F404, minus 10.0 at 0x5A324C, 1.5 at 0x5A6A48, 80.0 at 0x5A32A0, 30.0 at 0x5A3244, 20.0 at 0x5A3298, as the port has them. With `--flags` the harness never armed a drift either, 0x10 is set on every sample
- The 0x80 flag of samples 23 to 26 is a boost pad. `car_ghost_sample_record` writes 0x80 for car+0x3304 equal 0, kind 0, which is the mini turbo kind and also the kind of boost.ini rows 0 to 4 of Race 01. The harness now loads boost.ini and the port's own `car_respawn_state_machine` pad scan finds the col cell BOOST_001 under a wheel at 4.52 s and starts a kind 0 boost, 608 ms for wire stat 3 at 0.52, between the recorded 4.4 and 4.6 s. The 0x50 samples later in the race are the kind 1 pads (rows 5 to 8 and 10)
- The yaw come back after the pad is a right tap the mask never shows. Sample 26 (5.2 s) has flags 0x92 with mask 0x80. Bit 0x02 is car+0xA78E4 equal 2, written by `input_poll_keyboard` 0x497DA9 from game+0x20 (slot 3) at the poll of the tick before the record, the mask reads the key at the record itself one tick later. So the right key was down at tick 259 and up at tick 260, a tap that ended at 5.19 s and started after 5.0 s (sample 25 has neither bit). The same pair pins the release of the 3.6 to 3.8 s right press to tick 189 (flags 0x12, mask 0x80 on sample 19). Over the race the two readings disagree on 16 samples, every one a key edge inside the 20 ms
- The steer response itself is the client's. The tyre model 0x4F32E0 and its immediates at `body_create` 0x4EC950 (85947.66 85943 1.2 minus 0.2 grip 5e minus 5 1.0 per axle, 0.2 0.2 1.0 6 3 50 50 20 shared) match the port line by line, the steer channel 0x4EEBB0 and the world step 0x4EFE90 too. `body_wheel_axis_correct` 0x4EFC90 is a quadratic yaw damper, spin z times (6 minus spin z) over 6 every tick, that runs when acos of R z z minus one degree is at or over the double 0.0 at 0x5A83E8, the principal frame leans 1.6 degrees at speed so it runs on the straight and in every turn, the port has it and without it the short presses overshoot by 4 to 9 degrees. The path curvature of the port matches the recording within a degree per sample through the turn, only the sample where the key edge falls differs

`--flags` now also feeds the turn side bits to the steer keys, ticks 5 to 9 of each sample take the side bits of the next sample and its mask follows at tick 0, and reads the boost bit on the last tick of the sample so a pad the port crossed itself is not started twice. `--centre --flags`, the first 9 s, 20 ms ticks, the yaw gap is the continuous one then the byte one:

| t | mask | rec yaw | port yaw | gap horiz | gap yaw | gap byte | rec speed | port speed |
|---|---|---|---|---|---|---|---|---|
| 1.0 | 0x80 | 0.0 | 0.0 | 2.46 | 0.0 | 0.0 | 51.8 | 52.7 |
| 2.0 | 0x80 | 0.0 | 0.0 | 4.73 | 0.0 | 0.0 | 83.6 | 83.9 |
| 2.8 | 0x80 | 1.4 | 0.7 | 5.39 | -0.7 | -1.4 | 92.9 | 93.0 |
| 3.0 | 0x90 | 8.5 | 7.7 | 5.44 | -0.8 | -1.4 | 93.7 | 93.8 |
| 3.2 | 0x80 | 8.5 | 9.0 | 5.48 | 0.6 | 0.0 | 94.3 | 94.4 |
| 3.4 | 0x80 | 7.1 | 7.5 | 5.52 | 0.4 | 0.0 | 95.0 | 95.1 |
| 3.6 | 0xa0 | 2.8 | 0.8 | 5.60 | -2.0 | -2.8 | 95.5 | 95.4 |
| 3.8 | 0x90 | 8.5 | 8.1 | 5.71 | -0.4 | -1.4 | 95.8 | 95.6 |
| 4.0 | 0x80 | 14.1 | 15.1 | 5.81 | 1.0 | 0.0 | 95.7 | 95.4 |
| 4.2 | 0x80 | 12.7 | 13.8 | 5.89 | 1.1 | 0.0 | 95.8 | 95.7 |
| 4.4 | 0xa0 | 2.8 | 4.5 | 5.90 | 1.7 | 1.4 | 95.8 | 95.5 |
| 4.6 | 0xa0 | 347.3 | 351.0 | 6.26 | 3.7 | 2.8 | 98.4 | 104.2 |
| 4.8 | 0xa0 | 340.2 | 338.9 | 6.08 | -1.3 | -1.4 | 102.3 | 103.0 |
| 5.0 | 0x80 | 343.1 | 339.9 | 6.27 | -3.2 | -4.2 | 102.4 | 105.0 |
| 5.2 | 0x80 | 345.9 | 343.1 | 6.60 | -2.8 | -2.8 | 103.2 | 102.4 |
| 5.4 | 0x80 | 348.7 | 346.6 | 7.12 | -2.2 | -2.8 | 101.4 | 100.9 |
| 5.6 | 0x80 | 348.7 | 346.7 | 7.78 | -2.0 | -2.8 | 100.1 | 99.8 |
| 6.0 | 0x80 | 348.7 | 346.2 | 9.35 | -2.5 | -2.8 | 98.6 | 98.4 |
| 6.6 | 0xa0 | 341.6 | 338.4 | 12.19 | -3.3 | -4.2 | 97.3 | 97.0 |
| 7.0 | 0x80 | 341.6 | 337.9 | 14.57 | -3.7 | -4.2 | 96.8 | 96.7 |
| 7.4 | 0x80 | 338.8 | 337.8 | 16.96 | -1.1 | -1.4 | 96.7 | 96.6 |
| 7.6 | 0xa0 | 327.5 | 329.1 | 17.58 | 1.6 | 1.4 | 96.0 | 96.0 |
| 7.8 | 0xa0 | 313.4 | 316.0 | 17.59 | 2.6 | 1.4 | 93.2 | 93.5 |
| 8.0 | 0xa0 | 304.9 | 304.6 | 17.42 | -0.3 | -1.4 | 90.1 | 90.8 |
| 8.4 | 0x80 | 309.2 | 308.0 | 18.14 | -1.2 | -1.4 | 91.9 | 92.6 |
| 9.0 | 0xa0 | 300.7 | 299.6 | 19.72 | -1.1 | -1.4 | 94.4 | 88.9 |

Mean horizontal gap over 45 samples 8.7, max 19.7 at 9 s, the gap is along the track (the 5 units of the launch never close, the port sits a hair ahead then behind). The yaw is inside 3.7 degrees through 9 s, inside three bytes (4.2) on the byte grid, and the run stays on the road, it drives the 9 to 14 s hairpin and comes out at 228 degrees like the recording. The two samples over 3 degrees, 4.6 s (3.7) and 5.0 s (minus 3.2), sit on the two key edges the pair of readings cannot pin, the release of the left key between 4.60 and 4.78 s and the start of the right tap between 5.00 and 5.18 s, `--centre` puts each at the middle of its window and a 0.05 s shift of either moves that sample by 3 degrees at the recorded 77 degrees per second. The turn 7.4 to 8.0 s and its come back read 329.1 316.0 304.6 305.8 308.0 against 327.5 313.4 304.9 307.8 309.2 with no tap at all.

What the fourth run leaves for the next one:

- the port loses speed through the long turns, 88.9 at 9.0 s against 94.4 and 71 to 76 through the 9.8 to 10.8 s hairpin against 88 to 90, so it runs the hairpin 30 units inside the recorded line and misses the ramp the recording climbs at 11.4 s (z 1.85 to 9.67). The short turns scrub the same in both (7.4 to 8.0 s, 96.6 to 90.8 against 96.7 to 90.1), the long ones do not, the tyre drag at high slip or the throttle terms of step 11 are the candidates
- the peak rate of a held turn, the port plateaus at 1.05 to 1.08 rad per second under the axis correct damper where the recording shows at least 1.23 over one sample (14.1 degrees on the byte grid at 4.4 to 4.6 s), inside the key edge uncertainty but always on the same side

## The fifth run, 2026-09-15, the speed through the long turns

The fourth run left the port at 71 to 76 units per second through the 9 to 11 s hairpin against 88 to 90 recorded, and the resync mode read a loss of 3 units per sample against 1. Both were the harness, not the physics:

- The resync teleport went through `body_place_and_probe`, which runs `body_wheel_state_reset` 0x4EEAD0 as the client does at a respawn. Every 0.2 s the four wheel spins, the engine speed, the steer channels and the two slip banks went to zero, so every sample started with slipping tyres and a dead steer, the port read 80 units per second on a straight the recording drove at 93 and turned nothing inside a sample. The resync moves the pose only now (`resync_pose` in the harness, `body_set_pose` plus the four cell binds), the velocity it puts back is the central difference of the recorded positions (the chord of one pair lags a turn by half a sample) and the R heading keeps its lead over the smoothed car+0x3220 across the teleport, since the recorded byte is the quarter smoothed value (0x49E02E). With that the per sample error over the race drops from 1.349 mean 2.394 max to 0.448 mean 1.986 max, 0.442 mean 1.340 max before the finish
- On the recorded line the port keeps the hairpin speed. The per sample speeds with the resync:

| t | mask | rec yaw | port yaw | gap horiz | rec speed | port speed |
|---|---|---|---|---|---|---|
| 9.0 | 0xa0 | 300.7 | 300.7 | 0.36 | 94.4 | 94.0 |
| 9.2 | 0xa0 | 286.6 | 287.7 | 0.30 | 93.0 | 91.6 |
| 9.4 | 0x20 | 279.5 | 276.2 | 0.15 | 87.9 | 86.4 |
| 9.6 | 0x80 | 282.4 | 280.4 | 0.19 | 87.0 | 86.8 |
| 9.8 | 0x80 | 283.8 | 284.9 | 0.17 | 88.6 | 89.4 |
| 10.0 | 0x90 | 289.4 | 292.8 | 0.38 | 90.4 | 90.5 |
| 10.2 | 0x90 | 303.5 | 303.8 | 0.25 | 90.4 | 89.7 |
| 10.4 | 0x90 | 313.4 | 315.6 | 0.24 | 88.9 | 88.1 |
| 10.6 | 0xa0 | 304.9 | 306.1 | 0.29 | 89.8 | 90.4 |
| 10.8 | 0xa0 | 289.4 | 289.6 | 0.36 | 90.2 | 89.7 |
| 11.0 | 0xa0 | 273.9 | 275.4 | 0.21 | 88.1 | 86.8 |
| 11.2 | 0xa0 | 262.6 | 262.5 | 0.11 | 94.3 | 96.3 |
| 11.4 | 0x80 | 265.4 | 262.4 | 0.18 | 96.2 | 97.3 |

- The free run lost the speed on the grass. Its lateral offset to the recorded line was 18 units at 9 s, so it ran the hairpin on GRASS_L (surface 8, friction 0.9, contact drag 0.01 per loaded wheel, one percent of the velocity a tick with four wheels down) while the recording stays on ASPHALT, the `terms` trace line shows the surface under each wheel. The offset itself came from one key edge. The left press of 4.1 to 4.7 s has its release anywhere between 4.60 and 4.78 (mask 0xa0 at 4.6, flags 0x90 and mask 0x80 at 4.8), `--centre` put it at 4.70, the recorded yaw of 4.6 to 4.8 (347.3 to 340.2, minus 7.1 through the four tick smoothing) says 4.60 to 4.62. With that one edge moved the free run stays inside 18 units through 16 s, with it at 4.70 it leaves the road at 11 s
- `--edges` places every unpinned steer edge that way. An edge is open when the mask at sample i and the side bit at sample i plus 1 disagree (the two readings of round seven), the window is ticks 1 to 9 of the interval, the harness runs the interval and the next one for each tick of the window from a snapshot and keeps the tick whose yaw at the two following samples is closest to the recorded bytes. It turns `--centre` and `--flags` on. Over 16 s it places 26 edges, 23 off the centre tick, the free run reads 10.5 mean and 18.8 max, the yaw inside 2 degrees on every sample. Over the race 186 edges, the run holds inside 15 units to 20 s and leaves on the banked back straight, see the bank below
- The largest per sample errors before the finish sit on the banked back straight at 120 units per second under the kind 1 pad boost (110.0 s 1.34, 22.4 s 1.21, 16.8 s 1.21, 63.2 s 1.16), the recorded speed there is the 120 clamp of `body_chassis_integrate_k1` on x and y and the port sits 2 to 4 under it after each teleport jolt, and the z runs 0.2 to 0.5 under the recorded one on every banked section, the port slides down the bank. The four samples after the finish (132.2 to 132.8 s, 0.82 to 1.99) are the client braking to 41 units per second while the mask still reads 0x80 or 0x00, the finish sequence the harness does not run, not a physics term
- The bank. The cells of the back straight (14 to 26 s and the same road on the later laps) carry normals 24 to 28 degrees off the vertical, the recorded car sits 0.245 over that plane like on the flat and its z follows the bank slope as it moves across the road, so the bank is real and driven. With a resync every second the lateral gap grows 0.6 1.0 1.5 2.1 2.7 over the five samples between two teleports, about 3.6 units per second of crab down the bank beyond the recording. The pushes are per substep on the bytes (the loop 0x49CF90 to 0x49D13E), the tyre model, the steer, the channel and the axis correct match, a world push of a quarter fixes the bank slide but lifts the flat rest z from 0.79 to 0.92, a doubled lateral stiffness or a disabled upright slerp do not reproduce the recorded slide. Open, see CLIENT_PHYSICS_MAP.md
- Read again on the bytes this run, nothing changed in the port: the tyre model 0x4F32E0 (the combined slip is the client's), `body_wheel_axis_correct` with the registers named (the damper writes the spin), `body_spring_channel_update`, `body_steer_front_wheels`, the drift update steer gains at 0x49B1F9 and the fact that +0x2F34 has no runtime writer, the step 11 factor and the velocity scale at 0x49CE2F, the boost start, update and push (`car_boost_push` 0x496B40 named, `car_boost_start_effect` 0x496960 is the scene flash), the origin of +0xA7988 +0xA798C +0xA7990 (the part def record fields 0xCC 0xD0 0xD4 of loadout slot 1 through `part_def_record_lookup` 0x44FBC0, zero with no part), the jitter at game+0x6F8 and `body_vec3_normalize_to` 0x4ED620 under `body_quat_from_axis_angle`

The free run with `--centre --flags`, 8 to 16 s, every second sample, the fourth run convention:

| t | mask | rec x y z yaw | port x y z yaw | gap horiz vert yaw byte | rec speed | port speed |
|---|---|---|---|---|---|---|
| 8.0 | 0xa0 | -981.56 159.20 0.78 304.9 | -972.38 144.39 0.78 304.6 | 17.42 0.00 -0.3 -1.4 | 90.1 | 90.8 |
| 8.2 | 0x80 | -993.73 145.84 0.79 307.8 | -984.48 130.82 0.79 305.8 | 17.64 -0.00 -2.0 -2.8 | 90.4 | 91.3 |
| 8.4 | 0x80 | -1005.69 131.89 0.79 309.2 | -996.06 116.51 0.79 308.0 | 18.14 0.00 -1.2 -1.4 | 91.9 | 92.6 |
| 8.6 | 0x80 | -1017.77 117.73 0.79 309.2 | -1007.69 101.96 0.79 308.7 | 18.72 0.00 -0.5 -1.4 | 93.1 | 93.6 |
| 8.8 | 0x80 | -1030.00 103.45 0.79 309.2 | -1019.31 87.43 0.79 308.1 | 19.26 -0.00 -1.1 -1.4 | 94.0 | 91.8 |
| 9.0 | 0xa0 | -1041.82 88.73 0.79 300.7 | -1030.03 72.92 0.79 299.6 | 19.72 -0.00 -1.1 -1.4 | 94.4 | 88.9 |
| 9.2 | 0xa0 | -1051.58 72.89 0.79 286.6 | -1038.53 57.82 0.78 286.7 | 19.93 -0.00 0.1 0.0 | 93.0 | 84.5 |
| 9.4 | 0x20 | -1058.15 56.58 0.78 279.5 | -1043.85 42.76 0.78 275.8 | 19.88 -0.00 -3.7 -4.2 | 87.9 | 76.3 |
| 9.6 | 0x80 | -1062.95 39.86 0.79 282.4 | -1046.73 28.33 0.78 276.0 | 19.91 -0.01 -6.4 -7.1 | 87.0 | 72.2 |
| 9.8 | 0x80 | -1067.40 22.71 0.79 283.8 | -1048.84 14.14 0.79 278.2 | 20.43 -0.00 -5.6 -5.6 | 88.6 | 71.7 |
| 10.0 | 0x90 | -1072.23 5.29 0.79 289.4 | -1051.57 -0.46 0.79 286.6 | 21.44 -0.00 -2.8 -2.8 | 90.4 | 76.3 |
| 10.2 | 0x90 | -1078.92 -11.50 0.79 303.5 | -1056.29 -14.93 0.78 299.7 | 22.88 -0.00 -3.8 -4.2 | 90.4 | 74.5 |
| 10.4 | 0x90 | -1088.17 -26.69 0.78 313.4 | -1063.24 -27.60 0.78 311.0 | 24.95 -0.01 -2.4 -2.8 | 88.9 | 71.1 |
| 10.6 | 0xa0 | -1098.80 -41.17 0.79 304.9 | -1071.36 -39.21 0.78 304.4 | 27.51 -0.01 -0.5 -1.4 | 89.8 | 70.9 |
| 10.8 | 0xa0 | -1108.24 -56.56 0.79 289.4 | -1078.62 -51.81 0.78 289.8 | 30.00 -0.01 0.4 0.0 | 90.2 | 74.3 |
| 11.0 | 0xa0 | -1114.66 -72.96 0.78 273.9 | -1083.53 -65.97 0.77 274.7 | 31.90 -0.01 0.8 0.0 | 88.1 | 75.5 |
| 11.2 | 0xa0 | -1117.69 -91.57 0.80 262.6 | -1085.40 -82.80 0.79 260.8 | 33.46 -0.01 -1.8 -2.8 | 94.3 | 84.5 |
| 11.4 | 0x80 | -1118.22 -110.81 1.85 265.4 | -1084.59 -99.80 0.95 260.4 | 35.38 -0.91 -5.1 -5.6 | 96.2 | 87.0 |
| 11.6 | 0xa0 | -1117.75 -129.99 2.49 262.6 | -1082.26 -117.01 1.05 255.3 | 37.79 -1.45 -7.3 -8.5 | 95.9 | 87.8 |
| 11.8 | 0x80 | -1116.05 -148.94 4.15 258.4 | -1078.52 -134.30 1.31 253.7 | 40.29 -2.85 -4.6 -5.6 | 95.2 | 89.2 |
| 12.0 | 0xa0 | -1112.17 -168.90 6.43 247.1 | -1073.45 -151.51 1.14 245.8 | 42.44 -5.29 -1.3 -1.4 | 101.7 | 89.8 |
| 12.2 | 0xa0 | -1106.05 -188.54 7.59 241.4 | -1065.68 -169.20 0.72 234.6 | 44.77 -6.86 -6.8 -7.1 | 102.8 | 94.6 |
| 12.4 | 0x80 | -1098.01 -207.68 9.29 238.6 | -1055.88 -185.97 0.82 235.7 | 47.39 -8.47 -2.9 -4.2 | 103.8 | 95.5 |
| 12.6 | 0x80 | -1088.69 -226.20 9.67 238.6 | -1045.53 -202.41 0.83 237.3 | 49.28 -8.84 -1.2 -1.4 | 103.6 | 97.3 |
| 12.8 | 0x80 | -1077.89 -245.46 9.37 238.6 | -1035.57 -218.10 0.80 237.1 | 50.39 -8.57 -1.5 -2.8 | 110.4 | 89.8 |
| 13.0 | 0xa0 | -1065.95 -264.10 8.81 228.7 | -1025.75 -232.37 0.80 228.8 | 51.22 -8.01 0.1 0.0 | 110.7 | 84.1 |
| 13.2 | 0xa0 | -1051.65 -280.94 8.05 214.6 | -1013.45 -246.06 0.83 215.7 | 51.73 -7.22 1.1 0.0 | 110.5 | 90.4 |
| 13.4 | 0x80 | -1035.31 -294.91 6.97 208.9 | -999.06 -257.72 0.83 209.9 | 51.94 -6.14 0.9 0.0 | 107.5 | 92.3 |
| 13.6 | 0x80 | -1018.01 -307.05 6.10 214.6 | -983.83 -268.14 0.81 212.5 | 51.79 -5.29 -2.1 -2.8 | 105.7 | 91.2 |
| 13.8 | 0x90 | -1001.21 -319.47 5.65 225.9 | -969.49 -278.28 0.82 220.1 | 51.99 -4.83 -5.8 -7.1 | 104.5 | 85.4 |
| 14.0 | 0x80 | -985.84 -332.86 6.58 228.7 | -956.52 -288.65 0.81 221.5 | 53.06 -5.77 -7.2 -8.5 | 101.9 | 81.4 |
| 14.2 | 0x80 | -971.29 -346.84 7.69 225.9 | -944.32 -298.95 0.81 220.8 | 54.96 -6.88 -5.1 -5.6 | 100.9 | 78.6 |
| 14.4 | 0x80 | -956.68 -360.33 10.52 223.1 | -932.54 -309.00 0.81 220.6 | 56.72 -9.71 -2.5 -2.8 | 99.4 | 76.6 |
| 14.6 | 0x80 | -942.10 -373.65 12.31 221.6 | -921.03 -318.83 0.80 220.5 | 58.73 -11.51 -1.1 -1.4 | 98.8 | 75.0 |
| 14.8 | 0x80 | -927.02 -386.09 15.77 218.8 | -909.70 -328.47 0.80 219.8 | 60.17 -14.97 1.0 0.0 | 97.8 | 73.8 |
| 15.0 | 0xa0 | -911.17 -397.64 17.94 206.1 | -898.16 -337.45 0.79 211.8 | 61.57 -17.14 5.7 5.6 | 98.0 | 72.4 |
| 15.2 | 0xa0 | -893.90 -406.29 19.33 192.0 | -885.88 -344.60 0.79 199.0 | 62.21 -18.55 7.0 5.6 | 96.5 | 69.9 |
| 15.4 | 0xa0 | -874.77 -411.41 19.24 176.5 | -872.13 -349.24 0.81 185.1 | 62.22 -18.43 8.6 8.5 | 99.0 | 74.5 |
| 15.6 | 0x80 | -854.07 -412.61 18.38 170.8 | -856.87 -351.16 0.82 177.8 | 61.51 -17.57 7.0 5.6 | 103.7 | 79.2 |
| 15.8 | 0x80 | -832.01 -411.38 16.63 173.6 | -840.47 -351.59 0.83 179.3 | 60.38 -15.80 5.6 4.2 | 110.5 | 84.2 |
| 16.0 | 0x80 | -808.82 -409.16 14.31 173.6 | -822.69 -351.70 0.84 180.1 | 59.11 -13.47 6.5 5.6 | 116.4 | 93.5 |

Mean horizontal gap over 80 samples 24.0, max 62.2, the port is on GRASS_L from 9.0 s.

The same with `--edges`:

| t | mask | rec x y z yaw | port x y z yaw | gap horiz vert yaw byte | rec speed | port speed |
|---|---|---|---|---|---|---|
| 8.0 | 0xa0 | -981.56 159.20 0.78 304.9 | -972.44 155.41 0.78 304.8 | 9.87 0.00 -0.2 -1.4 | 90.1 | 90.0 |
| 8.2 | 0x80 | -993.73 145.84 0.79 307.8 | -984.46 141.89 0.79 307.1 | 10.07 -0.00 -0.7 -1.4 | 90.4 | 91.0 |
| 8.4 | 0x80 | -1005.69 131.89 0.79 309.2 | -996.16 127.75 0.79 309.0 | 10.38 0.00 -0.2 -1.4 | 91.9 | 92.4 |
| 8.6 | 0x80 | -1017.77 117.73 0.79 309.2 | -1007.97 113.39 0.79 309.5 | 10.72 -0.00 0.3 0.0 | 93.1 | 93.4 |
| 8.8 | 0x80 | -1030.00 103.45 0.79 309.2 | -1019.90 98.90 0.79 309.2 | 11.07 -0.00 -0.0 -1.4 | 94.0 | 94.2 |
| 9.0 | 0xa0 | -1041.82 88.73 0.79 300.7 | -1031.45 83.98 0.79 301.4 | 11.40 0.00 0.7 0.0 | 94.4 | 94.3 |
| 9.2 | 0xa0 | -1051.58 72.89 0.79 286.6 | -1041.13 68.02 0.79 288.4 | 11.53 0.00 1.8 1.4 | 93.0 | 92.4 |
| 9.4 | 0x20 | -1058.15 56.58 0.78 279.5 | -1047.76 51.61 0.78 279.9 | 11.51 0.00 0.4 0.0 | 87.9 | 86.6 |
| 9.6 | 0x80 | -1062.95 39.86 0.79 282.4 | -1052.44 34.99 0.79 281.7 | 11.59 -0.00 -0.7 -1.4 | 87.0 | 86.5 |
| 9.8 | 0x80 | -1067.40 22.71 0.79 283.8 | -1056.54 18.34 0.79 283.2 | 11.70 -0.00 -0.5 -1.4 | 88.6 | 84.9 |
| 10.0 | 0x90 | -1072.23 5.29 0.79 289.4 | -1060.92 1.75 0.79 289.3 | 11.85 -0.00 -0.1 -1.4 | 90.4 | 86.8 |
| 10.2 | 0x90 | -1078.92 -11.50 0.79 303.5 | -1066.89 -14.23 0.78 301.6 | 12.33 -0.00 -1.9 -2.8 | 90.4 | 83.3 |
| 10.4 | 0x90 | -1088.17 -26.69 0.78 313.4 | -1075.19 -28.53 0.78 313.1 | 13.11 -0.00 -0.3 -1.4 | 88.9 | 83.2 |
| 10.6 | 0xa0 | -1098.80 -41.17 0.79 304.9 | -1085.13 -42.24 0.79 305.3 | 13.71 -0.00 0.4 0.0 | 89.8 | 85.9 |
| 10.8 | 0xa0 | -1108.24 -56.56 0.79 289.4 | -1094.10 -56.99 0.78 289.9 | 14.14 -0.00 0.5 0.0 | 90.2 | 86.4 |
| 11.0 | 0xa0 | -1114.66 -72.96 0.78 273.9 | -1100.30 -72.91 0.78 275.5 | 14.36 -0.00 1.6 1.4 | 88.1 | 84.7 |
| 11.2 | 0xa0 | -1117.69 -91.57 0.80 262.6 | -1103.25 -91.19 0.80 263.6 | 14.44 -0.00 1.0 0.0 | 94.3 | 91.6 |
| 11.4 | 0x80 | -1118.22 -110.81 1.85 265.4 | -1103.61 -110.07 1.71 265.9 | 14.63 -0.14 0.5 0.0 | 96.2 | 95.9 |
| 11.6 | 0xa0 | -1117.75 -129.99 2.49 262.6 | -1102.92 -129.03 2.01 261.5 | 14.87 -0.49 -1.1 -1.4 | 95.9 | 94.7 |
| 11.8 | 0x80 | -1116.05 -148.94 4.15 258.4 | -1100.75 -147.78 3.25 257.2 | 15.35 -0.90 -1.2 -1.4 | 95.2 | 93.5 |
| 12.0 | 0xa0 | -1112.17 -168.90 6.43 247.1 | -1096.80 -165.96 4.66 247.8 | 15.65 -1.77 0.7 0.0 | 101.7 | 93.5 |
| 12.2 | 0xa0 | -1106.05 -188.54 7.59 241.4 | -1090.71 -185.11 4.66 239.3 | 15.73 -2.93 -2.1 -2.8 | 102.8 | 98.8 |
| 12.4 | 0x80 | -1098.01 -207.68 9.29 238.6 | -1082.03 -203.51 5.98 238.1 | 16.52 -3.31 -0.5 -1.4 | 103.8 | 100.8 |
| 12.6 | 0x80 | -1088.69 -226.20 9.67 238.6 | -1072.24 -221.94 5.50 238.2 | 16.98 -4.17 -0.4 -1.4 | 103.6 | 114.2 |
| 12.8 | 0x80 | -1077.89 -245.46 9.37 238.6 | -1060.99 -241.11 4.89 238.5 | 17.45 -4.48 -0.1 -1.4 | 110.4 | 109.2 |
| 13.0 | 0xa0 | -1065.95 -264.10 8.81 228.7 | -1048.60 -259.66 4.38 228.4 | 17.91 -4.42 -0.3 -1.4 | 110.7 | 109.0 |
| 13.2 | 0xa0 | -1051.65 -280.94 8.05 214.6 | -1033.83 -276.39 3.71 214.8 | 18.39 -4.33 0.2 0.0 | 110.5 | 111.9 |
| 13.4 | 0x80 | -1035.31 -294.91 6.97 208.9 | -1017.21 -290.12 2.73 209.1 | 18.73 -4.24 0.2 0.0 | 107.5 | 105.8 |
| 13.6 | 0x80 | -1018.01 -307.05 6.10 214.6 | -999.97 -301.87 1.89 214.5 | 18.77 -4.21 -0.1 -1.4 | 105.7 | 103.4 |
| 13.8 | 0x90 | -1001.21 -319.47 5.65 225.9 | -983.41 -313.77 1.64 225.8 | 18.68 -4.00 -0.1 -1.4 | 104.5 | 100.8 |
| 14.0 | 0x80 | -985.84 -332.86 6.58 228.7 | -968.39 -326.63 2.34 228.9 | 18.53 -4.24 0.2 0.0 | 101.9 | 98.2 |
| 14.2 | 0x80 | -971.29 -346.84 7.69 225.9 | -954.16 -339.79 3.90 225.7 | 18.52 -3.80 -0.2 -1.4 | 100.9 | 97.6 |
| 14.4 | 0x80 | -956.68 -360.33 10.52 223.1 | -940.08 -352.77 5.79 224.4 | 18.23 -4.73 1.4 0.0 | 99.4 | 96.1 |
| 14.6 | 0x80 | -942.10 -373.65 12.31 221.6 | -925.96 -365.36 8.20 222.9 | 18.15 -4.11 1.3 0.0 | 98.8 | 95.0 |
| 14.8 | 0x80 | -927.02 -386.09 15.77 218.8 | -911.68 -377.44 10.96 219.4 | 17.61 -4.81 0.5 0.0 | 97.8 | 94.3 |
| 15.0 | 0xa0 | -911.17 -397.64 17.94 206.1 | -896.57 -388.29 13.12 207.8 | 17.34 -4.82 1.7 1.4 | 98.0 | 93.2 |
| 15.2 | 0xa0 | -893.90 -406.29 19.33 192.0 | -880.21 -396.45 14.32 193.4 | 16.86 -5.02 1.4 0.0 | 96.5 | 90.6 |
| 15.4 | 0xa0 | -874.77 -411.41 19.24 176.5 | -862.15 -401.11 14.37 179.0 | 16.29 -4.87 2.5 1.4 | 99.0 | 95.6 |
| 15.6 | 0x80 | -854.07 -412.61 18.38 170.8 | -842.54 -402.05 13.58 171.5 | 15.63 -4.80 0.7 0.0 | 103.7 | 101.2 |
| 15.8 | 0x80 | -832.01 -411.38 16.63 173.6 | -821.73 -400.71 11.58 172.8 | 14.81 -5.05 -0.8 -1.4 | 110.5 | 107.7 |
| 16.0 | 0x80 | -808.82 -409.16 14.31 173.6 | -799.90 -398.10 9.54 172.2 | 14.21 -4.77 -1.5 -2.8 | 116.4 | 112.3 |

Mean 10.5, max 18.8, 26 edges placed and 23 moved off the centre tick. The port still touches the grass with its left wheels at 9.6 to 10.2 s (11 units inside the line, 83 against 90 through 10.0 to 11.0) and from 11.6 s it runs 4 to 5 units below the recorded z, the low side of the banked ramp.

What the fifth run leaves:

- the bank slide above
- the turn response shape, the port builds the yaw rate a little slower on a press and keeps it a little longer after the release, so each corner leaves it one to two units wide of the recorded line even with the edges placed, 11 units of lateral offset by 9 s

## The sixth run, 2026-09-15, the bank slide was the harness

The fifth run left a slide down the banked back straight (14 to 26 s, cell normals 24 to 28 degrees off the vertical) of about 3.6 units per second beyond the recording, 1.2 to 1.3 units per sample against 0.45 elsewhere. Four candidate terms went to the bytes first, none differs from the port (TICK_HELPERS.md round nine, RIGID_BODY.md sixth pass, SUSPENSION_AND_TELEPORT.md the tyre frame). Then the harness itself was read:

- The resync teleport posed the body with a yaw only quaternion. On the bank that set the body flat over a road leaning 25 degrees, all four hubs rose over their clearance and the tyres carried no load for two ticks, then the body dropped onto the bank, lost 1 to 2 units per second and took a lateral kick from the tyre normals. The trace showed it, the force sum 0 and the R z z term 0.955 right after a teleport against 0.931 before it. `resync_pose` now turns R about world z by the yaw delta and moves T, the roll and the pitch survive the teleport
- The resync yaw went to the recorded byte. The recorder truncates 255 over 360 times the yaw (0x49FB69), so the byte sits 0 to 1.41 degrees under the real yaw and the teleport headed the port 0.7 degrees low on average. At 183 degrees low is toward plus y wire, the downhill side of that bank, 0.7 degrees at 120 units per second is 1.5 units per second of crab. The resync yaw is the byte centre now

With the two harness fixes the per sample error over the race reads 0.329 mean 1.979 max (0.322 mean 1.140 max before the finish, was 0.447 and 1.340), the bank 14 to 26 s reads 0.397 mean with the vertical gap minus 0.025 (was 0.5 to 1.3 per sample and minus 0.13 to minus 0.5), and the plain straight 17.2 to 18.6 s reads 0.23 to 0.29 per sample, the same as the flat. With a resync every five samples the lateral gap over the five samples between two teleports reads 0.08 0.11 0.14 0.20 0.27 (17.2 to 18.0 s) and 0.05 0.08 0.10 0.14 0.28 (25.2 to 26.0 s), was 0.6 1.0 1.5 2.1 2.7, the residual 0.3 units per second is inside the byte. Samples that follow a steer tap still grow 1.4 to 2.1 units over the second (21.2 to 22.0 s and 23.2 to 24.0 s), that is the turn response below, not a bank term. The free run with `--edges` is unchanged, 10.5 mean 18.8 max through 16 s.

The two alternative readings of the substep pushes were run for the record and both are worse than the bytes: the R column push taken along world z reads 0.618 mean on the bank with the port 0.19 under the recorded z, the world push taken along the body up reads 0.465 with the port 0.09 over it, the port as read stays at 0.397 and minus 0.025.

The largest per sample error before the finish is now 19.2 s, 1.14, the accelerator release on the back straight (mask 0x80 at 18.6, 0x00 at 18.8, no side bit to pin it). The recording falls from 120 to 85 units per second in 0.8 s with nothing pressed (the throttle factor gap clause over the turn force cap) and the port falls the same way but starts on the centre tick of the ten tick window, an accelerator edge `--edges` does not place.

The turn response. The 3.0 s press traced tick by tick with `--centre --flags --edges` (the right key on ticks 2.70 to 2.88): the steer channel climbs 0.0152 per tick from its floor 0.03 (dt times 0.76 over 14 substeps, the client ramp of 0x4EEBB0), the spin z builds 0.105 0.225 0.343 0.456 0.560 0.653 0.734 0.803 0.862 0.910 rad per second over the ten ticks and decays 0.739 0.489 0.334 0.225 0.143 0.078 0.028 after the release, the smoothed yaw reads 1.16 at 2.8, 8.10 at 3.0, 9.12 peak at 3.12, 8.96 at 3.2 and 7.95 at 3.4 against the bytes 1.4 8.5 8.5 7.1. The recorded byte is truncated, its real value sits in [1.41 2.82) [8.47 9.88) [8.47 9.88) [7.06 8.47), the port sits 0.25 and 0.37 degrees under the floor on the two build up samples and inside the interval on the two decay samples, and both turn 7 degrees inside the 2.8 to 3.0 s sample (6.94 against 7.1). Over the race with the resync the samples under a steer key read 0.363 mean with the yaw 1.03 degrees off the byte, the samples without 0.307 and 0.77 degrees, so a corner costs 0.06 units per sample, inside the 1.41 degree byte and the one tick edge window. The channel ramp, the decay, the clamp, the Ackermann swing and the tyre model read on the bytes again this run and match, nothing is located and the difference may not exist.

The 14 to 26 s section with `--centre --flags --edges --resync`, the fifth run convention, the yaw gap is the continuous one then the byte one:

| t | mask | rec x y z yaw | port x y z yaw | gap horiz vert yaw byte | rec speed | port speed |
|---|---|---|---|---|---|---|
| 14.0 | 0x80 | -985.84 -332.86 6.58 228.7 | -985.98 -332.52 6.53 228.5 | 0.37 -0.05 -0.2 -1.4 | 101.89 | 99.86 |
| 14.2 | 0x80 | -971.29 -346.84 7.69 225.9 | -971.37 -346.47 7.61 226.4 | 0.38 -0.08 0.6 0.0 | 100.91 | 98.89 |
| 14.4 | 0x80 | -956.68 -360.33 10.52 223.1 | -956.89 -360.00 10.45 224.1 | 0.39 -0.07 1.1 0.0 | 99.41 | 98.00 |
| 14.6 | 0x80 | -942.10 -373.65 12.31 221.6 | -942.19 -373.45 12.25 222.1 | 0.22 -0.06 0.4 0.0 | 98.75 | 97.22 |
| 14.8 | 0x80 | -927.02 -386.09 15.77 218.8 | -927.29 -385.78 15.69 218.5 | 0.42 -0.08 -0.3 -1.4 | 97.76 | 97.13 |
| 15.0 | 0xa0 | -911.17 -397.64 17.94 206.1 | -911.33 -397.22 17.80 207.4 | 0.45 -0.14 1.3 0.0 | 98.04 | 96.45 |
| 15.2 | 0xa0 | -893.90 -406.29 19.33 192.0 | -894.22 -405.98 19.26 193.1 | 0.44 -0.08 1.1 0.0 | 96.55 | 94.75 |
| 15.4 | 0xa0 | -874.77 -411.41 19.24 176.5 | -875.07 -411.27 19.22 178.5 | 0.33 -0.03 2.1 1.4 | 99.01 | 99.24 |
| 15.6 | 0x80 | -854.07 -412.61 18.38 170.8 | -854.61 -412.61 18.43 171.6 | 0.54 0.04 0.8 0.0 | 103.68 | 104.04 |
| 15.8 | 0x80 | -832.01 -411.38 16.63 173.6 | -832.57 -411.61 16.76 174.0 | 0.61 0.13 0.3 0.0 | 110.51 | 110.72 |
| 16.0 | 0x80 | -808.82 -409.16 14.31 173.6 | -809.44 -409.34 14.41 174.1 | 0.64 0.09 0.4 0.0 | 116.44 | 116.30 |
| 16.2 | 0x80 | -784.94 -406.00 13.02 173.6 | -785.57 -406.15 13.09 175.2 | 0.66 0.07 1.6 1.4 | 120.48 | 116.08 |
| 16.4 | 0x90 | -761.16 -403.45 10.74 180.7 | -761.67 -403.34 10.71 178.1 | 0.53 -0.02 -2.6 -2.8 | 119.57 | 119.43 |
| 16.6 | 0x90 | -737.16 -402.42 10.31 192.0 | -737.69 -402.51 10.36 191.2 | 0.54 0.05 -0.8 -1.4 | 120.11 | 117.81 |
| 16.8 | 0x80 | -713.16 -403.58 10.81 189.2 | -713.65 -403.39 10.72 190.1 | 0.53 -0.09 0.9 0.0 | 120.13 | 118.57 |
| 17.0 | 0x80 | -689.16 -405.36 11.76 186.4 | -689.48 -405.12 11.64 186.3 | 0.41 -0.12 -0.1 -1.4 | 120.32 | 119.81 |
| 17.2 | 0x80 | -665.16 -406.93 12.91 184.9 | -665.44 -406.85 12.87 185.3 | 0.29 -0.04 0.4 0.0 | 120.27 | 120.13 |
| 17.4 | 0x80 | -641.15 -408.36 13.71 183.5 | -641.42 -408.34 13.69 185.0 | 0.26 -0.02 1.4 1.4 | 120.23 | 120.11 |
| 17.6 | 0x80 | -617.15 -409.56 14.39 183.5 | -617.39 -409.47 14.35 183.9 | 0.26 -0.04 0.4 0.0 | 120.19 | 120.03 |
| 17.8 | 0x80 | -593.14 -410.62 14.82 183.5 | -593.38 -410.55 14.79 183.6 | 0.25 -0.03 0.1 0.0 | 120.15 | 120.17 |
| 18.0 | 0x80 | -569.14 -411.49 14.95 182.1 | -569.38 -411.48 14.94 183.4 | 0.24 -0.01 1.3 0.0 | 120.08 | 119.86 |
| 18.2 | 0x80 | -545.13 -411.99 15.09 182.1 | -545.37 -411.93 15.06 182.1 | 0.24 -0.02 0.0 0.0 | 120.07 | 119.98 |
| 18.4 | 0x80 | -521.13 -412.28 14.79 180.7 | -521.36 -412.28 14.80 181.8 | 0.23 0.01 1.1 0.0 | 120.02 | 119.96 |
| 18.6 | 0x80 | -497.13 -412.15 14.39 179.3 | -497.36 -412.13 14.38 180.3 | 0.23 -0.01 1.0 0.0 | 120.00 | 119.91 |
| 18.8 | 0x00 | -473.12 -411.65 13.79 179.3 | -473.47 -411.61 13.77 179.2 | 0.35 -0.02 -0.1 -1.4 | 120.05 | 118.96 |
| 19.0 | 0x00 | -449.13 -410.72 13.04 177.9 | -449.64 -410.76 13.06 178.6 | 0.51 0.02 0.8 0.0 | 120.06 | 117.65 |
| 19.2 | 0x00 | -426.97 -409.34 12.41 176.5 | -428.11 -409.40 12.44 177.6 | 1.14 0.02 1.2 0.0 | 111.03 | 98.64 |
| 19.4 | 0x00 | -407.58 -407.84 11.41 176.5 | -408.01 -407.84 11.41 176.4 | 0.43 0.00 -0.0 -1.4 | 97.23 | 90.11 |
| 19.6 | 0x00 | -390.11 -406.14 10.22 175.1 | -390.52 -406.20 10.26 175.9 | 0.42 0.04 0.8 0.0 | 87.76 | 82.11 |
| 19.8 | 0x10 | -373.16 -404.29 9.00 177.9 | -373.19 -404.33 9.02 178.0 | 0.06 0.02 0.1 0.0 | 85.27 | 85.10 |
| 20.0 | 0x10 | -356.13 -403.45 8.29 187.8 | -356.53 -403.53 8.33 187.7 | 0.40 0.05 -0.1 -1.4 | 85.21 | 82.49 |
| 20.2 | 0x80 | -339.19 -403.86 8.48 187.8 | -339.47 -403.76 8.43 188.8 | 0.30 -0.05 1.0 0.0 | 84.72 | 84.10 |
| 20.4 | 0x80 | -321.66 -404.76 8.77 184.9 | -321.97 -404.61 8.68 185.8 | 0.35 -0.08 0.8 0.0 | 87.79 | 87.50 |
| 20.6 | 0x80 | -303.32 -405.53 8.94 183.5 | -303.71 -405.43 8.89 183.9 | 0.40 -0.05 0.3 0.0 | 91.77 | 91.22 |
| 20.8 | 0x80 | -284.22 -406.01 8.97 183.5 | -284.69 -406.02 8.98 185.0 | 0.47 0.01 1.5 1.4 | 95.54 | 94.20 |
| 21.0 | 0x90 | -264.70 -407.07 9.28 193.4 | -265.23 -407.22 9.36 193.2 | 0.55 0.08 -0.2 -1.4 | 97.74 | 95.22 |
| 21.2 | 0x80 | -245.26 -409.55 10.41 194.8 | -245.70 -409.38 10.32 194.7 | 0.47 -0.09 -0.1 -1.4 | 98.00 | 95.95 |
| 21.4 | 0x80 | -225.71 -412.63 12.03 192.0 | -226.12 -412.39 11.89 192.4 | 0.47 -0.13 0.4 0.0 | 98.94 | 97.31 |
| 21.6 | 0xa0 | -205.92 -415.43 13.45 186.4 | -206.32 -415.26 13.36 187.3 | 0.43 -0.09 0.9 0.0 | 99.94 | 98.16 |
| 21.8 | 0x80 | -185.72 -416.97 14.20 180.7 | -186.24 -416.89 14.16 182.2 | 0.52 -0.04 1.5 1.4 | 101.29 | 98.88 |
| 22.0 | 0x80 | -165.07 -417.47 14.37 180.7 | -165.55 -417.44 14.36 180.8 | 0.47 -0.01 0.1 0.0 | 103.29 | 103.17 |
| 22.2 | 0x80 | -142.94 -417.37 14.29 179.3 | -143.49 -417.40 14.30 180.8 | 0.55 0.01 1.5 1.4 | 110.65 | 110.68 |
| 22.4 | 0x80 | -119.48 -416.82 14.12 179.3 | -120.19 -416.77 14.09 179.4 | 0.72 -0.03 0.1 0.0 | 117.35 | 115.85 |
| 22.6 | 0x80 | -95.48 -415.95 13.73 179.3 | -95.96 -416.00 13.76 180.8 | 0.49 0.03 1.5 1.4 | 120.10 | 119.10 |
| 22.8 | 0x90 | -71.47 -415.60 13.59 189.2 | -71.80 -415.84 13.72 189.4 | 0.41 0.12 0.2 0.0 | 120.02 | 118.98 |
| 23.0 | 0x80 | -47.47 -417.08 14.42 192.0 | -47.96 -416.97 14.36 191.9 | 0.50 -0.06 -0.1 -1.4 | 120.24 | 118.58 |
| 23.2 | 0x80 | -23.47 -419.58 15.93 189.2 | -23.87 -419.27 15.76 189.3 | 0.50 -0.17 0.1 0.0 | 120.64 | 119.76 |
| 23.4 | 0x80 | 0.53 -422.11 17.60 187.8 | 0.21 -421.94 17.50 187.9 | 0.36 -0.10 0.1 0.0 | 120.68 | 120.28 |
| 23.6 | 0x80 | 24.54 -424.48 19.05 186.4 | 24.25 -424.41 19.02 187.6 | 0.30 -0.04 1.2 0.0 | 120.62 | 120.39 |
| 23.8 | 0x80 | 48.54 -426.72 20.14 186.4 | 48.28 -426.62 20.09 186.5 | 0.28 -0.05 0.1 0.0 | 120.54 | 120.43 |
| 24.0 | 0x80 | 72.54 -428.65 21.21 184.9 | 72.27 -428.60 21.18 186.1 | 0.27 -0.03 1.1 0.0 | 120.38 | 120.36 |
| 24.2 | 0x80 | 96.54 -430.18 22.45 184.9 | 96.28 -430.10 22.40 185.1 | 0.28 -0.05 0.2 0.0 | 120.25 | 120.20 |
| 24.4 | 0x80 | 120.55 -431.65 23.26 184.9 | 120.29 -431.62 23.24 185.3 | 0.26 -0.02 0.4 0.0 | 120.27 | 120.17 |
| 24.6 | 0x80 | 144.55 -432.95 23.85 183.5 | 144.31 -432.93 23.85 184.8 | 0.25 -0.01 1.2 0.0 | 120.19 | 120.10 |
| 24.8 | 0x80 | 168.56 -433.97 24.35 183.5 | 168.32 -433.89 24.32 183.4 | 0.25 -0.04 -0.1 -1.4 | 120.12 | 120.00 |
| 25.0 | 0x80 | 192.56 -434.69 24.70 182.1 | 192.32 -434.69 24.70 183.2 | 0.24 -0.00 1.1 0.0 | 120.07 | 119.97 |
| 25.2 | 0x80 | 216.55 -435.04 25.04 180.7 | 216.32 -435.00 25.02 181.8 | 0.24 -0.02 1.1 0.0 | 119.99 | 119.94 |
| 25.4 | 0x80 | 240.56 -435.02 25.42 180.7 | 240.32 -434.96 25.38 181.1 | 0.25 -0.04 0.4 0.0 | 120.04 | 119.93 |
| 25.6 | 0x80 | 264.57 -434.85 25.40 180.7 | 264.33 -434.82 25.39 180.7 | 0.24 -0.01 0.0 0.0 | 120.02 | 119.94 |
| 25.8 | 0x80 | 288.53 -434.37 25.31 179.3 | 288.27 -434.39 25.31 179.8 | 0.27 0.01 0.5 0.0 | 119.86 | 118.26 |
| 26.0 | 0xa0 | 312.47 -433.17 24.86 172.2 | 312.23 -432.94 24.75 172.8 | 0.34 -0.10 0.5 0.0 | 119.85 | 120.09 |

What the sixth run leaves:

- the accelerator edges, `--edges` places the steer edges only, the release at 18.6 to 18.8 s costs 1.14 on one sample
- the turn response residual above, 0.06 units per sample under a steer key, inside the byte

## The seventh run, 2026-09-15, the wire numbering

The stat feed was two places off since the first run: the port's `KartStatIndex` counted from car+0x3440 where the record stat block lands at car+0x3448 (`car_apply_kart_loadout` 0x490A70 copies the 0xC0 record from byte 0 to car+0x33A4), so every formula read the wire float two slots up. On the recorded row that changed four physics inputs: the steering gain 0.30 to 0.52 (channel rate 0.76 to 1.02), the turn force 0.30 to 0.52, the drift steer 0.70 to 0.52, the mini turbo hold 0.0 to 0.70. The grip went 9.0 (the camera distance) to 0.52, which only touches the random wheel bump in the port. Nothing else changed, the launch curve is the same since the max speed stays on 0.52 and the velocity gain on 0.52.

`--centre --flags`, 20 ms ticks, before and after, the recorded yaw then the port yaw and the byte gap:

| t | mask | rec yaw | port before | gap | port after | gap |
|---|---|---|---|---|---|---|
| 2.8 | 0x80 | 1.4 | 0.7 | -1.4 | 1.0 | -1.4 |
| 3.0 | 0x90 | 8.5 | 7.7 | -1.4 | 9.5 | 0.0 |
| 3.2 | 0x80 | 8.5 | 9.0 | 0.0 | 10.9 | 1.4 |
| 3.4 | 0x80 | 7.1 | 7.5 | 0.0 | 9.0 | 1.4 |
| 4.4 | 0xa0 | 2.8 | 4.5 | 1.4 | 4.9 | 1.4 |
| 4.6 | 0xa0 | 347.3 | 351.0 | 2.8 | 350.1 | 2.8 |
| 4.8 | 0xa0 | 340.2 | 338.9 | -1.4 | 337.5 | -2.8 |
| 5.0 | 0x80 | 343.1 | 339.9 | -4.2 | 338.7 | -5.6 |
| 9.0 | 0xa0 | 300.7 | 299.6 | -1.4 | 293.0 | -8.5 |

The first turn now comes out in one sample: the 0.2 s press at 3.0 s turns the port 8.5 degrees inside the sample (1.0 to 9.5) where the recording turns 7.1 (1.4 to 8.5), the old feed managed 7.0. The port overshoots by 1.0 at 3.0 s and 2.4 at 3.2 s, the release side of the tap. The held right turn of 4.4 to 4.8 reads 4.9 350.1 337.5 338.7 against 2.8 347.3 340.2 343.1 recorded, the come back at 5.0 is 4.4 degrees short where it was 3.2 before. The speed through the 9 s hairpin drops to 76.6 at 9.0 s against 94.4 recorded (90.5 with the old feed), the port now leaves the road at 11 s and stops against the world at 12 s (the speed reads 13 then 1.3), the free run over 16 s reads mean 73.0 max 350 against 22.1 and 56.4 before.

With `--resync`, the per sample error, the old feed reproduced with `--stats 0.52 0.52 0.30 0.52 0.30 0.30 0.52 0.52 0.52 0.70 0.52 0.0 9.0 37.0 3.5 0 0` on the same build (16 s: 0.251 and max 0.925, the same numbers the old build printed):

| window | samples | before mean | before max | after mean | after max |
|---|---|---|---|---|---|
| 10 s | 50 | 0.203 | 0.463 | 0.237 | 0.575 |
| 16 s | 80 | 0.251 | 0.925 | 0.284 | 0.940 |
| the race | 660 | 0.255 | 0.963 | 0.271 | 0.940 |

Split per stat over the race with the same overrides: the steering gain alone gives 0.276, the turn force alone 0.251, the drift steer and hold alone 0.255, so the whole move is the steering gain. The resync mean was at the 0.25 line before and sits 0.02 over it now, not tuned: the feed is now what the bytes say, the tick formulas are as the bytes say, so the extra yaw of a held key at 1.02 against the recorded 1.35 rad per second and the speed loss in the long turns are the tyre model's, for the body owner (the fifth run already had both on its list with the old rate).

## What a drift recording must show

The RageZone recording never drifts, so the drift path of the port (`drift.cpp`, TICK_HELPERS.md round ten) is read on the bytes only, never compared to the client. The next recording must hold drifts. It was recorded on 2026-09-15, the section after this one has the numbers. Record a race on the stock client against our server, drift on every corner with the drift key (slot 5 of the input manager 0xF1DA80 bindings) plus a steer key held for over a second, release the drift key, lift the gas and press it again inside 1.5 s for the mini turbo. The server keeps the upload in `ghost_record` and `ghost_replay_chunk`, the recipe to pull it and to write the `KCGR` header is in docs/tools/README.md, the ghost replay section.

What the samples carry, one every ten ticks of 20 ms, as `car_ghost_sample_record` 0x49FAD0 writes them:

- the input mask bit 3 (0x08) is slot 5, the drift key, read through `input_key_down` at the top of the tick. Bit 7 is the gas, bit 5 slot 2 (left with the camera not reversed), bit 4 slot 3 (right). A drift sample reads 0x98 (gas right drift) or 0xA8 (gas left drift). The mini turbo needs the gas bit to drop to 0x08 or 0x00 then come back, the rising edge sits between two samples and the mask cannot show it closer than that
- the status flags 0x100 is drift state 1 (left, gauge positive), 0x200 state 2 (right, gauge negative), 0x10 state 0. 0x08 is mini turbo stage 1, the stage that arms 0.35 s into the drift for basic 1 (wire stat 11 is 0.70 so the hold is 352 ms, the tenth pass wrote 0.8 s off wire 13 which is 0). Stages 2 and 3 are not recorded, stage 2 lasts from the key release to the gas edge, stage 3 one tick. 0x80 is a kind 0 boost, the mini turbo (or a kind 0 pad, Race 01 has one at 4.5 s on the recorded line), 0x40 any other kind. 0x02 and 0x04 are the steer keys of the poll one tick before the record
- the low nibble of the nibbles byte is the gauge, `(gauge plus 45) times 15 over 90` truncated, 7 is gauge 0, 0 is minus 45 (a full right drift), 15 is plus 45 (a full left one). The gauge fills at (wire stat 8 times 0.5 plus 0.3) times 80 a second, 44.8 a second for basic 1, so a full drift shows the nibble walk 7 6 5 4 3 2 1 0 over one second of samples, one nibble step is 6 gauge units, 0.13 s
- the yaw byte is the smoothed yaw car+0x3220, the motion angle is atan2 of the position delta between two samples in the wire frame, forward is (minus cos A, sin A) so the motion angle is `atan2(dy, minus dx)` in degrees. The slip angle is the yaw minus the motion angle wrapped to plus minus 180, positive when the nose points right of the motion

What the port predicts for a right drift held one second from 90 km per hour (basic 1 stats, TICK_HELPERS.md round ten), the recording proves or corrects each line:

| Sample | Mask | Flags | Nibble | Yaw | Motion | Slip | Gauge |
|---|---|---|---|---|---|---|---|
| drift start | 0x98 | 0x200 | 6 | turning 10 to 15 degrees a sample | lags by 1 | 1 | minus 4 to minus 8 |
| 0.4 s in | 0x98 | 0x200 | 3 | 35 past the start | 26 | 7 | minus 21 |
| 0.8 s in | 0x98 | 0x200 or 0x208 | 0 | 65 past | 52 | 12 | minus 39 |
| 1.0 s in | 0x98 | 0x208 | 0 | 74 past | 60 | 16 | minus 44 |
| release, key up | 0x90 or 0x80 | 0x208 then 0x10 | 4 then 7 | 13 more degrees over 0.2 s, the unwinding | catches up | 3 | minus 18 then 0 |
| gas edge inside 1.5 s | 0x00 then 0x80 | 0x90 | 7 | steady | steady | under 5 | 0 |

Seventh run note: the table was computed with the old stat feed. With the wire numbering stage 1 arms 0.35 s in (the 0x208 flag shows from the 0.4 s sample), the drift steer is 1.51 not 1.62, the steer channel rate 1.02 not 0.76, and `tests/drift_test.cpp` prints the current prediction: 73 degrees at the release after one second, slip 16.3, gauge 43.9, stage 1 at tick 63, the unwinding 16 degrees.

The three things to read first on a real drift, in this order:

1. the yaw rate under 0x98. The port turns 15 degrees a sample (74 degrees in a second) with the front wheels at 64 degrees (the 2 times 2 steer scale of 0x49B1F9 on the 16 degree catalogue steer, near the Ackermann pole at 63 degrees) and the spin add of up to 1.4 radians a second a tick. If the recording turns much less, the steer gains or the spin add are not read right, the tick side owns them
2. the slip. The port body slips 10 to 17 degrees at a full gauge, the model on top turns a further 1.62 times the smoothed gauge (up to 73 degrees) which the recording cannot show. If the recorded motion lags the yaw by far more than 17 degrees, the rear grip pair is not the catalogue 0x134 in a drift, or the drift scales both pairs and not the front alone
3. the unwinding. After the key goes up the port turns 13 more degrees toward the drift side over 10 ticks while the gauge walks to 0 (nibble 4 then 7), through `car_body_set_yaw` which snaps R. The recorded yaw must show that step, if it does not the unwinding is a body pose write the port has wrong

Command line, the RageZone options plus the drift flags the status bits arm:

```
build-kartclient\replay\Release\ghost_compare.exe "<client>\Data\Public\World\Race\Race_01" <drift ghost> --seconds 90 --centre --flags --edges
build-kartclient\replay\Release\ghost_compare.exe "<client>\Data\Public\World\Race\Race_01" <drift ghost> --seconds 90 --centre --flags --edges --resync
build-kartclient\replay\Release\ghost_compare.exe "<client>\Data\Public\World\Race\Race_01" <drift ghost> --seconds 12 --centre --flags --trace
```

`--flags` reads the 0x100 0x200 bits into the drift state and the drift key, the gauge nibble into the gauge at the state change, the 0x08 bit into stage 1 and the 0x80 bit into a kind 0 boost, so a mini turbo the mask cannot place still starts on its sample. With `--resync` the per sample error under a drift sample is the number to read, a resync every sample hides the yaw rate, so read the free run through the first drift as well. The scripted mode drives the port alone with the same keys:

```
build-kartclient\replay\Release\ghost_compare.exe "<client>\Data\Public\World\Race\Race_01" --script drift.txt --start-row 7 --seconds 3 [--trace] [<ghost> for the spawn]
```

A script line is `time accel brake left right drift item`, the keys hold until the next line, a hash starts a comment, the gas edge for the mini turbo is a line with the gas at 0 then one with it at 1. It prints every 0.2 s the position, the yaw, the motion angle, the slip, the gauge and its smoothed copy, the drift state, the mini turbo stage, the boost state and kind, the km per hour, the spin z, the front steer tangent, the roll, the pitch and the model slip turn, and one line on every state change. `tests/drift_test.cpp` runs the same script and keeps the numbers above green.

## The drift recording, 2026-09-15

DaveDebile (char 7) on Race 01, kart basic 1 (template 10010, the same 17 wire stats as the RageZone row, checked on the 0xC0 frame of the login burst), the ghost stage 15 with the RageZone ghost ahead, three laps in 233.4 s, 1171 samples. The stock client was driven by `tools/stock_drive.py` through the pilot held keys (docs/tools/README.md, clientpatch): the follow line at 20 Hz, and on the two long left turns a scripted drift, the drift key with the left steer and the gas held 0.8 s, the key up, the gas lifted 0.12 s and pressed again. 37 drift runs, 173 samples with a drift state, 152 with the mask 0xA8. The server keeps the faster stored time (`GhostPackets::saveRecord`) so this slower run was not stored, the file `reference/ghost/drift_t90_c7_20260915.ghost` (outside git, `reference/` is ignored) was read from the client ring instead, car+0x3754, 28 bytes a sample, the count at car+0xA7858, the same bytes `car_ghost_sample_record` uploads (`python tools/stock_drive.py ring <file>` after the finish). The runs of one second and the 0xA8 mask read on the harness:

```
build-kartclient\replay\Release\ghost_compare.exe "<client>\Data\Public\World\Race\Race_01" reference\ghost\drift_t90_c7_20260915.ghost --seconds 140 --centre --flags --edges --resync
```

What the samples show, the first clean run at 90 units a second (samples 57 to 61, 11.4 to 12.2 s) as the example:

| Sample | Mask | Flags | Nibble | Gauge | Yaw | dYaw | Motion | Slip | Speed |
|---|---|---|---|---|---|---|---|---|---|
| 56 | 0x80 | 0x014 | 7 | 0 | 321.9 | | -31.4 | -6.7 | 89.9 |
| 57 | 0xA8 | 0x104 | 8 | 3 | 312.0 | -9.9 | -37.5 | -10.5 | 90.1 |
| 58 | 0xA8 | 0x104 | 9 | 9 | 302.1 | -9.9 | -44.8 | -13.1 | 87.8 |
| 59 | 0xA8 | 0x10C | 11 | 21 | 292.2 | -9.9 | -53.0 | -14.8 | 83.3 |
| 60 | 0xA8 | 0x10C | 12 | 27 | 280.9 | -11.3 | -61.8 | -17.3 | 78.7 |
| 61 | 0x10 | 0x102 | 11 | 21 | 266.8 | -14.1 | -71.7 | -21.5 | 74.8 |
| 62 | 0x90 | 0x092 | 7 | 0 | 261.2 | -5.6 | -86.9 | -12.0 | 77.8 |
| 63 | 0x90 | 0x092 | 7 | 0 | 276.7 | +15.5 | -91.9 | 8.6 | 84.2 |
| 64 | 0x90 | 0x092 | 7 | 0 | 293.6 | +16.9 | -81.8 | 15.4 | 83.0 |
| 65 | 0x50 | 0x092 | 7 | 0 | 307.8 | +14.1 | -69.4 | 17.2 | 80.8 |
| 66 | 0x50 | 0x012 | 7 | 0 | 320.5 | +12.7 | -56.5 | 17.0 | 77.1 |

- the drift yaw rate. Under 0xA8 with the state on the yaw moves 10 to 16 degrees a sample, 10.5 on average over the 132 such samples (52 degrees a second), 9.9 9.9 9.9 11.3 14.1 in the run above at 90 units a second, 14.1 11.3 12.7 14.1 in run 42 to 46 at 56. The first sample of a run is smaller (3 to 10) while the gauge is under 9. The table of the previous section predicted 15 a sample at a full gauge, the gauge never fills here, the key holds 0.8 s
- the gauge. The nibble walks 8 9 11 12 over the first four samples (3 9 21 27 gauge units), 30 to 60 units a second, 45 on average from the first sample to the fourth, the 44.8 a second fill of the previous section holds. The nibble never passes 13 (33), the highest gauge of the 37 runs
- stage 1. The 0x08 flag shows on the third drift sample of every run of four or more (76 samples), 0.4 to 0.6 s after the key went down, the 0.35 s arm of the previous section sits inside that window
- the slip. The nose leads the motion by 10.5 on the first drift sample then 13 15 17 and 21.5 on the fifth, the largest slip of a run averages 19.5 over the 33 runs, 22 to 24 at the most. The previous section put the body slip at 10 to 17 at the full gauge, the client slips more at a gauge of 27 to 33
- the speed. 90 to 88 83 79 75 units a second over the run, 17 percent lost in 0.8 s, the gas held the whole time
- the unwinding. The gauge drops from 21 to 33 at the release sample to 0 at the next sample in every run, so the decay takes under 0.2 s. In the release sample the yaw still moves 4 to 11 degrees toward the drift side against a held opposite steer (5.6 above, 4.2 in run 27 to 31, 11.3 in run 443 to 446), then the opposite steer swings it back 15 to 17 a sample. With the drift side steer kept after the release the yaw keeps moving 13 to 20 in the release sample and 23 to 28 in the boost sample after it (runs 42, 391, 603), a spin the follower had to catch
- the mini turbo. `car_drift_state_set(0)` at the gauge zero crossing (0x49A9E0) writes 0 to the stage car+0x35F0, so the 1500 ms window of 0x5EB70C only lives while the gauge decays, under 0.2 s after the key goes up. The gas edge on the sample right after the release fired the kind 0 boost in 6 of 6 runs with no other boost running (42 57 140 391 443 603), the 0x80 flag stays four samples (0.8 s), the speed rises 3 then 6 units a second (74.8 77.8 84.2 above, 34 55 79 from the slow run 42). With a brake sample between the release and the edge (0x50 or 0x60) the boost fired in 0 of 20 runs, the edge came 0.6 to 1.2 s after the release, the state was long gone. With a pad boost running (flags 0x052, runs 257 656 671) no mini turbo, the `car+0x3300 == 0` gate of 0x49B08F
- the mask. 0xA8 152 samples, 0x98 3, the drift key never shows without the gas since the follower holds it, the brake 0x50 and 0x60 on 153 samples are the follower catching the nose after each release

The harness on it, `--centre --flags --edges --resync`, 700 samples of the first 140 s, the per sample error by the key held:

| Samples | n | gap units | yaw byte gap | port speed minus recorded |
|---|---|---|---|---|
| all | 700 | 0.61 (max 27.8) | 2.8 | -0.9 |
| the drift key down, mask bit 3 | 105 | 0.37 (max 3.0) | 1.27 (max 7.1) | -1.2 |
| the two samples after a release | 54 | 1.21 (max 5.8) | 4.8 (max 21) | -5.4 |
| the brake down, no drift | 108 | 1.67 (max 27.8) | 11.3 (max 126) | -13.2 |
| the gas alone | 477 | 0.42 (max 19.7) | 1.16 (max 12.7) | +1.9 |

What differs in the port under the drift, none of it changed here:

1. the drift samples are the best matched part of the run, 0.37 units and 1.27 degrees a sample against 0.42 and 1.16 on the plain gas samples, the yaw per sample under 0xA8 lands inside the byte on almost every sample once the state and the gauge come from the flags (5.6 to 6.2 s, 8.6 to 9.0 s, 11.6 to 12.2 s all read 0.0 to 1.4). The port sheds 3 to 4 units a second more than the client in the first two drift samples (92.3 against 96.0, 85.9 against 89.3 at 5.6 and 5.8 s)
2. the release is the weak spot, 4.8 degrees and 1.2 units a sample over the two samples after the key goes up, the port 5.4 units a second slower. Under the flags the port unwinds 2.7 degrees more than the client in the release sample (258.5 against 261.2 at 12.4 s) then turns 4.8 more under the opposite steer (278.8 against 276.7)
3. the mini turbo push. The port starts the kind 0 boost on the flag sample and jumps 10 units a second in that sample (84.7 against the client 77.8 at 12.4 s) where the client ramps 3 then 6 over two samples, a push shape not a strength
4. the brake right after a drift is the largest gap of the whole run and is not a drift term. Under 0x50 the port sheds 15 to 20 units a second in the first brake sample where the client sheds 4 to 14 (55.5 against 72.4 at 6.6 s, 59.4 against 77.1 at 13.2 s) and turns 7 to 8 degrees a sample under brake plus steer where the client turns 15 to 17 (322.1 against 331.8, 313.1 against 331.8), the brake force or its steer coupling in the port is off
5. the free run without `--resync` reads nothing on this recording. The follower steers at 20 Hz so the launch carries 0x90 0xA0 taps shorter than a sample, `--centre` and `--edges` stretch each to a sample and the port sits 18 degrees off at 0.8 s and leaves the road at 2.8 s. A hand driven recording with long presses is the one for the free run, the RageZone file stays that reference

The server saw the stock client mini turbos too, `RaceHandler` logged `drift boost violation miniturbo_fast` and `boost_unmatched` on the room race of the same session, the anti cheat window is shorter than what the real client does.

## The drift recording, closed, 2026-09-15

The five differences of the section above went to the bytes and to the harness, TICK_HELPERS.md round eleven has the reads. One was a physics term, the brake, one was the harness reading its own numbers wrong, the mini turbo push, and three sit inside the one tick edge window now.

The harness first. Three things it did hid the client:

- the port speed of a row was the speed at the end of the ten ticks, the recorded one is the mean over the sample. A boost that fires inside a sample reads 10 units on the first and 3 on the second, the mean reads 3 then 6 like the recording. The row prints the port mean now (the port distance over the ten ticks over 0.2 s) and the class table below uses it
- `--edges` placed the two steer edges only. The drift key, the gas and the brake have no second reading (no side bit), so their edge sat on the centre tick. Each is placed now inside its one sample window, the drift key by the recorded yaw and the gauge nibble of the next samples, the gas and the brake by the recorded mean speed of the next two samples. The 6 of 6 mini turbos of the client need the gas edge inside the unwinding, one of them (run 443, the release at 89.2 s) has its window closed two ticks before the centre tick, placed it fires
- `--flags` forced mini turbo stage 1 from the 0x08 bit even when the port had no drift state, so a stage could outlive the state and a gas edge up to 1.5 s later fired a boost the client never had. The stage is forced only inside a drift now, as the client zeroes it with the state
- `--resync-until T` stops the resync at T so a slice runs free from a true pose, `--trace-from T` starts the trace lines at T, the harness prints the drift state and stage changes under `--trace`, and a class table at the end: the drift key down, the two samples after a release, the brake down without a drift, the gas without a brake, each with the mean gap, the max, the mean yaw byte gap and the mean port minus recorded speed

The per sample error with `--centre --flags --edges --resync`, the whole race of 1170 samples (233 s), before is the port and the harness of the section above, harness is the three harness changes alone on the same port, after adds the brake rate:

| Samples | n | before gap | before yaw | harness gap | harness yaw | after gap | after yaw | after speed |
|---|---|---|---|---|---|---|---|---|
| all | 1170 | 0.715 (max 27.8) | | 0.668 | | 0.576 (max 28.0) | | |
| the drift key down | 160 | 0.510 | 1.70 | 0.528 | 1.51 | 0.522 | 1.39 | +0.54 |
| the two samples after a release | 82 | 1.162 | 4.56 | 0.933 | 2.94 | 0.680 | 1.53 | -0.65 |
| the brake down, no drift | 155 | 1.763 | 11.11 | 1.429 | 9.55 | 0.855 | 2.54 | -1.98 |
| the gas alone | 834 | 0.559 | 1.48 | 0.553 | 1.40 | 0.532 | 1.41 | -0.20 |

The same on the 700 samples of the table above (140 s): all 0.440, the drift 0.352 and 0.94 degrees, the release 0.699 and 1.46, the brake 0.737 and 2.34, the gas 0.395 and 1.10, was 0.61, 0.37 and 1.27, 1.21 and 4.8, 1.67 and 11.3, 0.42 and 1.16.

The RageZone regression: 664 samples read 0.266 mean 1.948 max (was 0.268 and 1.979 on the same build before the harness changes, 0.250 in the tenth pass on the older feed), no brake sample exists there so the brake rate does not move it. The RageZone free run with `--edges` through 16 s reads 14.3 mean 30.4 max (15.6 and 37.9 before). The plain recording of the same day (`plain_t90_c7_20260915.ghost`, 678 samples, no drift) reads 0.232 mean 2.10 max. The free run of the drift recording, `--centre --flags --edges` with no resync, holds inside 1.9 units and 1.6 degrees through 9.0 s, the first drift run, the brake sequence 6.4 to 7.2 s, the hairpin, then hits the wall at the hairpin apex at 9.2 s where it sits 5.5 units outside the recorded line, it left the road at 2.8 s before.

Per point:

1. The gas edge window. `car_drift_state_set(0)` at 0x49AA0D 0x49AA13 and the in place clear at 0x49AE1E 0x49AE24 both zero car+0x35F0, the stage 2 to 3 gate at 0x49B030 needs stage 2, the slot 0 rising edge and now minus the stage 2 stamp under 1500 ms, so the window is the unwinding of the gauge, 2.4 units a tick, 5 to 14 ticks for the gauges of 21 to 33 the recording holds. The port had all three lines, the scripted run with the edge 0.7 s after the release fires nothing. The 1.5 s of the docs was the window the port would allow if the state stayed, it never does. On the recording the port fires the kind 0 boost on 6 of the 6 client runs (9.22 12.30 28.86 79.04 89.32 121.54 s) and on none of the 20 runs with a brake between the release and the edge, plus one at 132.02 s where the client shows a kind 1 pad two ticks later (the port finds BOOST_011 at 132.14 s, the flags 0x52 cannot tell which came first)
2. The mini turbo push. `car_boost_start` 0x496D2B writes the kind 0 strength 6.0 and the target clamp(wire stat 3 times 0.2 plus 1, 1, 1.2) times 120 then at least km per hour plus 10.0, `car_boost_update` 0x496FB0 pushes `car_boost_push` 6.0 along the yaw minus 1.8 times the smoothed gauge every tick while the km per hour is under the target, `body_apply_force` 0x4EC420 is a plain velocity add on wheel set 0x1A0. Two pushes of 6 at 55 degrees off the motion give the port 7 units in two ticks, the sample means read 77.4 and 84.1 against 77.8 and 84.2 recorded at 12.4 and 12.6 s, the free slice from 12.2 s reads 77.4 82.4 82.9 84.3 against 77.8 84.2 83.0 80.8. The 10 unit jump was the end of sample speed against the sample mean, not a push shape. Under the resync each sample puts the velocity back under the target so the port pushes again, 2 to 3 units over the client on the two samples after the peak, a harness artefact
3. The brake. `body_set_mass_friction` 0x4EC180 is not {a, a, b, b, c, c}. The bytes at 0x4EC189 to 0x4EC1B2 store the first float in slots 0 and 5, the second in 1 and 4, the third in 2 and 3, so `car_physics_setup` 0x49510C gives the rates accel 5.0, brake 1.0, right 0.4, left 0.4, handbrake 1.0, reverse 5.0. The port ramped the brake channel at 5 per second, full in 0.2 s, the client takes 1.0 s, so the port put the whole 20000 and 30000 brake torques on the wheels ten ticks after the key, locked all four wheels (the spin rates 165 to 3 radians a second in six ticks) and a locked tyre pushes against its sliding velocity alone, the net tyre force read 108000 along the velocity and 1500 across, no steering. With the rate 1.0 the brake torque grows 2000 and 3000 a tick, the wheels roll, the fronts keep their lateral force and the car turns under the brake. The brake sequence 6.4 to 7.6 s reads 0.19 0.62 0.34 0.19 0.15 0.31 0.28 with the yaw inside the byte on every sample (was 0.15 0.72 1.52 1.18 0.82 with minus 17 and minus 13 degrees), the speeds 71.3 56.8 40.3 24.4 against 72.4 58.5 41.1 24.8, and 13.2 to 13.8 s reads 0.79 0.43 0.39 0.21 (was 1.90 1.99 1.14 0.76 with minus 19 to minus 13 degrees). The brake torques 0xEC 0xF0 0xF4 0xF8 (3000, 20000, 30000, 60000 for basic 1) reach wheel set 0x5A0 to 0x5AC through `body_load_wheel_config` 0x4EE91A and the caps 0x984 0x988 through `body_gear_update` 0x4EFBAE as the port has them, `gear_wheel_force_solve` 0x4EECD0 clamps spin times 0xEC to the cap per wheel as the port, the brake or reverse choice of `input_poll_keyboard` 0x497C7B reads car+0x32FC at or over 0, car+0x3234 at or over 1.0 and car+0x32F8 the engine rpm over 1000, the engine idles at 4600 rpm in neutral under the brake so the key stays a brake at speed
4. The release unwinding. The gains at 0x49B0F1 to 0x49B249 read again with the registers: state 0 gives tau 16.0 and gain 1.0 side 1.0 (0.5 gain in reverse), a state gives tau 8.0, the counter steer (state 1 with 0x27 or state 2 with 0x25) gain 1.0 side 0.5, else gain 2.0 side 2.0, the max steer car+0x2974 is car+0x2F34 times pi over 180 times gain times side, the rates car+0x2A78 0x2A7C the steer stat times car+0x32E8 times gain times the slowed scale, the smoothed gauge eases by one over tau. The unwinding block 0x49ADD3 to 0x49AEDC reads as the round ten table, 1.34 degrees a tick for basic 1 while the gauge walks 2.4 a tick, a left drift clears in place under 10, a right one through `car_drift_state_set(0)` past 0. All as ported. With the brake rate the two samples after a release read 0.68 units and 1.53 degrees (was 1.16 and 4.56), most of the class was the brake the follower holds after each release. What stays: in the release sample the port unwinds 1 to 3 degrees more than the client on the flat releases (12.4 s minus 6.3 against minus 5.6, 128.2 s minus 8.2 against minus 4.2, 157.4 s minus 3.9 against minus 1.4) and turns 1.5 to 3 more under the opposite steer in the sample after (12.6 s 17.5 against 15.5, 135.2 s 20.4 against 17.0), the path itself matches (the motion direction of the port reads 275.9 269.7 277.1 289.8 at 12.3 to 12.9 s against the recorded chords 273.1 268.1 278.2 290.6). The release sample carries three edges in one interval (the left key up, the right key down, the drift key up) placed one after the other, the residual sits inside their one tick windows
5. The drift entry. The friction pair at 0x49C8BB to 0x49C93A scales pair 0 alone by 0.8 (0x5A322C) off the gas and 0.6 (0x5A164C) on it while car+0x35A4 is set, pair 1 is untouched, `car_apply_force_if_valid` hands them to `body_set_tire_grip` 0x4EC130 which writes wrapper 0xB50 and 0xB6C, wheel set 0x810 and 0x82C, the peak scale of the front and the rear tyre block, and the two step 11 drift clauses at 0x49CBA1 (the cap plus 9 times one minus car+0x1397198 over 60 km per hour) and 0x49CCD9 (the throttle times one minus the smoothed gauge over 45 times 0.012 times car+0x1397198, then the 0.97 decay) read as ported. The port sheds nothing more in a free slice: from the sample before the key, 5.4 s reads 100.1 94.3 88.3 82.1 against 100.6 95.9 89.3 83.0, 11.4 s reads 89.2 87.6 83.4 79.0 against 90.1 87.8 83.3 78.7, 54.4 s reads 112.1 109.6 106.6 101.7 against 113.6 111.7 107.2 101.9. The 1 to 2 units under the client in the resync run (the drift class speed reads plus 0.5 over the race, minus 0.4 over the first 140 s) come from the resync itself, the velocity goes back to the recorded chord each sample while the tyre slip states of a sliding car stay, the transient costs a unit or two on the sample mean

`tests/drift_replay_test.cpp` replays the first 30 s of the recording (150 samples, two drift runs, two releases, three mini turbos, the brake sequences) with the centre mask, the flags and the resync, no edge search, and keeps the classes under bounds: the drift samples under 0.6 units and 2 degrees (0.33 and 0.81 read), the two after a release under 1.5 and 6 (0.80 and 3.23), the brake under 1.0 and 4 (0.53 and 1.93), the port fires the three kind 0 boosts of the slice on its own gas edge inside 0.4 s of the release, and no drift state outlives the release by more than 0.4 s.
