# Motion Channel Verified

Resolves bug B01. Which opcode carries in race player motion. PROVEN from DevClient/KnC-new.exe.c.

## Verdict

Client sends C2S `0x40` (64) for its own motion. Client NEVER sends C2S `0x31` (49). `0x31` is S2C only.

| Claim | Result | Evidence |
|-------|--------|----------|
| C2S 0x31 exists | NO | grep `sub_44ECD0(_, 49)` zero hits in the dump |
| C2S 0x40 exists | YES | `sub_44ECD0(v8, 64)` line 225847 in sender `sub_4818A0` |
| 0x31 is S2C only | YES | handler `sub_479950` reads it as 2D minimap feed no client sender |
| PHYSICS_SPEED_VERIFIED was right | YES | motion is 0x40 not 0x31 |
| prior room doc was wrong | YES | it labeled 0x31 as C2S position wrong |

## C2S 0x40 motion sender sub_4818A0 @0x4818A0 (net_motion_pack_0x40)

builds opcode 64 then writes per the branch. anti hook `sub_487230` picks path. Full
decompile in `docs/reverse/physics/REMOTE_AND_INPUT.md` (net_motion_pack_0x40, motion_send_0x40).

normal path 19 bytes own position no id:

| # | field | bytes | how |
|---|-------|-------|-----|
| 1 | packed pos | 8 | `sub_44E610(x,y,z)` 3 floats to 8 byte pack |
| 2 | packed predicted pos | 8 | `sub_44E610(x,y,z)`, pos + frame_dt * 50.0 * velocity, NOT a rotation, proven at sub 4818A0 |
| 3 | yaw byte | 1 | `a2+6`, 0..255 over 0..360 degrees |
| 4 | status word | 2 | `a2+26` int16, bit table below |

alt path anti hook (or world id 20000000) sends 28 bytes raw `sub_44E9C0(v8, a2, 0x1C)`:
pos xyz, predicted pos xyz (6 floats, uncompressed), yaw byte, 1 pad byte, status ushort.

rate ~10 Hz. Timer gated at >= `DAT_01396E90` (100 ms, PHYSICS_SPEED_VERIFIED) since
`DAT_01396E80`, both game relative, sub 49BEB0 (motion_send_0x40).

## Status word bits and the two nibbles

From `docs/reverse/physics/REMOTE_AND_INPUT.md`, the int16 status word, low byte first:

| Bit | Meaning |
|---|---|
| 7 | mini turbo boost active |
| 6 | item boost active, only read when bit 7 is clear |
| 5 | reverse or brake |
| 4 | drift active |
| 3 | mini turbo stage 1 |
| 2, 1 | turn state, 0 none, bit2 = 1, bit1 = 2, cosmetic lean only, does not move the car |
| 0 | unused |

High byte: bits 12-15 (top nibble) are the speed nibble, decoded on the receiver into
the remote drift gauge, `(2*DAT_005EB700)*DAT_005A6B1C*nibble - DAT_005EB700`. Bits 8-11
(low nibble) are the rpm nibble, eased into rpm with `rpm += (nibble*600.0 - rpm) * DAT_005A32AC`.
Both nibbles are sender side `FUN_0059029C()` round-to-int calls; the exact encode
formula is only known from this decode side inverse.

## Packed coord space

`sub_44E610` is the ENCODER inverse of decoder `sub_44E7F0` = our `writePackedVec3`. Same format as the 0x40 START_RACE grid. selector nibble plus per axis 12 bit int plus 8 bit frac res 0.01 range 0..4095 world units. so the server can DECODE the C2S 0x40 packed pos to world coords and measure speed in world units. matches PHYSICS_SPEED base ~42 world u per s.

## S2C relay

- S2C 0x31 `sub_479950` reads `[int32 id][int32 x][int32 y]` = 2D minimap and rank feed raw int no scale.
- S2C 0x40 grid `sub_47FD30` (net_motion_recv_0x40) per player = `[int32 id][int16 hint][8 pos][8 predicted pos][int8 yaw][int16 status]` = 25 bytes, this is the 3D per player form. The second packed field is the sender's own predicted position, pos plus frame dt times 50 times velocity, not a rotation. The `hint` int16 is used on the receiver as the divisor `1000.0 / hint` when advancing the interpolation anchor, so send 1000, a 0 hint divides by zero client side. Anti hook / world id 20000000 path is 34 bytes, `[int32 id][int16 hint][28 raw]`, same uncompressed struct as the C2S alt path. Proven in `docs/reverse/physics/REMOTE_AND_INPUT.md`.
- Queue depth is 1, latest wins: `net_motion_sample_push` (0x4A4260) always drops any unconsumed sample for that car before writing the new one, so the receiver never has more than one pending sample per car.

PROVEN the server relays a remote player's 3D motion as S2C 0x40 per player 25 bytes, id prefixed, same field order as the grid. The C2S 19 byte own motion becomes the S2C 25 byte form by prefixing `[int32 senderId][int16 hint]`. Whether the server also feeds the 2D minimap/room list 0x31 from the same tick is a separate, unrelated question, not settled here.

## Interpolation, car_remote_update (0x49ED90)

From `docs/reverse/physics/REMOTE_AND_INPUT.md`, the receiver never snaps straight to a
sample, it eases toward it on two independent tick based countdowns (decrement 1.0 per
tick, not per millisecond):

- Position: countdown resets to 50.0 on a fresh non-first sample, each tick moves
  `1/N` of the remaining distance toward the sender's own predicted position (the
  anchor), so the rendered car chases a point the sender already extrapolated ahead.
- Yaw: a separate countdown resets to 6.0 on a fresh non-first sample, same `1/N` per
  tick shape, catches up much faster than position.
- The first sample ever received for a car snaps instead of easing, both countdowns
  start already expired.
- 4 consecutive frames where the rendered position has diverged from the last anchor
  past a threshold force a re-anchor (teleport), same snap as the first sample case.
- No fresh sample for a while: `net_remote_watchdog` (0x49ECD0) fires
  `car_remote_coast_extrapolate` (0x49EAE0) once, which dead reckons 8 shrinking steps
  along the car's last heading (~9.4 * 0.94^n) and pushes the most decayed one as a
  synthetic sample, then waits ~700 ms before it can fire again.

## Server impact fix pipeline

current server binds `C_POSITION = 0x31` and `handlePosition` reads `[id][x][y]` and rebroadcasts. this is DEAD the client never sends 0x31.

correct pipeline:
1. read C2S `0x40` 19 byte own motion decode the 8 byte packed pos to world x y z via the writePackedVec3 inverse.
2. anti cheat measure speed = world delta over dt cap grounded per PHYSICS_SPEED_VERIFIED not the guessed 600.
3. relay to remotes as S2C `0x40` per player 25 bytes, prefix senderId and a hint of 1000. optionally S2C 0x31 for minimap.
4. the server 0x40 is direction overloaded S2C = start grid count plus per player 25 vs C2S = one player own motion 19. dispatch by direction.

## Open

- exact meaning of `DAT_00B23154`, a debug/tuning knob read on the anchor-advance path, 0 in the shipped binary, not resolved.
- `net_motion_sample_pop`/`push`'s internal addressing has a 4 byte wrinkle in the decompiled pointer arithmetic, does not change the depth-1 mailbox conclusion, see REMOTE_AND_INPUT.md.
- whether the server should also drive the 2D minimap/room list 0x31 from the same C2S 0x40 tick, separate from motion, not decided here.
