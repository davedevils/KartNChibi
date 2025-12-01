# KnC Server Emulator

Emulator server for Kart n' Crazy  / Chibi Kart (This one tested)

## Structure

```
server/
├── cmake/              # CMake modules
├── common/             # Shared library (libknc-common)
│   ├── include/
│   │   ├── net/        # Packet, Session, Protocol
│   │   ├── game/       # Player, Vehicle, Item, Room
│   │   ├── security/   # RateLimiter, AntiCheat, BanManager
│   │   ├── logging/    # Logger, PacketLogger
│   │   ├── config/     # Config loader
│   │   └── db/         # Database (MariaDB)
│   └── src/
├── login-server/       # LoginServer (port 50017)
│   ├── include/handlers/
│   ├── src/handlers/
│   └── config/
├── game-server/        # GameServer (port 50018)
│   ├── include/handlers/
│   ├── src/handlers/
│   └── config/
├── web-admin/          # HTTP Admin Panel (port 8080)
│   ├── include/
│   ├── src/
│   └── static/         # HTML/CSS/JS
├── tests/              # Tests (optionnel, -DBUILD_TESTS=ON)
├── tools/              # CLI utilities
├── config/             # Shared config files
├── scripts/            # Build/deploy scripts
├── lib/                # Dependencies (header-only)
│   ├── asio/           # ASIO standalone
│   ├── json/           # nlohmann/json
│   └── httplib/        # cpp-httplib
└── logs/               # Runtime logs
```

## Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run tests
cd build && ctest --output-on-failure
```

## Docker

```bash
# Start MariaDB
docker-compose up -d

# Connect via Navicat/DBeaver:
# Host: localhost:3306
# User: knc / Pass: knc_password
# Database: knc_emu
```

## Protocol

See `../docs/packets/` there is a list of all packet i can have see on client side

## Ports

| Service | Port |
|---------|------|
| LoginServer | 50017 |
| GameServer | 50018 |
| Web Admin | 8080 |
| MariaDB | 3306 |

