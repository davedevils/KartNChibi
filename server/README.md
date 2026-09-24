# Server

The private server for Kart n' Crazy / Chibi Kart. Two processes and a database.

| Part | What it does | Port |
|---|---|---|
| `login/` | Accounts, launcher tokens, channel list, signup page, redirect to the game server | 50017, web 8080 |
| `game/` | Everything after the redirect, lobby, rooms, races, shop, quests, social | 50018 |
| `web-admin/` | Admin panel, opt in, needs OpenSSL | 8080 |
| `lib/` | Header only libraries, asio, nlohmann json, httplib | |
| `scripts/` | SQL, applied in name order on a fresh database, then by the launcher for new ones | |
| `tools/` | Dev tools, dependency fetch, MITM proxy, packet trace, not part of the build | |
| `data/` | Reference capture the game server compares its login burst against | |
| `dist/` | Start scripts for a bare Windows run without Docker | |

Shared code, packets, sessions, database and logging, lives in `../shared/src` and builds as `knc-common`.

## Run with Docker

```
docker compose up -d
docker compose logs -f
```

Settings come from `.env`, see `.env.example`. MariaDB is only reachable inside the compose network.

## Build on Windows

```
cmake -S server -B build-server -G "Visual Studio 17 2022" -A x64
cmake --build build-server --config Release
```

Links `thirdparty/mariadb-connector-c`. Binaries in `build-server/bin/Release`.

## Database

The SQL under `scripts/` is the whole schema and content. A fresh database runs every file. An existing one runs the files it has not seen, recorded in `schema_migrations`. Write new content as a new numbered file, never edit an old one.

A migration that renames columns (056 is one) must ship together with the image built from the same commit. The old binary reads the old names, so copy a new migration into a running package only when the new image goes with it.

## Secrets

`KNC_INTERNAL_KEY` (login to game server registration) and `ADMIN_TOKEN` (admin HTTP API) have no default. The servers refuse to start when one is missing or still the old sample value. Set both in `.env`, the compose file forwards them.

## Protocol

`../docs/packets/` has the opcode registry and the per screen documents. The server is the reference implementation, the client rewrite follows it.

## Log level

`KNC_LOG_LEVEL=INFO` for a live server. `DEBUG` logs every packet both ways, useful with the client's `packets.log` when something breaks.
