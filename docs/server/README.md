# Server

Server emulator for Kart N'Chibi. Two processes talk to one MariaDB database:
LoginServer and GameServer. A third process, WebAdmin, is optional.

## Tree

```
server/
  login/       LoginServer source
  game/        GameServer source
  web-admin/   WebAdmin source, opt in, needs OpenSSL
  lib/         Header only third party libraries, vendored
  scripts/     SQL migrations, numbered, run in order
  tools/       Dev tools, not part of the build
  data/        Reference capture used to compare the login burst
  dist/        Start scripts and config for a bare Windows run
```

## login/

Builds `LoginServer`. Accounts, launcher tokens, the channel list, the signup
page, redirect to the game server.

Port 50017 for the game protocol, port 8080 for the signup page.

Config lives in `server/login/config/`: `login.json` and `loginserver.ini`.

## game/

Builds `GameServer`. Everything after the redirect: lobby, rooms, races,
shop, quests, social.

Port 50018.

Config lives in `server/game/config/`: `game.json` and `gameserver.ini`.

## web-admin/

Builds `WebAdmin`. Admin panel. Opt in, needs OpenSSL to build, off by
default.

Port 8080. Only run one process on 8080 at a time, login and web-admin both
default to it.

Config lives in `server/web-admin/config/webadmin.json`.

## lib/

Header only libraries used by all three servers: asio, nlohmann json,
httplib, plus the shared `crypto/PasswordHash.h`. Vendored in the tree, no
fetch step needed to build.

## scripts/

SQL only. Files are numbered, for example `001_init.sql`. A fresh database
runs every file in order. An existing one runs only the files it has not
seen yet. Never edit an old numbered file, add a new one instead.

`099_default_admin.sql` always runs last and seeds the admin account. Change
its password before the server is reachable from outside.

## tools/

Not built, not part of any migration. Dev helpers only:
`download-deps.sh` / `.ps1` fetch build dependencies, `mitm_proxy.py` and
`packet_injector.py` and `trace_client.py` help inspect the wire protocol,
`setup_mariadb.bat` copies the MariaDB connector DLL on Windows.

## data/

`reference_login_burst.bin`, a captured login burst the game server can
compare its own output against.

## dist/

Start scripts and config for running the built `.exe` files directly on
Windows without Docker: `start.bat`, `start.sh`, `config/*.ini`.

## Database

MariaDB. Schema and seed data both live in `scripts/`, see above.

## Starting the servers

### Docker

```
docker compose -f server/docker-compose.yml up -d
```

Builds the image from `server/Dockerfile` and starts three containers:
mariadb, login-server, game-server. Settings come from `server/.env`, see
`server/.env.example` for every variable. MariaDB is only reachable inside
the compose network, its port is not published. `KNC_INTERNAL_KEY` and
`ADMIN_TOKEN` have no default, the servers refuse to start without them.
A migration that renames columns ships with the image built from the same
commit, never alone into a running package.

### Windows, no Docker

```
cmake -S server -B build-server -G "Visual Studio 17 2022" -A x64
cmake --build build-server --config Release
```

Binaries land in `build-server/bin/Release`. Copy them next to `server/dist/`
and run `start.bat`, or run `LoginServer.exe` and `GameServer.exe` directly.

## Protocol

Custom binary protocol, opcode based. See `docs/packets/` for the opcode
registry and the per screen documents. The server is the reference
implementation, the client rewrite follows it.

## Log level

`KNC_LOG_LEVEL=INFO` for a live server. `DEBUG` logs every packet both ways.

## Health check and the accept loop

The game server runs one asio loop on the main thread. Accepts, reads,
timers and every handler share that thread. A handler that blocks stops the
accept loop with it, and the kernel keeps the port in LISTEN, so a port probe
alone says healthy while the server takes no new client.

The loop refreshes a liveness stamp every five seconds and writes it to the
file named by `KNC_LIVENESS_FILE`, `/app/logs/gameserver.alive` in the image.
The container health check first probes the listening port, then reads the age
of that file, and fails when it is older than `KNC_LIVENESS_MAX_AGE`, 30
seconds by default. A frozen loop now turns the container unhealthy.

The loop also logs `accept loop alive, sessions N` once a minute.

### Rules that keep the loop turning

- Never call `Session::stop()` and expect the disconnect handler to run in
  line. It is posted to the loop. A handler may hold a lock the disconnect
  path takes again, and an in line call deadlocked the whole server.
- A handler that throws drops only that client. The catch sits at the loop
  level in `Session::dispatch` and in `GameServer::handlePacket`.
- An accept error never ends the loop. A descriptor shortage pauses a moment,
  every other error arms again at once. Only a closed acceptor stops it.

Covered by `tests/server/test_server_resilience.cpp`.
- Database work does not belong on the loop. `GameServer::postDb` runs it on a
  worker pool keyed by the session so a client frames stay in order, and
  `GameServer::postIo` hands the answer back. Only the loop may call
  `Session::send`. Which handler still blocks and in which order they should
  move is in [DB_OFFLOAD.md](DB_OFFLOAD.md).

## Anti cheat

Four judges run, all of them where the wire is.

- `MotionPackets::motionStep`, the per axis motion budget on C2S 0x0040, built
  on the client velocity clamp of 120 world units a second on x and y and 60 on
  z, counted in whole 100 ms send periods. A rescue up to 80 units snaps with no
  strike, a bigger jump accuses. See `docs/packets/opcodes/0x0040.md`.
- `DriftBoostPackets::checkSpeed`, the distance speed against the kart ceiling
  and a 300 unit single step teleport cap.
- `DriftBoostPackets::observeState`, the drift and boost state machine.
- `RateLimiter` on the packet rate and a lap time floor in `RaceHandler`.

Every strike goes into one leaky bucket per player that bleeds a point every
three seconds and kicks past ten.

`AntiCheatHandler` is the escalation ladder only: `reportViolation`, the
`anticheat_logs` insert and the ban. Its old `validatePosition`,
`validateSpeed`, `validateLapTime`, `validatePacketRate` and `validateItemUse`
were dead code with a second, ungrounded set of thresholds, 500 units a second,
a 100 unit teleport and a packet flood counter that duplicated the rate limiter.
They are deleted. The dead C2S 0x0031 `handlePosition` path carried a third set,
150 and 600, and is deleted with them. One judge per rule, where the wire is.
