# BOTS ASSESSMENT

Server side bots to fill a race grid up to 16 for solo and partial rooms.

Question answered here. Can server drive bots as normal players over verified opcodes. Yes.

Sources read. `engine/ai` certified packet docs under `docs/packets/` decompile `DevClient/KnC-new.exe.c` server `server/game/src/handlers/RaceHandler.cpp` `shared/src/include/game/Room.h`.

Tags. PROVEN means seen in certified packet doc or decompile or server code. INFERRED means reasoned not yet confirmed on wire.

---

## 0. Landed on 2026-09-11, the bots drive

Everything below this section is the assessment that led here. What shipped, in
`server/game/include/handlers/RaceBots.h`, `RaceBots.cpp` and the `armBots` `tickBots`
pair in `RaceHandler.cpp`:

| Need from section 3 | State |
|---|---|
| bot participant list in Room | `Room::addBots`, `bots()`, `getPlayerByCharacter` walks them too, without that the grid builder found no row and no 0x3E ever went out for a bot |
| inject bots into the roster | `handleStartRace` adds every bot to `m_racePlayers` and broadcasts its 0xBF |
| server bot tick | `tickBots` runs first inside `RaceHandler::tick`, every 50 ms off `m_raceTickTimer`, and writes a `MotionSlot` per bot so the 0x40 fan out at 100 ms carries it like a remote human |
| reserved ids | `kBotIdBase` 900000 plus the slot |
| DB exclusion | `finishBot` writes nothing, the reward path is `completeFinish` and only a session reaches it |
| grid placement | the bot starts on its `track_spawn` row, the same 0x68 the humans get, and aims at the nearest `follow_01.ini` node in front of the grid yaw |
| 0x31 space | moot, the live wire is 0x40 and its floats are the ini space, proven by the rescue predictor and the headless driving the same lines through the motion gate |

Pace is the line length over 46 seconds, clamped to 42 and 92 units per second, times
a per bot 0.90 to 1.06, under the 113 the motion gate allows. Laps count off distance
along the line so the grid offset never shortens the first one. Boxes come from
`itembox.ini` within six units, the bot holds a booster, spike, rocket, bomb or ice for
one to four seconds and then uses it on the wire: 0x49 grant, 0x47 spawn or 0x4B homing
at the nearest racer ahead with a 0x57 lock warning to a human target, 0xC9 sound cue.
A rocket or turtle a client fires at a bot lands from the server 1.5 or 2.5 seconds
later on 0x69, and a spike, bomb or ice on the road is a hazard any bot within three
units drives into. A rubber band eases a bot far ahead of the best human and pushes
one far behind. A track with no racing line keeps the clock finishers.

---

## 0b. Fixed on 2026-09-18, the seat and the anchor

Two reports from the package server. The bots "do anything" and "you see one bot in the room,
you start the race and there are two". Six causes, all server side.

| Bug | Cause | Fix |
|---|---|---|
| a human and a bot on one seat | `Room::findEmptySlot` scanned humans only and only slots 0 to 7 while `Room::addBots` scanned humans and bots over 0 to 15 | one allocator `nextFreeSlot` for both, it scans every seat and returns `kNoSlot` when the room is full |
| the room shows one car less than the race | guard 2 of `sub_40D650` drops a 0x21 whose slot is taken, the 0x3E grid spawn has no such guard, so the member hidden in the room still raced | the seat fix, plus one roster list |
| one socket seated twice | `Room::addPlayer` pushed the session again with no check, the race then spawned two cars for one player id | `addPlayer` refuses a session already in `m_players` |
| the race roster and the room member list were two lists | `handleStartRace` walked `room->sessions()` then `room->bots()` | it walks `room->participants()` once, the same list the 0x13 room screen draws, one 0xBF per seat |
| a bot could still be seated after the roster snapshot | the match tick seats one bot per wait while the room state is Waiting | `handleStartRace` sets Starting before it reads the seats, and the tick holds while the podium is up |
| the CPU car was drawn behind itself and jittered | `BotDriver::lookahead` sent an anchor a quarter second ahead clamped to 4 to 24 units, the client anchor is `pos + frameDt * 50 * velocity` and the receiver eases one fiftieth of that gap per tick | the anchor is `speed * 50 / 60` walked along the racing line so it also stays on the road |
| the bots always ranked ahead of the humans and never caught up | `BotDriver::progressScore` added one lap, the C2S 0x67 space is `laps * 5000 + bucket` | the extra lap is gone, so `score / 5000` is the completed laps on both sides |
| the bot car vanished for the ten second grid hold | `tickBots` returned early during `BOT_GO_HOLD_MS` so `MOTION_STALE_MS` 500 marked its slot stale | the hold refreshes the slot every tick, the car sits on the grid like a parked client |
| a bot held a team seat the team cap never counted | `handleTeamChange` counted `getPlayers` | it counts `participants` |

The line follower itself was clean. `tests/server/test_room_bots.cpp` drives a CPU car from grid
row 0 and row 7 of all 35 shipped race tracks, three laps each, and every one joins the line
inside 200 units, never leaves it, never steps more than the pace allows and finishes.

---

## 1. What existing design says about bots

The race controller design is engine side only. It is the offline single process demo not the network server.

| Topic | Design says | Source |
|-------|-------------|--------|
| Where bots live | an excluded engine demo BotKart struct not the server | race-controller-plan Task 8 |
| Representation | RacerState with `isBot=true` treated same as player for lap and rank | design AI Bot System |
| Spawn | place at start grid slot `playerCount + i` register racer isBot true | design Bot Spawning |
| Waypoints | follow proportional steering to `waypoints[currentWP]` ease throttle on corners | design Steering Algorithm |
| Difficulty | per bot `speedMultiplier` 0.85 or 1.0 or 1.15 also 0.90 0.95 1.00 variant | design BotController |
| Track data | follow_01.ini up to 4x400 pts start.ini grid regen.ini respawn NIF Zup to engine Yup | design Coordinate Conversion |
| AI module | `engine/ai` NavMesh NavQuery CrowdManager owner Unassigned | ai.md |

Key point. No server bot support exists anywhere. Design bots are local sim in the engine demo. `engine/ai` NavMesh path is unused the design bots skip NavMesh and just chase follow_ points. Server has zero bot code grep of `server/` for bot npc fake player finds nothing.

---

## 2. KEY QUESTION bot as normal player over verified opcodes

Answer PROVEN yes. Nothing on the wire distinguishes a bot from a player.

### Roster packets carry no bot flag

| CMD | Name | Handler | Fields client reads | Bot marker |
|-----|------|---------|---------------------|-----------|
| 0x21 | ROOM_PLAYER_INFO | sub_4797C0 | playerId name flag1 flag2 flag3 vehicle44 item56 extra60 | none |
| 0x3E | PLAYER_JOIN | sub_479D60 | playerId name vehicle44 item56 extra60 | none |
| 0x46 | LARGE_GAME_STATE | sub_47A760 | localId count then per entry playerId name flag1 flag2 stats | none |

Each handler reads fixed fields into a player entry. No branch on is this a bot. flag1 flag2 flag3 are unknown purpose bytes fully server authored so bot sets them same as a human. PROVEN handlers parse fixed layout. INFERRED safe the unknown flags are not a bot discriminator meaning unknown but server owns the value.

### Grid and race packets are id driven

| CMD | Name | Handler | Note |
|-----|------|---------|------|
| 0x40 | START_RACE grid | sub_47FD30 | per entry playerId packed pos packed rot looks up id via sub_48DEA0 applies only if id already in roster |
| 0x31 | POSITION | sub_479950 | reads 3 int32 id x y server drives bot with same call |
| 0x35 | SCORE lap progress | sub_479C20 | playerId score used for lap broadcast |
| 0x39 | FINISH | sub_479CB0 | zero bytes ui trigger only |
| 0x3A | RESULTS | sub_47AE00 | 4 byte result code |
| 0x46 | scoreboard | sub_47A760 | per row rank playerId name time |

Grid handler binds each entry to an already known player by id. PROVEN `idx = sub_48DEA0(byte_1B19090, playerId); if (idx >= 0) init`. So a bot must be added to the client roster first via 0x3E or 0x21 or 0x46 with a unique id then the grid entry sticks.

Server outbound 0x31 is `[id, x, y]` three int32 PROVEN `PacketBuilder::position` writes id then int32 x then int32 y matching sub_479950. This is the exact packet the server already relays for a real remote player so a bot 0x31 is byte identical to a human 0x31.

### Conclusion

The cheapest correct path is real. Server spawns fake roster entries drive their 0x31 and lap and finish and results. Client sees a normal opponent. Reuse of verified opcodes needs no new wire.

---

## 3. Server side support needed

Current server race path proven from `RaceHandler.cpp`. Server only relays inbound client 0x31 it has no position sim. Roster is built from `room->sessions()` real connected clients only. Grid 0x40 is a staggered placeholder start.ini not loaded see comment 0x40 grid staggered placeholder until track start line reversed.

| Need | Why | State |
|------|-----|-------|
| Bot participant list in Room | Room is 100 percent session keyed `m_sessions` and `m_players` by sessionId bots have no Session | NEW must add non session participants that feed roster and isFull and grid |
| Inject bots into race roster | `handleStartRace` loops `room->sessions()` to build `m_racePlayers` bots skipped | NEW add bots to `m_racePlayers` at start |
| Server bot tick | no per frame sim exists positions are relayed only | NEW asio steady_timer 10 to 20 Hz advance bots on waypoints broadcast 0x31 pattern exists see startCountdown and scheduleRaceWatchdog timers |
| Reserved bot ids | wire playerId is int32 must not collide with DB character id | NEW pick negative or high offset id range |
| DB exclusion | finish path writes stats race_history level by characterId | NEW bots must skip all DB writes and reward inserts |
| Grid placement | 0x40 grid is placeholder start.ini not loaded | NEW load start line or reuse placeholder for MVP |
| Lift 16 cap | `RoomSettings.maxPlayers = 8` slot uint8 0 to 7 | NEW raise to 16 slot 0 to 15 wire supports it MSG_MAX_ROOM_USER_16 exists grid is 1 plus 25 times n |

Broadcast needs no change. `room->broadcast` sends to sessions. Bots never receive packets they are pure server side actors so only the humans get the bot 0x31 and lap and finish.

Bot roster entry fields to fake. id name vehicleTemplateId driverId slot team. RoomPlayer already has these fields so a bot is a RoomPlayer plus a RacePlayer minus the Session.

---

## 4. Track data dependency

### Waypoints are reversed and are plain ini not rep

PROVEN by decompile sprintf paths in `DevClient/KnC-new.exe.c`.

| File | Path | Line | Purpose |
|------|------|------|---------|
| follow_%02d.ini | ./Data/Public/World/{world}/{track}/follow_01..04.ini | 231057 | up to 4 waypoint lists x y z angle NIF space |
| start.ini | ./Data/Public/World/{world}/{track}/start.ini | 231789 | grid slots 16 lines |
| regen.ini | ./Data/Public/World/{world}/{track}/regen.ini | 231832 | respawn points |

Client load and lap logic reversed. sub_489730 load follow. sub_489890 find closest waypoint. sub_489860 increment index wrap to lap. Format is plain text x y z angle documented in race-controller-design.

The .rep files are NOT track data they are ghost or replay. PROVEN License_Track_11.rep ghost.rep at lines 154574 161638 unrelated to bots skip them.

### Ready vs needs reverse

| Item | State |
|------|-------|
| follow_ start regen ini format | READY reversed and documented plain text |
| client side waypoint follow and lap wrap | READY reversed sub_489730 sub_489890 sub_489860 |
| NIF Zup to engine Yup conversion | READY in engine GimmickManager |
| server ingest of follow_ per track | NEEDS WORK server loads no track geometry today ship the ini or bake a table per map |
| mapId to {world}/{track} folder mapping | NEEDS REVERSE mapId is uint8 the sprintf uses string world and track names mapping table unknown |
| 0x31 coordinate encoding vs ini space | NEEDS VALIDATION 0x31 carries int32 x y only two coords the ini is NIF x y z bots must emit x y in the same space and scale a real client sends confirm by capturing a real 0x31 stream on a known track |

The 0x31 space is the one real blocker for realistic pathing. Relaying real client 0x31 works today because bytes pass through untouched. Generating plausible bot x y needs the map from follow_ NIF coords to the 0x31 int encoding which is not yet proven.

---

## 5. Phased plan

### MVP bots as fake players

Goal fill grid to 16 opponents that move finish and score. Path parity not perfect.

1. Raise maxPlayers to 16 slot 0 to 15.
2. Add bot participant list to Room feed roster 0x3E or 0x21 or 0x46 with reserved ids and fake name vehicle driver slot.
3. On start inject bots into `m_racePlayers` place on grid 0x40 alongside humans.
4. Server tick asio timer advance each bot along start.ini and follow_ points by simple time based interpolation broadcast 0x31 each tick.
5. On bot cross line emit 0x35 lap and on final lap 0x39 finish and feed the bot row into 0x3A and 0x46 results.
6. Exclude bots from all DB writes and reward inserts.

Ready now. opcodes verified. `PacketBuilder::position` and lapInfo and finish and resultsScoreboard exist. asio timer pattern exists. waypoint format known.
Needs work first. server track ingest and mapId to folder table and 0x31 coordinate calibration.

### Better

| Feature | Note | State |
|---------|------|-------|
| Difficulty | per bot speed and skill and corner ease reuse design speedMultiplier | ready to code |
| Item use | bots throw items server already drives item wire 0x45 announce and 0x47 or 0x4B effect see RaceHandler item path | ready reuse existing item broadcast |
| Rubber band catch up | scale bot speed to nearest human | ready to code |
| Smooth pathing | true follow_ chase with NIF to 0x31 map | blocked on 0x31 space validation |
| Team modes | bots fill team slots RoomPlayer has team field | ready to code |

---

## Return summary

| Question | Answer |
|----------|--------|
| Bot as player feasible | YES PROVEN roster packets 0x21 0x3E 0x46 carry no bot flag grid 0x40 binds by id 0x31 finish results all id driven client cannot tell |
| Cheapest path | fake roster entry plus server driven 0x31 lap finish results reuse verified opcodes no new wire |
| Server work | non session bot list in Room inject into race roster server sim tick reserved ids DB exclusion raise 8 to 16 |
| Track dependency | follow_ start regen ini plain text reversed NOT rep server does not load them yet needs ingest plus mapId to folder table plus 0x31 coord calibration |

### Open unknowns

| Unknown | Risk | Action |
|---------|------|--------|
| 0x31 int x y space and scale vs follow_ NIF coords | HIGH blocks realistic bot pathing | capture real client 0x31 on a known track compare to ini |
| mapId to {world}/{track} name mapping | MED needed to pick the right ini per room | enumerate world folders map to mapId |
| Client roster array cap for 16 | MED 0x46 rebuilds a player list 0xBF per player broadcast may assume 8 | check client array bounds sub_47A760 and sub_47F390 before shipping 16 |
| flag1 flag2 flag3 meaning in 0x21 and 0x46 | LOW server owns them set same as human | leave identical to human roster |
| Bot with no inbound packets | LOW remote humans are also server relayed so likely fine | confirm client keeps a roster player that never self reports |
