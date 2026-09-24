# Auth, network and infrastructure

What the login and game servers do from the TCP accept to a bound account, plus the frame
format, flood limits, database and threads. The wire of each opcode is on its page in
`docs/packets/opcodes/`, the profile blob in `docs/packets/PROFILE_BLOB.md`.

## Source files

| Area | File |
|------|------|
| login accept, dispatch, launcher, registration | `server/login/src/LoginServer.cpp` |
| login `0xFA` `0x07` `0xA7` `0x18`, redirect | `server/login/src/handlers/HandshakeHandler.cpp` |
| login token | `server/login/src/handlers/AuthHandler.cpp` `generateToken` |
| game accept, timers, rate config | `server/game/src/GameServerBootstrap.cpp` |
| game dispatch | `server/game/src/GameServerDispatch.cpp` |
| game `0x07` `0xA7`, bind, rooms | `server/game/src/GameServerRoomHandlers.cpp` |
| login burst after the bind | `server/game/src/GameServerLoginBurst.cpp` |
| game boot, health check, registration | `server/game/src/main.cpp` |
| session, frames, idle deadline | `shared/src/src/net/Session.cpp` |
| frame gates | `shared/src/src/security/PacketValidator.cpp`, `RateLimiter.cpp` |
| retired C2S frames | `server/game/include/util/RetiredRoutes.h` |
| launcher token rule | `shared/src/include/security/LoginToken.h` |
| profile blob offsets | `shared/src/include/net/ProfileBlob.h` |
| database | `shared/src/src/db/Database.cpp`, `DbWorkerPool.cpp` |

## Frames

Every frame is an 8 byte header then the payload: u16 payload size, u16 opcode, u32 pad,
little endian. The client never sends more than 0x2000 bytes in one frame (the room craft
save stops at 170 records for that reason), so `Session::parseFrames` closes a socket whose
header announces more than `PACKET_MAX_C2S_PAYLOAD` (0x2000) and `PacketValidator::validate`
refuses the same. The 12 byte header with a `K` magic that older builds accepted is gone:
the stock client never sent it, a first frame of 75 bytes starts with 0x4B and was taken for
it, and its size sum could wrap.

## Dispatch

Both servers switch on the full u16 opcode. The game server runs these gates first, in order:
validator, per socket rate limiter, the before login list, the retired list. See the steps in
[README.md](README.md). Unknown opcodes are logged with their payload and classed as echo,
swallowed or unknown by the tables in the default branch. The payload of `0x07`, `0xA7`,
`0xFE`, `0xD0` and `0xF0` is never dumped (`PacketValidator::payloadIsSecret`).

Before login the game server accepts only `0xA6`, `0x07`, `0xFA`, `0xD0`, `0xA7`, `0x0B` and
`0x0130`.

## Login

| Step | Frame | Server |
|------|-------|--------|
| 1 | C2S `0xFA` | login answers `0x0B` |
| 2 | C2S `0x07` action 4, account and password | `validateLogin`, PBKDF2 against `accounts.password_hash`, a legacy sha256 hash is upgraded on success, banned accounts refused |
| 3 | S2C `0x07` | 4 byte character id then the 1224 byte profile blob |
| 4 | S2C `0x02` code 1 then `0x0E` | channel list from the `servers` rows that registered |
| 5 | C2S `0x18` screen 0x0E | `sendServerRedirect` writes `active_sessions` then S2C `0x54` |
| 6 | C2S `0xA7` on the game server | `handleSessionConfirm` checks the ticket, binds, sends the login burst |

The blob head carries the ticket: the account id at +0x000 and the 32 char token at +0x004
(`ProfileBlob::writeTicket`). The client echoes both on every C2S `0xA7`. The login server
stores the same pair in `login_ticket`. The wallet goes at the offsets the client reads, exp
at +0x4A4, astro at +0x4A8, gold at +0x4AC, level at +0x4A1
(`ProfileBlob::writeWallet`). The game server `0xA7` blob keeps the same ticket head so a
later channel return still resolves.

A password of 32 letters and digits is tried as a launcher token first. It passes only when
`LoginServer::validateToken` holds that exact token for that account name
(`launcherTokenAccepted`), otherwise the PBKDF2 check decides. Before 24 September any such
password logged into any existing account.

`test` / `test` logs in as account 1 only when the config sets `auth.allow_test_login`. The
code default is off and the login server reads `loginserver.ini`, it never loads
`login.json`, so the flag stays off on a stock start. `auth.auto_create_accounts` is in the
same state.

A login is refused while `online_players` shows the account in a game with a heartbeat
younger than 90 s.

### Game server bind

`handleSessionConfirm` needs a token and an account id in the frame, then an
`active_sessions` row with that token and account younger than 5 minutes. It deletes every
row of the account, sets the session token and calls `bindAccount`, which drops any older
socket on the same account and writes `online_players`. The login burst then runs on the
database worker pool (`sendPlayerData`). A tool that skipped the login server can send
`0x07` to the game server, it is checked with PBKDF2 the same way.

### Channel return

C2S `0x19` on the game server answers S2C `0x19` with the login host and mode 4. The client
reconnects to the login server and sends `0xA7` with the blob head. `handleReauth` matches
account and token against `login_ticket` inside one day and answers `0xA7`, `0x02` and the
`0x0E` channel list. An unknown ticket answers `MSG_REINPUT_IDPASS` and closes after the send.

### Launcher mode

`KnCLauncher` sends C2S `0xFE` with the account and password. `handleLauncherLogin` checks
them with PBKDF2 and answers S2C `0xFE` with a 32 char token, kept in memory per account name
for 30 minutes. The launcher starts the client with that name and token, the client skips its
login screen and sends `0x07` action 4 with the token as the password, which passes
`launcherTokenAccepted`. The other launcher frame, `0xD0` with a wide `session_<name>`, is
compared with the stored token and never matches, nothing depends on it.

## Sessions

`Session` owns one socket. A read restarts the idle deadline, 180 s of silence
(`SESSION_IDLE_LIMIT_SEC`) closes it, the stock client sends `0xA6` every second. The
disconnect handler is posted to the loop, never run inside a packet handler. A handler that
throws drops that one client. `closeAfterSend` lets a refusal text reach the client before
the close.

`GameServer::onDisconnect` clears the presence row of that socket, the per session caches,
the anti cheat counter, leaves the room with race bookkeeping and removes rooms left empty.

## Heartbeat

C2S `0xA6` gets no answer, an S2C `0x12` would move the client to the lobby. The game server
refreshes `online_players.last_seen` at most once a minute from it. It has no rate floor.

## Flood limits

`RateLimiter` per socket, keyed on the full u16 opcode, a global cap of
`RateLimit::GLOBAL_MAX_PACKETS_SEC` (400) frames per second and a 50 ms floor per opcode.

| Server | Opcodes without a floor | Longer floor |
|--------|------------------------|--------------|
| game | `0x40` motion, `0xA6`, `0x0B`, `0x67`, `0x41`, replay count and chunk, `0xB9`, `0xBA`, `0x0D` | chat `0xB4` and whisper 200 ms |
| login | `0xA6` | `0x07` and `0xFE` 1 s, each try runs PBKDF2 |

A failed login closes the socket, so password guessing costs a new connection per try. There
is no per address limit.

## Server registration

The game server sends `0xF0` with the inter server key, its id, name, host, port and size to
the login server at boot and every 60 s after. The key comes from `KNC_INTERNAL_KEY` or the
config. Both servers refuse to start when it is empty or still the old leaked value and log a
warning when it is the sample value of `env.example` and `server/docker-compose.yml`. The
login server clears the `servers` table once at start.

## Database

`Database` keeps a MariaDB connection pool, prepared statements for every user input, a
transaction object with rollback on drop, and `getConnection` pings and reconnects a dropped
connection. The game server checks at boot that the tables its first paths read exist
(`checkDatabaseHealth`), the schema itself comes from the migrations in `server/scripts`,
run in name order by the image.

## Threads

One io thread per server. The game server adds a database worker pool of 4 threads
(`KNC_DB_THREADS`, 0 keeps everything on the io thread) used by the login burst and the ghost
submit, a 50 ms race tick, a 5 s match tick that seats bots, and a 5 s liveness stamp
(`KNC_LIVENESS_FILE`). Other handlers still query the database on the io thread.

## Open items

| Item | Why |
|------|-----|
| launcher `0xD0` never passes | the string compared is not the stored token, the launcher login goes through `0x07` |
| database on the io thread | only the login burst and the ghost submit use the pool |
| no per address login limit | each try costs a connection and a one second floor |
| sample inter server key | the compose default is readable, a public host must set its own |
| `login.json` and `game.json` | copied next to the binaries and never loaded, the ini files are the config |
