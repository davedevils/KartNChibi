# Progression

Missions, licence tests, quest mode, quests, ghost records, levels, pendants and the profile
numbers on the game server. Wire layouts are on the opcode pages in `docs/packets/opcodes/`,
the profile blob in `docs/packets/PROFILE_BLOB.md`, quest mode in
`docs/packets/systems/scenario.md`, missions in `docs/packets/systems/mission.md`.

## Source files

| Area | File |
|------|------|
| missions | `server/game/src/handlers/MissionHandler.cpp`, `packets/gen/MissionPackets.cpp` |
| licence | `server/game/src/handlers/LicenseHandler.cpp` |
| quest mode stages 22 and 17 | `server/game/src/handlers/ScenarioHandler.cpp`, `packets/gen/ScenarioPackets.cpp` |
| quests | `server/game/src/handlers/QuestHandler.cpp`, `packets/gen/QuestPackets.cpp` |
| ghost records and replays | `server/game/src/handlers/GhostHandler.cpp`, `packets/gen/GhostPackets.cpp` |
| levels, awards, pendants, profile refresh | `server/game/src/handlers/ProgressionHandler.cpp`, `packets/gen/ProgressionPackets.cpp` |
| level curve and pendant rules | `server/game/include/util/LevelCurve.h`, `PendantRules.h` |

## Rule

Every reward comes from a server row: `mission_def`, `license_test_def`, the quest mode
definitions, the reward rules of a race. A value the client echoes is logged and ignored. A
reward pays once where the stock game pays once, the flag flips before or with the payout.

## Missions

| C2S | Server |
|-----|--------|
| `0x8F` menu open | the `0x87` definitions once per session, the `0x88` progress rows up to the first one not cleared, the `0x8F` ack that opens stage 24. A run left half way is dropped |
| `0x90` start | the definition must exist and the row must be playable, the entry fee is taken until the first clear, the run is stored in `char_mission_state` with its deadline, S2C `0x90` with the gold opens stage 25 |
| `0x8C` finish | only the run this server started pays, repeats of the same frame are dropped, the first clear pays the mileage as gold and the exp and the item reward, a repeat pays nothing, S2C `0x8C` then the profile refresh, earned pendants and the rows the clear opened |
| `0x8E` | the tail of every screen init, no answer |

The older tracker on the `missions` and `mission_progress` tables (race and item counters, a
claim path, a time mission) had no client route and was removed on 24 September.

## Licence

| C2S | Server |
|-----|--------|
| `0x16` screen open | the test definitions, the progress rows, a grade the player earned but never got with its `0xA4`, then the ack |
| `0x62` test start | S2C `0x62` and the camera, the run is client side |
| `0xA3` test pass | the definition row sets exp and gold, the first pass of a key pays, a pendant reward (type 7) is granted, the grade is the number of whole bands passed, a new grade sends `0xA4` |

C2S `0xA9`, `0xAB`, `0xAC` and the 12 byte `0xAA` have no client sender. Their old handlers
paid gold and wrote the licence class on every send and were retired on 24 September, see
`RetiredRoutes.h`.

## Quest mode

The `0xF3` definitions ride the login burst with the catalogues, the `0xF4` rows of the
character follow.

| C2S | Server |
|-----|--------|
| `0x011C` menu open | `0xF4` rows then S2C `0x011C` pushes stage 22 |
| `0xF5` start | the level gate and the fee, the rival ghost frames on `0xAE` and `0xAF`, S2C `0xF5` opens stage 17 |
| `0xF8` result | always answered or the client waits in state 2013. Success needs the run this server started and a race time above the minimum, no longer than the wall clock since the start, and under the rival and the goal. The first clear flips `completed` in one guarded update then pays exp, gold and the item tail |

The older `0xC7` to `0xCD` scenario routes had no client sender, the 16 byte `0xCB` among them
paid gold from a star count the client chose. They were retired on 24 September. C2S `0xCB`
of 29 bytes is the slot exchange item, `0xCC` the part use notify, `0xCD` the ability ack, see
[RACE_COMBAT.md](RACE_COMBAT.md) and [ECONOMY.md](ECONOMY.md).

## Quests

The unlocked quest catalogue and the player state go out at character enter. C2S `0xFE`
accepts, `0x0100` discards, `0x0102` reports progress for quests 0 to 3, each answered only on
success. S2C `0xFB` publishes a theme step.

## Ghost records

| C2S | Server |
|-----|--------|
| `0x2C` kind 1 sub 0, `0x011D` | the record board, dripped since it is about 30 KB |
| `0xAA` 4 bytes | ghost enter for a track, the best record replay goes out first then S2C `0xAA` |
| `0xB1`, `0xB2` empty | stage begin, final lap |
| `0xAE` count, `0xAF` chunks | the upload, 28 byte frames, the count clamped |
| `0xB0` 12 bytes | submit on the worker pool: the upload must be coherent (begin, final lap, announced frames), the record is stored when it beats the player best, the answer carries the rank and the top rows |

A ghost submit pays nothing.

## Levels and profile

`level_curve` gives the exp floor and the next level, `ProgressionHandler::levelForExp` the
level. `awardAndPush` and `applyAward` add exp and gold, move the level, and push S2C `0x0A`
(38 bytes, the profile tail from +0x4A0), the `0x0135` level banner and the pendants earned.
An award for a player who is offline is queued in `progression_grant` and drained on the next
login or wallet poll. The 1224 byte blob of `0x07` and `0xA7` uses the same offsets on the
login and the game server, `ProfileBlob.h`.

## Pendants

Thirteen pendant definitions and the owned ones go out at login. The level, the race count
and the rookie grade earn them (`grantEarnedPendants`), a new one goes out on S2C `0x011B`.
C2S `0x0123` wears an owned pendant or takes it off, the key rides the profile blob at
+0x4C4 and the room member row.

## Open items

| Item | Why |
|------|-----|
| a licence pass is trusted | the test runs on the client, a `0xA3` for a key pays once per key |
| the ghost record time is the client's | the upload is checked for coherence only, no currency rides it |
