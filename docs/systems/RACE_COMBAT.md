# Race and combat

What the game server does from the master start press to the result board: grid, motion,
laps, items, drift and boost, CPU cars, rewards and the anti cheat judge. Wire layouts are on
the opcode pages in `docs/packets/opcodes/`, the motion channel in
`docs/packets/MOTION_CHANNEL_VERIFIED.md`, the grid in `docs/packets/systems/race_core.md`,
items in `docs/packets/systems/race_items.md`.

## Source files

| Area | File |
|------|------|
| race lifecycle, motion, laps, items, finish, bots | `server/game/src/handlers/RaceHandler.cpp` |
| CPU car driving | `server/game/src/handlers/RaceBots.cpp` |
| ability class and fire, shield token | `server/game/src/handlers/ItemHandler.cpp` |
| motion gate and wire | `server/game/src/packets/gen/MotionPackets.cpp` |
| drift and boost observer, speed judge | `server/game/src/packets/gen/DriftBoostPackets.cpp` |
| grid, laps, respawn wire | `server/game/src/packets/gen/SpawnPackets.cpp` |
| item model and wire | `server/game/src/packets/gen/ItemPackets.cpp` |
| rewards, scoreboard, finish frame | `server/game/src/packets/gen/ResultsPackets.cpp` |
| violation log and kick | `server/game/src/handlers/AntiCheatHandler.cpp` |

## Trust model

The client drives its own car, rolls its own item boxes, applies hits to itself and counts
its own checkpoints. The server relays, keeps a model of each racer to judge the samples,
owns the lap count from the checkpoints, the finish, the ranks and every reward.

## Start

1. The master press on C2S `0x33` passes `Room::startRefusal`, see
   [LOBBY_ROOM_CHAT.md](LOBBY_ROOM_CHAT.md).
2. `handleStartRace` sets the room to Starting so no bot can be seated, takes
   `room->participants()` as the roster, sends one `0xBF` row per seat, draws a real track for
   the random pick, loads `track_spawn`, the COL checkpoint count and the boost pads, then
   S2C `0x33`, `0x34` and the `0x14` scene change with the catalogue track id.
3. Five seconds later `sendGridPhase`: one S2C `0x3E` grid spawn per racer, a bare `0x0D`, the
   camera mode, one `0x131` item roll stream of 100 values per human, one `0x68` per remote
   racer placed from `track_spawn`.
4. Every human owes C2S `0x0D` scene loaded. GO waits for all of them or 30 s, then S2C `0x0D`
   and `0x3A`. A racer who leaves before GO no longer holds it.
5. The watchdog checks every 30 s and ends a race at 10 minutes. CPU cars arm on the racing
   line, a track with no line finishes them on timers.

## Motion

C2S `0x40` carries the own car only, the id comes from the socket. The body size picks the
packed or the raw world form. Each sample goes through:

| Judge | Code | Effect |
|-------|------|--------|
| per axis budget from the client clamp at 0x5a6a14 and 0x5a6a68 | `MotionPackets::motionStep` | a rescue within reach snaps, over budget adds a strike |
| distance over time | `DriftBoostPackets::checkSpeed` | a teleport or a speed over the ceiling adds a strike, a non finite float is dropped |
| drift and boost state bits | `DriftBoostPackets::observeState` | a boost with no drift charge, pad or item adds a strike |

Strikes bleed one per 3 s of clean race. Above 10 the racer is disconnected and reported to
`AntiCheatHandler` as critical, which writes `anticheat_logs` and bans the account and the
address for 24 hours.

The fan out runs every 100 ms from the 50 ms race tick: one S2C `0x40` per recipient without
its own id, chunked under the 8 KB client buffer, with an S2C `0x68` snap when a car starts,
warps or resumes. A car silent for 500 ms drops out of the fan out. S2C `0x45` standings go
out twice a second.

The waiting room also sends `0x40` for the kart on the room field, it is relayed with no
judge.

## Laps and ranks

C2S `0x41` checkpoint transitions feed `SpawnPackets::LapTracker` with the checkpoint count
of the track COL. A skipped or repeated checkpoint is refused, a full lap advances the lap
board with S2C `0x44`, the last lap calls the finish. C2S `0x67` is the client progress score,
used for the live rank and to log a client that runs a lap ahead of the server. The rank
order is finish time, then progress score, then lap, then height. C2S `0x68` respawn is
relayed to the others and resets the judges of that car.

## Items

| C2S | Server |
|-----|--------|
| `0x49` grant report, the client rolled it | mirrored in `ItemModel`, broadcast to the others |
| `0x47` use | 250 ms floor, S2C `0xC9` sound cue to the others, spawn echo for the kinds the receiver handles, a hazard for the CPU cars |
| `0x4B` homing launch, `0x5C` turtle | target must be in the roster, a CPU car target is hit by the server |
| `0x57` lock state | relayed to the target only |
| `0x69` hit report from the victim | broadcast for the codes the receivers handle |
| `0xCF` slot sync | mirrored and relayed |
| `0x5F` pet reached | relayed |
| `0xF2` ability class | 10 and 11 send the S2C `0xCD` shield token |
| `0xCD` ability ack | logged in a race, ignored outside |
| `0xCB` slot exchange, item 1000 | one use taken from `owned_item`, the stored count resent on `0x0115` when it differs |

`0x6A` is logged and not relayed, an S2C 0x6A would make the client drop every relayed 0x40.
`0x65` gauge, `0x125` overheat and `0xA1` prop hit are relayed, `0x121` waypoint is logged.

## Drift and boost

No opcode carries drift or boost. The server reads them from the state bits of each 0x40
sample, with the kart thresholds from the 0xC0 stats and the boost pads of the track COL,
and only judges. Remotes see the effect through the relayed motion.

## CPU cars

Bots are seats with ids from 900000. In a race they drive the racing line of the track, pick
boxes, fire items, drive into hazards and take hits (`RaceBots.cpp`, `tickBots`), their
motion rides the same fan out. They finish on their real time, never touch the database and
leave the room with the race.

## Finish and results

`completeFinish` ranks the racer, computes the reward from the reward rules
(`ResultsPackets::computeRewards`), then in one transaction adds gold, exp, a win or a loss,
a `race_history` row and a level up. The selected kart loses durability and its list is
resent. The racer gets S2C `0x3C` with the new totals, the others the finish animation, the
room the rank. The first finisher starts a 20 s rest counter after which the others retire.
When everybody finished `endRace` sends `0x39`, one `0x46` scoreboard per recipient and the
result board, drops the bots and returns the room to waiting. The room screen goes out again
when the board time is over.

A racer who leaves mid race (C2S `0x3B` or a disconnect) is removed, S2C `0xF0` despawns the
car on the other screens, and the wallet is docked the same way the client docks it.

## Open items

| Item | Why |
|------|-----|
| motion, items and hits are client authority | the server judges and mirrors, it runs no physics and cannot refuse a roll without desyncing the stock client |
| a track with no readable COL | no lap source, the race ends on the watchdog |
| `0xBF` roster rows at start | they land in the driver catalogue container of the client, the reference race start sends none, removing them needs a live check |
| 16 racer grids | the client array bounds for 16 cars are not reversed |
| a critical anti cheat hit bans the account and address | the ban grows with each earlier one, 10 min, 1 h, 3 h, 1 day, 7 days, then 30 days, a false positive still costs time |
| S2C `0x11F` race clock | the builder exists, the value encoding is not known so it is never sent |
