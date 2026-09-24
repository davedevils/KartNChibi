# Tick helpers

Read in KnC.exe.raw, image base 0x400000, on 2026-09-14, following up docs/reverse/CLIENT_PHYSICS_MAP.md. Covers every helper car_physics_tick_local (0x49C0D0) and car_drift_update (0x49AA90) call that is not the rigid body layer (0x4EC000-0x4F2000) and not world collision (0x486xxx, 0x487xxx, 0x498590, 0x498800, 0x499050, 0x4A0750, 0x4A1310, 0x4A1380 world part, 0x4B4B50, 0x4C37D0).

Convention, matching CLIENT_PHYSICS_MAP.md: "game" is the pointer passed as the this/param_1 argument of every helper below. It is one allocation holding the 30-car array (stride 0xA7260, car N at game+N*0xA7260) followed by shared per-race state (crash recovery timers, cheat state, durability curve, active-car flags). "car" means game+index*0xA7260. A byte guard of the shape `(&DAT_01397384)[param_1]` that opens almost every function reads the single byte at game+0x1397384 ("session running" flag) - it does not vary with car index, since param_1 is the game base itself, not an index.

## 0x4998F0, car_draft_factor

Arguments: game, carIndex, float maxDistance, float coneHalfAngleDeg. Returns float10.

Reads car+0x740 (a per-car "slot occupied" byte, checked on every other car), car+0x3244/0x3248/0x324c (position), car+0x3220 (yaw). Loops the 30 car slots (stride 0xA7260) skipping itself and any car whose +0x740 byte is 0.

For each candidate: dz = self.z - other.z, kept only if _DAT_005a6a74 (-8.0) <= dz <= _DAT_005a15f0 (8.0). Bearing = math_atan2_deg(self.x-other.x, self.y-other.y) (0x44E240). angle = wrap360(bearing - self.yaw - 90.0 (_DAT_005a323c)) (0x44D9C0). Candidate qualifies if angle is within coneHalfAngleDeg of the 0/360 wrap point, i.e. inside a forward cone of half-width coneHalfAngleDeg centered on the car's heading. Among qualifying candidates keeps the one with the smallest 2D distance math_hypot2d(dx,dy) (0x44D900), provided that distance is below the running best (seeded with maxDistance).

If a candidate was found: return clamp(1.0 - closestDistance/maxDistance, 0.0(DAT_0059f44c), 1.0(_DAT_0059f480)). Otherwise return 0.0.

This is a slipstream/draft factor: 1.0 right behind a qualifying car ahead, fading linearly to 0 at maxDistance, 0 if nothing qualifies. Only caller is car_physics_tick_local, which stores the result at game+0x1397360 (line marked `*(float *)(&DAT_01397360 + param_1) = (float)fVar21;`) and only lets it add to the throttle force when the result is greater than 0, the car has more than the min-speed-bucket wheel count on the ground (local_7a8 test), and the car is not reversing (car+0x332d==0) and the race is not in states 0xf/0x11 (battle-style). When enabled the extra push is `_DAT_005eb6f8(90.0) * _DAT_005a1650(0.3) * draftFactor`, added to the base throttle force; game+0x3715 (car-relative, the "drifting well"/draft-active byte from CLIENT_PHYSICS_MAP) is set to 1 while this is active, 0 otherwise. The literal maxDistance/coneHalfAngleDeg values passed by the caller were not resolved from the decompile (stack setup collapsed by the decompiler) - guess only.

## 0x497E40, car_input_poll_dispatch

Arguments: game, carIndex. No return.

Reads game+0x1397384 (session flag) and DAT_00b23614 (a second global enable flag, address far from the per-car array, likely "controls enabled"). If both set, reads game+0x94, an integer 0-3 selecting one of four device-reading sub-routines: 0 -> FUN_00497ab0, 1 -> FUN_00497940, 2 -> FUN_00497270 then FUN_00497ab0, 3 -> FUN_004975e0. These four (not renamed, outside the assigned list) read keyboard/gamepad/wheel state through FUN_0045af30/FUN_0044b580/FUN_0044b590 and write the raw control flags at game+0x18 (accel), +0x1c (brake), +0x20/+0x24 (steer), +0x28, +0x2c (stuck) - the same relative offsets CLIENT_PHYSICS_MAP lists as the car's own input flags; for the local car these two locations line up. This function is not an item-effect handler despite the task description's paraphrase - the sub-handlers read control devices, not item state. Nothing in it or its sub-handlers touches the 0xBFC3B0-0xBFDA1C range; see the separate section below on what gates the call.

After the dispatch, clears car+0x332c and car+0x332d (brake/reverse latches), then if car+0x2de8 (a wheel/suspension velocity-ish field) is negative sets car+0x332d = 1 (reverse flag).

## 0x4960A0, car_effect_update

Arguments: game, carIndex. No return.

Only runs its state-transition block (kind==0 checks) when car+0x36a8 (active effect code, CLIENT_PHYSICS_MAP's "active effect code") is 0: it then tries in turn item-collision helpers FUN_004cfcb0, FUN_004c37d0 (twice), each of which can start a new effect by writing car+0x36cc/0x36d0/0x36d4/0x36d8 (a saved yaw+position snapshot), car+0x36a8 = 500, 700 or 1000, car+0x36ac = 0, then plays a sound (FUN_0048b460(car+0x3520, 6)), calls FUN_00496930 (reset, not in scope) and FUN_0049a9e0(carIndex, 0) (clears drift state).

Whatever car+0x36a8 holds (own or freshly started) is then dispatched by value and scales car+0x25fc/+0x2600 (a local-frame velocity pair feeding the rigid body) and sometimes car+0x2604:
- 100 (spin?): scale by _DAT_005a6a00, add a per-frame decaying term to car+0x36ac using time_frame_delta (0x44D1A0) * _DAT_005a3bf4, cap it at _DAT_005a69fc.
- 200: scale by _DAT_005a6a04, subtract a decaying term from car+0x36ac using _DAT_0059ff80; ends (calls FUN_00495be0, effect-end, not in scope) once car+0x36ac hits 0 or a durability check (FUN_0048d730) returns > 1.
- 300 (hit/spin-out): scales by DAT_0059f44c (0.0, i.e. zeroes) and _DAT_005a69f8; computes car+0x36c8 from car+0x36bc and car+0x3680(+14000 bytes) and a constant _DAT_005a69f4; clears the raw input flags at param_1+0x18..+0x2c (game-relative, see 0x497E40); calls FUN_00499050(carIndex,1) (respawn/uprighting, world-collision scope); ends when car+0x36c8 >= 0.
- 400: scale by _DAT_005a69e8; ends via a durability check FUN_004cf020.
- 500 (banana?): scale by _DAT_005a69ec; ends via FUN_004cfcb0.
- 600: ends via FUN_004b9fe0, else calls car_effect_apply (0x495c30, not in scope) again.
- 700: scale by _DAT_005a69e4, plays sound 6, ends via FUN_004c37d0.
- 900: ends via FUN_004b9fe0.
- 1000: plays sound 6, scale by _DAT_005a69e8, ends via FUN_004c37d0.
- 0x44c (1100): looks at DAT_00d6e1d0 (a cutscene-busy flag reused across the file) and ends the effect (FUN_00495be0) when it is 0.

The end helper FUN_00495be0 is out of scope; it is what ultimately clears car+0x36a8 back to 0.

## 0x499000, car_apply_force_if_valid

Arguments: int index, force x, force y (forwarded, types undefined4). No return.

If 0 <= index < 30, forwards straight to FUN_004ec130(x,y) (rigid body layer, out of scope). A bounds-checked wrapper, nothing else.

## 0x49BE20, car_decode_id5

Arguments: char* out[5], int* in[5]. Returns out.

For i in 0..4: `out[i] = (in[i]/21 + sign(in[i])) - round_toward_zero_correction - (i+1)`, using the classic reciprocal-multiplication-by-0x30c30c31 pattern for division by 21; out[5] is NUL. Pure integer-to-5-char decode, nothing car-specific in the body.

Called once inside car_physics_tick_local, in the "collision happened this substep" branch (car+0x2e20==1), with five literal denormal-float-encoded integers (1743, 1491, 1554, 1533, 1743) as the input, immediately followed by FUN_0049a350 (0x4A1350, car_node_name_is) comparing the decoded 5-character string against the name of scene-node/bone index 2 (car+0x6b4 base). If the name does NOT match, FUN_00498960 (car_substep_collision_response) runs; if it matches, the collision response for that substep is skipped. Reads at the call site: the five inputs are compile-time constants baked into the binary, not read from car state. Interpretation of this as a hidden tag/name check gating collision response is inferred from the control flow, not confirmed by any comment - guess.

## 0x498960, car_substep_collision_response

Arguments: game, carIndex, float substepDt. Returns 1.

Calls FUN_00496930(carIndex) first (reset helper, not in scope - very likely ends any in-progress boost/effect on hard impact). Then:
- Reverts the substep's motion: car+0x3220 (yaw) = car+0x3230 (yaw saved before the substep), car+0x3244/0x3248/0x324c (position) = car+0x325c/0x3260/0x3264 (position saved before the substep).
- If game+0x1397174[carIndex] (the "stuck" flag CLIENT_PHYSICS_MAP mentions) is 0, computes car+0x6fc = wrap360((yaw - smoothedDriftGauge(car+0x35ac)*_DAT_005a32b0(1.2)) - math_atan2_deg(car+0x2e34, car+0x2e38) - 90.0(_DAT_005a323c)) using 0x44E240/0x44D9C0 - a deflection heading from the collision normal stored at car+0x2e34/0x2e38.
- durabilityIndex = FUN_0059029c() (damage/durability tier lookup, 0..99). restitution = 0.98 normally, 0.94 if stuck, else if durabilityIndex < 20: durabilityCurve[durabilityIndex]*_DAT_005a6a44(0.06) + _DAT_005a69e4(0.94). Passed to FUN_004ed600 (rigid body, out of scope) - sets bounce/friction for the impact.
- impulseScale = (speed(car+0x3234) - 15.0(_DAT_005a3230)) * 0.3(_DAT_005a1650) * durabilityCurve[durabilityIndex], clamped to [0.5(_DAT_0059f414), 3.0(_DAT_005a32b8)].
- If stuck: impulseScale forced to 1.0, and a separate "spin" term is forced to 0.
- Else if durabilityIndex > 50: spinTerm = clamp((1.0 - durabilityCurve[durabilityIndex]) * kmh(car+0x32f4) * 0.25(_DAT_005a32d4), 0, 1.5(_DAT_005a6a48)); if speed > 25.0(_DAT_005a3ec0) sets car+0x6f8 (the random-jitter factor) to 0.94; unless race state is 9, plays a low-speed particle (speed <= 40.0 (_DAT_005a1644)) or a high-speed particle plus engine-pitch bump FUN_0043ead0(speed*250.0(_DAT_005a6a34)) and sound 5.
- If speed > 15.0(_DAT_005a3230): triggers a plain (non-damage) particle and calls FUN_004981b0(carIndex, durabilityIndex > 20).
- Friction floor: 0.5(_DAT_0059f414) normally, 0.8 if not drifting and fewer than 3 wheels are on the ground (wheels_on_ground_count).
- Builds an impulse vector (collisionNormalY, collisionNormalX, spinTerm) each scaled by frictionFloor*impulseScale*substepDt (spin term instead scaled by spinTerm*substepDt*0.25) and hands it to FUN_004ed3f0 then FUN_004adbf0 (rigid body layer, out of scope).
- If race state is 0xd (13) and DAT_00bfdac4 == 0x17 (23), increments DAT_00bfdabc. This sits just outside the 0xBFC3B0-0xBFDA1C item-counter block (0xbfdabc is 0xa0 past 0xbfda1c) but is clearly the same family of per-mode-13 counters.

This is the real per-substep collision response: undo the substep's motion, derive a bounce/spin impulse from speed and durability, play damage feedback, and push the impulse into the rigid body.

## 0x49B3D0, car_effect_lean_update

Arguments: game, carIndex. No return. Called once per tick, after the car's world matrix (car+0x2fe0) has been rebuilt for the frame.

If car+0x36a8 (active effect code) is 200 or 300 (spin-type effects): builds a roll-only rotation from car+0x36ac (the effect's decaying wobble term) * _DAT_005a1e88 (deg->rad, 0.0174533). Otherwise builds a lean from the smoothed drift gauge (car+0x35ac), car_suspension_shake (0x49A8A0) and, wire stat 6+bonus (car+0x3460, the wheel spin stat) and wire stat 7+bonus (car+0x3464, the wheel steer angle), every kind - the ordinary cornering lean used every tick.

That local transform is then combined with car+0x2fe0 through a second state machine at car+0x36dc (values 0-9), independent from the boost state machine: state 0 applies the lean directly to the matrix and copies car+0x221c (a saved rotation) back in; states 1/2/3 are a wobble-decay chain gated by time_now_ms (0x44ED50) with thresholds _DAT_005a15ec, then a 3000 ms hold, then a decay by _DAT_005a6ab0 (1.18, i.e. this stage grows back toward 1.0 rather than decaying) until it passes 1.0 (_DAT_0059f480), which resets the chain to state 0; states 4/5 are a second decay pair using thresholds 200 ms / _DAT_005a6aac (0.82); states 7/8/9 a third using _DAT_005a3218 (1.05) / 300000 (ms) / _DAT_005a6aa8 (0.95). Whichever state is active, it also adds car+0x3018 (accumulated body roll) += _DAT_005a15ec, and if fewer than 2 wheels are on the ground it adds a small random kick to the matrix via rand() scaled by _DAT_005a8418 and DAT_0059f44c/_DAT_005a68d0.

This is the per-tick visual lean/wobble overlay on the car's world matrix; the deep state-by-state timing constants above were read but the overall design intent (why 9 states) is inferred from the control flow, not from any label - treat the state count as confirmed, the naming of what each is "for" as a guess.

## 0x49A8A0, car_suspension_shake

Arguments: game, carIndex. Returns float10.

Only non-zero for vehicle kind 2 (car+0x33b4==2): sums the four wheel suspension compressions at car+0x2cc0/0x2cd4/0x2ce8/0x2cfc, multiplies by _DAT_005a6a80 (0.000204) * _DAT_005a6a8c (-2.0), clamps to [_DAT_005a6a88 (-25.0), _DAT_005a3ec0 (25.0)]. Returns 0.0 for every other vehicle kind. Used by car_effect_lean_update (0x49B3D0) and by 0x48E6A0 (car_visual_update) as a steering/lean contribution specific to kind-2 vehicles.

## 0x49A390, car_boost_speed_gate

Arguments: game, carIndex. No return. Runs only for the local car (param_2 == car+0x6b0).

Handles a small unrelated bundle: if game+0x1396fcc[car] is 1 (some UI/HUD latch) and either car+0x6bc (cheat-teleport state) != 0 or kmh(car+0x32f4) < _DAT_005a6a14 (120.0), clears the latch and calls FUN_004489f0 (scene/UI, out of scope). Otherwise, if boost is active (car+0x3300 != 0) with a kind (car+0x3304 != 0) and kmh > _DAT_005a6a84, triggers a camera-shake state FUN_0049a2d0(1) (not in scope, not assigned). If the game+0x1396fcc latch is set, runs a 50 ms two-phase toggle at game+0x1396fd0 using time_now_ms.

For race modes 0xb/0xf/0x11/0x19, if a cheat/dev flag (FUN_0046c1e0(4)) is > 0, and speed is above _DAT_0059f404 (10.0) with car+0x3598 below _DAT_0059ff80, and neither of the car_effect_lean_update wobble sub-states is running, updates the two "item mark" slots at car+0x3748/0x374c via FUN_004a1310 (world collision, excluded) and FUN_00486dd0/FUN_004a44a0 (not assigned).

## 0x49A130, car_stuck_surface_check

Arguments: game, float carIndexAsFloat. No return.

If car+0x744 (current surface/node id) equals DAT_01a20658 (a specific fixed id - looks like "track surface"), and it has held for at least 3000 ms since game+0x1397348[car] (checked with time_now_ms), and kmh >= _DAT_005a3244, looks up the car's part-slot type at car+0x3520 and, unless it is kind 3 or 0xb, finds a nearby target via FUN_00499aa0(carIndex, 30.0) and, if found, computes angle = wrap180(math_atan2_deg(dx,dy) - car+0x3220 + _DAT_005a32a8(180.0)) (0x44E240/0x44DD00) between the car and that target; if the angle falls inside [_DAT_005a6998, _DAT_005a6994] plays sound 3 on the car (FUN_0048b460) and calls FUN_004815f0(3) (not assigned). Reads as "car has been sitting on the same surface facing roughly away from the next target for 3+ seconds" - a stuck/wrong-way hint.

## 0x496E50, car_boost_update

Arguments: game, carIndex. No return. Runs every tick unless the active effect (car+0x36a8) is 500/600/900 (which cancel boost/drift outright).

If no boost is active (car+0x3300==0), decays the two rear/side suspension-linked floats at param_1+0x5c..0x64 (game-relative, three slots) by _DAT_005a6a1c (0.86) each tick, floored to 0 below _DAT_005a2494 (0.1), and forwards the result to any attached scene node still flagged live (FUN_00448a30/FUN_00448a50, not in scope).

State machine at car+0x3300 (0 idle, 1 push, 2 decay, 3 end):
- State 1: pushes toward target speed car+0x330c using car_boost_start's steering-lock helper FUN_00496b40 while kmh < target; computes the duration for the boost's kind (car+0x3304): 0 -> FUN_0059029c() (durability/position based, read twice, same call both branches), 1 -> 3800 ms, 2 -> 6000 ms, 3 -> 6000 ms, 4 -> 5000 ms, 5 -> 15000 ms, 7 -> 1500 ms, 6 -> forces car+0x3300 back to 0 immediately (kind 6 is a cancel pseudo-kind, not a real boost). Once now - car+0x3310 (start timestamp) exceeds the duration, sets car+0x3310 = now and car+0x3300 = 2.
- State 2: car+0x3308 *= _DAT_005a6a1c (0.86) each tick; once it drops below 1.0 (_DAT_0059f480), sets car+0x3310 = now and car+0x3300 = 3.
- State 3: calls FUN_00496930(carIndex) (reset, not in scope) and returns - this is what actually clears the boost (car+0x3300 back to 0, presumably inside that reset helper).

## 0x4A1420, car_respawn_state_machine

Arguments: game, carIndex. No return. Huge (about 1000 decompiled lines) driver-name/cheat-code table plus a small state machine at param_1+0x6bc (game-relative: 0 idle, 100/0x65/0x66 "trigger a scripted camera pan and rename the car", 200/0xc9 "warp to a saved spot", 0xffffffff "waiting for a named-part animation to finish" via car_name_match_count/0x4A1380).

State 0 (idle) walks the car's four scene-node/bone names (0x4a0680) and, unless the active effect is 500/600/900, compares each against a long list of hardcoded strings (kart part codes, cheat words like "PUSH", pet names) using strcmpi; matches call car_boost_start with a fixed kind, or FUN_00497190 (a bigger warp/teleport helper, not in scope) with hardcoded target coordinates (each a literal float in the switch). One branch ("PUSH") instead walks a ring of raycasts (0x44dec0/0x485970 sample points) ahead of the car and calls car_substep_collision_response (0x498960) at the first blocked sample. A second nested table of name matches sets a family of one-shot flags at DAT_00bfd96c..0xbfd974 and DAT_00bfd770 (all inside the 0xBFC3B0-0xBFDA1C block, see below) the first time a given cheat name is recognized.

States 100/0x65/0x66/200/0xc9 are a scripted sequence: freeze input (FUN_0043ed70), pan the camera (FUN_0043d7e0), wait one tick for a cutscene-busy flag (DAT_00d6e1d0) to clear, then teleport the car to a saved position (game+0x1397324.. or a location looked up via FUN_00489b40 using car+0xa7878.. as a 3-float hint), reset velocity through the rigid body (FUN_004ec290/FUN_00481a80, out of scope), restart drift/effects (FUN_0049a9e0, FUN_0043ed70), and for state 0xc9 finish with a small forward car_boost_start(carIndex, 0, 0) (a landing mini-turbo).

State 0xffffffff waits (car_name_match_count / 0x4A1380 > 0) for a named part to stop moving before returning to idle.

Not traced string-by-string; the structure (idle name scan -> cheat/kart-code table -> optional scripted teleport -> resume) is confirmed, the meaning of each individual matched string is not.

## 0x4A3BD0, car_mission_rally_update

Arguments: game, carIndex. No return. Runs only for the local car, only when DAT_01af2b68 (a "mission/rally active" byte) is set.

Two branches selected by FUN_00487230 (camera-reversed test, same helper as car_is_camera_reversed's underlying check):
- Reversed-camera / race-state-0xb branch: only when car+0x9d8/+0x9d9 mark the car as active-and-not-finished and DAT_00b231c8 is set, advances a rally waypoint index at game+0x13972e4[car] via FUN_004a0780, and when the car is within _DAT_005a6ad4 of the next stored rally point (DAT_01adf360 array) advances DAT_01adf358 and calls FUN_004b3720/FUN_004b5560/FUN_004839a0/FUN_004b34b0 (rally path/UI, not in scope) plus a sound. A 1000/300 ms debounce (time_now_ms) throttles a follow-up FUN_004819e0(checkpoint) call, done through the shared tail at LAB_004a41b5.
- Normal branch: advances a lap/checkpoint counter at car+0x3334 once the car's checkpoint index (FUN_004a3a40) reaches it, wrapping through FUN_00485a10 (checkpoint count) and updating a small per-lap counter at param_1+0x720 (game-relative). For race states 0xf/0x11/0x19 this drives that mode's specific lap-complete bookkeeping (elapsed-time accumulators at DAT_00c1a9xx / DAT_00c70axx, FUN_004b0cf0, FUN_004b23c0); otherwise calls FUN_004810d0(checkpointIndex, lapCount) (not in scope). Also recomputes car+0x3338 = math_atan2_deg(car.x - checkpointPos.x, car.y - checkpointPos.y) (0x44E240) each tick, a bearing-to-next-checkpoint value, and calls FUN_004a0970 for HUD arrow purposes when not finished.

## 0x496BE0, car_boost_start (already named)

Arguments: game (this), carIndex, kind. No return.

Refused when the car is inactive/finished (car+0x9d8==0 or car+0x9d9==1), or when a boost is already running (car+0x3300 != 0) and either the camera is reversed (0x487230) and kind equals the current kind (car+0x3304), or the camera is not reversed and kind < the current kind (lower-or-equal-priority kinds cannot interrupt a running boost, except the reversed-camera case which only blocks an exact repeat).

On success: zeroes car+0x125c and car+0x12f8 (fields not otherwise traced - guess: steering-lock timers), plays the boost start (FUN_00496960(carIndex, kind)), sets car+0x3304 = kind, car+0x3310 (8 bytes) = time_now_ms(), car+0x3300 = 1. Looks for an equipped part of catalogue type 0x15 (21) among the car's parts (FUN_004504e0/FUN_00451490) and if present sets a 0.04 bonus used below.

Per kind, sets car+0x3308 (decay-phase starting strength, consumed by car_boost_update/0x496E50) and car+0x330c (target speed, km/h):
- kind 6: 3308=1.0, 330c=1.0 (cancel pseudo-kind).
- kind 0 (mini turbo): 3308=6.0; 330c = clamp((stat[car+0x3454]+bonus[car+0xa794c]) * 0.2(_DAT_005a15ec) + 1.0(_DAT_0059f480), 1.0, 1.2(_DAT_005a32b0)) * 120.0(_DAT_005a6a14) - i.e. a target speed between 120 and 144 km/h scaled by that stat.
- kind 1: 330c = 214.0 (0x435c0000); 3308 = 1.0 + the 0.04 part bonus.
- kind 2: 330c = 260.0 (0x43820000); 3308 = 1.0 + bonus.
- kind 3, 4, 5, 7 (item boosts): 330c = 200.0 (0x43480000); 3308 = 1.0 + bonus.
- any other kind: 3308/330c left untouched.

Also sets game+0x1397300[car] = 1, game+0x1397308[car] (8 bytes) = the start time, game+0x1397304[car] = 0.7 (0x3f333333) normally or 0.3 (0x3e99999a) when kind == 0 - both look like a boost-flash/shake intensity flag for HUD or camera use, not confirmed. Finally, if car+0x330c (the just-set target) is below kmh + 10.0 (_DAT_0059f404), raises car+0x330c to kmh + 10.0 so the target is never below the car's current speed.

How the boost ends: see car_boost_update (0x496E50) above - state 1 pushes toward car+0x330c for the kind's duration, state 2 decays car+0x3308 by 0.86/tick until under 1.0, state 3 calls the (out of scope) reset helper FUN_00496930 which clears car+0x3300 back to 0.

## 0x4968F0, car_apply_engine_force

Arguments: game, carIndex, float x, float y, kind (undefined4). No return.

Gated by game+0x1397384 (session flag) only - no per-car check. Forwards to FUN_004ec420(-x, -y, kind) (rigid body layer, out of scope), i.e. applies a force in the plane with sign flipped. Called once per substep inside car_physics_tick_local as `car_apply_engine_force(0, carIndex, ...)` (observed at the top of the substep loop, args beyond the first two not visible in the decompile - the substep's real longitudinal engine force is a separate, adjacent FUN_004ec420 call built from car+0x2610 * a durability-scaled step time, so this wrapper's own x/y at that call site were 0 in what the decompiler preserved) and elsewhere.

## 0x49A9E0, car_drift_state_set

Arguments: game, carIndex, newState (0, 1 or 2). No return.

newState == 0: clears car+0x35a4 (drift state) and car+0x35f0 (mini turbo stage) to 0; if car+0x3600 (gauge threshold count) was above 1 and no boost is running (car+0x3300==0), calls car_boost_start(carIndex, 0, 0) - this is the mini-turbo release boost - and returns without touching anything else.

newState != 0: only when not mid cheat-teleport (car+0x6bc==0), sets car+0x35a4 = newState, car+0x35f0 = 0, car+0x3600 = 0, car+0x35f8 (8 bytes) = time_now_ms(); if the state actually changed from before, pokes a scene-node effect FUN_00448c90(car+0x2114, 0) and sets game+0x13971a4[car] = 1 (a "drift state just changed" latch consumed elsewhere in car_physics_tick_local's crash-recovery block).

## 0x49A970, car_body_set_yaw

Arguments: game, carIndex, float yawDeg. No return. Only caller is car_drift_update.

Computes pitch = car+0x3224 * _DAT_005a1e88 (deg->rad) and yaw = (360.0(_DAT_005a3c78, read as 180 at first, corrected in round ten) - yawDeg) * _DAT_005a1e88, builds the matrix Rz(yaw) Ry(0) Rx(pitch) through `body_mat3_from_euler_zyx` 0x4ED2B0 and writes it with `body_set_orientation_matrix` 0x4F1A10 as R times the transposed principal axes, Q from R (round ten). Effectively forces the rigid body's yaw/pitch directly, used by the drift/steer system to snap the body orientation rather than integrate it.

## 0x4C31D0, car_is_slowed

Arguments: this (a fixed 16-entry table, stride 0x2A dwords = 0xA8 bytes), id. Returns bool.

Linear scan of up to 16 entries; an entry matches when its "active" byte (entry-4, i.e. one dword before the entry start) is 1 and its id field equals the argument. Used by car_drift_update to quarter the drift gauge rate while the car is slowed (per CLIENT_PHYSICS_MAP) and by 0x498960/others as `FUN_004c37d0`-style status probes elsewhere in the file (different table, same pattern). Pure table lookup, nothing else.

## 0x4BD8D0, car_is_camera_reversed

Arguments: this (a fixed 16-entry table, stride 0x51 dwords = 0x144 bytes), id. Returns bool.

Same linear-scan pattern as car_is_slowed, different stride/table. Used throughout (car_drift_update's stick-swap, car_mission_rally_update's branch selection, car_effect_update's input sub-handlers) to ask "is the reversed/mirrored camera active for this id".

## 0x4A1350, car_node_name_is

Arguments: game, carIndex, char* name. Returns bool.

_Str1 = FUN_004a0680(game, carIndex) (scene-node name lookup, not in scope); returns 1 if non-null and strcmpi(_Str1, name) == 0. Thin string-compare wrapper, used by car_decode_id5's collision-response gate and by car_respawn_state_machine.

## 0x49FF90, car_ghost_sample_apply

Arguments: game, carIndex. No return. Companion (read side) to car_ghost_sample_record.

Reads a recorded-sample ring buffer at car+0x3754, stride 0x1c, index car+0xa785c: position (3 floats), yaw (byte, *_DAT_005a6b20 deg-per-unit), drift gauge (packed nibble * 2*_DAT_005eb700*_DAT_005a6b1c - _DAT_005eb700), rpm (packed nibble), and a bitfield at +0x3764: bit7 sets boost active with kind 1, bit6 sets boost active with kind = 0, bit5 sets car+0x332d (reverse), bit4 clears drift state, bit8/bit9 set drift state 1/2, bit3 sets mini-turbo stage, bits 2/1 set the camera/"handbrake view" field car+0xa78e4 to 1 or 2. On the very first sample (index 0) it snaps position/yaw/gauge directly; on later samples it blends position and wrapped yaw/gauge toward the new sample by _DAT_005a2494 (0.1) per tick (0x44DD00 for the angle wrap), i.e. a smoothed ghost/replay played back at a fraction of full speed. Also blends rpm the same way and, once the blend reaches the recorded sample (or after 10/4 ticks depending on race mode 0xd), snaps the rest of the way and calls FUN_00496930 when the whole buffer has been consumed (car+0xa785c+1 >= car+0xa7858, the recorded length), which ends the spectate-replay pass. This is the function car_physics_tick_local calls, together with car_visual_update (0x48E6A0), specifically when car+0xa7854 == 2 (spectating).

## 0x49FAD0, car_ghost_sample_record

Arguments: game, carIndex. No return. Write side of the same ring buffer, called from car_physics_tick_local only when car+0xa7854 == 1 (finished).

Every 10 ticks, or every tick in race mode 0xd (the divider at 0x49FB0E is 10 or 0), writes into car+0x3754[index] (index at car+0xa7858, stride 0x1c): position (car+0x3244..), yaw byte (wrap180 via 0x44D9C0 then FUN_0059029c encodes it), rpm/drift-gauge nibbles (also FUN_0059029c), and the same bitfield car_ghost_sample_apply reads: boost active/kind from car+0x3300/0x3304, reverse from car+0x332d, drift state from car+0x35a4, mini-turbo stage from car+0x35f0, camera/handbrake-view from car+0xa78e4. It then independently samples all 8 input bindings (FUN_0045af30 slots 0,1,2,3,5,4,7 through FUN_0044b580) into a second bitfield at +0x376c, for replay of raw input alongside the resulting state. Advances the ring index, wrapping (and resetting the "finished" state, car+0xa7854 = 0, and the sample index, car+0xa785c = 0) once it passes 0x960 (2400) samples, or 24000 in mode 0xd.

## 0x4A1380, car_name_match_count

Arguments: game, carIndex, char* name. Returns int (0-4).

For each of the car's 4 forward/right/up axis slots (car+0x3274, stride 3 floats), calls FUN_004ec100(-axis.x, -axis.y) (rigid body, out of scope, likely selecting/orienting a probe) then FUN_004ec7e0(i) to fetch a name and compares it with strcmpi against the argument, counting matches. Used by car_respawn_state_machine's state 0xffffffff to wait until a named part stops being reported (count reaches 0) before resuming the idle cheat-name scan; name is read from a small scratch buffer at param_1+0x6c0 that the cheat state machine fills in.

## 0x44D1A0, time_frame_delta

No arguments. Returns float10, the value of a single global _DAT_005a3be4. Plain accessor; the global itself is a runtime variable (the current frame's delta time in seconds, per CLIENT_PHYSICS_MAP's car+0x0034), not a constant, so reading it with read_memory would only show whatever the static image happens to hold, not a meaningful value.

## 0x44ED50, time_now_ms

Arguments: this (a clock object, ECX/fastcall param_1, offset +8 stores the last sample, +0x10/+0x14 cache the QueryPerformanceFrequency-derived ticks-per-ms). On first use calls QueryPerformanceFrequency and stores frequency/1000 at +0x10/+0x14; every call then does QueryPerformanceCounter and __alldiv's it by that cached value, storing and returning the result as a 64-bit millisecond count (EDX:EAX; the decompiled signature under-types the return as 32-bit but every caller treats it as 8 bytes). This is the millisecond clock used throughout the tick (crash recovery, boost timing, ghost recording, cheat-name state machine).

## 0x44E240, math_atan2_deg

Arguments: float y-ish (param_1), float x-ish (param_2). Returns float10, degrees in [0, 360).

Handles the four axis-aligned special cases directly (0,0 -> 0; param_1==0,param_2>0 -> 0; param_1==0,param_2<=0 -> 180; param_2==0,param_1>0 -> 90; param_2==0,param_1<0 -> 270), otherwise computes `fpatan(param_1/param_2, 1) * _DAT_005a3c64 (57.2958, i.e. 180/pi)`, adding 180.0(_DAT_005a32a8) when param_2<0 and subtracting 360.0(_DAT_005a3c78) when param_1<0, then always wraps the result into [0,360) through math_wrap_angle_360 (0x44D9C0) before returning. Every caller in this file passes (dx, dy) or similar deltas, i.e. this is the project's bearing/heading function, in degrees, not radians.

## 0x44DD00, math_wrap_angle_signed180

Argument: float* in/out. No return.

Calls an unlisted helper FUN_0044db60 first (a coarse wrap, not in scope), then folds the result to keep it within [-180,180]: if negative, prefers value+360 when that has smaller magnitude; if positive, negates 360-value when that has smaller magnitude. Used for signed angle differences (deflection angles, checkpoint bearings).

## 0x44D900, math_hypot2d

Arguments: float, float. Returns float10. 0 if both are exactly 0, otherwise sqrt(a*a + b*b) via the x87 SQRT instruction on float10 intermediates. Plain 2D distance/length.

## 0x44D9C0, math_wrap_angle_360

Argument: float* in/out. No return. Repeatedly adds or subtracts 360.0(_DAT_005a3c78) (unrolled in batches of 8, capped at 64 iterations = works for magnitudes up to ~23040 degrees) until the value lands in [0,360). Companion to math_wrap_angle_signed180; math_atan2_deg calls this one internally.

## 0x444720, car_node_set_transform

Argument: fastcall this = scene node pointer, plus an implicit second (matrix pointer) passed on the stack that the decompiler could not name. If node+0x94 (a "has a live transform" byte) is set, calls the node's own vtable slot 0x1c (i.e. `node->vtbl[7](node, matrixPtr)`), passing the 4x4 matrix built earlier in the caller. This is the generic "push a computed matrix down into the engine's scene graph" call used for the car body, the driver, and every attached part; called repeatedly from car_visual_update (0x48E6A0) and from car_physics_tick_local's own wheel-matrix loop.

## 0x48E6A0, car_visual_update

Arguments: game, carIndex. No return. Shared by car_physics_tick_local (only in the car+0xa7854==2 spectating branch, together with car_ghost_sample_apply), car_remote_update (0x49ED90), and two further unassigned callers (0x48f620, 0x4990f0) - so it is the general "recompute visuals from a position/yaw sample" routine, not physics-exclusive to spectating.

Recomputes, from car+0x3244/0x3248/0x324c (current position) and car+0x325c/0x3260/0x3264 (previous position), divided by car+0x34 (frame time): car+0x3238/0x323c/0x3240 (velocity), car+0x3234 (speed, math_hypot2d), car+0x32f4 (kmh = speed * _DAT_005a69a8, sign-flipped by car+0x332d), car+0x32f0 (an engine-rpm-ish value via the rigid-body helper FUN_004ec800, out of scope). Updates the four wheel steer angles at car+0x32a4.. with a durability-curve-scaled offset. Recomputes pitch (car+0x3224) and roll (car+0x3228) from the four wheel-height deltas using math_hypot2d/math_atan2_deg/math_wrap_angle_signed180 (0x44D900/0x44E240/0x44DD00), the same construction car_physics_tick_local itself uses after the substep loop. Rebuilds car+0x3224's steering-scaled offset (car_suspension_shake, 0x49A8A0, for kind-2 vehicles) and car+0x3018 body roll. Rebuilds the car world matrix (car+0x2fe0) and all four wheel matrices (car+0x3020, stride 0x40) with steer/spin and, when fewer than 2 wheels are grounded and the car is fast, a random bump (rand() scaled by car+0x3728[wheel] grip and _DAT_005a69a0). Pushes every resulting matrix to the engine via car_node_set_transform (0x444720): body, driver, each wheel, each attached part. Detects a boost-kind change since the previous call (car+0x3300/0x3304 vs car+0x3318/0x331c, cached each call) and plays the boost-start sound (FUN_00496960) on a change. Calls car_respawn_state_machine (0x4A1420) for the local car when not finished/spectating-cancelled, and car_boost_speed_gate (0x49A390) unconditionally. Runs FUN_0048b460 with a state code 0/1/2/3/4 derived from reverse flag, boost state, speed, and car+0xa78e4 (correction 2026-09-15: that is `driver_clip_play`, the driver body clip id, EFFECTS_AND_GEAR.md Driver clips). For vehicle kind 5, updates the extra spin accumulator car+0xa7998 against its period car+0xa7994 using time_frame_delta and pushes it through FUN_00444820/FUN_004447f0 (scene, out of scope) - the same tail car_physics_tick_local runs itself for kind-5 vehicles.

Given its size (about 500 decompiled lines) this is documented at the structural level above, not statement by statement; every field name given was read directly, the summary of "why" for the boost-sound state codes and the kind-5 tail is carried over unchanged from car_physics_tick_local's own copy of the same logic.

---

## Crash recovery state machine, game+0x13972f4

Read directly at the top of car_physics_tick_local (0x49C0D0), lines ~166-200 of its decompile. State byte at game+0x13972f4, an 8-byte millisecond timestamp at game+0x13972f8 (time_now_ms). Four states: 0 idle, 1, 2, 3 (there is no separate state 4 - "four windows" refers to the four timed windows below, not four numbered states).

- State 1 (a rollover/impact was detected elsewhere and started this state - the code that sets state=1 was not located in this pass): once elapsed (now - stored timestamp) is between 800 ms and 2001 ms, checks wheels_on_ground_count() > 2; if the car has landed on its wheels within that window, and the "recover" key is not currently held, stores a fresh timestamp and moves to state 2 (if the key is held, nothing advances, it re-checks next tick). If the car has not landed by 2001 ms, or landed with fewer than 3 wheels down when the window closed, resets to state 0.
- State 2: while elapsed < 1001 ms and the car remains grounded (>2 wheels), stays in state 2. As soon as either 1001 ms have passed or the car leaves the ground again, stores a fresh timestamp and moves to state 3.
- State 3: for the first 200 ms of this state, checks the "recover" key every tick; if held, calls car_boost_start(carIndex, 0, 0) (a kind-0/mini-turbo reward boost) and returns to state 0. If not held, or once 200 ms have passed, it simply waits; at 1000 ms with no key press it silently resets to state 0 (no reward).

So the four measured windows are 800-2001 ms (waiting to land), 1000 ms (settle), 200 ms (key-press reward window) and 1000 ms (overall timeout for state 3). This matches CLIENT_PHYSICS_MAP's "800, 2000, 200 and 1000 ms windows, a boost when the key is held at the end".

## Item counters, 0xBFC3B0 to 0xBFDA1C

The gate that "feeds" car_input_poll_dispatch (0x497E40) is not inside that function; it is in car_physics_tick_local itself, immediately before the call (decompile line ~257):

```
if ( race_state != 0xd
  || DAT_00bfc3b0 > 1 || DAT_00bfc510 > 1 || DAT_00bfc830 > 1 || DAT_00bfc8d0 > 1
  || DAT_00bfcd2c > 1 || DAT_00bfd458 > 1 || DAT_00bfd6d0 > 1 || DAT_00bfd780 > 1
  || DAT_00bfd824 > 1 || DAT_00bfd8c8 > 1 || DAT_00bfd978 > 1 || DAT_00bfda1c > 1 )
    car_input_poll_dispatch(carIndex);
```

In every race state except 0xd (13) the dispatch always runs. In state 0xd specifically it is skipped unless one of these twelve counters is above 1. These are twelve separate global int32s (not one uniform array - the gaps between them are irregular: 0x160, 0x320, 0xa0, 0x45c, 0x72c, 0x278, 0xb0, 0xa4, 0xa4, 0xb0, 0xa4), so each is its own named field rather than one loop-indexed table, consistent with them being individual per-effect-type or per-event "how many active" counters inside a larger struct.

get_xrefs_to on 0xBFC3B0, 0xBFC510, 0xBFD780 and 0xBFDA1C found only read sites: the gate above (car_physics_tick_local, 0x49c462/0x49c46a/0x49c49a/0x49c4ba), and one further read of DAT_00bfc510 from FUN_004c7ed0, an unrelated world item/gimmick-object manager (loops up to 8 track hazards such as bananas/thunderclouds, not called from the physics tick, out of scope). No static write xref was found for any of the four sampled addresses. The likely explanation is that whatever writes these counters (spawning/consuming an item or hazard) indexes into this block with a runtime-computed address (e.g. itemType*stride + tableBase) rather than a compile-time-constant displacement, which Ghidra's static xref table does not resolve to the individual field addresses. Locating the actual writer would mean tracing the item/gimmick pickup and spawn system, which is outside the assigned tick-helper scope - left unresolved, marked as a guess only: the block is almost certainly "active count per item/hazard type, this race", written by that system.

## Constants read (address: float32 LE value)

0x59f404: 10, 0x59f414: 0.5, 0x59f44c: 0, 0x59f450: 50, 0x59f480: 1,
0x5a15ec: 0.2, 0x5a15f0: 8, 0x5a1644: 40, 0x5a1650: 0.3, 0x5a1e88: 0.0174533 (pi/180),
0x5a2494: 0.1, 0x5a24ec: 2, 0x5a3218: 1.05, 0x5a3230: 15, 0x5a323c: 90,
0x5a32a0: 80, 0x5a32a8: 180, 0x5a32b0: 1.2, 0x5a32b8: 3, 0x5a32d4: 0.25,
0x5a3c64: 57.2958 (180/pi), 0x5a3c78: 360, 0x5a3ec0: 25, 0x5a68cc: 1.6, 0x5a69e4: 0.94,
0x5a6a14: 120, 0x5a6a1c: 0.86, 0x5a6a34: 250, 0x5a6a44: 0.06, 0x5a6a48: 1.5,
0x5a6a74: -8, 0x5a6a80: 0.000204082, 0x5a6a88: -25, 0x5a6a8c: -2, 0x5a6aa8: 0.95,
0x5a6aac: 0.82, 0x5a6ab0: 1.18, 0x5eb6f8: 90.

All read with read_memory, program KnC.exe.raw, 4 bytes little endian, interpreted as IEEE-754 float32. Every other _DAT_ constant named in the sections above was seen in the decompile but not individually read with read_memory in this pass; treat its role (from the surrounding expression) as read evidence, its numeric value as unread unless listed here.

## Round four, read on the bytes 2026-09-14

### 0x59029C, crt_ftol_trunc

The CRT float to int64 helper, truncation toward zero of the value on the FPU stack. Every caller in this file loads its own value first.

- `car_physics_tick_local` 0x49C503 rounds ceil of frame time over the fixed step, the substep count
- 0x49CEFC and 0x49CF4C round the capped km per hour minus 30.0 (0x5A3244) times 0.5 (0x59F414), clamped 0 to 99 at 0x49CF01 to 0x49CF0E, the index into the 100 float table at game+0x1396FE0. The table is a speed curve, at 30 km per hour it reads 1, near 228 it reads 0
- `car_substep_collision_response` 0x498B1D rounds the heading at game+0x6FC folded to 0 90 (0 to 90 gives 90 minus h, 90 to 180 gives h minus 90, 180 to 270 gives h minus 180, 270 to 360 gives h minus 270, bands at 0x5A323C 0x5A32A8 0x5A6A50 0x5A3C78) times 1 over 90 (0x5A6A4C) clamped 0 to 1 times 100 (0x5A1648). An out of range heading keeps the 0.0 first loaded. The same table is an impact angle curve here, index 100 reads the float after the table
- `car_boost_update` 0x497092 and 0x4970B6 round wire stat 3 plus one clamped 1 to 2 times 400.0 (0x5A6A18), the kind 0 duration in ms
- `car_ghost_sample_record` rounds the yaw byte and the two nibbles, their FPU inputs stay untraced

### 0x496BE0, car_boost_start, corrected

Kind 1 target is 220.0 (0x435C0000 at 0x496D98), not 214. Kind 4 writes neither the decay nor the target (0x496DBD jumps to 0x496DE3), only kinds 3, 5 and 7 take 200.0. Kind 0 decay strength is 6.0 (0x496D31). The part bonus 0.04 is the immediate at 0x496CFE. game+0x1397300 is set to 1, game+0x1397308 to the start time, game+0x1397304 to 0.7 for kind 0 (0x496E11) and 0.3 otherwise (0x496DFD), a HUD flash. The call at 0x496C34 with ECX 0x1ADF810 is not a camera check, it is `world_theme_is_special_row` 0x487230, true when the theme record at world+0x13348 has its field 4 equal to 0x1312D00, a BSS row. It only decides whether an equal kind or a lower kind refuses the restart.

### 0x496E50, car_boost_update, corrected

The durations are immediates. Kind 1 3800 (0x4970C8) plus 500 (0x49701E) when a part of catalogue type 0x16 is equipped, kind 2 6000 plus the same 500, kind 3 6000, kind 4 5000, kind 5 15000, kind 7 1500, kind 6 5 and the state drops to 0 at once, kind 0 the formula above. The state 1 push is `car_boost_push` 0x496B40, FUN_0044DD50 turns yaw minus the smoothed gauge times 1.8 (0x5A3214) and the pitch at car+0x3224 into a direction, `body_apply_force` receives minus x times the decay strength, minus y times it, and a tenth (0x5A2494) of it as z.

### 0x498960, car_substep_collision_response, corrected

Restitution 0.98 (0x498B2B), 0.94 while stuck (0x498B35), else curve times 0.06 plus 0.94 under index 20, applied by `body_vec3_scale` on the velocity at car+0x25FC. Impulse scale clamped 0.5 (0x498B97) to 3.0 (0x498BB2). Spin term one minus curve times km per hour times 0.04 (0x5A3EB8, not 0.25), capped 1.5 (0x498C13), only above index 50. Friction floor 0.8 (0x498D46) with no drift and under 3 wheels. The impulse at 0x498D6C is x from car+0x2E34, y from car+0x2E38, z the spin term times dt times 0.25, added by `body_vec3_add` on car+0x25FC. The index is the impact angle index above.

### 0x4968F0, car_apply_engine_force, the four tick call sites

`body_apply_force(minus x, minus y, z)`, the third argument passes untouched.

- 0x49C54B engine kick, `(0, 0, game+0x1397190)` while game+0x1397188 is 1, then the value decays by game+0x1397194 and clears under 0.1
- 0x49C704 air kick, `(0, 0, minus ramp)` where the ramp at 0x2EB0688 grows 0.1 a tick while the baseline is armed and `world_theme_is_special_row` is true and no effect runs, pushes only above 3.0 and holds at 8.0
- 0x49CED4 lateral push, with four wheels down, no drift and a yaw rate at car+0x32E4 outside plus minus 0.01: heading is yaw when the rate is positive and yaw minus 180 otherwise, `math_dir_from_heading` 0x44DDD0 gives cos and sin of minus heading minus 90 times 0.3 (0x49CEB6), the push is `(x, y, 0)`
- 0x49CFAE every substep, `(0, 0, tuning 0x5EB6F0 times minus 0.16 (0x5A6AE0))`, the gravity of this client

The second substep call at 0x49D00A is `body_apply_force` on the wrapper with minus car+0x2610, minus car+0x261C, minus car+0x2628 each times S, S is one minus the speed curve (or 1.0 while an effect runs) times 0x5EB6F4 times 0.16 (0x5A6ADC). Those three floats are column 2 of the 3 by 3 at car+0x2608, the R the corners below use, so the push follows the body's own z axis. Car+0x2610 is not an engine force base. The drive is inside `body_step_world`, `gear_wheel_force_solve` reads wheel set +0x550, +0x574, the gear ratio at +0x580 plus gear times 4, and the per wheel limits at +0x984 and +0x988, BODY_MOTION.md and EFFECTS_AND_GEAR.md own that path.

### The throttle factor scales the velocity

The step 11 factor ends at 0x49CE2F as `body_vec3_scale` on car+0x25FC by gain times factor, gain is one plus wire stat 0 (car+0x3448 plus the bonus at car+0xA7940) times 0.01 (0x5A05E0) clamped 1.0 to 1.01 (0x5A6AE4). Two clauses of step 11 were read wrong before. The taper by 0.988 or 0.96 runs when the coast flag is clear, the flag is set whenever any of accel, stuck, right or left is held, two or more wheels touch, a boost or a drift runs, or the slot 0 key itself is down at 0x49C884, so the taper only acts while coasting airborne with nothing pressed, and under speed 1 it sets game+0x28 and zeroes the factor. The gap clause at 0x49CC50 subtracts capped km per hour minus the turn force cap times 0.00014 (0x5A6AFC). The item clause at 0x49CA6B is `item_slot_lookup` 0x451D30 on car+0x348C index 0, record stride 0x10 from +4, count at +0x44, kind at record +4 and count at record +8, kind 3 with a count under 1 multiplies by 0.978 (0x5A6B00).

### Step 9 details

The friction pair at 0x49C8BB is friction of wheel 0 plus wheel 1 plus twice car+0xA7988, times 0.5, times the wheelbase at car+0x32DC, and the same for wheels 2 3 with the track at car+0x32E0. After the drift update the first entry scales by 0.8 off gas or 0.6 on gas while drifting, both scale by 8.0 with the slow flag and by 0.75 when the view global 0x2F0DDB0 is 2, then `car_effect_update` takes the pair and `car_apply_force_if_valid` forwards it. The per wheel grip at car+0x3728 is wire stat 12 (car+0x3478) times the surface grip plus car+0xA7990 times 0.2 (0x5A15EC). The air penalty adds car+0xA798C to the surface value, game+0x28 is cleared right before that loop.

### Step 18, yaw pitch roll from the body R

0x49DE10 to 0x49E12B. For i 0 to 3 the point is (1, 1, 0), (1, minus 1, 0), (minus 1, 1, 0), (minus 1, minus 1, 0), `body_vec3_madd` adds zero times (0, 0, 1), `body_vec3_transform` multiplies by the 9 floats copied from car+0x2608 with a zero translation. The wheel object read at 0x49DE1C is overwritten before use. Front is the mean of corners 0 and 1, rear of 2 and 3, the y plus side of 0 and 2, the y minus side of 1 and 3. The dy is pushed first at 0x49DFC7 and dx second, so the call is `math_atan2_deg(dx, dy)` with dx rear x minus front x and dy rear y minus front y, plus 90. With R identity that is 270 plus 90, the yaw 0 of the launch straight. The wrapped difference to car+0x3220 outside plus minus 0.1 moves the yaw by a quarter and wraps to 360. Base is `math_hypot2d` of the same dx dy. Pitch delta is `math_atan2_deg(base, rear z minus front z)` minus 90 minus car+0x3224, applied by 0.0625 outside the deadzone, a flat car gives 90 minus 90. Roll is the same with the y minus side height minus the y plus side height into car+0x3228. This is the only yaw writer of a normal turn, the steering reaches the yaw through R.

### 0x49D135, body_step_world arguments and the probe arming

Every substep the tick calls `body_step_world(wrapper, dt, game+0x18, flag)`. The second argument is the six flags accel, brake, game+0x20, game+0x24, game+0x28, game+0x2C read as floats, the body copies them into wheel set +0x5FC. The flag is car+0x9D4 loaded at 0x49D0F3, set to 1 by `car_physics_setup` 0x494F4C and the kart loadout 0x490EE7, and zeroed on the gear clamp path when car+0x9D8 is 0 or car+0x9D9 is 1. The probe context car+0x3608 is armed once at spawn by `world_place_probe_local` 0x486300, which clears +0x9C +0x08 +0x90 +0x94 +0x98, binds piece 0, runs the height scan and sets +0x9C to 1, without it `world_ground_test_point` returns 0 and the stuck probe fires every tick. The drift update writes car+0x2974 as the wheel set max steer radians, catalogue 0x94 times pi over 180 times the two scales, and car+0x2A78 0x2A7C as the material rates of spring channels 2 and 3, the steer channels.

### 0x49FAD0, car_ghost_sample_record, cadence

The divider at 0x49FB0E is 10 outside race mode 0xD and 0 inside it, so mode 0xD records every tick. The wrap at 0x49FE9E is 24000 in mode 0xD and 2400 (0x960) otherwise, the ring memory from car+0x3754 ends at car+0xA7834 right before car+0xA7854, so it holds the 24000. On the wrap car+0xA7854 and car+0xA785C go to 0. The input mask byte at entry +0x18 is in REMOTE_AND_INPUT.md.

## Round five, read on the bytes 2026-09-14, the ghost run

### The ground byte and the count

car+0x2DF0 to 0x2DF3 is 0 for a loaded wheel and 1 for a wheel in the air (0x4EECE2 and 0x4EF068 0x4EF070), `body_wheels_on_ground_count` 0x4EFA50 returns the number of wheels in the air. Every tick read in the client, with that count:

- 0x49C2C2 and 0x49C322 crash recovery state 1 goes to 2 when count over 2, more than two wheels in the air, state 2 goes to 3 when count 2 or less
- 0x49C58E the airborne flag car+0x3716 arms with count 4 and no drift and no REGE or LAVA wheel, the landing branch fires with count under 2 (0x49C743), so the flag reads airborne then landed, not a ground baseline
- 0x49C85E the coast flag stays clear with count under 2 among the other conditions
- 0x49CA05 the loop subtracts `world_surface_contact_drag` 0x486D70 (was `world_surface_air_penalty`, renamed) plus car+0xA798C times a quarter for every wheel whose byte is 0, the loaded wheels, a per surface drag on contact, water 0.01, ice minus 0.001, asphalt 0
- 0x49CC9A the throttle drag 0.997 (no boost) or 0.99 (item boost) applies with count over 2, airborne
- 0x49CE34 the lateral push applies with count over 3, all four wheels in the air, no drift, and car+0x32E4 outside plus minus 0.01
- 0x498D46 the collision friction floor is 0.8 with no drift and count 2 or less, 0.5 otherwise
- `input_poll_keyboard` forces accel off with count over 3

The port's expressions were literal copies and already matched, its names and comments read the count as grounded wheels and are corrected, `wheelsInAir`, `airborneFlag`, `contactDragExtra`, `kEngineDragAirborneNoBoost`, `wheels_over_three_in_air`.

### 0x49CF48, the down push index

Two indices into the 100 float table at game+0x1396FE0 in step 12. The first at 0x49CEEC is the capped km per hour minus 30 times a half, clamped 0 to 99, its value times 19.6 goes to `body_apply_durability_scale`, the gravity scale of the body. The second at 0x49CF48 is the raw capped km per hour truncated and clamped 0 to 99, one minus its value is the S of the substep push `body_apply_force(minus R column 2 times S times 0x5EB6F4 times 0.16)`. With the curve 1 minus sin(i pi over 200) the push term is 0 at rest and saturates at 99 km per hour, which is the recorded z settling by 0.8 s. The port used the first index for both.

### car+0x32E4 is the steer average

0x49D4CD copies car+0x297C into car+0x32E4 right after the `car_effect_lean_update` call at 0x49D4B4. car+0x297C is wheel set +0x520, the mean of the two front Ackermann tangents `body_steer_front_wheels` 0x4F1BE0 writes, not a yaw rate. The lateral push of 0x49CE34 reads it as the sign of the push direction. The port copies it after `body_finalize_wheels`.

### world_on_track_check 0x4A0750

Returns 1 for car+0x6BC under 100 or over 299 and 0 for 100 to 299. The tick forces the brake and clears accel and stuck on 0. So 100 to 299 is the respawn teleport sequence (100 set by the watchdog, 0x65 reposition, 0x66 fade) with the brake forced, and 0 is free driving. The harness and the tick test start at 0.

### car_apply_engine_force 0x4968F0

`if (game+0x1397384 != 0) body_apply_force(minus x, minus y, z)`. The same session flag gates `input_poll_keyboard` and `world_wheel_surface_index`, set for the whole race, the harness sets it.

### car_ghost_sample_record 0x49FAD0, the ftol inputs

- 0x49FB69 yaw byte: car+0x3220 through `math_wrap_angle_360` times 0x5A6ABC (255 over 360) truncated
- 0x49FBA1 low nibble: (car+0x35A8 plus 0x5EB700 (45, the gauge cap)) times 0x5A3230 (15) over twice the cap, truncated and masked, 7 at gauge 0
- 0x49FBD9 high nibble: car+0x32F8 minus 0x5A3BF4 (1000) clamped 0 to 0x5A6AB8 (9000) times 0x5A6AB4 (1 over 600), truncated and masked
- the position is car+0x3244 0x3248 0x324C as three dwords at 0x49FB4E to 0x49FB62

The port recorder writes all three the same way now.

### Steer sense, the chain

`input_poll_keyboard` 0x497BA7 to 0x497BED writes slot 2 to game+0x24 and slot 3 to game+0x20 with the camera not reversed. The tick pushes game+0x18 at 0x49D12E so channel 2 is game+0x20. `body_world_step` steers by channel 2 minus channel 3. `body_geometry_setup` builds the swing as up cross base at 0x4F2E50, a positive steer tilts the wheel 0 axis toward plus x and the car turns toward the wheel 1 side, the wire yaw grows. The drift update tags slot 3 as 0x27 VK RIGHT. Game+0x20 is the right turn, game+0x24 the left turn, the port names them `steerRight` and `steerLeft`. The port body had base cross up, the one body line this round changed.

## Round six, read on the bytes 2026-09-15, the marker sweep

Every `TODO open` of the tick side was taken to the decompile and the disassembly. What each one turned out to be.

### 0x44DD50, math_dir_from_heading_pitch, the boost push direction

The three thunks are the statically linked D3DX with its PSGP dispatch table at 0xAFB598 (0x51BF63 fills it from 0xAFB6C0). 0x5047B4 is `D3DXMatrixRotationZ` (sincos, m11 cos m12 sin m21 minus sin m22 cos), 0x50485B is `D3DXMatrixRotationAxis` (sincos, one minus cos, `D3DXVec3Normalize` at 0x502F99 on the axis), 0x503C4E is `D3DXMatrixMultiply`. The function builds M1 = RotZ(minus heading), M2 = RotAxis(row 0 of M1, pitch), M = M1 times M2 and returns row 1 of M, the three floats at ESP+0x10 to ESP+0x18. Row 0 of M1 is (cos h, minus sin h, 0) and row 1 is (sin h, cos h, 0), the rotation about row 0 by the pitch keeps the dot zero, so the result is `(sin h cos p, cos h cos p, sin p)`.

`car_boost_push` 0x496B40 calls it with heading minus 90 and car+0x3224, then x and y take the strength and z takes a tenth of the strength alone (the sin p of the vector is dropped), then `body_apply_force(minus x, minus y, z)`. With h = H minus 90 the plane part is (minus cos H, sin H) times cos p, the forward of the wire yaw H. The port had `(sin H, cos H)`, a sideways push, fixed. `car_push_along_yaw` 0x497160 is the same call with car+0x3220 as the heading, the carry drop uses it with 40.

### 0x496BE0, car_boost_start, no camera flag

The only gate of the refusal is `world_theme_is_special_row` 0x487230 at 0x496C34. There is no `car_is_camera_reversed` call in this function, the earlier wording was wrong. Special theme, an equal kind refuses, otherwise a lower kind refuses. The global 64 bit at 0x5C8508 takes the start time of any boost. The 0.04 bonus at 0x496CFE is not a part, it is the equipped pet, see below.

### The pet container, not the part catalogue

`pet_owned_record_lookup` 0x4504E0 (was FUN_004504E0) returns `0x1A69708 plus 4 plus index times 0x1C` under the count at 0x1A69E0C, the owned pet list of the S2C pet packet, record 0x00 instance id, 0x04 the pet key, 0x08 equipped. `pet_key_to_kind` 0x451490 (was FUN_00451490) maps the key 10 20 30 40 to 0x13 0x14 0x15 0x16 and everything else to minus one. The three race side readers walk the records and stop at the first one whose 0x08 is 1:

- `car_boost_start` 0x496CD0, kind 0x15 adds 0.04 to the decay strength of kinds 1 2 3 5 7
- `car_boost_update` 0x496FDB, kind 0x16 adds 500 ms to the kind 1 and kind 2 durations
- `car_drift_update` 0x49B0B0, kind 0x14 rolls `rand() mod 100` and under 3 starts a kind 1 boost instead of kind 0 on the mini turbo release, the Chai chance

The port keeps the list in `stats.h` as `OwnedPetList` with `pet_equipped_kind`.

### 0x496930, car_boost_clear

`if (game+0x1397384 and car+0x3300) car+0x3300 = 0`. Every "reset helper" of the earlier rounds is this one line. The collision response, boost state 3, the effect apply of every code but 800 and 0x44C, the keyboard poll of the special theme and the ghost ring wrap call it.

### 0x496E50, car_boost_update, the rest

- state 0 decays game+0x5C and game+0x60 by 0.86 a tick, floors them at 0 under 0.1, and pushes each to three scene nodes at game+0x44 and game+0x50 through FUN_00448A30 and FUN_00448A50, cosmetic, the port keeps the two floats as `boostNodeFade`
- state 2 goes to 3 at or under 1.0, the pair of flag tests at 0x497176 reads `less or equal`
- the update returns at once for codes 500 600 900 (0x496E6E), it does not clear the boost, the apply of those codes already did

### car_drift_update, three settled points

- the `body_vec3_add` at 0x49B2B9 has ECX = car+0x263C, wheel set +0x1E0, the spin in the principal frame. The drift adds a z spin of wire stat 9 times the gauge times the slowed scale in radians once a tick
- the state set refuses while car+0x6BC is not 0. That field is 0 while driving, 100 to 201 during the respawn sequence and minus 1 while a wheel sits on a boost pad cell, so the refusal covers both
- 0x49AB17 compares the cell under wheel 0 with the string "FLY" at 0x5A6AA4, a FLY cell disarms the drift like an active effect

### car_node_name_is 0x4A1350 reads the cell under a wheel

FUN_004A0680 is `world_wheel_query_surface`, so the name the function compares is the col cell name under wheel N, not a scene node. The collision gate of the substep compares the cell under wheel 2 with REGEN, `car_name_match_count` counts wheels on a named cell, and `car_respawn_state_machine` scans the four wheel cells. The port drops the node name hook and queries the probe.

### The tick, four reads settled

- the crash recovery key at 0x49C229 and 0x49C32B is `input_key_down(input_key_binding_lookup(0, minus 1))`, the raw slot 0 accelerate key, the same read as the coast check
- at the green light (0x1A20B21 is 4 or 5) 0x49C3B0 tests `input_key_pressed(0x31)` then 0x32 then 0x33, the keys 1 2 3, and starts a boost of kind 0 1 2. Debug keys, not the accel brake stuck flags
- the draft factor call at 0x49CBF3 pushes 0x5EB714 and 0x5EB710, read as 30.0 and 150.0, the cone half angle and the max distance, both have no writer
- `input_key_pressed` 0x44B590 reads `this plus 0x104 plus code times 4`, every caller passes a key code (0x41 0x5A 0x31 0x56 and the slot 0 code from the binding lookup), it is indexed by raw key code not by slot

### 0x498DB2, the licence test collision counter

`if (DAT_00B2360C == 0xD and DAT_00BFDAC4 == 0x17) DAT_00BFDABC += 1`. 0xBFDAC4 is the current licence test id, FUN_00461F40 looks it up through FUN_00453330 (the 0xC5 licence test container) for the HUD strings, so test 23 counts the wall hits. Port fields `licenceTestId` and `licenceTestCollisionCount`.

### 0x49A130, car_rival_nearby_cue, was car_stuck_surface_check

`car+0x744 == DAT_01A20658` compares the player id of the car with the local player id (the packet docs prove 0x1A20658, STRUCTURES_VERIFIED.md), so the function runs for the local car only. Every 3000 ms (game+0x1397348), over 30 kmh, unless the driver animation state at 0x1AF2CAC plus car+0x3520 times 0x7C0 is 3 or 0xB, it takes `car_nearest_racing_car` 0x499AA0 (was FUN_00499AA0): the closest other occupied car whose car+0x3724 is negative, car+0x3724 is the S2C 0x3D finish rank, minus one while racing, inside 30.0 (0x41F00000 at 0x49A1D8). The bearing from that car to the local one minus the yaw plus 180 wrapped, inside 70 (0x5A6998) to 290 (0x5A6994) plays sound 3 and the voice cue FUN_004815F0(3). A rival nearby cue, no stuck check.

### 0x4B4B50, standings_local_row_index, was world_zone_table_index

The this is the standings table 0x2EBD6A0 (ITEM_DROP_TABLE_VERIFIED.md), rows of three ints at +0xDC0, the first int is the player id, the function returns the row of the local player id. The 30 float table at 0x5EB718 the tick indexes with it is a per row throttle factor, rows 0 to 6 sit at 0.99934 to 0.99983, rows 7 to 15 at 1.0, a hair off the leaders. Port `standings_rank_throttle_multiplier`.

### 0x497190, car_launch_pad_kick, the engine kick

Refused while game+0x1397188 is 1. Clears the boost, sets car+0x3220 to the row float 1, `body_place_and_probe` at the current position with that yaw, `car_boost_push` along it with the row float 2, game+0x1397190 = row float 3 times 1.6 (0x5A68CC), game+0x1397194 = row float 4, game+0x1397188 = 1, game+0x139718C = the yaw, then `car_boost_start(kind 2)`. The tick step 8 pushes (0, 0, game+0x1397190) every tick and decays it by game+0x1397194 until under 0.1. So the engine kick is the vertical launch of a boost ini kind 3 row, see EFFECTS_AND_GEAR.md round six for the pad detector.

### 0x498800, world_car_push_apart, the terms

`world_car_find_overlap` 0x497F80 (was FUN_00497F80) walks the 30 slots, skips self, car+0x740 zero and car+0x9DA one, keeps a car whose z extents overlap (`self z <= other z + other 0x2EA8` and `other z <= self z + self 0x2EA8`, car+0x2EA8 is the catalogue chassis extent z) and passes `world_car_overlap_test` 0x497EE0 (was FUN_00497EE0): `math_hypot2d` of the plane delta at or under 10.0 and under 0.8 times the sum of the two car+0x2EA4 (chassis extent y). Then the delta other minus self is normalised, self car+0x25FC scales by 0.994 (0x3F7E76C9) and adds (0.8 dx, 0.8 dy, dz), the other car+0x3244 0x3248 move by 0.8 dx and 0.8 dy times 0.06 (0x5A6A44), and the faster car (ties to the other) plays `car_impact_effect_play` 0x4981B0 tier 0 over 30 kmh or tier 1 over 15. The tick calls it every substep outside modes 0xF 0x11 and drops the result. The port runs it in `world_car_push_apart_tick`, a no op with one car.

### 0x4981B0, car_impact_effect_play

Tier 0 or 1 picks a scene node at game+0x3C plus tier times 4, plays it a quarter of the way from the camera reference at 0x5C83A8 toward the car unless it already plays. The collision response calls it with tier 1 over impact index 20, the push apart with the speed tier, the 1000 effect with tier 1. Port hook `impactEffect`.

### The wheel matrix block 0x49D4F5 to 0x49DA27

After the car matrix and one `car_effect_lean_update` call (the only one, 0x49D4B4), car+0x32E4 takes car+0x297C. With car+0x373C set or speed over 1 each car+0x32A4 takes minus the wheel spin angle at car+0x26C8 plus i times 0xB8, times 0.25 when the ground byte at car+0x2DF0 plus i is 0. With car+0x9E0 under 5 the body pitch offset car+0x3738 eases a quarter toward `clamp((0.1 minus the sum of the four tyre peaks at car+0x2CBC stride 0x14 times 1e minus 6) times 8, 0, 0.4)`. Per wheel a random bump `(rand mod 600 minus 300) times min(kmh, 100) times the grip car+0x3728 times 4e minus 6` with under two wheels in the air, over 10 kmh and no bump slot, a squash scale of one plus a quarter of the bump when positive and not drifting, the translation is the model node position at car+0x3528 plus i times 0xC (x, y, z plus the bump minus car+0x3738), the front wheels steer by car+0x32E4 times 6, times 6 again while drifting, clamped plus minus 0.5236, the roll term is the kind 2 wire stat 7 lean or plus minus 0.0873 by side, the spin term is car+0x32A4, the product goes through the car matrix into `car_node_set_transform`. The port keeps the node positions as `wheelNodePos` the host fills and builds steer then spin.

### Constants read this round (address, value)

0x5EB710 150.0, 0x5EB714 30.0, 0x5A6A20 minus 0.2, 0x5A6A24 65536.0, 0x5A6A28 1 over 32768, 0x5A6A2C 1 over 256, 0x5A32D0 1 over 32, 0x5A6998 70.0, 0x5A6994 290.0, 0x5A6B0C 0.958, 0x5A6B10 0.64, 0x5A6B14 1.068, 0x5A6B24 0.34, 0x3F7E76C9 0.994, 0x41166666 9.4, 0x42200000 40.0, 0x40666666 3.6, 0x3F59999A 0.85, 0x3DB2B8C2 0.0873, 0x3F060A92 0.5236.

## Round seven, read on the bytes 2026-09-15, the turn after the pad

### 0x44DB60, math_wrap_angle_open360, the coarse pre wrap of math_wrap_angle_signed180

The same unrolled loop as `math_wrap_angle_360` with the first test on 0x5A3C7C (minus 360.0, the float after the 360.0 at 0x5A3C78): adds 360 while the value is at or under minus 360 (minus 360 itself steps to 0), then subtracts 360 while the value is at or over 360, eight steps per outer round, the counter stops at 0x40, so at most 64 steps each way and a magnitude over 23040 comes back only partly wrapped. The result sits in the open interval minus 360 to 360, then `math_wrap_angle_signed180` folds it: under 0 it takes value plus 360 when that is smaller than minus value, else it keeps the value, at or over 0 it keeps the value when value is at most 360 minus value and takes minus (360 minus value) otherwise. Port `math_wrap_angle_open360`, `math_wrap_angle_360` now runs the same capped loops instead of fmod, `kAngleWrapMaxSteps` 64. Renamed in Ghidra.

### car_drift_update, the zero gauge branch

The unwinding block at 0x49ADD3 runs whenever the drift key is up or the speed is under 30 kmh. With the gauge at or over 10.0 (0x59F404) it unwinds down by tick scale times 1.5 (0x5A6A48) and turns the yaw, under 10.0 it adds tick scale times 1.5 to the gauge and, once over 0, writes gauge 0 state 0 stage 0 at 0x49AE18 to 0x49AE24 and fires the release boost on a threshold count over 1. So with a zero gauge the block resets the state every tick and never reaches `car_body_set_yaw`. Tick scale is dt times 80.0 (0x5A32A0), the drift needs 20.0 kmh (0x5A3298), the gauge floor of the upper branch is minus 10.0 (0x5A324C). All six read this round, the port has them.

### The ghost record, the turn side bits and the cadence

`car_ghost_sample_record` 0x49FAD0 runs at step 4 of the tick, before the key poll of step 5, so the mask it reads through `input_key_down` is the key state at the top of the tick and the position and yaw are the state after the previous tick. The bits 0x02 and 0x04 of the flags are car+0xA78E4, written by `input_poll_keyboard` at 0x497DA9 and 0x497DBB as 1 for game+0x24 (slot 2) and 2 for game+0x20 (slot 3), at the poll of the previous tick. The two readings are 20 ms apart, when they disagree the key edge sits inside that tick, over the RageZone recording that pins 16 edges, one of them a right tap at 5.0 to 5.19 s the mask never shows. The boost bit is 0x80 for car+0x3304 equal 0 (0x49FC6F) and 0x40 for any other kind, a kind 0 boost is a mini turbo or a boost.ini kind 0 pad, the drift state bits are 0x100 and 0x200 at 0x49FD07 and 0x49FD31. The harness `--flags` reads the turn side bits into the steer keys of the tick before each record and reads the boost bit on the last tick of a sample so a pad the port crossed itself is not started twice.

### body_wheel_axis_correct 0x4EFC90, the yaw damper

Read again for the held turn. The gate is `acos(R z z) minus one degree` against the double 0.0 at 0x5A83E8 through `FCOMP double ptr` and `TEST AH 5` `JNP`, the body runs when the lean is at or over one degree. The body slerps the orientation toward the upright by 0.12 (0x3DF5C28F), then transforms the spin to world axes, scales x and y by `body_ramp_toward(v, 8 minus the count of wheels in the air)` and z by `body_ramp_toward(v, 6.0)` (0x40C00000) when at most two wheels are in the air, else zeroes z, and transforms back. `body_ramp_toward` 0x4EF370 returns (target minus abs v) over target when abs v is under target and target over 1, else 0, so the spin z loses v squared over 6 per tick, 17 percent at 1 rad per second. The principal frame leans 1.6 degrees at speed (the two box tensor plus the front heavy pitch), so the damper runs on the straight and in every turn, it is what holds the held turn at about 1.05 rad per second, without it the short presses of the recording overshoot by 4 to 9 degrees. The port has it as read.

### Constants read this round (address, value)

0x5A3C7C minus 360.0, 0x5A83E8 double 0.0, 0x59F404 10.0, 0x5A324C minus 10.0, 0x5A6A48 1.5, 0x5A32A0 80.0, 0x5A3244 30.0, 0x5A3298 20.0, body create immediates 0x47A7DBD5 85947.66, 0x47A7DB80 85943.0, 0x3F99999A 1.2, 0xBE4CCCCD minus 0.2, 0x3851B717 5e minus 5, 0x3E4CCCCD 0.2, 0x40C00000 6.0, 0x40400000 3.0, 0x42480000 50.0, 0x41A00000 20.0.

## Round eight, read on the bytes 2026-09-15, the speed through the long turns

The held turn speed of GHOST_REFERENCE.md the fifth run. Every candidate term went back to the disassembly, none differs from the port, the two gaps were the harness resync and the grass under the free run.

### The substep loop 0x49CF90 to 0x49D13E

The loop head is 0x49CF90, the entry jumps to it from 0x49CF8B with the count in [ESP 0x28], the back edge is `DEC dword ptr [ESP 0x28]` then `JNZ 0x49CF90` at 0x49D13E. Inside, in order: the world push `car_apply_engine_force(0, 0, 0x5EB6F0 times 0x5A6AE0)` at 0x49CFAE, the R column push at 0x49D00A with the FPU chain `FLD [0x2628]`, `FLD [0x261C]`, `FLD [0x2610]` scaled by [ESP 0x1C] times 0x5EB6F4 times 0x5A6ADC and negated into the three stack args, `body_integrate` 0x4ECAB0, the collision gate on car+0x2E20, the position copy, the push apart, the gear clamp, `body_step_world` 0x4EC560. Both pushes run 14 times a tick at 20 ms, as the port.

### The velocity scale 0x49CDDA to 0x49CE2F

`FLD [car 0x3448]`, `FADD [car 0xA7940]`, times 0x5A05E0, plus 1.0, clamped 1.0 to 0x5A6AE4, times the factor at [ESP 0x18], `body_vec3_scale` on car+0x25FC. The factor took the jitter at `FMUL [EBP 0x6F8]` with EBP the game base, so the jitter is game+0x6F8, one per game, the port keeps it on the local car. The taper and the gap clause read as ported.

### car_drift_update 0x49B1F9, the steer gains

`car+0x2974 = car+0x2F34 times 0x5A6A98 times 0x5A6A94 times fVar2 times param_2` with both scales 1.0 in state 0 off reverse, and `car+0x2A78 = car+0x2A7C = clamp((car+0x3450 plus car+0xA7948) times 0x5A32B8 plus 1, 1, 0x5A0054) times car+0x32E8 times fVar2 times the slowed scale`. 16 degrees and 0.76 for basic 1. The literal 0x2F34 appears in 13 instructions program wide, 12 are `CMP [EAX 0x2F34], EBP` in FUN_0040A070 on an object with fields 0x1CC84 and 0x2370, not the car, the last is this FLD, so the catalogue load is the only writer.

### 0x490C20, the origin of car+0xA7988 +0xA798C +0xA7990

`car_apply_kart_loadout` zeroes the three at 0x490B46 to 0x490B52 with the 17 bonus floats (`REP STOSD` of 0x11 dwords at 0x490B37), then, with [EBX 0x14] equal 1, applies the eight loadout pairs through `stat_bonus_add_part` and looks the second pair's id [ESI 0xC] up at 0x490C24 through `part_def_record_lookup` 0x44FBC0 (was FUN_0044FBC0) on the container 0x1A7932C, writing record 0xCC to +0xA7988, 0xD0 to +0xA798C and 0xD4 to +0xA7990 at 0x490C38 to 0x490C50. With no part the lookup fails and the three stay zero, so the friction pair of step 9 on asphalt is the catalogue 3.8 and 5.4 and `body_set_tire_grip` writes exactly the `body_create` values every tick.

### 0x496BE0 0x496E50 0x496B40, the boost once more

`car_boost_start` calls `car_boost_start_effect` 0x496960 (was FUN_00496960) right after the gate, it lerps the car position toward the camera reference 0x5C83A8 by 0x5A3294 and plays a scene node of game+0x44, cosmetic, then sets the kind, the time, state 1, the decay strength (6.0 for kind 0, 1.0 plus the pet bonus for 1 2 3 5 7) and the target (kind 0 clamp(wire stat 3 times 0.2 plus 1, 1, 1.2) times 120, kind 1 220, kind 2 260, kinds 3 5 7 200, then at least the km per hour plus 10.0 at 0x59F404). `car_boost_update` state 1 calls `car_boost_push` 0x496B40 (named this round) with car+0x3220 minus car+0x35AC times 1.8 and car+0x3308 while car+0x32F4 is under car+0x330C, the push is `math_dir_from_heading_pitch(heading minus 90, car+0x3224)` times the strength on x and y and a tenth of the strength on z into `body_apply_force`, a velocity add. The port's mean speed over each sample through the pad boosts sits within 0.6 of the recording (102.5 101.8 103.4 against 102.3 102.4 103.2 at 4.8 to 5.2 s).

### 0x4EFC90 with the registers

`LEA EBX, [ESI 0x1AC]` R, `LEA EBP, [ESI 0x1E0]` the spin, `LEA EDI, [ESI 0x998]` the scratch. `body_mat3_transform_vec` with ECX EDI and the args EBX EBP writes the world spin to the scratch, the two `body_ramp_toward` calls scale x and y by 8 minus the count and z by 6.0 under a count of 2 or less, the last call `body_mat3_transform_vec_transposed` with ECX EBP and the args EBX EDI writes the spin. The damper is what the port has. `body_quat_from_axis_angle` 0x4ED8E0 normalises the axis through `body_vec3_normalize_to` 0x4ED620 (was FUN_004ED620) first, whose gate 0x5A3C70 holds the bit pattern 0xD9D7BDBB, a negative float, so it never rejects.

### The tyre model 0x4F32E0, read once more

Unchanged. nLat is 85947.66 times the lateral slip over (f2 times load), nLong 85943 times the longitudinal slip over (f1 times load), the magnitude goes through x over 1.2, atan, the E term of minus 0.2 and sin of 1.2 times the angle, then splits by a and b, the drive force does shrink under lateral slip as in the port. The return 0 path (load under 1.0) zeroes the five results and the ground byte becomes 1.

### Constants read this round (address, value)

0x5A69A8 1.728 (speed to km per hour, bytes 1C 2F DD 3F), 0x5A3C70 the bit pattern 0xD9D7BDBB, 0x5A83E8 the double 0.0, 0x59F404 10.0, 0x5A3294 the camera lerp of the boost flash, 0x435C0000 220.0, 0x43820000 260.0, 0x43480000 200.0, 0x40C00000 6.0.

## Round nine, read on the bytes 2026-09-15, the bank slide candidates

The bank slide of GHOST_REFERENCE.md the fifth run. Four candidates went to the disassembly before the harness was read, the tick side owns the first two.

### The two substep pushes 0x49CF90 to 0x49D00A, the FPU chain

The world push: `FLD [0x5EB6F0]`, `FMUL [0x5A6AE0]` (minus 0.16), `FSTP [ESP]` into the third argument, two `PUSH 0`, `PUSH ESI` (the car index), `CALL car_apply_engine_force` at 0x49CFAE. `car_apply_engine_force` 0x4968F0 negates the first two arguments and passes the third untouched into `body_apply_force` 0x4EC420 with ECX the wrapper, so the push is (0, 0, tuning 0 times minus 0.16) on world z, and `body_apply_force` is `body_vec3_set` then `body_vec3_add` on wrapper 0x4E0, no rotation.

The second push at 0x49CFB3: `FLD [car 0x2628]`, `FLD [car 0x261C]`, `SUB ESP 0xC`, `FLD [car 0x2610]`, then `FLD [ESP 0x1C]` (the curve term, [ESP 0x10] before the SUB) times 0x5EB6F4 times 0x5A6ADC into [ESP 0x38] as S, `FMUL` on the top (0x2610 times S) into [ESP 0x74], `FMUL` on the next (0x261C times S), `FLD [ESP 0x38]`, `FMUL ST2` (S times 0x2628), `FCHS`, `FSTP [ESP 8]` the z, `FCHS`, `FSTP [ESP 4]` the y, `FSTP ST0`, `FLD [ESP 0x74]`, `FCHS`, `FSTP [ESP]` the x, `CALL body_apply_force`. Car 0x2608 is R as nine floats, 0x2610 is m 0 2, 0x261C is m 1 2, 0x2628 is m 2 2, the third column, the world direction of the body z axis. The push is minus that column times S, along the body up, which is what the port has. On a 25 degree bank the body leans with the road so this push has no slope component beyond the 3 degrees the tilt correction keeps the body more upright than the road, the world push has sin 25 degrees of its 44.8 units per second squared down the slope in both the client and the port.

The curve term: [ESP 0x10] is 1.0 when car 0x36A8 (the effect code) is set (0x49CF3E to 0x49CF46), else 1.0 minus the table entry at the raw capped km per hour clamped 0 to 99 (0x49CF48 to 0x49CF70). `body_apply_durability_scale` is called once at 0x49CF30, before the loop, with the first index (km per hour minus 30 times a half) times 19.6 or 0.0 under an effect. The port runs the same order.

### The alternative readings in the harness

For the record both other readings of the second push were run with the resync every sample: the column push taken along world z reads 0.618 mean on the bank with the port 0.19 under the recorded z, the world push taken along the body up reads 0.465 with the port 0.09 over it, the port as read reads 0.397 and minus 0.025. The bank slide itself was the harness teleport, GHOST_REFERENCE.md the sixth run.

### Constants read this round (address, value)

0x5EB6F0 0.4 (tuning 0), 0x5EB6F4 0.6 (tuning 1), 0x5A6AE0 minus 0.16, 0x5A6ADC 0.16, 0x5A6A10 19.6, 0x5A3244 30.0, 0x59F414 0.5.

## Round ten, read on the bytes 2026-09-15, the drift settled

The user says the kart feel is far from the original, above all the drift. `car_drift_update` 0x49AA90 was read whole on the disassembly as one machine, with `car_drift_state_set` 0x49A9E0, `car_body_set_yaw` 0x49A970 and its two body callees, the friction pair of the tick around it, the step 15 slip turn and `car_effect_lean_update` 0x49B3D0. The RageZone recording never drifts, so nothing below is compared to the client yet, GHOST_REFERENCE.md "What a drift recording must show" says what to record.

### The drift, settled

The frame is `SUB ESP 0x2C` plus EBX ESI EDI, then EBP pushed at 0x49AB34. `armed` sits at [ESP+0x12], the reverse byte at [ESP+0x13], the tick scale at [ESP+0x18], the km per hour at [ESP+0x28] until the mini turbo block reuses that slot, the steer key in EBP and [ESP+0x1C], the slowed scale at [ESP+0x20], the rate at [ESP+0x14], the time in [ESP+0x30] [ESP+0x34].

| Address | What the bytes do | Port |
|---|---|---|
| 0x49AAA4 | km per hour car+0x32F4, reverse byte car+0x332D | `speedKmh`, `reverseFlag` |
| 0x49AAC6 | `input_key_binding_lookup(5)` then `input_key_down`, or game+0xB0 not 0, arms the drift | `driftKeyHeld`, `driftArmLatch` |
| 0x49AAF3 | tick scale is `time_frame_delta` times 80.0 (0x5A32A0) | `tickScale` |
| 0x49AB08 | `body_wheels_on_ground_count` on the wheel set car+0x245C, the result is dropped | not ported, dead call |
| 0x49AB17 | `car_node_name_is(car, 0, "FLY")` at 0x5A6AA4 disarms, so does car+0x36A8 not 0 | same |
| 0x49AB3F | slot 2 down gives 0x25, else slot 3 down gives 0x27, then `car_is_camera_reversed` swaps the two, slot 2 read first wins the swap | `car.input.steerLeft` first, the poll swapped the same pair |
| 0x49ABF8 | `car_is_slowed` on 0x2EF2550 gives 0.25 else 1.0 | `slowedScale` |
| 0x49AC11 | rate is (wire stat 8 plus bonus 8) times 0.5 plus 0.3, clamped 0.3 to 0.8 | `rate` |
| 0x49AC5D | reverse 1 jumps to the unwinding block, reverse 0 runs the gauge block, any other byte skips both | same three branches |
| 0x49AC69 | the gauge block needs km per hour over 20.0 (0x5A3298) and armed | same |
| 0x49AC93 | state 0, left key sets state 1, right key sets state 2 through `car_drift_state_set`, game+0x1397198 = 1.0, car+0x3604 = 0 (nothing reads 0x3604) | same, the 0x3604 write dropped |
| 0x49ACD2 | state 1 with the left key, gauge plus rate times slowed times tick scale, capped 45.0 (0x5EB700) | same |
| 0x49AD0B | state 1 without it, gauge minus tick scale times 0.12 (0x5A6AA0), under 0 clears the state | same |
| 0x49AD48 | state 2 with the right key, gauge minus the same, floored minus 45.0 | same |
| 0x49AD83 | state 2 without it, gauge plus tick scale times 0.12, over 0 clears the state | same |
| 0x49ADB6 | the unwinding block is skipped only with km per hour at or over 30.0 (0x5A3244) and armed | `unwind` |
| 0x49ADD3 | gauge at or over 10.0 (0x59F404) and over minus 10.0 (0x5A324C), gauge minus tick scale times 1.5 (0x5A6A48), under 0 the gauge is 0 and the state clears, else yaw minus rate times tick scale times 1.5 then `car_body_set_yaw` | same |
| 0x49ADEA | gauge under 10.0, gauge plus tick scale times 1.5, over 0 writes gauge 0 state 0 stage 0 and fires the release boost on car+0x3600 over 1, else yaw plus rate times tick scale times 1.5 then `car_body_set_yaw` | same |
| 0x49AEE1 | stage 0, frac12 is 1 minus (wire stat 10 plus bonus) times 0.8 clamped 0.2 to 1, frac13 the same on wire stat 11, stage 1 when the state is set, the gauge size passes frac12 times 10.0 (0x5EB704), armed, and now minus car+0x35F8 passes frac13 times 800.0 (0x5EB708) ms | same |
| 0x49B001 | stage 1 and not armed, stage 2, car+0x35F8 = now | same |
| 0x49B030 | stage 2, `input_key_pressed` on the slot 0 code, the rising edge only, inside 1500.0 (0x5EB70C) ms, stage 3, and with no boost `car_drift_state_set(0)` then the pet loop, a kind 0x14 pet with `rand() mod 100` under 3 starts a kind 1 boost, else a kind 0 boost | same, `accelKeyPressed` is the edge |
| 0x49B0F1 | state 0 gives tau 16.0 (0x5A6A9C), gain 1.0 and side 1.0, gain 0.5 in reverse. Any state gives tau 8.0 (0x5A15F0), counter steering (state 1 with 0x27 or state 2 with 0x25) gives gain 1.0 side 0.5, steering in or no key gives gain 2.0 side 2.0 (0x5A24EC) | same |
| 0x49B1B2 | steer stat is clamp((wire stat 2 plus bonus) times 3.0 plus 1, 1, 4.0) | same |
| 0x49B1F9 | car+0x2974 = car+0x2F34 times pi times 1 over 180 times gain times side, the wheel set max steer, 64 degrees for basic 1 steering into the drift | `wheels.maxSteerRad` |
| 0x49B217 | car+0x2A7C = car+0x2A78 = steer stat times car+0x32E8 times gain times slowed, the steer channel rates | `materialPairRates[2] [3]` |
| 0x49B22F | car+0x35AC plus (gauge minus car+0x35AC) over tau | `driftGaugeSmoothed` |
| 0x49B24D | state set, drift steer is clamp((wire stat 9 plus bonus) times 0.6 plus 1.2, 1.2, 1.8), `body_vec3_add` on car+0x263C of (0, 0, drift steer times gauge times slowed times 0.0174533), the spin in the principal frame, once a tick | `angularVelocity` add |
| 0x49B2BE | slot 5 up, state set, gauge under 0.5 and over minus 0.5 (0x5A68C4), or car+0x36A8 500 600 900, state 0 stage 0 and the release boost on car+0x3600 over 1 | same |
| 0x49B351 | game+0x1397174 set, game+0x24 turns the yaw down by dt times 300.0 (0x5A69F0) times car+0x2A7C, else game+0x20 turns it up with car+0x2A78, then `car_body_set_yaw` | same |

Facts of the machine the port and the docs had wrong or missing:

- car+0x3600 is written 0 by `car_drift_state_set` and read at 0x49AA06, 0x49AE0D and 0x49B326, nothing ever writes it above 0, so the release boost of the state clear never fires. The only mini turbo is stage 3, the accel rising edge inside 1.5 s after the drift key goes up. A held accel never fires it, the player lifts the gas and presses it again. `input_manager_update_keys` 0x44B4C0 (named this round) is the state machine, 1 the frame the key goes down, 2 held, 3 the frame it goes up, 0 up, `input_key_pressed` tests 1.
- `car_drift_state_set` stamps car+0x35F8 with `time_now_ms` and the update subtracts the same clock. The port stamped the wall clock and subtracted the tick clock, so stage 1 armed after a random delay. `car_drift_state_set` takes the tick clock now, every caller passes it.
- `car_body_set_yaw` 0x49A970 builds R = Rz((360 minus yaw) degrees) times Ry(0) times Rx(car+0x3224) through `body_mat3_from_euler_zyx` 0x4ED2B0 (named this round, the three standard rotation matrices multiplied with `body_mat3_multiply` 0x4ED150, out is A times B), then `body_set_orientation_matrix` 0x4F1A10 (named this round) writes the wheel set R at +0x1AC as that matrix times the transpose of the principal axes at +0x124, and Q at +0x1D0 from R. The constant is 360.0 (0x5A3C78), not 180.0 as the older section above said. The port wrote a yaw times pitch quaternion as the reference orientation only, without the principal axes and without R, the port builds R and Q as the bytes do.
- the unwinding turns the yaw further into the drift, minus for a left drift, plus for a right one, 1.34 degrees a tick for basic 1 (rate 0.56 times 1.6 times 1.5), 13 degrees over the 10 ticks a full right gauge needs to reach 0 and 19 degrees over the 15 ticks a full left gauge needs to fall under 10. The slip angle is what comes back, the body snaps in line with its motion within 0.4 s of the release.
- `car_effect_lean_update` 0x49B3D0 never writes car+0x3228. The roll is written by step 18 alone (0x49E152) and by `car_visual_update` for a spectated car. The port wrote the lean into `rollDeg` every tick, then step 18 smoothed toward the real roll from that value, so the roll that feeds the step 11 turn force (`fabs(rollDeg)` times 1.5 capped 50) read the smoothed gauge in a drift and a fraction of the real roll elsewhere. Fixed, the lean has its own fields.
- the lean of 0x49B488, when no spin effect runs: D3DXMatrixRotationYawPitchRoll(yaw about y = (wire stat 6 plus bonus 6) times lift, pitch about x = minus (wire stat 7 plus bonus 7) times side, roll 0). Drifting, lift is the size of the smoothed gauge and side is `car_suspension_shake` plus the smoothed gauge. Not drifting, side is the shake, lift is 0, or clamp((wire stat 4 plus bonus 4) times 30, 0, 30) under a boost, a wheelie. The bonuses sit at car+0xA7940 plus 4 times the wire index (0xA7958 and 0xA795C here), the port had the lean for kind 2 only. Effects 200 and 300 turn the model about y by car+0x36AC through D3DXMatrixRotationY 0x504711, the port had z.
- the model slip turn, step 15 at 0x49D3C9: car+0x35B0 is D3DXMatrixRotationZ of the drift steer (the 1.2 to 1.8 clamp above) times the smoothed gauge car+0x35AC, minus car+0x36AC under the effects 100 400 700, up to 73 degrees for basic 1 at a full gauge. The car matrix car+0x2FE0 is lean times that times the body world matrix car+0x221C, which `body_finalize_wheels` 0x4EC570 builds with D3DXMatrixTransformation from the quaternion (minus x, minus y, z, w) and the position (minus x, minus y, z), the wire frame of R. So the visible kart turns far past the physics body in a drift while the body itself slips 10 to 17 degrees, that is the drift look of the client. Port `driftSlipDeg`, `leanPitchDeg`, `leanRollDeg`, and `carMatrix` built as F R F times Rz(slip) times Ry(lift) times Rx(lean) in column order with F the wire flip of x and y.
- car+0x36AC is the effect wobble, no drift path writes it, the step 15 slip turn and the lean read it.
- `car_drift_state_set` tests game+0x6BC with EDI the game base (0x49AA34), the car 0 field, not the car of the index. The port keeps `trackProgress` on the car, the same memory for the local car at index 0.
- the friction pair 0x49C8BB, pair 0 is wheels 0 and 1 times car+0x32DC, pair 1 wheels 2 and 3 times car+0x32E0, pair 0 alone scales by 0.8 off the gas and 0.6 on it in a drift, both by 8.0 under the slow flag and 0.75 (0x5A6B04) in view mode 2, `car_apply_force_if_valid` 0x499000 hands (pair 0, pair 1) to `body_set_tire_grip` 0x4EC130 which writes wrapper 0xB50 and 0xB6C, the peak scale of the tyre block 0x800 (wheels 0 and 1, the steered pair, `gear_wheel_force_solve` picks 0x800 for the first two) and 0x81C. The front loses grip in a drift, the rear keeps it, the port had this.
- the sound of the state change is `FUN_00448C90` on the manager 0xE22080 with car+0x2114, a channel play, the port hook `playSound(carIndex, 0, 0)` stands for it.

What the scripted drift shows (the harness `--script`, tests/drift_test.cpp): from grid row 7 at 90 km per hour a right drift fills the gauge to minus 43.9 in 1 s, arms stage 1 at 0.82 s, turns the yaw 74 degrees in the second with the front wheels at 64 degrees and the spin z at minus 0.8 to minus 1.0 radians a second, the body slips 17 degrees at most, the release gives stage 2, the unwinding turns 16 more degrees, the gas edge fires the kind 0 boost to 132.5 km per hour, and the slip is back under 5 degrees 0.2 s later. On the launch straight the car crosses the 94 unit road in 1.5 s and hits the right wall, so the test window ends at 2.3 s.

Open until a drift recording exists: whether the client turns as fast (the 64 degree steer with the Ackermann formula near its pole, the spin add of up to 1.4 radians a second a tick against the damper), whether the recorded yaw shows the 13 degree unwinding turn, and the mini turbo cadence of a real player.

### Constants read this round (address, value)

0x5EB6FC 320.0, 0x5EB700 45.0, 0x5EB704 10.0, 0x5EB708 800.0, 0x5EB70C 1500.0 (initialised data, no writer), 0x5A3214 1.8, 0x5A322C 0.8, 0x5A3244 30.0, 0x5A324C minus 10.0, 0x5A3298 20.0, 0x5A32A0 80.0, 0x5A32B0 1.2, 0x5A32B8 3.0, 0x5A6A48 1.5, 0x5A6A94 0.0055556, 0x5A6A98 3.14159, 0x5A6A9C 16.0, 0x5A6AA0 0.12, 0x5A6AA4 "FLY", 0x5A6B04 0.75, 0x5A68C4 minus 0.5, 0x5A69F0 300.0, 0x5A3C78 360.0.

## Round eleven, read on the bytes 2026-09-15, the drift recording

The five differences of GHOST_REFERENCE.md the drift recording, each taken back to the disassembly with the recording in hand. The port changed on one term, the brake channel rate, the rest was the harness or sits inside the edge window. The numbers are in GHOST_REFERENCE.md the drift recording, closed.

### 0x49A9E0 and 0x49B030, the release path and the stage 3 gate

`car_drift_state_set(0)` at 0x49AA0D and 0x49AA13 writes 0 to car+0x35A4 and car+0x35F0 next to the dead car+0x3600 test. The in place clear of the unwinding block at 0x49AE18 to 0x49AE24 writes 0 to the gauge, the state and the stage (three `MOV [ESI+...], EAX` with EAX zero). The stage 2 to 3 gate at 0x49B030: `CMP [ESI+0x35F0], 2`, `input_key_pressed` on the slot 0 code, the 64 bit now minus car+0x35F8 through `FILD` against 1500.0 at 0x5EB70C with `TEST AH,5 JP` (skip at or over), then `MOV [0x35F0], 3` and the car+0x3300 test before `car_drift_state_set(0)` and the pet loop. So the stage lives exactly as long as the state and the state lives as long as the gauge, 2.4 units a tick from the release (0x49ADEA and 0x49AE7D, tick scale times 1.5). A left drift (positive gauge) ends in place on the first tick under 10.0 (0x49ADD9 `JP` on gauge at or over 10 goes to the decrement, the rest goes to the increment which passes 0 at once), a right one through `car_drift_state_set(0)` on the tick past 0. The port had every line, the 1.5 s of the tenth pass was the window the port would grant a stage that outlived its state, the harness could force one (the flags armed stage 1 with no state), the port never makes one. Scripted, the gas edge 0.7 s after the release fires nothing, 0.2 s after it fires.

### 0x496BE0 0x496E50 0x496B40, the kind 0 push shape

Read a third time, the strength 6.0 at 0x496D31, the target at 0x496D3E to 0x496D6B, the km per hour plus 10.0 floor at 0x496E2A, the push at 0x496FB0 every tick of state 1 while car+0x32F4 is under car+0x330C, `car_boost_push` builds the direction from car+0x3220 minus car+0x35AC times 1.8 (0x5A3214) through `math_dir_from_heading_pitch` 0x44DD50 (Rz of minus the heading, the pitch about row 0, row 1 of the product) and `body_apply_force` 0x4EC420 adds the three floats to wheel set 0x1A0, the velocity, no mass, no dt. As ported. The push is 6 units a tick for two or three ticks until the speed passes the target, the sample mean of the client and the port both read plus 3 then plus 6, the earlier 10 was the harness printing the end of sample speed.

### 0x4EC180, body_set_mass_friction, the six channel rates

The body of the function at 0x4EC183 to 0x4EC1B6 loads the first argument into [ESP+0x14] and [ESP], the second into [ESP+0x10] and [ESP+0x04], the third into [ESP+0x08] and, after the push of the array pointer, into [ESP+0x10] which is the old [ESP+0x0C], then `body_set_material_pair` 0x4EEB70 copies the six dwords to wheel set 0x614 to 0x628. So the array is {a, b, c, c, b, a}, not {a, a, b, b, c, c} as RIGID_BODY.md and the port had it. With the call of `car_physics_setup` 0x49510C, 5.0 1.0 car+0x32E8, the rates are accel 5.0, brake 1.0, right 0.4, left 0.4, handbrake 1.0, reverse 5.0. `body_spring_channel_update` 0x4EEBB0 ramps a channel by dt times its rate per substep and decays it by 4 times (16 for the steer pair) the rate, so the brake channel fills in 1.0 s, the port filled it in 0.2 s, locked the wheels (the brake torque `wheel_spin_rate_get` times 0xEC clamped to ch1 times 0xF0 and 0xF4 at 0x4EED5B to 0x4EEE50, 20000 and 30000 for basic 1 against a wheel inertia of 9.2) and the tyres lost their lateral force. The steer pair is overwritten by the drift update every tick so its setup value never shows, the handbrake channel never rises in a race, the reverse channel rises through game+0x2C when the brake key is held under 1000 rpm or below speed 1.0. Ported, `body_set_mass_friction` builds {a, b, c, c, b, a}.

### The drift entry and the release, read once more

The friction pair loop 0x49C8C0 to 0x49C905 sums the surface friction of the two wheels of a pair plus twice car+0xA7988, halves and scales by car+0x32DC then car+0x32E0, the drift scale at 0x49C921 to 0x49C93A touches [ESP+0x1C], pair 0, alone, 0.8 off the gas and 0.6 on it, the slow flag and the view mode 2 touch both, `car_apply_force_if_valid` 0x499000 hands them to `body_set_tire_grip` 0x4EC130, wrapper 0xB50 and 0xB6C, wheel set 0x810 and 0x82C, the fifth float of the front and the rear tyre block that `gear_wheel_force_solve` passes for wheels 0 1 and 2 3. The step 11 turn force 0x49CAF4 to 0x49CB99 and its drift clauses 0x49CBA1 and 0x49CCD9, the gains 0x49B0F1 to 0x49B249 and the unwinding 0x49ADD3 to 0x49AEDC read as the round ten table. `car_effect_lean_update` 0x49B3D0 decompiled whole writes car+0x2FE0, 0x3018, 0x36DC to 0x36F4 and the model, never car+0x3228, the only writers of the roll are the tick at 0x49E152 and `car_visual_update` on the finished branch at 0x49C443, so the turn force cap in a drift reads the body roll of a degree or two and the drift drag is the front pair at 0.6 with the fronts at 64 degrees, as ported. Nothing changed here, the residual of the release (1 to 3 degrees in the release sample) and of the entry (1 to 2 units on the resync mean, none in a free slice) sit in the harness edges and the resync.

### Constants read this round (address, value)

0x40A00000 5.0 and 0x3F800000 1.0 the immediates of 0x495101 and 0x4950FC, 0x5EB70C 1500.0, 0x5A6A48 1.5, 0x59F404 10.0, 0x5A324C minus 10.0, 0x5A3214 1.8, 0x5A2494 0.1, 0x5A6A14 120.0, 0x5A15EC 0.2, 0x5A32B0 1.2, 0x40C00000 6.0, 0x5A322C 0.8, 0x5A164C 0.6, 0x5A15F0 8.0, 0x5A6B04 0.75, 0x5A6A68 60.0, 0x5A6AF4 0.012, 0x5A6AF0 0.97, 0x5EB700 45.0, 0x5A3BF4 1000.0, 0x59F480 1.0, 0x5A6AC4 9.5493, 0x5A6AC0 500.0, 0x5A371C 10000.0, basic_1.car 0xEC 3000 0xF0 20000 0xF4 30000 0xF8 60000 0xFC 0.9.
