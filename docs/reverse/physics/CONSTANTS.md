# Kart physics tuning constants

Read in KnC.exe.raw, image base 0x400000, project "Reverse HBO", on 2026-09-14. Every value below came from `read_memory` on the exact address, then cross checked against the decompile of `car_physics_tick_local` (0x49C0D0) and `car_drift_update` (0x49AA90). None of these are guessed. Hex is the raw little endian dword as it sits in memory; value is that dword read as a 32 bit float unless noted otherwise (none needed a double, every address here decoded to a sane float).

Two addresses are not floats and are called out separately:

- `DAT_005a6aa4` holds the ASCII bytes `FLY\0`, not a number. `car_drift_update` passes `&DAT_005a6aa4` as a tag argument to `FUN_004a1350(param_2, 0, &DAT_005a6aa4)`, which is a state/animation query ("is this car in the FLY state"), used to stop the drift gauge from charging while gliding.
- The numeric address `0x5a3720` (global constant, -1.0) is unrelated to the per-car struct field at offset `+0x3720` ("slow flag" in the car record doc). Same hex digits, two different things. Likewise `0x5a371c` (RPM ceiling, 10000.0) is unrelated to the per-car field `+0x371c` ("airborne peak height").

## Generic / shared constants

These are reused for unrelated purposes in different places, so they get one row with several uses rather than one row per use.

| Address | Hex | Value | Used by | Meaning |
|---|---|---|---|---|
| 0x59f44c | 00000000 | 0.0 | both functions, dozens of sites | generic zero / sign-flip test (`if (x < 0.0) x = -x;`) and decay floors |
| 0x59f480 | 0000803f | 1.0 | both functions, dozens of sites | generic identity constant: clamp ceilings, "full" suspension extension, default multiplier |
| 0x59f414 | 0000003f | 0.5 | both functions, ~10 sites | generic half: wheel height averaging for pitch/roll, drift gauge rate base, drift auto-cancel upper gauge bound, reverse-gear turn multiplier |
| 0x59f404 | 00002041 | 10.0 | `car_physics_tick_local`: landing snap-to-ground position delta test; wheel random-bump gating (`iVar14<2 && DAT_0059f404<unaff_EBP`) | threshold used in the airborne-landing and wheel-bump checks |
| 0x5a2494 | cdcccc3d | 0.1 | `car_physics_tick_local`: crash-recovery state-1 growth per tick; yaw/pitch/roll correction deadzone (paired with 0x5a32d8); a bone-transform counter increment | generic per-tick increment / deadzone bound |
| 0x5a32d4 | 0000803e | 0.25 | `car_physics_tick_local`: yaw correction blend, per-wheel suspension sum blend, wheel steer angle damping while airborne, body pitch-offset decay | generic 0.25 smoothing/damping coefficient, reused across several smoothing steps |
| 0x5a322c | cdcc4c3f | 0.8 | drift gauge rate max clamp (`car_drift_update`); mini-turbo wire stat 10/11 raw-to-gauge scale (`car_drift_update`) | stat-to-gauge normalization factor, doubles as drift charge-rate ceiling |
| 0x5a1650 | 9a99993e | 0.3 | drift gauge rate min clamp (`car_drift_update`); turn-force baseline scale (`car_physics_tick_local`, `fVar1*_DAT_005a1650 + DAT_005eb6f8`) | 0.3 tuning scalar, reused for drift charge floor and turn-force baseline |
| 0x5a6a48 | 0000c03f | 1.5 | `car_drift_update`: direct low-speed yaw turn rate; `car_physics_tick_local`: roll-based turn-force clamp ceiling (`fVar1=roll*5a6a48`, clamp to 0x59f450) | 1.5 rate/clamp constant, reused in two different turn calculations |

## Drift gauge state machine (`car_drift_update`)

| Address | Hex | Value | Used by (expression) | Meaning |
|---|---|---|---|---|
| 0x5a32a0 | 0000a042 | 80.0 | `fVar1 = FUN_0044d1a0() * DAT_005a32a0` | per-tick drift gauge scale (dt x 80), the base unit every charge/decay step multiplies |
| 0x5a3298 | 0000a041 | 20.0 | `if (DAT_005a3298 < speed && grounded)` | minimum speed to start a drift |
| 0x5a3244 | 0000f041 | 30.0 | `if (DAT_005a3244 <= speed && grounded) goto skip` | speed above which the low-speed direct-yaw-turn fallback is skipped |
| 0x5a324c | 000020c1 | -10.0 | `if (gauge <= DAT_005a324c) goto skip` | gauge deadzone bound gating the low-speed direct-yaw-turn fallback |
| 0x5eb700 | 00003442 | 45.0 | `if (DAT_005eb700 < gauge) gauge = DAT_005eb700` (and negative mirror) | drift gauge cap, +/-45 |
| 0x5a6aa0 | 8fc2f53d | 0.12 | `gauge -= fVar1 * DAT_005a6aa0` | drift gauge decay rate when the stick is released |
| 0x5a68c4 | 000000bf | -0.5 | `(gauge < 0.5) && (DAT_005a68c4 < gauge)` | lower bound of the +/-0.5 residual-gauge deadzone that auto-cancels a released drift |
| 0x5a6a9c | 00008041 | 16.0 | `smoothedGauge += (gauge-smoothedGauge)/DAT_005a6a9c` | smoothing time-constant for the drift gauge while NOT drifting (state 0) |
| 0x5a15f0 | 00000041 | 8.0 | same smoothing formula, state != 0 | smoothing time-constant for the drift gauge while drifting (states 1-3) |
| 0x5a69f0 | 00009643 | 300.0 | `yaw += FUN_0044d1a0()*DAT_005a69f0*steerGain` | yaw-turn rate used while the "stuck" flag steers the car with the gauge |

## Mini turbo (`car_drift_update`)

| Address | Hex | Value | Used by (expression) | Meaning |
|---|---|---|---|---|
| 0x5a15ec | cdcc4c3e | 0.2 | clamp floor for both threshold calcs below | mini-turbo threshold clamp floor |
| 0x59f480 | 0000803f | 1.0 | clamp ceiling for both threshold calcs below | mini-turbo threshold clamp ceiling |
| 0x5a322c | cdcc4c3f | 0.8 | `(wire stat 10+bonus)*DAT_005a322c`, `(wire stat 11+bonus)*DAT_005a322c` | wire stat 10/11 raw-to-fraction scale before clamping |
| 0x5eb704 | 00002041 | 10.0 | `gaugeThreshold * DAT_005eb704 < gauge` | stage-1 gauge threshold multiplier (wire stat 10) |
| 0x5eb708 | 00004844 | 800.0 | `holdThreshold * DAT_005eb708 < heldTime` | stage-1 hold-time threshold multiplier, ms (wire stat 11) |
| 0x5eb70c | 0080bb44 | 1500.0 | `heldTime < DAT_005eb70c` | stage2-to-stage3 boost window after release, ms |

## Steering / turn force

| Address | Hex | Value | Used by (expression) | Meaning |
|---|---|---|---|---|
| 0x5a24ec | 00000040 | 2.0 | `car_drift_update`: steering-gain multiplier while counter-steering against an active drift; `car_physics_tick_local`: clamp ceiling for wire stat 1 (max speed) and wire stat 5 (turn force), both "+1, clamp to 2.0" | generic "+1 stat" clamp ceiling, also the countersteer-during-drift gain |
| 0x5a32b8 | 00004040 | 3.0 | `car_drift_update`: `(wire stat 2+bonus)*DAT_005a32b8+1.0` steering-gain scale; `car_physics_tick_local`: speed-band boundary for the engine-force taper (`speed>=3.0`) | 3.0 tuning constant, steering-gain scale and speed-band boundary |
| 0x5a0054 | 00008040 | 4.0 | `car_drift_update`: steering-gain clamp ceiling | steering gain (wire stat 2) max clamp |
| 0x5a6a98 | db0f4940 | 3.14159265 (pi) | `car_drift_update`: `stat*DAT_005a6a98*DAT_005a6a94*...` | steering gain radian-conversion factor A |
| 0x5a6a94 | 610bb63b | 0.0055556 (1/180) | same expression | steering gain radian-conversion factor B; A x B = pi/180 = deg-to-rad |
| 0x5a164c | 9a99193f | 0.6 | `car_drift_update`: `(wire stat 9+bonus)*DAT_005a164c+DAT_005a32b0` drift-steer scale; `car_physics_tick_local`: identical formula recomputed for per-wheel spin | drift-steer (wire stat 9) raw-to-value scale, used in both functions |
| 0x5a32b0 | 9a99993f | 1.2 | drift-steer clamp floor, both functions | drift-steer (wire stat 9) min clamp |
| 0x5a3214 | 6666e63f | 1.8 | drift-steer clamp ceiling, both functions | drift-steer (wire stat 9) max clamp |
| 0x5a1e88 | 35fa8e3c | 0.0174533 (pi/180) | `car_drift_update`: final yaw-torque scale sent to `FUN_004ed3d0`; `car_physics_tick_local`: wheel-spin yaw torque scale | deg-to-rad constant applied to the final drift/wheel-spin torque |
| 0x5a6a30 | 0000c040 | 6.0 | `car_physics_tick_local`: `yawRate*DAT_005a6a30`, doubled again while drifting | front-wheel cosmetic steer-angle scale from yaw rate |
| 0x5a6ac8 | 920a06bf | -0.523599 (-pi/6) | clamp floor for the cosmetic front-wheel angle above | front wheel visual steer angle min clamp, -30 deg |
| 0x5a6acc | 920a063f | 0.523599 (pi/6) | clamp ceiling, same site | front wheel visual steer angle max clamp, +30 deg |
| 0x5a68cc | cdcccc3f | 1.6 | `turnForceBase = drifting ? DAT_005a68cc : 1.0` | turn-force multiplier while actively drifting |
| 0x5a6a68 | 00007042 | 60.0 | `if (DAT_005a6a68 < speedKmh && drifting)` | speed threshold (km/h) for extra drift turn-torque |
| 0x5a6ad0 | f5f48835 | 0.000001 | `(sumOfForces)*DAT_005a6ad0` | camera/body shake sum scale-down |

## Speed, RPM, engine force, durability wear

| Address | Hex | Value | Used by (expression) | Meaning |
|---|---|---|---|---|
| 0x5eb6fc | 0000a043 | 320.0 | `maxSpeed = clamp(wire stat 1+bonus+1, 1.0, DAT_005a24ec) * DAT_005eb6fc` | max-speed unit-to-km/h conversion factor |
| 0x5a69a8 | 1c2fdd3f | 1.728 | `kmh = rawSpeed * DAT_005a69a8` | raw speed units to km/h factor |
| 0x5a6ac4 | efc91841 | 9.5493 (60/2pi) | `rpm = rpmSource * DAT_005a6ac4` | rad/s to RPM conversion |
| 0x5a6ac0 | 0000fa43 | 500.0 | clamp floor test for rpm | RPM clamp floor (matches immediate 500.0 written on the low branch) |
| 0x5a371c | 00401c46 | 10000.0 | clamp ceiling test for rpm | RPM clamp ceiling |
| 0x5a05e0 | 0ad7233c | 0.01 | `if (DAT_005a05e0 < yawRate \|\| yawRate < DAT_005a69a4)` | positive bound of the yaw-rate deadzone that triggers a yaw snap-correction |
| 0x5a69a4 | 0ad723bc | -0.01 | same test | negative bound of the same deadzone |
| 0x5a1648 | 0000c842 | 100.0 | `if (DAT_005a1648 < unaff_EBP) unaff_EBP = 100.0` | cap substituted into the wheel random-bump multiplier |
| 0x5a69a0 | bd378636 | 0.000004 | `randBump = (rand()%600-300) * unaff_EBP * wheelGrip * DAT_005a69a0` | random wheel-bump final scale (tiny, keeps the bump subtle) |
| 0x5a3720 | 000080bf | -1.0 | `bVar5 = DAT_005a3720 <= compression*DAT_005a6ad8; ... else compression=-1.0` | suspension compression normalized floor (global constant, not the per-car "slow flag" field of the same hex digits) |
| 0x5a6ad8 | acc52737 | 0.00001 | `compression = rawCompression * DAT_005a6ad8` | suspension compression raw-to-normalized scale |
| 0x5a6b08 | 1f856b3f | 0.92 | `airTimeScale *= DAT_005a6b08` | per-tick decay of the airborne "air time scale" while off ground |
| 0x59f450 | 00004842 | 50.0 | `fVar1 = roll*DAT_005a6a48; clamp to DAT_0059f450` | roll-based turn-force clamp ceiling |
| 0x5a6a10 | cdcc9c41 | 19.6 | `durabilityEffect = durability[idx] * DAT_005a6a10` fed to `FUN_004f1a60` | durability value scaled for an engine side-effect (sound/smoke), not the force itself |
| 0x5eb6f4 | 9a99193f | 0.6 | `local_790 = (1-durability[idx]) * DAT_005eb6f4 * DAT_005a6adc` | engine-force durability-penalty scale A |
| 0x5a6adc | 0bd7233e | 0.16 | same expression | engine-force durability-penalty scale B |
| 0x5a6af8 | 643b7f3f | 0.997 | `force *= DAT_005a6af8` when >2 wheels grounded and not boosting | light engine drag per tick while grounded, no boost |
| 0x5a328c | a4707d3f | 0.99 | `force *= DAT_005a328c` when >2 wheels grounded and boosting | engine-force decay per tick while boosting |
| 0x5a6ae8 | 91ed7c3f | 0.988 | `force *= DAT_005a6ae8` when speed >= 3.0 (DAT_005a32b8) | engine-force taper, high speed band |
| 0x5a6aec | 8fc2753f | 0.96 | `force *= DAT_005a6aec` when 1.0 <= speed < 3.0 | engine-force taper, mid speed band |
| 0x5a6af4 | a69b443c | 0.012 | `fVar1 = (residual/DAT_005eb700)*DAT_005a6af4`, clamped to itself | engine-force reduction cap tied to the decaying drift-start boost |
| 0x5a6af0 | ec51783f | 0.97 | `driftStartBoost *= DAT_005a6af0` | per-tick decay of the "just started drifting" torque-boost scalar |
| 0x5a6b00 | 355e7a3f | 0.978 | `force *= DAT_005a6b00` while holding item kind 3 with count < 1 | engine-force penalty for a specific carried item |
| 0x5a6b04 | 0000403f | 0.75 | `force *= DAT_005a6b04` when camera mode == 2 | engine-force penalty in the reversed/rear camera view |
| 0x5a15f0 | 00000041 | 8.0 | `force *= DAT_005a15f0` (both components) while slow-flagged | engine-force multiplier while the "slow" flag is set |
| 0x5a69e4 | d7a3703f | 0.94 | `turnForce *= DAT_005a69e4` while slow-flagged | turn-force multiplier while the "slow" flag is set |
| 0x5a164c (reused) | 9a99193f | 0.6 | `turnForce *= (accelHeld ? DAT_005a164c : DAT_005a322c)` | engine-force multiplier while drifting and holding the gas |
| 0x5a322c (reused) | cdcc4c3f | 0.8 | same site, gas released | engine-force multiplier while drifting and off the gas |
| 0x5a3be8 | 0ad7a33b | 0.005 | `crashTimer -= DAT_005a3be8` | crash-recovery state-0 decay per tick |
| 0x5a3294 | cdcccc3e | 0.4 | crash-recovery state-1 growth cap; also the clamp ceiling for the body pitch-offset target | 0.4 clamp reused by crash-recovery flash intensity and body pitch offset |
| 0x5a32cc | 00004041 | 12.0 | `if (reverseFlag && speedKmh <= DAT_005a32cc)` | reverse-gear speed threshold (km/h) for toggling the "slow" flag on |
| 0x5a6ad4 | 00009041 | 18.0 | `if (DAT_005a6ad4 < speedKmh && slowFlag)` | speed threshold (km/h) clearing the "slow" flag |
| 0x5a323c | 0000b442 | 90.0 | `yaw = atan2(...) - DAT_005a323c` (and +/- variants for pitch/roll) | 90 degree axis-correction applied to the atan2 results for yaw/pitch/roll |
| 0x5a329c | 0000803d | 0.0625 | pitch and roll smoothing blend | smoothing lerp factor for body pitch and roll (yaw uses 0x5a32d4 = 0.25 instead) |
| 0x5a32d8 | cdccccbd | -0.1 | paired with 0x5a2494 (0.1) | negative bound of the yaw/pitch/roll correction deadzone |

## Not used by either function, but adjacent tuning values

| Address | Hex | Value | Note |
|---|---|---|---|
| 0x5a69dc | fcad803c | 0.0157080 (pi/200) | frequency constant for the durability curve formula, see below |

## Surface speed table, 30 floats at 0x5EB718

Indexed by the surface type returned from `FUN_004b4b50`, multiplies the wheel grip in `car_physics_tick_local` (`if (-1<idx && idx<30) grip *= table[idx]`).

| idx | address | hex | value |
|---|---|---|---|
| 0 | 0x5eb718 | bfd47f3f | 0.99934 |
| 1 | 0x5eb71c | 12d97f3f | 0.99941 |
| 2 | 0x5eb720 | 55dd7f3f | 0.99947 |
| 3 | 0x5eb724 | a8e17f3f | 0.99954 |
| 4 | 0x5eb728 | fbe57f3f | 0.99960 |
| 5 | 0x5eb72c | 4fea7f3f | 0.99967 |
| 6 | 0x5eb730 | 1ff57f3f | 0.99983 |
| 7 | 0x5eb734 | 0000803f | 1.0 |
| 8 | 0x5eb738 | 0000803f | 1.0 |
| 9 | 0x5eb73c | 0000803f | 1.0 |
| 10 | 0x5eb740 | 0000803f | 1.0 |
| 11 | 0x5eb744 | 0000803f | 1.0 |
| 12 | 0x5eb748 | 0000803f | 1.0 |
| 13 | 0x5eb74c | 0000803f | 1.0 |
| 14 | 0x5eb750 | 0000803f | 1.0 |
| 15 | 0x5eb754 | 0000803f | 1.0 |
| 16 | 0x5eb758 | 00000000 | 0.0 |
| 17 | 0x5eb75c | 00000000 | 0.0 |
| 18 | 0x5eb760 | 00000000 | 0.0 |
| 19 | 0x5eb764 | 00000000 | 0.0 |
| 20 | 0x5eb768 | 00000000 | 0.0 |
| 21 | 0x5eb76c | 00000000 | 0.0 |
| 22 | 0x5eb770 | 00000000 | 0.0 |
| 23 | 0x5eb774 | 00000000 | 0.0 |
| 24 | 0x5eb778 | 00000000 | 0.0 |
| 25 | 0x5eb77c | 00000000 | 0.0 |
| 26 | 0x5eb780 | 00000000 | 0.0 |
| 27 | 0x5eb784 | 00000000 | 0.0 |
| 28 | 0x5eb788 | 00000000 | 0.0 |
| 29 | 0x5eb78c | 00000000 | 0.0 |

Indices 0-6 are near 1.0 grip (normal road, ramping down slightly), 7-15 are exactly 1.0 (more road-like surfaces), and 16-29 are all 0.0, meaning those surface type IDs zero out grip entirely (off-track / void / not-yet-assigned slots). Only the first 17 or so entries look like they are actually tuned; the tail is unused padding at 0.0.

## 0x5EB6F4 to 0x5EB714, contiguous block check

Confirmed: this is a contiguous run of 9 tuning floats immediately before the surface table (0x5EB714 + 4 = 0x5EB718, where the table starts). All 9 decode to plausible tuning values, no garbage or embedded strings in between, unlike the 0x5a69xx region which mixes floats with UI strings.

| Address | Hex | Value | Used by | Meaning |
|---|---|---|---|---|
| 0x5eb6f4 | 9a99193f | 0.6 | `car_physics_tick_local` | engine-force durability-penalty scale (see table above) |
| 0x5eb6f8 | 0000b442 | 90.0 | `car_physics_tick_local` | turn-force baseline additive term |
| 0x5eb6fc | 0000a043 | 320.0 | `car_physics_tick_local` | max-speed unit-to-km/h factor |
| 0x5eb700 | 00003442 | 45.0 | `car_drift_update` | drift gauge cap |
| 0x5eb704 | 00002041 | 10.0 | `car_drift_update` | mini-turbo stage-1 gauge threshold multiplier |
| 0x5eb708 | 00004844 | 800.0 | `car_drift_update` | mini-turbo stage-1 hold-time threshold multiplier (ms) |
| 0x5eb70c | 0080bb44 | 1500.0 | `car_drift_update` | mini-turbo stage2-to-3 window (ms) |
| 0x5eb710 | 00001643 | 150.0 | not read by either function | not traced in this pass, part of the same block |
| 0x5eb714 | 0000f041 | 30.0 | not read by either function | not traced in this pass, part of the same block |

## Durability curve, game+0x1396FE0

`car_physics_tick_local` reads this as 100 floats (`&DAT_01396fe0 + param_1`, index `FUN_0059029c()` clamped to 0..99), used twice: once scaled by 0x5a6a10 (19.6) and fed to `FUN_004f1a60` (an engine side-effect, sound or smoke), and once as `1.0 - durability[idx]` feeding the engine-force scale in the substep loop (`local_7ac * DAT_005eb6f4 * DAT_005a6adc * engineForceBase`).

`get_xrefs_to DAT_01396fe0` found 7 references:

- `FUN_004955b0` at 0x4955d5 and 0x4955fd, the only writer.
- `car_physics_tick_local` at 0x49cf13 and 0x49cf69, the two reads described above.
- `FUN_00498960` at 0x498b44, 0x498b7f, 0x498bef, three more reads (this is the per-substep collision response function called from the tick's substep loop; it reads the same curve for its own restitution/impulse scaling, it does not write it).

`FUN_004955b0` is the per-car init/setup routine (zeroes most of the car record, sets up scene node handles, resets state machines). It fills the curve with:

```
pfVar7 = (float *)(&DAT_01396fe0 + param_1);
for (i = 0; i < 0x65; i++) {
    pfVar7[i] = 1.0 - sinf((float)i * DAT_005a69dc);
}
```

`DAT_005a69dc` = 0x5a69dc = `fcad803c` = 0.0157080, which is pi/200 exactly. So the formula is:

```
durability[i] = 1.0 - sin(i * pi / 200)      for i = 0 .. 100
```

This is a quarter-sine ease curve: `durability[0] = 1.0` (full curve value, healthy), `durability[100] = 1.0 - sin(pi/2) = 0.0` (fully worn). The loop actually writes 101 entries (i = 0..100, `local_168 < 0x65` with 0x65 = 101) even though the reader clamps its index to 0..99, so the very last written entry is never read. There is no external file or table behind this: it is a pure formula computed once at car setup, not loaded from data.

## Round ten, the drift constants read again, 2026-09-15

Every constant of `car_drift_update` above was read from memory again in the drift pass (TICK_HELPERS.md round ten), all hold. Three notes:

| Address | Hex | Value | Used by | Meaning |
|---|---|---|---|---|
| 0x5a3c78 | 0000b443 | 360.0 | `car_body_set_yaw` 0x49A970 | the body yaw is 360 minus the wire yaw in degrees, an older section of TICK_HELPERS.md read 180 here |
| 0x5eb700 to 0x5eb70c | | 45.0 10.0 800.0 1500.0 | `car_drift_update` | initialised data with no runtime writer, the 0xC3 track record writes 0x5eb6f0 to 0x5eb6f8 only |
| 0x5a6b04 | 0000403f | 0.75 | `car_physics_tick_local` 0x49C971 | the friction pair scale in view mode 2, both pairs, the drift scales pair 0 alone by 0.8 off the gas or 0.6 on it |

The stat bonuses read next to these sit at car+0xA7940 plus 4 times the WIRE index (wire 8 at 0xA7960, 9 at 0xA7964, 10 at 0xA7968, 11 at 0xA796C, 2 at 0xA7948, 6 at 0xA7958, 7 at 0xA795C), paired with the base stats at car+0x3448 plus 4 times the wire index. The stat numbers in the tables above were written in the old car+0x3440 numbering and are corrected in place to the wire index on 2026-09-15, INPUT_AND_STATS.md "The 17 stats, wire numbering, settled".
