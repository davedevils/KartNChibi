# KnC server systems

The emulator is two processes. The login server (`server/login`) checks the account and
hands the client to a game server (`server/game`), which runs everything after that: lobby,
rooms, races, shop, garage, missions, licence, quest mode, ghosts and the messenger. Shared
code (sessions, packets, rooms, security, database) lives in `shared/src`.

The wire of every opcode is on its page in `docs/packets/opcodes/`. The pages below say what
the server does with it.

| Doc | Covers |
|-----|--------|
| [AUTH_NET_INFRA.md](AUTH_NET_INFRA.md) | frames, dispatch, login, redirect, session ticket, flood limits, database, threads |
| [LOBBY_ROOM_CHAT.md](LOBBY_ROOM_CHAT.md) | channel, lobby list, rooms, seats, teams, start rule, chat and messenger |
| [RACE_COMBAT.md](RACE_COMBAT.md) | race start, motion, laps, items, drift and boost judge, bots, results, anti cheat |
| [ECONOMY.md](ECONOMY.md) | shop, gifts, gacha, garage and inventory, car craft, room craft |
| [PROGRESSION.md](PROGRESSION.md) | missions, licence, quest mode, ghost records, profile and level |

## How a client frame is handled

1. `Session` reads 8 byte header frames. A header announcing more than 0x2000 bytes of
   payload closes the socket. A socket that sends nothing for 180 s is closed.
2. `PacketValidator::validate` drops opcode 0 and oversize frames.
3. A per socket `RateLimiter`, keyed on the full u16 opcode, drops floods.
4. Before `0x07` or `0xA7` binds an account, only the login frames pass
   (`PacketValidator::allowedBeforeAuth`).
5. `isRetiredC2S` drops frames no stock client sends whose old handlers paid gold or wrote
   state (`server/game/include/util/RetiredRoutes.h`).
6. The switch in `GameServerDispatch.cpp` routes on the full u16 opcode. An unknown opcode is
   logged with its payload, frames that carry a password or a token are never dumped.

The login server runs steps 1 to 3 and routes on the full opcode too.

## Security in one table

| Point | Where | State |
|-------|-------|-------|
| password login `0x07` action 4 | `HandshakeHandler::validateLogin` | PBKDF2, a 32 char launcher token only when the launcher store holds it for that user |
| test login `test` / `test` | same | only when the config sets `auth.allow_test_login`, off by default, the login server does not load `login.json` so it stays off |
| game server `0xA7` | `GameServer::handleSessionConfirm` | account and token must match an `active_sessions` row younger than 5 minutes, the row is then deleted |
| channel return `0xA7` on the login server | `HandshakeHandler::handleReauth` | account and token must match `login_ticket` inside one day |
| one account one game | `online_players`, `bindAccount` | a second socket on the account drops the older one |
| inter server key `0xF0` | both `main` and `LoginServer` | from `KNC_INTERNAL_KEY` or the config, empty or the old value refuses to start, the sample value logs a warning |
| chat money commands `/getmoney` `/getastro` `/getexp` | `SocialHandler::handleDevCommand` | off unless `KNC_DEV_COMMANDS=1` or the account is an operator |
| prices and rewards | shop, missions, licence, quest | the server reads its own rows, echoed client values are ignored |

## Open items

| Item | System | Why it is open |
|------|--------|----------------|
| race motion is client authority | race | the server judges each sample (`MotionPackets::motionStep`, `DriftBoostPackets::checkSpeed`) and kicks on repeated strikes, it runs no physics of its own |
| item rolls and hits are client authority | race | the client rolls boxes and reports hits, the server mirrors the slots in `ItemModel` and logs a mismatch, refusing would desync the stock client |
| a track with no readable COL has no lap source | race | laps come only from the 0x41 checkpoints, such a race ends on the 10 minute watchdog |
| `0xBF` roster rows at race start | race | they land in the client driver catalogue container, the reference race start has no 0xBF, removing them needs a live check |
| 16 racer grids | race | the client array bounds for 16 cars are not reversed, rooms of 16 exist in the speed modes |
| licence pass is trusted | progression | a 0xA3 for a key pays once per key, the server cannot see the test run |
| ghost record time is the client's | progression | the upload must be coherent (start, final lap, announced frames) and pays nothing, a forged time can still top the board |
| part wear on 0xCC | economy | the client lowers its part count itself, the server does not mirror it yet |
| gift claim `0x9A` | economy | still routed to the legacy item use, no claim answer is built |
| launcher `0xD0` | auth | it compares `session_<user>` with the 32 char token so it never passes, the launcher login goes through `0xFE` then `0x07` with the token |
| database on the io thread | infra | the login burst and the ghost submit run on the worker pool, the other handlers still query inline |
| channels | lobby | one game server per `servers` row, the channel id of `0x18` does not split rooms |
| sample inter server key | infra | `server/docker-compose.yml` and `env.example` ship a readable default, a public host must set its own |
| legacy handlers without a route | all | `ChatHandler`, the chat and list functions of `LobbyHandler` and the old `InventoryHandler` equip and sell paths are unreachable |

## The September catalogue

An earlier version of this page listed 72 rows, B01 to B72, from the docs of early September.
Almost all of it was already fixed when the sources were first tracked on 21 August
(`448a9f9e`, `0cbfc0d4`), the rest was fixed later or in the 24 September pass. Rows whose
claim never held are marked wrong.

| Row | Now | Where |
|-----|-----|-------|
| B01 motion on 0x31 | fixed | motion is C2S 0x40 `RaceHandler::handleMotion`, 0x31 route gone `4de1b222` |
| B02 test login | fixed | `40e3687a` removed the old token, `auth.allow_test_login` off |
| B03 validator and limiter | fixed | wired in 21 August, full opcode keys and the before login gate 24 September |
| B04 0xA7 trust | fixed | `f8f34e2f` token and account against `active_sessions` |
| B05 two blob layouts | fixed | 24 September, `ProfileBlob::writeWallet` in the login server |
| B06 B07 anti cheat validators and caps | fixed | `4de1b222` one judge from the client clamp |
| B08 0x39 per finisher | fixed | 21 August, results once at `endRace` |
| B09 B10 B35 mission list and ack | fixed | 21 August, 0x87 0x88 0x8F then 0x8C in the proven shape |
| B11 0x6F swap | wrong | 0x6F is the friend add, the shop answers on S2C 0xB7 |
| B12 0x9C stat index | fixed | `e369624e` 0x9C is the gift mark read |
| B13 team change | fixed | 0x64 in a room, `RoomHandler` gone `bbb26f50` |
| B14 room chat | fixed | 0xB4 goes to the room when the sender sits in one |
| B15 ghost gold | fixed | the submit pays nothing, the board time stays open |
| B16 scenario stars | fixed | 24 September, the 0xC7 to 0xCB stubs retired |
| B17 ghost blob | fixed | 21 August, 28 byte frames and a clamped count |
| B18 0x30 reply | wrong | no client sends C2S 0x30, 0x0130 is option 11 |
| B19 start rules | fixed | 24 September, `Room::startRefusal` |
| B20 8 bit dispatch | fixed | game 21 August, login 24 September |
| B21 blocking sleep | fixed | `d14a719a` |
| B22 grid space | fixed | grid index on 0x3E, remotes placed with 0x68 from `track_spawn` |
| B23 drift broadcasts | fixed | 21 August, drift and boost are observed only, dead stubs removed 24 September |
| B24 B25 per tab buy and currency | wrong | 0x70 to 0x7B are messenger opcodes, the buy is 0xB7 with a price row key |
| B26 repair | fixed | `b615c3ca` repair scroll on the owned kart |
| B27 install categories | fixed | `35f7c622` `InventoryHandler::handleInstall` |
| B28 gameMode14Full | fixed | 24 September, builder deleted with its dead callers |
| B29 0x3F meaning | fixed | 0x3F is the racer despawn, create and join send the member chain instead `b25b6f2c` `b6a73d6d` |
| B30 create opcode | wrong | C2S 0x2D is proven, 0x63 is the S2C popup |
| B31 invented opcodes | fixed | ghost `41fa6d34`, scenario 24 September |
| B32 roster opcode | open | see open items |
| B33 B34 0x3C and 0x46 | fixed | 21 August, `ResultsPackets` proven layouts |
| B36 B65 loot roll | fixed | `fb1000e4` the client rolls |
| B37 item ownership | open | see open items |
| B38 shield | wrong | the victim reports its own hit after its own shield check |
| B39 checkpoints | fixed | 21 August, `LapTracker` on 0x41 |
| B40 B59 B61 mission states claim and time | fixed | 24 September, the old `missions` tracker removed |
| B41 licence opcodes | fixed | 24 September, 0xA9 0xAA 0xAB 0xAC retired |
| B42 gacha pity | by design | no pity and no duplicate guard, the roll follows the weights of the gacha table |
| B43 0x0A layout | fixed | `docs/packets/PROFILE_BLOB.md` |
| B44 non transactional completion | fixed | the quest first clear flips in one guarded update |
| B45 host seat | fixed | one member chain for create and join `b25b6f2c` `b6a73d6d` |
| B46 channels | open | see open items |
| B47 schema check | fixed | 24 September, the health check names the live tables |
| B48 16 cap | open | see open items |
| B49 bot motion | fixed | `ac0c616a` |
| B50 sha256 login | fixed | `bbb26f50` |
| B51 database inline | open | see open items |
| B52 0x1B request | fixed | 24 September, retired |
| B53 customization | fixed | car craft save and rename `7ad02513`, dead handlers out `6177cd90` |
| B54 garage open | fixed | `7e6aea3f` |
| B55 0xB7 | wrong | 0xB7 is the shop buy and is wired |
| B56 0x7A 0x7B | wrong | block list opcodes |
| B57 shop exit | fixed | no C2S page is a shop exit, leaving goes through the screen requests, the stub went in `bbb26f50` |
| B58 12 byte codec | fixed | 24 September, removed |
| B60 B70 idle socket | fixed | 24 September, 180 s deadline |
| B62 mode names | fixed | `91f26647` |
| B63 heartbeat interval | fixed | 24 September, 0xA6 has no floor, one cap constant |
| B64 0xC5 | fixed | the invented ghost list route is gone |
| B66 thread pool | open | by design, one io thread plus the database pool |
| B67 internal key | fixed | `40e3687a` |
| B68 min lap time | fixed | 24 September, 0x36 route retired |
| B69 servers cleared twice | fixed | 24 September |
| B71 reconnect | wrong | `Database::getConnection` pings and reconnects |
| B72 whisper | fixed | block list and offline answer on 0xB5 |
