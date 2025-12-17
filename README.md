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

Kart N'Chibi is an educational project exploring MMO kart racing game architecture through reverse engineering. The project will includes:

- 🔓 **Full protocol documentation** ( packets reverse-engineered)
- 🎮 **Server emulator** (Login + Game servers)
- 💻 **Custom client** implementation
- 🛠️ **Development tools** (packet inspector, map viewer, UI editor, etc.)
- 📚 **Comprehensive documentation** of the game's architecture

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
├── 📁 client/              # Custom client implementation
│   ├── app/                # Application entry point
│   ├── engine/             # Rendering, audio, input, UI
│   ├── game/               # Game logic (kart, items, tracks)
│   └── net/                # Network client
│
├── 📁 server/              # Server emulator
│   ├── gateway/            # Login server (auth, session)
│   ├── game/               # Game server (race simulation)
│   └── services/           # MMO services (profiles, inventory, shop)
│
├── 📁 engine/              # Core engine (NIF loader, renderer)
├── 📁 shared/              # Shared library (networking, security)
├── 📁 tools/               # Development tools
│   ├── packet_inspector/   # Network packet analyzer
│   ├── ui_editor/          # UI screen editor
│   ├── map_viewer/         # 3D map viewer
│   └── model_viewer/       # 3D model viewer
│
├── 📁 docs/                # Complete documentation
│   ├── protocol/           # Network protocol (192 packets)
│   ├── architecture/       # System design
│   ├── formats/            # File format specs
│   └── reverse/            # Reverse engineering guides
│
├── 📁 data/                # Game assets
└── 📁 tests/               # Unit/integration tests
```

---

## 🚀 Quick Start

### Prerequisites
- **Windows** (or Linux)
- **Visual Studio 2022** or **2019** (C++ Desktop Development)
- **CMake 3.15+**
- **Git**

### Build

```bash
# Clone the repository
git clone https://github.com/davedevils/KartNChibi.git
cd KartNChibi

# Build everything (Release)
scripts\build.bat release

# Or build Debug
scripts\build.bat debug
```

Executables will be in `release/` or `debug/` folders.

### Run

```bash
# Start the login server
cd release
gateway.exe

# In another terminal, start game server
game.exe

# Run the client
KartNChibi.exe
```

📖 **Full build guide:** [BUILD.md](BUILD.md)

---

## 📚 Documentation

| Document | Description |
|----------|-------------|
| [📖 BUILD.md](BUILD.md) | Complete build instructions |
| [📡 Protocol Documentation](docs/protocol/) | 192 packets reverse-engineered |
| [🏗️ Architecture](docs/architecture/) | System design and structure |
| [📦 File Formats](docs/formats/) | PAK, NIF, DDS, JSON formats |
| [🔍 Reverse Engineering](docs/reverse/) | IDA/Ghidra guides |
| [🎨 UI System](engine/ui/) | UI engine documentation |

### 🌟 Highlights

- **[Protocol Overview](docs/protocol/PROTOCOL_OVERVIEW.md)** - Complete network protocol spec
- **[Packet Index](docs/protocol/README.md)** - Searchable index of all packets

---

## 🛠️ Development Tools

All tools are available in `release/` or `debug/` after building:

| Tool | Description | Usage |
|------|-------------|-------|
| `packet_inspector.exe` | Sniff and decode network packets | Real-time protocol analysis |
| `ui_editor.exe` | Edit UI screens (JSON-based) | Design and test UI layouts |
| `map_viewer.exe` | View 3D maps | Explore track geometry |
| `model_viewer.exe` | View 3D models (NIF format) | Inspect character/kart models |
| `dat_manager.exe` | Extract/pack PAK archives | Manage game assets |

---

## 🤝 Contributing

Contributions are welcome! This project was made in **free time for fun**, so the code can be messy in places 
Even if i try use copilote for fix my doc and text for make it clean 

### Ways to Contribute

- 🐛 **Bug reports** - Open an issue
- 📝 **Documentation** - Improve docs or add examples
- 🔧 **Code cleanup** - Pull requests are appreciated!
- 🎮 **Testing** - Test with original client (DevClient)
- 🌍 **Translation** - Help translate UI/docs

### Pull Request Guidelines

1. Keep changes **focused** and **well-documented**
2. Test with official client to ensure **protocol compatibility**
3. Follow existing **code style**
4. Update **documentation** if needed
5. Add **unit tests** for new features

📖 **See:** [CONTRIBUTING.md](CONTRIBUTING.md) *(coming soon)*

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
- [x] Full protocol documentation (192 packets)
- [x] Login server (authentication, session management)
- [x] Game server (basic race simulation)
- [x] Client (connects to custom server)
- [x] NIF model loader (3D models)
- [x] UI system (JSON-based)
- [x] Packet inspector tool
- [x] Enterprise-grade project structure

### 🔄 In Progress
- [ ] Complete race simulation (physics, items)
- [ ] Anti-cheat system
- [ ] Replay system
- [ ] Load testing infrastructure
- [ ] Admin web panel

### 🔜 Planned
- [ ] Multiple platform support (Linux, Android)
- [ ] Modern rendering backend (Vulkan/DirectX 12)
- [ ] Improved networking (reliable UDP)
- [ ] Comprehensive test suite

---

## 🌟 Acknowledgments

- **IDA Pro & Ghidra** - Reverse engineering tools
- **Raylib** - Graphics library
- **Original game developers** - For creating an amazing game
- [**Community of Chibi Kart Reboot**](https://discord.gg/X2GkJtqTqG) - For keeping the memory alive - [Discord](https://discord.gg/X2GkJtqTqG)

---

## 💬 Community

- **Issues:** [GitHub Issues](https://github.com/davedevils/KartNChibi/issues)
- **Discussions:** [GitHub Discussions](https://github.com/davedevils/KartNChibi/discussions)

---

## 📊 Stats

![Lines of Code](https://img.shields.io/badge/Lines%20of%20Code-50k%2B-blue)
![Packets Documented](https://img.shields.io/badge/Packets%20Documented-192-green)
![Files](https://img.shields.io/badge/Files-2000%2B-orange)

---

## 🎓 Learning Resources

Want to learn reverse engineering and game networking? Check out:

- [Protocol Documentation](docs/protocol/) - Learn about binary protocols
- [IDA Reverse Guide](docs/reverse/IDA_GUIDE.md) - Reverse engineering with IDA
- [Network Protocol Implementation](docs/client-clone/05_network_protocol.md) - Practical networking

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

**Last Updated:** December 2025  
**Version:** 0.0.2A  
**Status:** Active Development
