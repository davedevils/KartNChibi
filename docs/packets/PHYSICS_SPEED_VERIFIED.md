# PHYSICS SPEED VERIFIED

Reverse of kart top speed movement math for a grounded server speedhack check.
Cross checked against binary `DevClient/KnC-new.exe.c` the 48 `DevClient/Data/Car/*.car`
physics structs server `RaceHandler.cpp` and `docs/packets/STRUCTURES_VERIFIED.md`
`docs/packets/PACKET_REGISTRY.md`.

Last verified 2026-07-07.
Cross checked 2026-09-14 against `docs/reverse/physics/REMOTE_AND_INPUT.md`,
`docs/reverse/physics/TICK_HELPERS.md`, `docs/reverse/physics/CONSTANTS.md` and
`docs/reverse/physics/INPUT_AND_STATS.md`. Confirmed with addresses: the 100 ms /
10 Hz send timer (`DAT_01396E90`), the 50.0 prediction constant (`DAT_0059F450`,
address 0x59F450), and the 1.728 speed to km/h factor (`DAT_005A69A8`, address
0x5A69A8, `kmh = rawSpeed * DAT_005a69a8` in `car_visual_update`). Corrected:
the second packed field of C2S/S2C 0x40 is a predicted position, not a rotation
(section 2 below); the network stat to physics map, called not found in section
5, is now proven (see the note there); the .car per kart engine force tiers in
section 4 are design/server side data only, the exe never opens a `.car` file.

## TL DR

| Claim | Verdict |
|---|---|
| Real client sends race position via 0x31 int32 | PROVEN FALSE no opcode 49 sender exists |
| Real client sends race movement via 0x40 packed | PROVEN sub_4818A0 sub_49BEB0 |
| Movement send rate | PROVEN 100 ms 10 Hz throttle dword_1396E90 |
| Packed coord format | PROVEN int 0..4095 plus frac 0..99 x 0.01 signed |
| Speed is finite diff of world pos | PROVEN v3213 = hypot dpos over dt |
| Speedometer km/h = 1.728 x speed | PROVEN line 234849 |
| World unit meter scale | INFERRED 1.728 = 3.6 x 0.48 so 1 unit approx 0.48 m |
| Per kart top speed from .car | PROVEN 3 engine force tiers 60k 70k 80k only |
| Network speed stat scales physics | INFERRED map not found in exe hot path |
| Item speed boost multiplier | PROVEN item type 19 sets 1.4x accel field |
| Mini turbo drift boost multiplier | INFERRED client local not located |
| Server 0x31 unit scale | UNKNOWN emulator client encoder not in repo calibrate |
| Current hardcoded 600 u/s | UNGROUNDED replace see section 8 |

## Source of truth

| Fact | Value |
|---|---|
| Authoritative dump | `DevClient/KnC-new.exe.c` Hex-Rays same build as Ghidra base 0x400000 |
| Physics structs | `DevClient/Data/Car/*.car` 416 bytes 104 floats raw fwrite |
| Read primitive | `sub_44E910(pkt, dst, N)` reads N bytes |
| Opcode setter | `sub_44ECD0(buf, opcode)` header size u16 opcode u16 |
| Enqueue send | `sub_476B80(net, buf)` on `dword_12124B0` |
| Timer | `sub_44ED50` QueryPerformanceCounter over freq div 1000 so MILLISECONDS |

---

# SECTION 1 two position channels the key finding

The server anti cheat reads C2S 0x31 as two int32. The REAL client never sends
opcode 49. There is no `sub_44ECD0(buf, 49)` anywhere. So the 0x31 based speed
check runs against a channel the shipping client does not emit.

The real in race movement is opcode 0x40 64 C2S.

| Channel | Opcode | Dir | Role | Format |
|---|---|---|---|---|
| Full movement | 0x40 64 | C2S | kart 3D pos plus predicted pos every tick | packed 8B pos 8B predicted pos |
| Minimap rank | 0x31 49 | S2C | 2D x y roster slot for map and rank | 3 int32 raw |

0x31 handler `sub_479950` line 220395 reads 3 int32 stores x to `dword_BCE208`
y to `dword_BCE20C` then `sub_407600` writes raw x y into the remote roster slot
base +26660 stride 136 no scaling. Roster slot is filled at join by `sub_408360`
line 139465 with raw int x y. So 0x31 is a 2D roster feed for the minimap and
rank sort not the physics kart.

Server `PacketBuilder::position` line 586 casts float x y to int32 and passes
through. The server does not own the unit it just relays whatever the client sends.

RESULT to check speed correctly the server must read the SAME channel the client
sends its motion on. On the shipping client that is 0x40. On the emulator the team
repurposed 0x31 as the C2S motion report so the check works only if the emulator
client fills 0x31 with real position deltas.

---

# SECTION 2 movement 0x40 format units and rate PROVEN

## Sender chain

| Func | Addr | Role |
|---|---|---|
| sub_49BEB0 | 0x49BEB0 | race tick builds v20 pos array gates on timer |
| sub_4818A0 | 0x4818A0 | packs and sends opcode 0x40 |
| sub_44E610 | 0x44E610 | pack 3 floats into 8 bytes |
| sub_44E7F0 | 0x44E7F0 | unpack 8 bytes into 3 floats S2C side |

## Send rate PROVEN 10 Hz

`sub_49BEB0` line 243420 reads current ms `sub_44ED50` and only sends when
elapsed minus last is at least `dword_1396E90`. That interval is set to 100 at
init `sub_495B20` line 239187.

```
if ( now_ms - last_send_ms >= 100 )  { build v20 ; sub_4818A0(net, v20) ; last = now }
```

So the client emits its motion every 100 ms which is 10 packets per second.

## Payload PROVEN

`sub_4818A0` line 225836 with anti hook off normal path:

| # | Bytes | Field | Source |
|---|---|---|---|
| 1 | 8 | packed position | sub_44E610(x, y, z) |
| 2 | 8 | packed predicted position, NOT rotation | sub_44E610(x, y, z) |
| 3 | 1 | yaw byte a2[6] | 0..255 over 0..360 degrees |
| 4 | 2 | status word a2+26 | bit7 mini turbo, bit6 item boost, bit5 reverse, bit4 drift, bit3 mini turbo stage1, bit2/1 turn state, high nibble speed, low nibble rpm |

v20 is built from the local kart physics block. v20[0..2] = world pos
`v6[3217..3219]`. v20[3..5] = predicted pos = pos plus dt x 50 x velocity
`v6[3214..3216]` line 243431. Field 2 was originally logged here as a packed
rotation; `docs/reverse/physics/REMOTE_AND_INPUT.md` (net_motion_pack_0x40
0x4818A0, car_remote_update 0x49ED90) proves it is this same predicted
position, read back by the receiver as the interpolation target.

## Packed coord format PROVEN

`sub_44E610` line 190538 and unpack `sub_44E7F0` line 190565. Each axis value
is stored as an integer part 0..4095 12 bits plus a fraction 0..99 two decimal
digits scaled by 0.01 plus a sign quadrant nibble.

```
coord = (intPart 0..4095) + (frac 0..99) * 0.01   signed
```

Unpack proof line 190575 `(byte * 0.0099999998 + twelvebit) * sign`.

So the world coordinate space is 0..4095 units per axis with 0.01 unit
resolution. A kart at world x 1234.56 packs as int 1234 frac 56.

---

# SECTION 3 speed derivation PROVEN

## Velocity is a finite difference

Physics update `sub_?` line 234830-234849 in the big race function computes
velocity from the change in world position.

```
dt   = this[13]
vx   = (x - x_prev) / dt          ; v[3214]  line 234840
vy   = (y - y_prev) / dt          ; v[3215]
vz   = (z - z_prev) / dt          ; v[3216]
speed = hypot(vx, vy)             ; v[3213]  sub_44D900 line 234846
speedo_kmh = 1.7280002 * speed    ; v[3261]  line 234849
```

`sub_44D900` line 189872 is `sqrt(a1*a1 + a2*a2)`. So v[3213] is the horizontal
ground speed in WORLD UNITS PER SECOND.

## Speedometer scale gives the world meter

The dash km/h is `1.728 * speed_units_per_sec`. If the gauge reads km/h then

```
km/h = 1.728 * v_units_per_sec
1.728 = 3.6 * 0.48
```

3.6 is m/s to km/h. So 1 world unit is about 0.48 m INFERRED. This is the single
best anchor for turning a unit delta into a real speed.

| Speed world u/s | m/s approx | km/h dash |
|---|---|---|
| 10 | 4.8 | 17.3 |
| 20 | 9.6 | 34.6 |
| 30 | 14.4 | 51.8 |
| 40 | 19.2 | 69.1 |
| 60 | 28.8 | 103.7 |

Code high speed gates land near these values. v[3213] compared 3 10 15 30 and
the branch `v[3213] <= 30.0` line 241155 treats 30 u/s as the normal ceiling and
above as very fast. Cruise sits about 30-40 u/s boost pushes past 40.

The key point the server speed math dist over dt IS the same finite difference
the client uses. Method is correct only the unit scale and the cap are missing.

---

# SECTION 4 per kart top speed from .car PROVEN

All 48 karts share gearbox torque curve and mass. Only engine force and axle
force vary. So the chassis alone gives THREE top speed tiers.

| .car field | Off | Value spread |
|---|---|---|
| engine force | 0x100 0x104 | 60000 70000 80000 only |
| front axle force | 0x108 0x10C | 4000..8000 |
| rear axle force | 0x110 0x114 | 4000..8000 |
| mass proxy | 0x0C idx3 | 200 CONST all karts |
| gearbox | 0xA4..0xB8 | 3.38 2.05 1.43 1.09 0.87 0.70 CONST |
| final drive | 0xBC | 2.20 CONST |
| torque curve peak | 0x168 0x16C | 14000 CONST |

Engine force tiers count 60000 x16 70000 x2 80000 x30.

| Tier engF | Karts | Rel top speed factor sqrt ratio |
|---|---|---|
| 60000 | bikes Circler Firedragon M500 Quatzalcuatl Squarer fr_01..03 ds_01 | 0.87 |
| 70000 | qd_01 qd_02 | 0.94 |
| 80000 | basic_1..6 tank mini monster oskart citroen wolf etc | 1.00 |

Rel factor uses drag balance v_max proportional to sqrt(engineForce) since mass
and drag are shared INFERRED. The chassis spread is modest about 13 percent
slowest to fastest. Real per kart spread must come from the upgrade speed stat
see section 5.

Note the .car engine units are NOT SI. Feeding gear 0.70 x final 2.20 with the
14000 torque and 200 mass into a textbook v_max gives hundreds of m/s which is
nonsense. So do NOT derive an absolute top speed from the .car. Use the
speedometer anchor section 3 instead.

CORRECTION 2026-09-14: `docs/reverse/physics/INPUT_AND_STATS.md` searched
KnC.exe.raw for `.car`, `%s\.car` and `Data/Car` and got zero string hits, and
no function in the traced call graph opens a path matching that pattern. The
running client never reads a `.car` file, the 17 base stats it actually races
with arrive over the wire in S2C 0xC0 KartDefinition and land at car+0x3440,
stored through `stat_catalog_store` (0x44F510) and `car_apply_kart_loadout`
(0x490A70). Treat this section's engine force tiers as server/tool side design
data, not something the exe applies at runtime; the live per kart top speed
knob is wire stat index 3, see the correction under section 5.

---

# SECTION 5 stat scaling INFERRED

VehicleData carries 7 stats speed accel handling drift boost weight special at
+0x10..+0x28 see STRUCTURES_VERIFIED. STRUCTURES notes these live in the DB and
the network vehicle catalog separate from the .car.

Where the network speed stat multiplies the sim was NOT found in the exe hot
path. The one clear stat driven multiplier `sub_43E850` line 177734 feeds a
vtable call not the velocity integrator it drives a speed proportional visual
audio effect. Its shape is still informative.

```
mult = ((tune - 1.0) * 0.15 + 1.0) * playerSpeed * 0.003 + 0.95
clamp [1.05, 1.8]
state cases  drive 1.05..1.8   slow 0.80   grass 0.65   mud 0.50
```

So the client scales an effect between 0.5 and 1.8 by state and a speed like
value. Best inference the upgrade speed stat maps to a top speed multiplier in a
similar narrow band roughly plus or minus 15 to 30 percent across the stat range.
The weight stat most likely trims acceleration and slightly top speed. Neither
map is proven pin them by dynamic trace before trusting exact numbers.

CORRECTION 2026-09-14: the network stat to physics map IS now found, in the S2C
0xC0 17 float stat block (car+0x3440, bonus car+0xA7940), read directly in
`car_physics_tick_local` (0x49C0D0) and `car_drift_update` (0x49AA90), proven in
`docs/reverse/CLIENT_PHYSICS_MAP.md` and `docs/reverse/physics/INPUT_AND_STATS.md`:

| Index | Used for | Formula |
|---|---|---|
| 3 | max speed | `clamp(stat3+bonus+1, 1.0, 2.0) * DAT_005EB6FC` (320.0) |
| 4 | steering gain | feeds car+0x2974 drift steering gain |
| 5 | mini turbo target speed | `clamp((stat5+bonus)*0.2+1.0, 1.0, 1.2) * 120.0` km/h |
| 7 | turn force | |
| 8 | wheel spin | kind 2 vehicles read this directly for wheel spin torque |
| 9 | wheel steer angle | kind 2 vehicles read this directly for the front wheel angle |
| 10 | drift charge rate | clamped 0.3 to 0.8, quartered while slowed |
| 11 | drift steer | into `body_vec3_set` 0x4ED3D0 |
| 12 | mini turbo threshold | gauge must pass `stat12 * DAT_005EB704` |
| 13 | mini turbo hold time | hold must pass `stat13 * DAT_005EB708` |
| 14 | grip | |

Indices 0, 1, 2, 6, 15, 16 still have no confirmed read site, open in
INPUT_AND_STATS.md. `sub_43E850`'s visual/audio multiplier above is a separate,
unrelated effect, it does not read this stat block and does not drive the
velocity integrator either.

---

# SECTION 6 boost PROVEN and INFERRED

| Boost kind | Effect | Proof |
|---|---|---|
| Item speed boost type 19 | sets accel field this[3308] to 1.4 from 1.0 | line 256083 |
| Item accel term | v10 = dt * this[3308] * 3.2 then x3 or x2 on pad flags | line 255657 |
| State drive multiplier | up to 1.8 clamp | sub_43E850 line 177757 |
| Off road penalty | 0.80 0.65 0.50 | sub_43E850 |
| Mini turbo drift boost | client local stage 0..3, car+0x35F0 | INPUT_AND_STATS.md car_drift_update 0x49AA90, not on wire |

The item boost lifts the accel scalar to 1.4x and pad conditions can double it.
CORRECTION 2026-09-14: the mini turbo stage 0..3 is now located, car+0x35F0,
driven inside `car_drift_update` (0x49AA90): stage 1 once the drift gauge passes
stat 12 times `DAT_005EB704`, stage 2 on stick release once the hold time passes
stat 13 times `DAT_005EB708`, stage 3 when the accelerator is pressed within
`DAT_005EB70C` ms of release, then `car_boost_start` kind 0 fires the boost.
Exact top speed gain and duration of the resulting boost are covered by
`car_boost_update` (0x496E50, see docs/reverse/physics/TICK_HELPERS.md) not by
this file; treat this row's 1.5 to 1.8x peak figure as still INFERRED.

---

# SECTION 7 0x31 space units and rate summary

| Question | Answer | Conf |
|---|---|---|
| 0x31 coord space | 2D x y world plane for minimap and rank | PROVEN handler |
| 0x31 scaling on wire | none raw int32 server casts float to int | PROVEN line 586 |
| 0x31 unit real client | client never sends it S2C only | PROVEN |
| Real motion channel | 0x40 packed pos every 100 ms | PROVEN |
| World unit size | approx 0.48 m from 1.728 speedo | INFERRED |
| Real send rate | 10 Hz | PROVEN |
| Emulator 0x31 unit | whatever emulator client encodes NOT in repo | UNKNOWN |

For a delta over dt speed check the dt is the wire gap not a server wall clock.
At 10 Hz a legit gap is 100 ms. The server currently uses steady_clock ms which
drifts from the true 100 ms cadence and inflates jitter. Prefer counting packets
or clamping dt to the known cadence.

---

# SECTION 8 concrete server speed cap replace 600

## Problem with the current code

`RaceHandler.cpp:182` `MAX_SPEED = 600.0f` and `:174` `MAX_DELTA = 150.0f` are
ungrounded. Neither matches any unit derived here. At 10 Hz world units a real
top of about 60 u/s is only 6 units per 100 ms so 150 per axis is 25x too loose
in world units or far too tight if the emulator sends centi units. The scale is
unknown because the emulator client encoder is not in the repo.

## Step 1 pin the server unit scale MUST DO FIRST

The server relays client ints it does not know if they are world units 0..4095
or centi units 0..409500. Pin it one of two ways.

- Read START_RACE grid spawn 0x40 packed pos it decodes to world units 0..4095.
  Compare to the range of the C2S x y the client then streams. Ratio is the scale.
- Or log peak dist over dt across several CLEAN races. The clean peak IS ground
  truth in the server own unit. Call it PEAK_OBS.

Define S = server position units per world unit. World unit is approx 0.48 m.

## Step 2 grounded cap formula

Express the cap in world units per second then multiply by S.

```
base_top_wu_s   = 42.0                  ; cruise ceiling near the 30..40 gate
tier_factor     = { 60000: 0.87, 70000: 0.94, 80000: 1.00 }[car.engineForce]
stat_factor     = 1.0 + 0.30 * (speedStat / STAT_MAX)   ; INFERRED band +30%
boost_factor    = 1.8                   ; item plus mini turbo peak window
tolerance       = 1.15                  ; jitter and packet timing slack

max_wu_s = base_top_wu_s * tier_factor * stat_factor * boost_factor * tolerance
MAX_SPEED_server_units_per_sec = max_wu_s * S
```

Worked example 80000 tier maxed speed stat.

```
42.0 * 1.00 * 1.30 * 1.8 * 1.15 = 113 world u/s   ( about 195 km/h dash peak )
```

So if the emulator sends WORLD units S = 1 then `MAX_SPEED` should be about 113
not 600. If it sends CENTI units S = 100 then about 11300. Pick S from step 1
then plug in. The old 600 is right for neither scale.

## Step 3 per axis delta cap

Replace the flat 150. Max per axis move per 100 ms is `max_wu_s * 0.1 * S`.

```
MAX_DELTA_per_axis = max_wu_s * 0.100 * S * 1.25
```

Example world units S = 1 gives 113 * 0.1 * 1.25 = 14 units per axis per 100 ms.
Use the real wire gap not wall clock for the 0.1 term.

## Step 4 recommended shape

- Compute the cap per player at race start from car engineForce and speedStat.
- Keep the strike counter kick on repeat not on one spike teleports and lag
  cause single frame spikes.
- Prefer a sliding window average speed over N packets to a single delta the
  finite difference is noisy at 10 Hz.
- If feasible move the check onto the real 0x40 motion channel and decode the
  packed pos so the units are the proven world units 0..4095.

---

# Open unknowns

| Item | Status |
|---|---|
| Emulator client 0x31 encode scale | not in repo pin via section 8 step 1 |
| Network speed stat to physics map | not found in exe dynamic trace needed |
| Weight stat effect on top speed | inferred accel trim not proven |
| Mini turbo exact speed gain and duration | client local not located |
| Absolute base top speed number | anchored via speedo 1.728 not a hard constant |
| .car engine unit system | non SI cannot back out SI top speed |
| Whether server should adopt 0x40 as the motion channel | design call see section 1 |

# Cited sources

| Ref | Location |
|---|---|
| 0x31 handler S2C | DevClient/KnC-new.exe.c:220395 sub_479950 |
| 0x31 roster store | DevClient/KnC-new.exe.c:138869 sub_407600 |
| roster slot init | DevClient/KnC-new.exe.c:139465 sub_408360 |
| no 0x31 sender | grep sub_44ECD0 opcode list has no 49 |
| movement tick | DevClient/KnC-new.exe.c:243398 sub_49BEB0 |
| movement send | DevClient/KnC-new.exe.c:225836 sub_4818A0 |
| send interval 100 | DevClient/KnC-new.exe.c:239187 sub_495B20 |
| timer ms | DevClient/KnC-new.exe.c:190813 sub_44ED50 |
| pack coords | DevClient/KnC-new.exe.c:190538 sub_44E610 |
| unpack coords | DevClient/KnC-new.exe.c:190565 sub_44E7F0 |
| velocity finite diff | DevClient/KnC-new.exe.c:234838 |
| speed hypot | DevClient/KnC-new.exe.c:189872 sub_44D900 |
| speedo 1.728 | DevClient/KnC-new.exe.c:234849 |
| speed gate 30 | DevClient/KnC-new.exe.c:241155 |
| stat effect mult | DevClient/KnC-new.exe.c:177734 sub_43E850 |
| item boost 1.4 | DevClient/KnC-new.exe.c:256083 |
| accel term | DevClient/KnC-new.exe.c:255657 |
| .car offsets | docs/packets/STRUCTURES_VERIFIED.md gearbox 0xA4 forces 0x100 |
| 0x31 server builder | server/game/src/packets/PacketBuilder.cpp:586 |
| current 600 cap | server/game/src/handlers/RaceHandler.cpp:182 |
</content>
</invoke>
