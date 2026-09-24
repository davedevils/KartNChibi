# Remote car and input

Read in KnC.exe.raw, image base 0x400000, on 2026-09-14. Covers the remote car mover, the network motion channel that feeds it, and how keys become drive input. Builds on docs/reverse/CLIENT_PHYSICS_MAP.md, docs/packets/MOTION_CHANNEL_VERIFIED.md and docs/packets/PHYSICS_SPEED_VERIFIED.md, and corrects two points in those: the second packed field of the 0x40 payload is a predicted position, not a rotation, and the byte at C2S offset 6 is a yaw byte, not a generic flag. Every function below was renamed in Ghidra with the name given here.

## car_remote_update, 0x0049ED90

Args: param_1 game base, param_2 car index. Base iVar3 = param_1 + param_2*0xA7260, the car record, same stride as the local tick.

Calls FUN_00486490 with the current position (a listener or debug sync), then net_remote_watchdog(param_2), then reads the sample staging block at iVar3+0x3374 (below).

If the active effect code at iVar3+0x36A8 equals 300, the position is force set from iVar3+0x36D0/0x36D4/0x36D8 plus a z offset from iVar3+0x36C8. This is a teleport/snap pose, effect 300 is not identified further here.

If iVar3+0xA7854 (finished/spectating, 1 or 2 per CLIENT_PHYSICS_MAP) equals 2 and this car is not the local car (param_1+0x6B4 != param_2), calls FUN_0049ff90 then FUN_0048E6A0 and returns. FUN_0048E6A0 is the common exit at the end of every branch, it runs last every time (scene node / transform commit, not traced further).

Calls FUN_00496600(param_2) (untraced, likely per car housekeeping), reads the tick timer via FUN_0044ED50.

Airborne/landing tracking, mirrors the local tick's own airborne logic at iVar3+0x3716 (airborne flag), +0x371C (peak height): while airborne, tracks the peak z; once wheels_on_ground_count() < 2 and the drop from peak exceeds a threshold, plays a landing effect through FUN_00448CD0/FUN_00444820/FUN_004447F0 and stamps a landed flag/time at +0x373C/+0x3740, matching CLIENT_PHYSICS_MAP's local-tick landing effect.

Bails to FUN_0048E6A0 (same early exit) when the global race/menu state DAT_00B2360C is 0xB or 9 with DAT_012124B8 == -1 (not in an online session), or state 0xD.

### Sample apply: interpolation toward a network-supplied predicted position

`cVar8 = FUN_004A42B0(pfVar2)` (now net_motion_sample_pop) with pfVar2 = iVar3+0x3374, dequeues one pending sample. Returns 1 only if a sample was pending (queue depth is 1, see net_motion_sample_push below: a new arrival evicts any unconsumed one, so there is never more than one sample queued per car).

When a sample was popped (cVar8==1):
- If the sample's flag byte at iVar3+0x3394 is 0 (guess: "not yet locally re-derived"), the code computes a predicted point itself: `pred = samplePos + frameDt(param_1+0x34) * DAT_0059F450(50.0) * velocity(iVar3+0x3238/0x323C/0x3240)`, with an asymmetric z scale (DAT_005A32B0=1.2 while descending, DAT_005A32D4=0.25 while rising) and stores it at iVar3+0x3380/0x3384/0x3388. In the normal case this field already arrived pre-computed from the wire (see net_motion_recv_0x40), so this branch is a local fallback using the same formula the sender uses, not the usual path, read at 0x49ED90 lines with `*(char*)(iVar3+0x3394)=='\0'` guard.
- First sample ever (iVar3+0x3398==0): initializes the interpolation anchor chain (iVar3+0xA7884.. and +0xA7890..) to the sample position, the "last known" position (+0xA789C/+0xA78A0/+0xA78A4) to the CURRENT rendered position, sets the position-interpolation countdown iVar3+0x33A0 = 1.0 and yaw countdown iVar3+0x339C = 1.0 (both effectively "done", i.e. next frame snaps rather than eases), and zeroes a divergence accumulator at +0xA78AC.
- Subsequent samples: measures how far the rendered position has diverged from the last anchor (+0xA789C/+0xA78A0) using FUN_0044D900 (hypot). If the divergence times DAT_0059F414 is below the divergence accumulator (+0xA78AC), a mismatch counter (+0xA78A8) increments; after 4 consecutive mismatches it force re-anchors exactly like the first-sample case and calls FUN_004A41F0 (an untraced reset/teleport-effect hook). Otherwise it advances the anchor chain one step: the old target becomes the new start, the new predicted position (+0x3380..) becomes the new target, scaled through `_DAT_005A3BF4(1000.0) / local_64` where local_64 comes from the sample's own int16 field at iVar3+0x3390 (see the wire layout below), optionally adjusted by `FUN_0059029C() - iVar3` when DAT_00B23154 > 0 (a debug/tuning knob, 0 in the shipped binary).

### Position interpolation, tick-based not sample-count-based

`iVar3+0x33A0` is a countdown that decrements by exactly 1.0 per tick (`DAT_0059F480 == 1.0`, confirmed by reading the raw bytes 00 00 80 3F) and resets to 1.0 once it drops below 1.0. On a fresh non-first sample it is set to 50.0. Every tick while `0x33A0 > 0`:
```
frac = DAT_0059F480(1.0) / iVar3_0x33A0        // 1/N of the remaining distance
pos += frac * (targetAnchor - pos)              // targetAnchor = iVar3+0xA7890/+0xA7894/+0xA7898
iVar3_0x33A0 -= DAT_0059F480(1.0)
```
This is linear interpolation of the rendered position toward an already-extrapolated anchor point, eased over up to 50 ticks (not 50 ms, the decrement unit is 1.0, a tick count). The anchor itself is the sender's own dead-reckoned position (see net_motion_pack_0x40), so the scheme is "interpolate the visible car toward a point the sender already extrapolated a bit ahead", i.e. interpolation of an extrapolated target, not raw extrapolation of the receiver's own velocity in the common case.

A landed/fall check (`FUN_00485970`, `_strcmpi(...,"PUSH")`, `FUN_0044DEC0`/`FUN_004858E0`, all untraced surface-probe helpers) runs only while `*(float*)(iVar3+0x3598) <= DAT_005C8320` (a per car "close/visible" gate, DAT_005C8320 is a runtime variable, 0 in the static image) and clamps the interpolated z against the ground and snaps onto a nearby "PUSH" surface path when found.

### Yaw and the status word, from iVar3+0x338C..0x338F

`iVar3+0x339C` is a second, independent countdown, same 1.0-per-tick decrement, reset to 1.0 when it underflows below 1.0, set to 6.0 on a fresh non-first sample. While `*(char*)(iVar3+0x338E)=='\x01'` (the flag copied from a fresh sample, gates this whole block):
```
targetYawDeg = (float)(byte at iVar3+0x338C) * DAT_005A6B20   // DAT_005A6B20 == 360/255 exactly (read as 1.41176..)
delta = targetYawDeg - yaw(iVar3+0x3220)
wrap(delta)                                    // FUN_0044DD00
yaw += delta / iVar3_0x339C
```
So the yaw byte at +0x338C is a 0..255 encoding of heading over 0..360 degrees, eased toward over up to 6 ticks (much faster catch-up than position).

Drift gauge and rpm/status decode, from the 16 bit word at iVar3+0x338E (little endian, low byte 0x338E, high byte 0x338F):
- bits 12..15 (top nibble of 0x338F) = speed nibble: `drift-gauge-ish value = (2*DAT_005EB700)*DAT_005A6B1C*nibble - DAT_005EB700`, stored at iVar3+0x35A8 (the same field CLIENT_PHYSICS_MAP calls the local car's drift gauge; here it is driven by the remote status word instead).
- bits 8..11 (low nibble of 0x338F) = rpm nibble, eased into iVar3+0x32F8 (rpm) via `rpm += (nibble*DAT_005A6B18(600.0) - rpm) * DAT_005A32AC`.
- bit 7 of 0x338E (sign bit of that byte) = mini-turbo boost active: sets iVar3+0x3300=1, +0x3304=0 (kind 0, matches car_boost_start's "0 mini turbo" per CLIENT_PHYSICS_MAP).
- bit 6 (0x40) = item boost active when bit 7 is clear: +0x3300=1, +0x3304=1 (kind 1, item).
- neither bit 6 nor 7 set: +0x3300=0 (no boost).
- bit 5 (0x20) = reverse/brake, copied to iVar3+0x332D.
- bit 4 (0x10) = drift active, copied to iVar3+0x35A4 (as 0/1, not the local car's 3 state 0/1/2 range).
- bit 3 (0x08) = mini turbo stage 1, copied to iVar3+0x35F0.
- bits 2 and 1 = a 3 way "turn state" (0 none, 1 = bit2 set, 2 = bit1 set) stored at iVar3+0xA78E4, used only to drive a cosmetic yaw-rate field (+0x32E4) toward +-DAT_005A6ACC/DAT_005A6AC8 (read as +-0.5236 rad, i.e. +-30 degrees), this does not affect the interpolated position, it only feeds wheel/body lean animation.

After the position/yaw work, iVar3+0x339C is decremented and clamped as described, then a separate "collision recovery" state machine at iVar3+0xA78B0 (0,1,2,3) runs a short scale-up/scale-down of a factor at +0xA78B4 gated by more of the same divergence tests; not fully traced, cosmetic/anti-clip in nature.

## net_remote_watchdog, 0x0049ECD0 (was FUN_0049ecd0)

Args: param_1 game base (implicit, called with just the index), param_2 car index. Base = param_1 + param_2*0xA7260.

Small state machine over iVar3+0xA78C0 (state 0/1/2), using a saved 64 bit ms timestamp at +0xA78C8/+0xA78CC and a per-car threshold at +0xA78C4 (field exists, default value not read):
- state 1: once elapsed-since-saved >= the +0xA78C4 threshold, calls car_remote_coast_extrapolate(param_2), saves now, moves to state 2.
- state 2: once elapsed-since-saved >= 700 ms (literal 699/700 comparison, proven in the disassembly-derived arithmetic), resets state to 0, re-arming the watchdog.

State is never set to 1 inside this function, something else (not traced, presumably net_motion_sample_push or net_motion_recv_0x40) arms it to 1 when a real sample is consumed. Net effect: if no real sample refreshes the watchdog for the +0xA78C4 window, the client synthesizes a coast sample once, then waits ~700 ms before it could fire again.

## car_remote_coast_extrapolate, 0x0049EAE0 (was FUN_0049eae0)

Args: param_1 game base, param_2 car index (declared float, used as int). Called only from net_remote_watchdog.

Reads current position (iVar3+0x3244/48/4C), calls FUN_004A41F0 (same reset hook used by car_remote_update's force-reanchor path), then loops 8 times with a magnitude that starts at 9.4 and multiplies by DAT_005A69E4 (read as 0.94) every step, i.e. exponential decay ~9.4 * 0.94^n. Each step:
- builds a direction vector from the car's current yaw (iVar3+0x3220 minus DAT_005A323C) via FUN_0044DDD0 (sin/cos style), scaled by the decaying magnitude and by `frameDt(param_1+0x34) * DAT_0059F450(50.0)`, the same prediction constant used everywhere else in this file.
- packs a 7 dword payload with the same layout net_motion_recv_0x40 builds (old pos, new/coasted pos, byte+ushort status word from three FUN_0059029C() calls, with bit 0x80 forced on, FUN_0059029C is a generic float-to-int64 round helper used by hundreds of callers throughout the binary, not renamed, the exact float pushed onto the FPU stack before each call is not visible in the decompilation).
- pushes it via net_motion_sample_push(&payload, 1000, 1).

Because net_motion_sample_push evicts any unconsumed sample before writing a new one (see below), this loop's first 7 pushes are normally discarded and only the 8th (most decayed) synthetic sample survives into the mailbox by the time the function returns, read directly from the code, not assumed. Net effect for the port: on a stall, the remote car keeps coasting in a straight line along its last heading with a quickly decaying speed, landing on one settled "rest" point that the normal interpolation code then eases the visible car toward. This is simple directional dead reckoning, not rigid body extrapolation, no gravity, no collision.

## The 1 deep network sample mailbox

Fields relative to the car record base iVar3, populated by net_motion_sample_pop out of the mailbox at iVar3+0x3374 and consumed by car_remote_update:

| Offset | Size | Content | Source |
|---|---|---|---|
| +0x3374 | float | position x | wire packed-pos field 1, unpacked |
| +0x3378 | float | position y | " |
| +0x337C | float | position z | " |
| +0x3380 | float | predicted position x | wire packed field 2 (labelled "rotation" in the older docs, it is not; see net_motion_pack_0x40) |
| +0x3384 | float | predicted position y | " |
| +0x3388 | float | predicted position z | " |
| +0x338C | byte | yaw byte, 0..255 over 0..360 deg | wire int8 field |
| +0x338D | byte | unused (uninitialized on the push side) |, |
| +0x338E | byte | status flags: bit7 miniturbo boost, bit6 item boost, bit5 reverse/brake, bit4 drift, bit3 miniturbo stage1, bit2/bit1 turn state | wire int16 field, low byte |
| +0x338F | byte | high nibble = speed nibble, low nibble = rpm nibble | wire int16 field, high byte |
| +0x3390 | int16 (stored widened) | the S2C-only "ticks/hint" field that rides between the sender id and the packed position on the wire; used as the divisor `1000.0 / value` when advancing the interpolation anchor | wire int16 field (registry's per-player `[int16]` right after `[int32 id]`) |
| +0x3394 | byte(as int) | "sample present" flag, always 1 after a push | set by net_motion_sample_push's constant argument |
| +0x3398 | int | "ever received a sample for this car" flag, set to 1 the first time net_motion_sample_push runs | net_motion_sample_push |

This is a mailbox, not a ring buffer of history: net_motion_sample_push always drains any unconsumed sample (`if (count>0) net_motion_queue_shift()`) before writing the new one, so car_remote_update never has more than one pending sample to interpolate from, there is no multi-sample smoothing window on the network layer itself; the smoothing instead comes from the tick-based countdowns described above. The exact internal addressing net_motion_sample_pop/net_motion_queue_shift use for the count field (iVar3+0x339C, proven identical in both push and pop) and the payload's starting byte inside the mailbox object have one unresolved wrinkle in the decompiled pointer arithmetic (the shift loop's first source slot lands 4 bytes off from where the push writer's first payload dword lands) that does not change the depth-1/evict-on-arrival conclusion, flagged here as guess, not load bearing for the port.

### net_motion_sample_push, 0x004A4260 (was FUN_004a4260)

`__thiscall(this=mailbox, param_2=7 dword payload ptr, param_3=the wire's int16 ticks field, param_4=1)`. If the mailbox already holds an unconsumed sample (`count>0`), calls net_motion_queue_shift first (drops it). Writes the 7 payload dwords, then param_3 as an 8th dword, then param_4 (the byte 1) as a 9th slot's first byte, increments count. This is what fixes the "9 dwords copied by pop, 7 dwords decoded from the wire" gap: the two extra dwords (index 8 = the ticks field, index 9's low byte = the presence flag) are added by the push call, not present on the wire itself.

### net_motion_sample_pop, 0x004A42B0 / net_motion_queue_shift, 0x004A4200

Pop returns 0 if count<1, otherwise copies 9 dwords into the destination and calls net_motion_queue_shift to drop the head and decrement count. Shift is a no-op body unless count was already >1 (never observed given push's evict-first policy), otherwise decrements count.

## net_motion_recv_0x40, 0x0047FD30 (was FUN_0047fd30), S2C 0x40 MotionBroadcast

Reached from the opcode dispatcher per docs/packets/PACKET_REGISTRY.md row `0x0040 | MotionBroadcast | sub_47FD30`. Signature `void net_motion_recv_0x40(char param_1)`, param_1 is the per-player entry count `n`, re-read fresh from the packet with `FUN_0044E910(&param_1,1)` (1 byte). `FUN_00487230()` picks anti-hook vs normal, same switch used by the sender (motion_send_0x40/net_motion_pack_0x40).

Normal path, per entry, 25 bytes total (matches registry's `1+25*n`):
```
int32 id        (local_48)
int16 hint      (local_50)
8 bytes packed pos       -> unpack (FUN_0044E7F0) -> pos.xyz
8 bytes packed pred-pos  -> unpack (FUN_0044E7F0) -> predicted-pos.xyz
int8  yawByte   (local_51)
int16 status    (local_4c)
```
`iVar2 = net_player_id_to_slot(local_48)`; if found (>=0), looks up the target car's record via two globals indexed by `iVar2*0xA7260` (`DAT_01b19a6a` a byte flag cleared to 0, `DAT_01bc0950` the car table base, checked `==0`, meaning: only apply when that per-car byte is 0, guess: "not a locally-driven/ghost slot"), optionally calls FUN_004A41F0 (reset hook) unless the local display mode DAT_01A20B21 is 2 or 6, then calls net_motion_sample_push(&decodedPayload, hint, 1).

Anti-hook path, per entry, 34 bytes total: `int32 id, int16 hint, 28 bytes raw` (the same 7 floats + byte + pad + status word net_motion_pack_0x40's alt path sends, uncompressed).

## net_player_id_to_slot, 0x0048DEA0 (was FUN_0048dea0)

`__thiscall(this=game base, param_2=player id)`. Linear scans up to 30 (0x1D inclusive) car slots, stride the usual 0xA7260, checking a byte at slot+0x743 (`piVar2[-1]` relative to a pointer based at slot+0x744) equals 1 (active) and the int32 id at slot+0x744 matches. Returns the slot index or -1. This is the same +0x744 field car_ghost_replay_apply reads for its own id lookup (FUN_004B6FD0), reused for two different purposes (network player id vs ghost id) depending on mode.

## motion_send_0x40, 0x0049BEB0, corrected field sourcing

Already named in Ghidra (from the earlier physics pass). Confirmed by re-reading: timer-gated at >= `DAT_01396E90` (100, PHYSICS_SPEED_VERIFIED) ms since `DAT_01396E80`. On fire, computes from the car record (iVar3 = base + index*0xA7260):
```
local_1c/18/14 = pos.x/y/z                          (iVar3+0x3244/48/4C, raw)
local_10       = pos.x + frameDt*DAT_0059F450(50.0)*vel.x(iVar3+0x3238)     // predicted pos, NOT rotation
local_c        = pos.y + frameDt*50*vel.y(iVar3+0x323C)
local_8        = pos.z + frameDt*50*vel.z(iVar3+0x3240)*asym(DAT_005A32B0/DAT_005A32D4)
yawSrc         = yaw(iVar3+0x3220) - driftSmoothed(iVar3+0x35AC)*DAT_005A164C, wrapped (FUN_0044D9C0)
local_4  (byte)   = FUN_0059029C()   // rounds yawSrc-derived value to a byte, this is the yaw byte
local_2 (ushort)  = speedNibble<<12 | (rpmNibble&0xF)<<8   // two more FUN_0059029C() calls
                     | 0x80 if miniturbo boost (iVar3+0x3300!=0 && +0x3304==0)
                     | 0x40 if item boost      (iVar3+0x3300!=0 && +0x3304!=0)
                     | 0x20 if reverse/brake   (iVar3+0x332D==1)
                     | 0x10 if drift active    (iVar3+0x35A4!=0)
                     | 0x08 if miniturbo stage1 (iVar3+0x35F0==1)
                     | 0x04 / 0x02 turn state  (iVar3+0xA78E4 == 1 / == 2)
```
then calls net_motion_pack_0x40(&local_1c).

## net_motion_pack_0x40, 0x004818A0 (was FUN_004818a0)

`void net_motion_pack_0x40(undefined4 *param_1)` where param_1 points at the 28 byte stack struct motion_send_0x40 just built (pos.xyz, predpos.xyz, yaw byte, 1 pad byte, status ushort, sizes and offsets proven from the local variable layout in motion_send_0x40: yaw byte at param_1+6 dwords = byte offset 0x18, status word at byte offset 0x1A). Opens opcode 0x40 (`FUN_0044ECD0(0x40)`), then `FUN_00487230()` selects the path:
- anti-hook (cVar1==1): sends the raw 28 byte struct unpacked (`FUN_0044E9C0(param_1, 0x1C)`).
- normal: `FUN_0044E610(pos.x,pos.y,pos.z)` -> 8 bytes, `FUN_0044E610(predpos.x,predpos.y,predpos.z)` -> 8 bytes, both written; then 1 byte from param_1+6 dwords (the yaw byte); then the pointer is advanced by 0x1A bytes (skipping the 1 pad byte) and the trailing 2 bytes (status ushort) are written. Total 8+8+1+2 = 19 bytes, matching PHYSICS_SPEED_VERIFIED's proven payload size.

This corrects MOTION_CHANNEL_VERIFIED.md's field table: field 2 ("packed rot") is a predicted position built from velocity, not a rotation, and field 3 ("flag a2+6 low byte") is specifically a yaw byte (0..255 over 0..360 degrees), not a generic flag, both proven from this decompile and cross-checked against net_motion_recv_0x40's decode of the identical field order.

## Key binding table

### input_key_binding_lookup, 0x0045AF30 (was FUN_0045af30)

`__thiscall(this=input manager, param_2=action slot 0..7, param_3=binding index, -1/0 = primary, 1 = secondary)`. Returns 0 if slot is out of 0..7. Otherwise: `*(this + (slot + 0x4028 + altIndex*8) * 4)`, i.e. a table at `this + 0x100A0` bytes, row stride 0x20 bytes (8 dwords) per altIndex, one dword per action slot within a row. Returns a raw key code (Windows VK code or DirectInput code, not decoded further here).

### input_key_down, 0x0044B580 (was FUN_0044b580)

`__thiscall(this=input manager, param_2=key code)` returns `*(byte*)(this + param_2 + 4)`, i.e. a raw per-key-code state byte array starting at this+4, indexed directly by key code (not by action slot), this is the function used on whatever code input_key_binding_lookup returned, and is also called directly with literal VK codes elsewhere (e.g. 0x25/0x27, VK_LEFT/VK_RIGHT, in car_drift_update per CLIENT_PHYSICS_MAP).

### input_key_pressed, 0x0044B590 (was FUN_0044b590)

`__thiscall(this=input manager, param_2=action slot 0..7)` returns `*(int*)(this+0x104+param_2*4) == 1`, a separate per-slot edge/pressed-this-frame array (not indexed by key code). Used for tap-triggered actions (item use in FUN_004AEFA0, mini turbo release confirmation) as opposed to input_key_down's held-state check.

### The 8 action slots

Proven directly from car_ghost_sample_record (0x0049FAD0, see below), which walks `input_key_binding_lookup(slot, -1)` + `input_key_down(code)` for slots in this exact order and OR's the result into one bitmask byte. Read again on the bytes on 2026-09-14: the byte lives at ring entry +0x18 (car+0x376C plus index times 0x1C), the order is slot 0 then 1 then 2 then 3 then 5 then 4 then 7, the bits are 0x80 0x40 0x20 0x10 0x08 0x04 0x02, bit 0 is never set and slot 6 is never sampled. So slot 0 accelerate is bit 7, the two steer slots 2 and 3 are bits 5 and 4, the seven minus slot order for slots 0 to 3. A real Race 01 recording confirms it, 0x80 through the launch, 0x90 while the yaw rises, 0xA0 while it falls, 0x00 at the finish. Slot 3 is the game+0x20 flag and slot 2 the game+0x24 flag when the camera is not reversed, the stuck block of the drift update turns the yaw up on game+0x20 and down on game+0x24.

| Slot | Bit in replay/record mask | Meaning | Confidence |
|---|---|---|---|
| 0 | 0x80 | accelerate | given/proven (drift and throttle code gate on this slot) |
| 1 | 0x40 | brake/reverse | guess, paired with slot 0 by position, not textually confirmed |
| 2 | 0x20 | steer left, game+0x24, drift tag 0x25 | proven on the bytes and the recording, round five below |
| 3 | 0x10 | steer right, game+0x20, drift tag 0x27 | proven, same |
| 5 | 0x08 | drift | given/proven, CLIENT_PHYSICS_MAP and car_drift_update |
| 4 | 0x04 | item use | guess, strong, FUN_004AEFA0's item-use switch statement is gated on slot 4's input_key_pressed/held |
| 7 | 0x02 | secondary item/use action | guess, also gated inside FUN_004AEFA0, distinct branch from slot 4 |
| 6 | not in the replay mask at all | cosmetic-only key (engine sound pitch bump seen in FUN_0048DF20) | guess |

`input_key_binding_lookup`'s altIndex parameter is 1 for a secondary/alternate binding of the same slot (seen in FUN_00497270, an on-screen control icon highlighter, which checks both altIndex -1 and 1 per slot), so each of the 8 slots has two bindable keys, primary and secondary.

### car_ghost_sample_record, 0x0049FAD0 (was FUN_0049fad0)

Not one of the physics-tick functions, but the direct source of the slot table above. `__thiscall(param_1=game base, param_2=car index)`. Runs a small frame-rate divider (records roughly every N frames depending on `iVar8 = *(param_1+0xA7858+iVar12)`, a record-slot index) into a per-car ghost/replay ring buffer at iVar3+0x3754, stride 0x1C (28 bytes) per recorded frame: position xyz (12 bytes, the three dwords at car+0x3244 0x3248 0x324C copied as they are, read again on 2026-09-14, no other field feeds them, so the sample z is the body origin car+0x21E8 the tick copies at 0x49D0A5), a yaw byte (car+0x3220 through `math_wrap_angle_360` then `crt_ftol_trunc`), a packed speed/rpm nibble dword, a flags dword (boost/reverse/drift/miniturbo bits identical in shape to the network status word), and finally the 7-slot key bitmask described above at +0x18 of the record. This buffer is what feeds car_ghost_replay_apply during a later ghost playback, and separately confirms the network status word's bit layout by construction (both are built the same way from the same car-state fields).

## car_pilot_path_update, 0x004990F0 (was FUN_004990f0), taken when the car's own +0x3714 (pfVar7+0x132 as a float-scaled index in cars_frame_update, see below) byte is 1

`__thiscall(param_1=game base, param_2=car index)`. Computes position as a quadratic-in-time curve from stored coefficients: `pos = coeff0*t + coeff1` per axis, using fields at iVar3+0x3700.. and a shared time parameter at iVar3+0x3710 that is advanced every call by `FUN_0044D1A0()` (untraced, presumably returns frame dt). Calls FUN_0048E6A0 (the same commit/finalize call car_remote_update and car_physics_tick_local end with). This is a scripted/pre-authored motion path (an intro lap, a stuck-car auto-drive, or similar), not physics or network driven, matches CLIENT_PHYSICS_MAP's mention of "the pilot path" in the per-tick step list.

## car_ghost_replay_apply, 0x0048F620 (was FUN_0048f620), taken when DAT_01AE8AE4 == 1

`__thiscall(param_1=game base, param_2=car index)`. Calls FUN_004489F0 on a per-car pointer at iVar3+0x210C (untraced). When `DAT_012124B8 == -1` (offline) and the car index is <=4, initializes a small 0x58-byte-stride ghost-id mapping table at globals `DAT_02EC1140.. ` for slots 0..4 and ensures a "coast/settle" state field at iVar3+0xA78F0 starts at 1. Otherwise (online) resolves the car's own ghost id (iVar3+0x744, the same field net_player_id_to_slot indexes network players by, here reused for a ghost handle) via FUN_004B6FD0; bails if not found. Calls `FUN_0048A3E0(ghostIndex, &pos(iVar3+0x3244), &yaw(iVar3+0x3220), carIndex)` which applies one recorded ghost frame's position/yaw directly (source not traced further, presumably a car_ghost_sample_record-format buffer played back); on success calls FUN_0048E6A0 (same finalize call as the other two movers).

## cars_frame_update, 0x00495330, how the four movers are selected

Confirmed directly (already named from the earlier physics pass, re-read here for the dispatch order). For each of the 30 slots, after per-car sound/LOD/culling calls (FUN_0048DF20 engine sound, FUN_0046F7A0 untraced, FUN_00499F00 3D audio pan, local car only):
```
if DAT_01AE8AE4 == 1:                         car_ghost_replay_apply(i)     // overrides every car, local included
elif *(char*)(carBase + 0x3714) == 1:          car_pilot_path_update(i)     // scripted path, this one car only
else:
    if i == localCarIndex(+0x6B0):             car_physics_tick_local(i)
    if raceState in {0xB,0xD,9,0xF,0x11} and i != localCarIndex:
                                                car_remote_update(i)
```
The byte the task calls "+0x132 of the car" is read in the decompiled source as `*(char*)(pfVar7 + 0x132)` where `pfVar7` is typed `float*` and already equals `carBase + 0x324C`; because of float-pointer scaling this resolves to absolute offset `carBase + 0x324C + 0x132*4 = carBase + 0x3714`, not literally `carBase + 0x132`. Reported both ways here since the two numbering conventions disagree; 0x3714 is the address actually dereferenced.

DAT_01AE8AE4 (ghost/replay mode) takes priority over everything, including the local car, in that mode every car's position is driven by car_ghost_replay_apply. car_remote_update only runs for a fixed set of race states (0xB, 0xD, 9, 0xF, 0x11) and never for the local car index.

## The game object input flags, +0x18/+0x1C/+0x20/+0x24/+0x2C, writer not located

Confirmed by reading all of car_physics_tick_local (1596 decompiled lines, in full): these four/five fields are only ever READ there, never written, except that the ground check result (`FUN_004A0750`, "cVar9") force-clears them when the car is off track:
```
if (FUN_004A0750() == 0) {           // off track
    param_1+0x1C = 1;                // brake forced on
    param_1+0x18 = 0;                // accel forced off
    param_1+0x2C = 0;                // stuck forced off
}
if (*(char*)(carBase+0x9D8) == 0) {
    param_1+0x1C = 1;                // brake forced on for a second, untraced, condition
}
```
and later they are only read (`if (accel==0 && stuck==0 && right==0 && left==0 && fVar10<2 ...)` at line 375 of the decompile, gating a "car is coasting with no input, listen for the accelerate key directly" branch that itself calls input_key_binding_lookup/input_key_down for slot 0).

The actual writer, presumably a per-frame "gather local car input" pass that reads input_key_down/input_key_pressed for slots 0/1/2/3 and stores booleans into these five offsets, was not found within the traced call graph (car_remote_update, motion_send_0x40, net_motion_recv_0x40, car_physics_tick_local, cars_frame_update, and the key/binding functions and their direct callers). The closest structural analogue actually read is FUN_0041B600, which snapshots input_key_binding_lookup(slot,-1)+input_key_down(code) for slots 0,3,1,2,5,4 into six consecutive bytes at a *different* object's +0x2024..+0x2029 (not the car record, and not renamed since its role is unclear), offered only as a pattern match, not proof the same function writes the car's own +0x18/0x1C/0x20/0x24/0x2C. This is a guess, flagged explicitly, not a finding.

## Functions renamed in this pass

| Address | New name |
|---|---|
| 0x0049ECD0 | net_remote_watchdog |
| 0x004A42B0 | net_motion_sample_pop |
| 0x004A4200 | net_motion_queue_shift |
| 0x004A4260 | net_motion_sample_push |
| 0x0047FD30 | net_motion_recv_0x40 |
| 0x0048DEA0 | net_player_id_to_slot |
| 0x0049EAE0 | car_remote_coast_extrapolate |
| 0x004818A0 | net_motion_pack_0x40 |
| 0x0045AF30 | input_key_binding_lookup |
| 0x0044B580 | input_key_down |
| 0x0044B590 | input_key_pressed |
| 0x004990F0 | car_pilot_path_update |
| 0x0048F620 | car_ghost_replay_apply |
| 0x0049FAD0 | car_ghost_sample_record |

Already named from the earlier physics pass and left unchanged: car_remote_update (0x0049ED90), motion_send_0x40 (0x0049BEB0), car_physics_tick_local (0x0049C0D0), car_drift_update (0x0049AA90), cars_frame_update (0x00495330), car_boost_start (0x00496BE0), wheels_on_ground_count (0x004EFA50).

Not renamed: FUN_0059029C (generic float round-to-int64 helper, hundreds of unrelated callers, not car/net/input specific), FUN_0041B600, FUN_0043F040, FUN_0045B3C0, FUN_004AEFA0, FUN_0048DF20, FUN_00499F00 (camera, options-menu key rebinding UI, item-use dispatch, engine sound and 3D audio pan, read for context but outside this task's scope and not fully enough understood to name with confidence), and any thunk/CRT functions.

## Open questions

- The exact writer of the local car's +0x18/+0x1C/+0x20/+0x24/+0x2C input flags (accel/brake/left/right/stuck) was not located.
- DAT_00B2360C race-state numeric meanings (0xB, 0xD, 9, 0xF, 0x11 etc.) are used pervasively but not enumerated anywhere read in this pass.
- FUN_0059029C is the CRT float to int truncation, now named crt_ftol_trunc. Its inputs on the ghost record side (the float loaded before each call) stay untraced, so the yaw-byte/speed-nibble encode formulas on the send side are only known by their decode-side inverse (DAT_005A6B20 = 360/255 for yaw, DAT_005A6B18 = 600.0 for rpm).
- Slot 1, 4, 6, 7 semantic names (brake, item, unknown, secondary-item) are inferred from structural position and call-site context, not proven by any string or label.
- net_motion_sample_pop/push's internal "this" addressing has an unresolved 4-byte wrinkle noted inline; does not change the depth-1 mailbox conclusion.

## Round five, 2026-09-14, the steer sense and the sample position

- The sample position of `car_ghost_sample_record` is car+0x3244 0x3248 0x324C, three dwords copied at 0x49FB4E to 0x49FB62, and car+0x324C is car+0x21E8 with no sign change (0x49D0AC), the z of the point R times the offset at wheel set +0x148 plus T that `body_set_pose` puts on the spawn. The 0x40 motion packet carries the same three floats raw. There is no lower field, the recorded z is the body origin. The port's z now sinks with the recording's timing, GHOST_REFERENCE.md, the rest depth is for the tyre model
- Slot 3 is the right turn and slot 2 the left turn. `input_poll_keyboard` puts slot 3 on game+0x20 and slot 2 on game+0x24 with the camera not reversed (0x497BA7 to 0x497BED, the reversed branch swaps them at 0x497B5C to 0x497BA2), the tick pushes game+0x18 as the six channels at 0x49D12E so channel 2 is game+0x20, `body_world_step` 0x4EFE90 steers by channel 2 minus channel 3, and with the swing `body_geometry_setup` 0x4F2AB0 builds at 0x4F2E50 (up cross base, the base axis pushed first is the second argument of `body_vec3_cross` which is A cross B) a positive channel 2 turns the heading about minus z in the physics frame, which is the wire yaw growing. The drift update 0x49AA90 tags slot 3 as 0x27 VK RIGHT and slot 2 as 0x25 VK LEFT at 0x49AB30 to 0x49AB6D, the stuck block turns the yaw up on game+0x20, and the recorded 0x90 sample (slot 3) turns the yaw up. The earlier reading of INPUT_AND_STATS.md, slot 2 right and slot 3 left, was the guess from the offset order, wrong. The port names game+0x20 `steerRight` and game+0x24 `steerLeft`
- `input_poll_keyboard` also writes the turn state car+0xA78E4 at 0x497DA4 to 0x497DBB: 1 while game+0x24 is held, 2 while game+0x20 is held alone, 0 otherwise, the cosmetic lean the status word carries as bits 4 and 2
- In race mode 9 with no accel no stuck and neither steer key `input_poll_keyboard` scales car+0x25FC by 0.85 (0x3F59999A) at 0x497D8A, a coast brake of that mode
- Functions and offsets renamed: `world_surface_air_penalty` 0x486D70 is `world_surface_contact_drag`, the tick charges it per loaded wheel (byte 0 at car+0x2DF0), TICK_HELPERS.md round five

## Round six, read on the bytes 2026-09-15

### net_motion_queue_clear 0x4A41F0

`this+0x28 = 0`. Every call site loads ECX with car+0x3348, the queue object `net_motion_sample_push` uses, so the field is the count at car+0x3370 and the "reset hook" empties the mailbox. `car_remote_update` calls it on the fourth mismatch, `car_remote_coast_extrapolate` before its eight pushes, `net_motion_recv_0x40` before a push, and `car_ground_flag_set` 0x49A920 at 0x49A942 when the ground flag goes to 1. The port has `net_motion_queue_clear` on the mailbox and `car_ground_flag_set_car` on the car record.

### DAT_00B23154 and DAT_00B23168, a dead knob

`if (DAT_00B23154 > 0) hint = hint minus trunc(DAT_00B23168), floored at 1`. Both globals are 0 in the image and no instruction writes either, so the branch never runs. The port keeps `hintDebugEnable` and `hintDebugOffset` at 0.

### The ground snap of the eased position 0x49F2C8

Only while car+0x3598 is at or under DAT_005C8320 (both 0 in the image, the gate passes by default, the same gate wraps the status decode below). `world_ground_height_at` 0x485970 (was FUN_00485970) takes the position, needs the probe active and `world_ground_test_point` to land, then writes `z minus (A times minus x plus B times minus y plus C times z plus D)` of the found cell into the third float, the cell plane is a unit normal so with C near 1 that is the plane height under the point. On a hit the z floors at the height plus 0.34 (0x5A6B24) when under the height plus 0.34 plus 0.3, a falling anchor target more than 0.8 over the height snaps onto it, and a cell named PUSH fans points 0 to 30 ahead in 0.5 steps along yaw minus the smoothed gauge times 1.2, one miss counts as a fall. A miss with car+0xA7874 clear sets it and slides the car: the two end vertices of the blocking edge (edge 0x10 and 0x12, two uint16 into the piece vertex array, `body_get_shape_point` 0x4ECE10) give a normal like the body fallback, a point 100 along it, the bearings from that point to the new and the old position give a reflected bearing, the car goes back to the old position, steps 0.2 (0x3E4CCCCD) along the bearing to itself, and the anchor target walks the reflected bearing over the anchor distance times 0.2 in steps of that times dt while the ground test holds. Then recovery state 0 goes to 1 when the plane is missing or the car sits more than one frame delta over it.

### The yaw and the status word

The effect wobble car+0x36AC joins the yaw before the delta unless code 300 runs (0x49F7D6), effect 300 also pins the position to the snapshot plus car+0x36C8 at the top of the update. The gauge decode writes twice, `90 times 0.0666667 times nibble minus 45` then that plus one more unit, so the gauge is `6 times (nibble plus 1) minus 45`. The rest of the decode is as documented. The turn lean eases with the same gate.

### Recovery state 2 to 3

State 2 holds only when the plane is found, the car sits 0.64 (0x5A6B10) or more over it and `now minus car+0xA78B8` is under 1501 ms. No instruction writes car+0xA78B8, it stays 0 from the record clear, so the hold never passes and state 2 lasts one tick. State 1 grows the scale by 1.068 (0x5A6B14) to 4, state 3 decays by 0.958 (0x5A6B0C) to 1.

### The watchdog threshold

`net_remote_watchdog_arm` 0x49E8A0 (was FUN_0049E8A0) is the only writer of car+0xA78C4: S2C 0x6A MotionBlock gives a player id and an i16 delay, the handler 0x47AFE0 stores the delay, sets state 1 and the time. No default, the field is 0 from the record clear, the watchdog only fires after that packet.

### The coast sample

`car_remote_coast_extrapolate` fills the yaw byte from `wrap360(yaw minus smoothed gauge times 0.6) times 255 over 360` truncated, the high nibble from the gauge plus 45 times 15 over 90, the low nibble from rpm minus 1000 clamped 0 to 9000 over 600, and forces 0x80 on the low byte. `motion_send_0x40` 0x49BEB0 encodes the same three the same way (0x49BFA9, 0x49BFB4, 0x49BFD9), the port send and coast use one encoder.
