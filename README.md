# Kart N'Chibi

[![Educational](https://img.shields.io/badge/Purpose-Educational-blue.svg)](https://github.com/davedevils/KartNChibi)
[![License](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](LICENSE.md)
[![Status](https://img.shields.io/badge/Status-Active%20Development-green.svg)](https://github.com/davedevils/KartNChibi)

> **Educational reverse engineering and game development project focused on understanding Kart N' Crasy / Chibi Kart architecture, network protocols, and file formats.**

---

## ⚠️ Educational Purpose Only

**This project is for EDUCATIONAL and RESEARCH purposes ONLY.**

By using this code, you acknowledge and agree that:
- The author is **NOT responsible** for any misuse, damage, loss of data, or consequences
- You must ensure compliance with all applicable laws and regulations
- This does NOT endorse illegal activity or ToS violations
- Use this code **responsibly and ethically**

**This is a learning project about KnC/Chibi Kart game networking, reverse engineering, and MMO architecture**

---

## 📖 About This Project

Kart N'Chibi is an educational project exploring the architecture of Kart n' Crazy, also known as Chibi Kart, a 2010 kart racer built on Gamebryo. The project includes:

- 🔓 **Full protocol documentation** (all 309 opcodes reversed and documented)
- 🎮 **Server emulator** that works with the stock client end to end: login, character creation, licence tests, lobby, rooms, item and speed races, shop, gacha, quests, missions, ghosts, room craft, car craft, notes, clans, and CPU cars in the empty seats
- 💻 **Engine and client rewrite** on a bgfx render stack with its own NIF reader, the client races, equips and buys on the real server with a 1 to 1 port of the stock client physics
- 🛠️ **Development tools** (headless client, packet spy dll, MITM proxy, packet inspector, PAK manager, model/map/UI viewers, NIF checks, scene viewer)
- 📚 **Full documentation** of the game's architecture, protocol and file formats

### Why This Project?

This educational project aims to:
1. **Learn** MMO game architecture and networking
2. **Understand** game protocols through reverse engineering
3. **Document** file formats and network communication
4. **Provide** a learning resource for game development students

---

## 📂 Project Structure

```
📦 Kart N'Chibi
├── 📁 client/              # Client rewrite on glfw and bgfx, creation, lobby, room, race, garage, shop, missions, messenger, sound on the real server, the stock look, --state opens any screen offline
│   ├── app/                # main, the frame loop, the screen stack, the options
│   ├── net/                # The netcode library KnCClientNet, the headless links it too
│   ├── ui/ assets/         # Sprite batch, font, JSON screens, widgets, pak and Data textures
│   ├── race/               # Track data, the race sim on the physics port, the wire, the view, the HUD
│   └── screens/            # Logo, login, channel, menu, lobby, room, race, garage, shop
│
├── 📁 server/               # Server emulator
│   ├── login/               # Login server (auth, session, ticket handoff)
│   ├── game/                # Game server (race simulation, CPU cars)
│   ├── web-admin/           # Admin panel
│   └── scripts/             # SQL migrations, server launcher
│
├── 📁 engine/               # C++ engine, only the bgfx stack ships here
│   ├── formats/             # NIF/KF/KFM reader and the glTF writer (KnCFormats)
│   └── render/               # bgfx scene renderer (KnCRender)
│
├── 📁 games/kart/            # The kart game layer
│   └── physics/client/       # 1 to 1 port of the client's own physics, no engine dependency
│
├── 📁 shared/                # Shared library (networking, security, database)
├── 📁 tools/                 # Development tools
│   ├── headless/             # Headless client, logs in and races
│   ├── clientpatch/          # Client patch dll (packet spy, cwd fix)
│   ├── mitm/ probe/ proxy_dll/ network_decrypt/   # Wire capture and client hooks
│   ├── pak_tool/             # PAK archive list, extract, pack
│   ├── col_tool/             # Track collision dump
│   ├── model_viewer/ map_viewer/ track_scene/   # Viewers on the new bgfx renderer
│   ├── replay/               # Ghost replay reader and dump
│   ├── nif/                  # NIF reader checks
│   └── ui_editor/            # UI layout editor (new bgfx render stack)
│
├── 📁 docs/                  # Complete documentation
│   ├── packets/               # Network protocol, all 309 opcodes
│   ├── reverse/                # Reverse engineering guides, client physics map
│   ├── engine/                  # Engine subsystem docs
│   ├── formats/                  # File format specs (NIF, KF, DDS, PAK)
│   ├── server/                    # Server layout and content guide
│   ├── client/                     # Stock client states and launch arguments
│   ├── tools/                       # Tools and UI file format
│   ├── conventions/                  # Coding standards, testing rules
│
├── 📁 tests/                # Server gtest suite and shared library tests, the physics port keeps its own tests next to its source
└── 📁 thirdparty/           # bgfx, bimg, bx submodules, glfw, mariadb-connector-c, and others
```

---

## 🚀 Quick Start

### Prerequisites
- **Windows** 10 or 11, x64
- **Visual Studio 2022**, any edition (C++ Desktop Development, toolset v143)
- **CMake 3.21+**
- **Docker Desktop** for the server image
- **Git**

### Build

```bash
# Clone the repository
git clone https://github.com/davedevils/KartNChibi.git
cd KartNChibi
git submodule update --init --recursive

# Engine, tools and tests
cmake --preset vs2022
cmake --build --preset vs2022

# Servers are their own CMake project
cmake -S server -B build-server -G "Visual Studio 17 2022" -A x64
cmake --build build-server --config Release
```

Engine executables land in `release/`, server binaries in `build-server/bin/Release/`.

### Run

The easiest way is the release package: the server image with a launcher script, and the clone exe to drop into your own client folder.

```bash
# From source, with Docker
cd server
docker compose up -d
```

Login on 50017, game on 50018, signup page on 8080. Chat commands `/getmoney`, `/getastro`, `/getexp` are available for a local server.

Run the client clone against your own copy of the game data:

```bash
release\knc_client.exe --game D:/path/to/your/client --host 127.0.0.1 --port 50017
```

`--game` points at a folder with the original game's `Data/` and PAK files, not included in this repository, see the disclaimer above. This repository ships only the `Data/Public/UI/*.json` layout files the client reads, reverse engineered coordinates and IDs, not copyrighted game assets.

📖 **Full build guide:** [BUILD.md](BUILD.md)

---

## 📚 Documentation

| Document | Description |
|----------|-------------|
| [📖 BUILD.md](BUILD.md) | Complete build instructions |
| [📡 Protocol Documentation](docs/packets/) | All 309 opcodes reversed and documented |
| [🏗️ Engine Docs](docs/engine/) | The bgfx render module, the only engine stack in this repository |
| [📦 File Formats](docs/formats/) | NIF, KF, DDS, PAK and other formats |
| [🔍 Reverse Engineering](docs/reverse/) | IDA/Ghidra guides, client physics map |
| [🎨 UI JSON Schema](docs/tools/UI_JSON_SCHEMA.md) | The layout format `client/ui/` reads and `tools/ui_editor` writes |

### 🌟 Highlights

- **[Packet Registry](docs/packets/PACKET_REGISTRY.md)** - Authoritative opcode table
- **[Client Physics Map](docs/reverse/CLIENT_PHYSICS_MAP.md)** - Ground for the 1:1 physics port, plus eleven files under `docs/reverse/physics/`
- **[Render Module](docs/engine/RENDER_MODULE.md)** - The new bgfx renderer and NIF reader

---

## 🛠️ Development Tools

| Tool | Description | Usage |
|------|-------------|-------|
| `tools/headless` | Headless client | Logs in, races, drives a line, replays ghosts, tests the server without a game window, on the `client/net` library |
| `tools/clientpatch` | Client patch dll | Packet spy, working directory fix, extra buttons |
| `tools/mitm` | MITM proxy | Redirect any client to a chosen server |
| `tools/probe`, `tools/proxy_dll`, `tools/network_decrypt` | Client hooks and config decrypt | Packet stream, class log, `Network2.ini` |
| `tools/pak_tool` | List, extract and pack PAK archives | Console, no window |
| `tools/dat_manager` | Browse the PAK archives with a preview of textures, NIF models, sounds and texts, extract and pack | New render stack, dear imgui |
| `tools/col_tool` | Dump a track collision folder | Cells, edges, surfaces, height probes |
| `tools/model_viewer` | View a NIF on the new bgfx renderer, with the client's own light rig | New render stack |
| `tools/map_viewer` | View a track with its collision and start markers, play a ghost recording on it | New render stack, `--ghost` |
| `tools/replay` | Ghost replay reader and `ghost_dump` | From a file or the live database |
| `tools/ui_editor` | Edit the client UI JSON layouts | New render stack, dear imgui |
| `tools/client_snap.ps1` | Capture the stock client per stage while someone plays it | PowerShell, PrintWindow, no focus taken |
| `tools/nif` | NIF reader checks against a client tree | `nif_smoke`, `nif_rewrite`, `nif_export` |

---

## 🤝 Contributing

Contributions are welcome! This project was made in **free time for fun**, so the code can be messy in places.

### Ways to Contribute

- 🐛 **Bug reports** - Open an issue
- 📝 **Documentation** - Improve docs or add examples
- 🔧 **Code cleanup** - Pull requests are appreciated!
- 🎮 **Testing** - Test with the original stock client
- 🌍 **Translation** - Help translate UI/docs

### Pull Request Guidelines

1. Keep changes **focused** and **well-documented**
2. Test with the official client to ensure **protocol compatibility**
3. Follow existing **code style**
4. Update **documentation** if needed
5. The server is the reference, the client follows it, never the reverse

📖 **See:** [CONTRIBUTING.md](CONTRIBUTING.md)

---

## 📜 License

### CC BY-NC-SA 4.0 with Additional Terms

This project is licensed under **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International** with the following terms:

✅ **You CAN:**
- Use this code for **learning and education**
- Modify and improve the code
- Create derivative works
- Host local servers for **non-commercial use**
- Share your modifications under the **same license**

❌ **You CANNOT:**
- Use this code for **commercial purposes** without written permission
- Sell this software or services based on it
- Remove or modify copyright notices
- Use the original game's assets without proper rights

📋 **Attribution Required:**
- Give appropriate **credit** to the original author
- Provide a **link to this repository**
- Indicate if **changes were made**
- Keep the **same license** for derivative works

### Commercial Use

If you want to use this project for commercial purposes, **contact the author** for written permission

### Game Assets

This repository does NOT contain any copyrighted game assets (textures, models, sounds). All assets must be legally obtained.

📄 **Full license text:** [LICENSE.md](LICENSE.md)

---

## ⚖️ Disclaimer

This project is an **independent educational project** focused on learning game development and reverse engineering techniques.

**All research was performed on legally obtained software for educational purposes.**

---

## 🎯 Current Status

### ✅ Completed
- [x] Full protocol documentation, all 309 opcodes reversed and documented in `docs/packets`
- [x] Server works with the stock client end to end: login, characters, licences, lobby, rooms, item and speed races, shop, gacha, quests, missions, ghosts, room craft, car craft, notes, clans, CPU cars
- [x] release package with Docker, chat commands `/getmoney` `/getastro` `/getexp` for a local server
- [x] New render stack: bgfx and its own NIF reader, drawing a KnC car and a KnC track through `tools/model_viewer` and `tools/map_viewer` with the client's own light rig
- [x] Client physics reversed end to end, `docs/reverse/CLIENT_PHYSICS_MAP.md` and eleven files under `docs/reverse/physics`
- [x] 1:1 port of the client's own physics in `games/kart/physics/client`, every module in, checked against a real ghost recording with `ghost_compare`
- [x] Development tools: headless client, client patch dll, MITM proxy, probe, PAK tool, collision dump, model and map viewers with ghost playback, UI editor, NIF checks

### 🔄 In Progress
- [ ] Physics port: compare against a finer capture than a ghost (a 0x40 stream at 100 ms with the key states), the ghost residual sits inside its own byte, see `docs/reverse/physics/GHOST_REFERENCE.md`
- [ ] Client rewrite: phases 1 to 3b in, window, JSON screens, login, character creation, channel, redirect, lobby, room with teams and passwords, race on the physics port, garage, shop, missions, messenger, whisper, sound on the real server, pets, gacha and the craft screens to come

### 🔜 Planned
- [ ] Client rewrite phase 4, pets, gacha, room craft and car craft, the licence screen

---

## 🌟 Acknowledgments

- **IDA Pro & Ghidra** - Reverse engineering tools
- **bgfx** - Graphics library behind the new render stack
- **Original game developers** - For creating the game this project studies
- [**Development Discord**](https://discord.gg/CKyNXXR2jj) - For keeping the memory alive

---

## 💬 Community

- **Development Discord:** [discord.gg/CKyNXXR2jj](https://discord.gg/CKyNXXR2jj)
- **chibikart.gg**, the private server, Discord: [discord.gg/mKSc55hr5H](https://discord.gg/mKSc55hr5H)
- **Issues:** [GitHub Issues](https://github.com/davedevils/KartNChibi/issues)
- **Discussions:** [GitHub Discussions](https://github.com/davedevils/KartNChibi/discussions)

---

## 📊 Stats

![Opcodes Documented](https://img.shields.io/badge/Opcodes%20Documented-309-green)

---

## 🎓 Learning Resources

Want to learn reverse engineering and game networking? Check out:

- [Protocol Documentation](docs/packets/) - Learn about the binary protocol, opcode by opcode
- [Reverse Engineering Guides](docs/reverse/) - Reverse engineering with IDA and Ghidra
- [Client Physics Map](docs/reverse/CLIENT_PHYSICS_MAP.md) - A worked example, from raw addresses to a 1:1 port

---

## ⭐ Show Your Support

If you find this project useful for learning or nostalgia:
- ⭐ **Star this repository**
- 🍴 **Contribute at cleaning**
- 📢 **Share with others**
- 💬 **Come to Discord**

---

<div align="center">

**Made with ❤️ for education and game preservation**

*Most tools were made in free time for fun, so code might be DIRTY - don't complain, just send a Pull Request if you want to clean it! =D*

</div>

---

**Last Updated:** 2026-09-23
**Version:** 1.1.0
**Status:** Active Development
