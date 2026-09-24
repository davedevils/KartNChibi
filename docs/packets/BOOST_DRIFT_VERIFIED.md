# BOOST DRIFT VERIFIED

Assessment of drift mini turbo boost ramps and lightning start wire protocol.
Cross checked against binary `DevClient/KnC-new.exe.c` server `RaceHandler.cpp`
`Protocol.h` and the old command map, the registry replaces it.

Last verified 2026-09-15, the section right below is the live state, the numbered sections
under it are the 2026-07-07 assessment kept as history.

## The state word judge on 0x40, verified against the stock client 2026-09-15

There is no drift or boost opcode. The client puts its drift and boost state in the u16 of every
C2S 0x40 self report (bit 7 mini turbo class boost, bit 6 item class boost, bit 4 drifting, bit
3 stage 1 charged) and the server only observes it, `DriftBoostPackets::observeState` in
`server/game/src/packets/gen/DriftBoostPackets.cpp`, called from `RaceHandler::handleMotion`,
strikes into the leaky suspicion bucket of `docs/packets/RECOVERED_RE_NOTES.md`.

On 2026-09-15 the stock client, driven by our pilot on our package (Race 01, template 10010, 37
drifts, twelve mini turbos, twenty pad boosts, one item boost), made the live server log
`miniturbo_fast` and `boost_unmatched` all race long. The recording of that race replayed through
the old judge trips 36 times, `miniturbo_fast` 22, `boost_unmatched` 6, `drift_charge` 8. None
was a cheat. The reasons, each read on the recording and on the client machine of
`docs/reverse/physics/TICK_HELPERS.md` (the drift, settled):

| Trip | Why the old judge fired | What the client does |
|---|---|---|
| `miniturbo_fast` on 8 mini turbos | it wanted bit 3 still up in the sample before the boost | the key release drops the stage to 2 while the state stays up for the gauge decay, so the sample before the boost reads drift with no charge bit |
| `miniturbo_fast` on 14 pad boosts | it took every class 0 boost for a mini turbo | a `boost.ini` kind 0 pad starts the same kind 0 boost with no drift at all, the client starts it alone on the wheel touch |
| `boost_unmatched` on 6 pad boosts | it wanted a C2S 0x47 for every class 1 boost | the kind 1 pads of Race 01 start a class 1 boost with no item |
| `drift_charge` on 8 drifts | it measured the hold on the receive clock with 100 ms of slack | the recording samples every 200 ms and the wire bunches, the hold must be counted in samples |

The judge now, the full rules are in `docs/packets/opcodes/0x0040.md`:

- hold and windows counted in samples of the fixed 100 ms send period, never receive ms
- stage 1 no sooner than floor(hold / 100) samples after bit 4 rose, hold from wire stat 11
- the mini turbo is the boost in the sample where bit 4 drops (one sample of grace) off a drift
  that showed stage 1 or outlived the hold, class 0 or the 3 percent lucky class 1
- a boost anywhere else needs a pad cell of the matching kind under the path since the last
  sample or, for class 1, a C2S 0x47 inside 600 ms
- the pads come from the `BOOST_NNN` cells of the track COL and its overlays plus `boost.ini`
- the kart numbers come from the same 17 wire floats the login burst ships in 0xC0, wire 3 mini
  turbo target, 8 drift charge rate, 10 threshold, 11 hold (migration 059 numbering)

Durations as `car_boost_update` 0x496E50 runs them, kind 0 `400 times clamp(stat 3 plus 1, 1, 2)`
ms, 608 on the basic kart, kinds 1 to 7 are 3800 6000 6000 5000 15000 5 1500 ms (kinds 1 and 2
plus 500 with the extend pet). The wire bit outlives the duration by the decay tail, twelve ticks
for kind 0, so the recording shows the bit four to five samples of 200 ms, and a pad chain
restarts a running boost with no gap, so the bit can stay up 2 s or 7 s legitimately, the judge
puts no ceiling on it.

Proof `tests/server/test_drift_boost_observer.cpp`, the recording
`tools/replay/recordings/drift_t90_c7_20260915.ghost` replayed with the Race 01 pads gives zero
violations, a boost with no drift and no pad, a mini turbo every second, a charge on the first
drift sample and a speed over the cap each still strike. Build with
`cmake -S server -B build-server-ac -G "Visual Studio 17 2022" -A x64 -DKNC_BUILD_TESTS=ON`.

Still open: the km per hour to world unit factor of the speed ceiling is one data point
(`KMH_TO_WORLD_UNPROVEN` 0.5788), the kind 1 pads read 120 units a second flat for a 220 kmh
target which says 0.545, the ceiling stays loose either way. The motion gate budget of
`handleMotion` (about 141 units a second per axis) sits under a kind 2 boost target of 260 kmh,
no shipped Race 01 pad reaches it. A pad cell without a `boost.ini` line starts nothing on the
client and passes any class here.

## TL DR of the 2026-07-07 assessment

| Claim | Verdict |
|---|---|
| Original client sends drift boost packets | PROVEN FALSE no send of 0xBC-0xBF in binary |
| C_DRIFT_START/END C_BOOST_ACTIVATE/END are real client opcodes | INFERRED FALSE server invented emulator side |
| Drift mini turbo is client local feature | PROVEN key bind plus tutorial plus assets |
| Server boost S2C 0x4B format correct | PROVEN WRONG client drops it |
| Server drift S2C 0x32 shows drift on remotes | PROVEN NO just per slot scalar store |
| Boost ramps reach wire | INFERRED NO client local track asset |
| Lightning start has a packet | PROVEN NO countdown is client local 0x33 |

## 1. Drift boost WIRE protocol

### Server side opcode bindings PROVEN server def

`shared/src/include/net/Protocol.h:406-409`

| Name | Opcode | Server handler |
|---|---|---|
| C_DRIFT_START | 0xBC 188 | RaceHandler::handleDriftStart |
| C_DRIFT_END | 0xBD 189 | RaceHandler::handleDriftEnd |
| C_BOOST_ACTIVATE | 0xBE 190 | RaceHandler::handleBoostActivate |
| C_BOOST_END | 0xBF 191 | RaceHandler::handleBoostEnd |

Routed in `GameServer.cpp:191-208`.

### Client never sends these PROVEN

Binary search for opcode send of 188 189 190 191 returns nothing.
Only 0xBC hit is `sub_44E910(a2, v4, 0xBC)` at line 223320 a 188 byte read size
not an opcode. So the real client emits no drift or boost C2S packet.
The 0xBC-0xBF block in `Protocol.h` is emulator invented.
Matches design doc `2026-03-16-boost-drift-design.md` FUTURE section which lists
network sync as not done and proposes packet 0x47.

### Server payload readers vs the old command map mismatch PROVEN

| Handler | Server reads | Old map says | Note |
|---|---|---|---|
| handleDriftStart | `int32 direction` RaceHandler.cpp:495 | `-` none | doc says no payload code reads 4 bytes |
| handleDriftEnd | `int32 boostLevel` RaceHandler.cpp:515 | `u8 level` | width mismatch int32 vs u8 |

Since client sends nothing these reads are dead. Underread is safe per
`sub_44E910` rules over declared read would crash.

### Opcode collision note

C2S 0xBE 0xBF reuse values that are S2C race packets.

| Opcode | S2C use old map | C2S use Protocol.h |
|---|---|---|
| 0xBE 190 | Race init sub_478B50 | C_BOOST_ACTIVATE |
| 0xBF 191 | Race player data 1 sub_47F390 | C_BOOST_END |
| 0xC0 192 | Race player data 2 sub_47F4F0 | C_GHOST_MENU |

Direction disambiguates on dispatch but the numbering is fragile.
0xBC 0xBD have no S2C handler so those two are safer to reuse.

### S2C the server sends

Drift start end use `PacketBuilder::position32` cmd 0x32 S_POSITION_32.

| Event | Server sends | Payload |
|---|---|---|
| drift start | position32 RaceHandler.cpp:508 | `[characterId, direction]` |
| drift end | position32 RaceHandler.cpp:535 | `[characterId, 0]` |
| boost activate | S_GAME_DATA_4B 0x4B:582-587 | `[characterId, boostLevel, boostDuration, 1]` |
| boost end | S_GAME_DATA_4B 0x4B:603-608 | `[characterId, 0, 0, 0]` |

### Client handler check PROVEN

0x32 handler `sub_479C70` line 220540 reads 2 int32 then calls
`sub_409F90(dword_B9ADD0, idx, val)` line 140644 which does
`*(base + 756*idx + 6792) = val`. This is a per slot scalar store into the
player struct not a drift spark or animation. So broadcasting drift over 0x32
draws nothing on remotes. Also first field is used as a slot index the server
passes raw characterId so index semantics are suspect.

0x4B handler `sub_47A460` line 220821 reads `[playerId, type, p1, p2]` looks up
player then acts ONLY when `type == 10` sub_4CA1A0 or `type == 16` sub_4C6910.
Any other type value falls through and spawns nothing. Confirmed by server
`buildItemEffect` RaceHandler.cpp:43 which routes type 10 or 16 to 0x4B and all
other types to 0x47.

RESULT the boost activate packet sends `type = boostLevel` 1 2 or 3 which is
never 10 or 16 so the client silently ignores it. Boost S2C is a no op.

0x47 handler `sub_47A110` line 220724 reads `[playerId, actionType, 4 params]`
24 bytes and switch on actionType cases 2 3 4 5 7 8 9 11-15 17-21 spawn effect
emitters. This is the item and effect channel not currently used for boost.

## 2. Mini turbo drift charge levels 0-3

Client local INFERRED. Server never simulates charge.
- Binary proof of feature keyboard bind `drift` at settings offset 65716 read by
  the input loader line 204891 plus joystick `drift` line 204923.
- `Tutorial/Miniturbo/Tutorial_Tip_MiniTurbo.png` line 156687 mini turbo is a
  taught mechanic.
- `LicenseGame/drift_left.PNG` `drift_right.PNG` line 154576 drift UI.

Server trust model PROVEN. `handleDriftEnd` accepts a client reported
`boostLevel` clamps to 0..3 else flags suspicious RaceHandler.cpp:522.
Server does not time the drift hold. Charge level is computed on the client and
reported. Design doc thresholds 1s 2s 3s per level and 900-1050ms release
window are engine side guesses not verified in the binary.

## 3. Boost RAMPS

INFERRED track authored client local not on wire.
- No boost pad opcode exists client side. No pad send in binary.
- Booster is a car attached model asset `Car/FactoryCar/BOOSTER/%s/BOOSTER`
  line 168279 and UI parts `parts/%s_booster_B_%02d.png` line 180782.
- Design doc `2026-03-16-boost-drift-design.md` models pads as `boost.ini` per
  map loaded client side triggered by distance. No wire path.

So a ramp boost never reaches the wire in the original client. If the emulator
wants remotes to see a ramp boost it must push a real effect over 0x47 or over
0x4B with type 10 or 16 not a made up type.

## 4. Lightning start

INFERRED perfect start boost is client local. No packet.
- Countdown has no server packet. 0x3B S_COUNTDOWN is nullsub see
  `docs/packets/game/0x3B_COUNTDOWN.md`.
- Race GO is driven by 0x33 S_GAME_STATE `sub_4799B0` line 220416. It reads
  `[idx, state]` and when `state == 2` sets `dword_BCE228 = 1` and calls
  `sub_40A040` the GO trigger. Other states call `sub_409FB0` per slot store.
- A perfect start would be measured on the client by input timing at the GO
  frame. No C2S opcode reports it. Server cannot validate a lightning start
  today because no packet carries it.

## 5. Server gap assessment

### Are the existing S2C formats correct

| Packet | Correct | Why |
|---|---|---|
| drift start end 0x32 | NO | 0x32 is a per slot scalar store no drift visual |
| boost activate 0x4B | NO | type field must be 10 or 16 server sends 1-3 client drops |
| boost end 0x4B | NO | same channel same problem |

To make boost visible on remotes the server must use 0x47 S_PLAYER_ACTION with
a real actionType the client switch handles or 0x4B with type 10 or 16.
The correct effect type ids for a boost trail are still unknown see open items.

### Validation present PROVEN

| Check | Where | Value |
|---|---|---|
| boost level range | handleDriftEnd:522 | 0..3 else suspicious clamp 0 |
| boost re trigger cooldown | handleBoostActivate:557 | 500 ms |
| boost spam cap | handleBoostActivate:564 | boostCount > 30 suspicious |
| boost duration derive | handleBoostActivate:575 | boostLevel * 500 ms |

### Validation possible but missing

| Rule | How | Status |
|---|---|---|
| drift duration | timestamp C_DRIFT_START require min hold before level accepted | MISSING server never times drift |
| boost level cap | already clamp 0..3 | DONE |
| ramp eligibility | need server track pad positions plus player pos from 0x31 C_POSITION | IMPOSSIBLE no track data server side |
| lightning start | server owns startRace GO time could validate a reported perfect start window | MISSING no packet exists |

## Open unknowns

- Real mini turbo charge thresholds and release window, FOUND since, `car_drift_update`
  0x49AA90 in `docs/reverse/physics/TICK_HELPERS.md` round ten, the top section has them.
- Which 0x47 actionType case renders a drift spark or a boost trail. Cases exist
  2-21 but none mapped to boost yet.
- What effects 0x4B type 10 sub_4CA1A0 and type 16 sub_4C6910 actually spawn.
- Whether the original netcode syncs any boost drift visual to remotes, ANSWERED since, the
  state word of 0x40 carries the drift and boost bits to every remote car.

## Cited sources

| Ref | Location |
|---|---|
| C2S opcode defs | shared/src/include/net/Protocol.h:406-409 |
| C2S routing | server/game/src/GameServer.cpp:191-208 |
| drift handlers | server/game/src/handlers/RaceHandler.cpp:491-609 |
| effect router | server/game/src/handlers/RaceHandler.cpp:43-48 |
| position32 builder | server/game/src/packets/PacketBuilder.cpp:589 |
| 4B and 47 builders | server/game/src/packets/PacketBuilder.cpp:910 1476 |
| 0x32 client handler | DevClient/KnC-new.exe.c:220540 sub_479C70 |
| 0x32 store target | DevClient/KnC-new.exe.c:140644 sub_409F90 |
| 0x4B client handler | DevClient/KnC-new.exe.c:220821 sub_47A460 |
| 0x47 client handler | DevClient/KnC-new.exe.c:220724 sub_47A110 |
| 0x33 game state GO | DevClient/KnC-new.exe.c:220416 sub_4799B0 |
| drift key bind | DevClient/KnC-new.exe.c:204891 |
| mini turbo tutorial | DevClient/KnC-new.exe.c:156687 |
| drift UI assets | DevClient/KnC-new.exe.c:154576 |
| booster car asset | DevClient/KnC-new.exe.c:168279 |
| countdown local | docs/packets/game/0x3B_COUNTDOWN.md |
