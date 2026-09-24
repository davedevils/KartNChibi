# 🤝 Contributing to Kart N'Chibi

**Thank you for considering contributing to Kart N'Chibi!**

This is a study project, reversing and rebuilding a 2010 kart racer server first. The server is the reference, the client rewrite adapts to the server, never the other way. We welcome contributions that keep that rule =D

---

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How Can I Contribute?](#how-can-i-contribute)
- [Development Setup](#development-setup)
- [Pull Request Process](#pull-request-process)
- [Coding Standards](#coding-standards)
- [Testing Guidelines](#testing-guidelines)
- [Documentation](#documentation)
- [Community](#community)

---

## 📜 Code of Conduct

### Our Pledge

We are committed to providing a welcoming and inspiring community for all

### Our Standards

✅ **DO:**
- Be respectful and inclusive
- Accept constructive criticism gracefully
- Focus on what's best for the community
- Show empathy towards others

❌ **DON'T:**
- Use inappropriate language or imagery
- Troll, insult, or make derogatory comments
- Publish others' private information
- Harass or discriminate against anyone

---

## 🎯 How Can I Contribute?

### 🐛 Reporting Bugs

**Before submitting a bug report:**
1. Check if it's already reported in [Issues](https://github.com/davedevils/KartNChibi/issues)
2. Test with the latest version
3. Test with the stock client or the headless client (`tools/headless`) to verify protocol compatibility

**When submitting:**
```markdown
### Bug Description
Clear description of the bug

### Steps to Reproduce
1. Step one
2. Step two
3. ...

### Expected Behavior
What should happen

### Actual Behavior
What actually happens

### Environment
- OS: Windows 10 or 11
- Build: Release
- Version: [commit hash]

### Logs
```
Paste relevant logs here
```
```

### 💡 Suggesting Features

**Before suggesting:**
- Check if it's already proposed in [Issues](https://github.com/davedevils/KartNChibi/issues)
- Consider if it fits the project's educational and preservation goals
- Remember the server is the reference, a feature the stock client never had is out of scope

**Feature request template:**
```markdown
### Feature Description
Clear description of the feature

### Motivation
Why is this feature needed?

### Proposed Solution
How would you implement this?

### Alternatives Considered
Other ways to solve this problem

### Additional Context
Screenshots, mockups, examples
```

### 🔧 Code Contributions

We welcome:
- **Bug fixes** 🐛
- **Code cleanup** 🧹
- **Performance improvements** ⚡
- **New features that match stock behaviour** ✨
- **Test coverage** 🧪
- **Documentation** 📚
- **Tooling improvements** 🛠️

### 📝 Documentation Contributions

Help improve:
- Engine API docs in `docs/engine/`
- Protocol docs in `docs/packets/`
- Build and contributing guides
- Code comments
- Tutorial content

### 🌍 Translations

Help translate:
- UI text in the client rewrite
- Documentation
- Error messages

---

## 🛠️ Development Setup

### Prerequisites

- **Windows** 10 or 11, x64
- **Visual Studio 2022**, any edition, C++ desktop workload, toolset v143
- **CMake 3.21** or later
- **Docker Desktop** for the server image
- **Git**

### Clone and Build

```bash
# Clone your fork
git clone https://github.com/YOUR_USERNAME/KartNChibi.git
cd KartNChibi

# Add upstream remote
git remote add upstream https://github.com/davedevils/KartNChibi.git

# Get the submodules
git submodule update --init --recursive
```

See **[BUILD.md](BUILD.md)** for the full build steps, the scripts and the output layout.

### Project Structure

```
📦 Kart N'Chibi
├── server/         # Login server, game server, web admin
├── shared/         # Shared code, packets, sessions, database
├── engine/         # Engine (render, formats, RHI, physics, UI)
├── games/kart/     # Kart game layer, race, items, physics
├── client/         # Client rewrite, builds as knc_client
├── tools/          # Development tools, headless client, viewers
├── docs/           # Documentation
├── tests/          # Tests
└── scripts/        # Build scripts
```

### Running Tests

```bash
# Server tests, off by default, gtest fetched at configure time
cmake -S server -B build-server -DKNC_BUILD_TESTS=ON
cmake --build build-server --config Release
```

Engine tests are targets of the main solution, run the `test_*.exe` in `release/` after building with `cmake --preset vs2022`.

---

## 📤 Pull Request Process

### 1. Fork & Branch

```bash
# Fork the repo on GitHub, then:
git clone https://github.com/YOUR_USERNAME/KartNChibi.git
cd KartNChibi

# Create a feature branch
git checkout -b feature/my-awesome-feature
```

### 2. Make Changes

- Follow the style of the file you are in, C++17
- Comments are one line, at most twenty words, letters, digits, space and `@ % - " '`. Say what the code cannot say, an address, an offset, a unit, a rule
- No banners, no file headers, no history in comments, no commented out code
- Update the matching doc in `docs/engine/` in the same commit, the map is in `docs/engine/INDEX.md`
- Nothing generated in the tree, no build output, no logs, no captures

### 3. Test Thoroughly

- ✅ Build succeeds (Release)
- ✅ No new compiler warnings
- ✅ Existing tests pass
- ✅ New features have tests
- ✅ **Test with the stock or headless client** when packets are touched (protocol compatibility!)
- ✅ The servers, the headless client and the client dll build, the engine solution when it is touched

### 4. Commit

One change per commit.

```bash
git add <files>
git commit -m "fix the room craft ack sent before the object catalogue"
```

**Commit message format:**
```
Present tense, one line of subject.

A short body when the why is not obvious. No trailers.
```

### 5. Push & PR

```bash
# Push to your fork
git push origin feature/my-awesome-feature
```

Then open a Pull Request on GitHub with:
- **Clear title** describing the change
- **Description** of what and why
- **Screenshots** (if UI changes)
- **Testing done** (what you tested)
- **Checklist** completed

**PR Template:**
```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Testing
- [ ] Built successfully (Release)
- [ ] Tested with stock or headless client (protocol compatibility)
- [ ] Added/updated tests
- [ ] All tests pass

## Checklist
- [ ] Code follows project style
- [ ] Self-reviewed the code
- [ ] Comments kept to one line
- [ ] Updated the matching doc in docs/engine/
- [ ] No new warnings
- [ ] No generated files committed

## Screenshots (if applicable)

## Related Issues
Closes #123
```

### 6. Code Review

- The maintainer will review your PR
- Address feedback promptly
- Be open to suggestions
- Keep discussions respectful

### 7. Merge

Once approved:
- Maintainer merges it
- Your contribution is live!
- You're added to the contributors list 🎉

---

## 💻 Coding Standards

### C++ Style Guide

**General:**
- Use **C++17** features
- Prefer `const` and `constexpr`
- Use RAII for resource management
- No raw owning pointers, `std::unique_ptr` for ownership

**Naming:**
```cpp
// Namespaces: PascalCase, nested
namespace KnC {
namespace Engine { }
namespace Render { }
namespace Kart {
namespace Client { }
}
}

// Classes: PascalCase
class PlayerManager { };

// Functions: PascalCase
void UpdatePosition();

// Variables: camelCase
int playerCount;
float velocityX;

// Constants: UPPER_SNAKE_CASE
constexpr int MAX_PLAYERS = 12;

// Members: m_ prefix
class Player {
private:
    int m_id;
    std::string m_name;
};
```

**Formatting:**
```cpp
// Braces on the same line, K&R style
void Function() {
    if (something) {
        DoThis();
    }
}

// Indentation: 4 spaces, no tabs
// Soft limit 100 chars, hard limit 120
```

**Headers:**
```cpp
#pragma once

// Include order:
// 1. Corresponding header
// 2. C++ system headers
// 3. Third-party headers
// 4. Project headers, shared first, then engine, then local
```

**Comments:**
```cpp
int count = 0;  // Player count
float kmh = speed * 3.6f;  // 0x5A69A8 speed to km per hour

// TODO open physics/RIGID_BODY.md wheel offset
```

- One line, at most twenty words
- Only letters, digits, space and `@ % - " '`, no other punctuation
- Say what the code cannot say, an address, an offset, a unit, a rule. Delete the rest
- Identifiers from the client keep a space, `sub 47F800`, `FUN 00418e00`

### Documentation

- **Public APIs**: short `/** @brief */` Doxygen comments, see `docs/engine/INDEX.md`
- **Complex algorithms**: one line pointing at the source doc, not a paragraph
- **Magic numbers**: named `constexpr`, the source address in the comment
- **TODOs**: `// TODO open <doc> <item>`, no parentheses or colon

---

## 🧪 Testing Guidelines

### Unit Tests

```cpp
// tests/unit/player_test.cpp
#include <cassert>
#include "Player.h"

void test_PlayerCreation() {
    Player p("TestPlayer", 1);
    assert(p.GetName() == "TestPlayer");
    assert(p.GetId() == 1);
}

int main() {
    test_PlayerCreation();
    // Add more tests...
    return 0;
}
```

### Integration Tests

Test full workflows:
- Login flow
- Join room
- Start race
- Item usage

### Protocol Compatibility Tests

**Critical:** always test against a real client when packets change

```bash
# 1. Run the servers
release\LoginServer.exe
release\GameServer.exe

# 2. Run the stock client, or tools/headless if you don't have one

# 3. Verify:
# - Can connect
# - Can login
# - Can join lobby
# - Can create/join room
# - Can start race
```

---

## 📚 Documentation

### Code Documentation

```cpp
/** @brief Sends a packet to the server, blocking until it is written */
bool SendPacket(const Packet& packet, bool reliable = false);
```

- Public headers carry a short `/** @brief */` summary, maintained when the API changes
- Regenerate the HTML reference with `docs/engine/generate_docs.bat`

### Markdown Documentation

- Plain short English, simple words, no emoji, no badges (this file and `BUILD.md` are the deliberate exception)
- The doc that describes a file changes in the same commit as the file, the map is in `docs/engine/INDEX.md`
- New reverse findings go to `docs/packets`, the registry row first
- Old material that stops being accurate gets removed, not archived

---

## 🌐 Community

### Where to Get Help

- 💬 **[Development Discord](https://discord.gg/CKyNXXR2jj)** - Questions and general discussion
- 🎮 **[chibikart.gg private server Discord](https://discord.gg/mKSc55hr5H)**
- 🐛 **[GitHub Issues](https://github.com/davedevils/KartNChibi/issues)** - Bugs and features
- 📖 **[Documentation](docs/)** - Technical reference, `docs/README.md` first

### Communication Guidelines

- Be kind and respectful
- Help others when you can
- Search before asking (it might be answered)
- Provide context when asking questions
- Thank people who help you

---

## 🎁 Recognition

Contributors are recognized in:
- **GitHub Contributors** page
- Release notes, `CHANGELOG.md`, for significant contributions
- Project documentation, for doc contributors

---

## 📝 License

By contributing, you agree that your contributions will be licensed under the same license as the project: **CC BY-NC-SA 4.0**.

See [LICENSE.md](LICENSE.md) for details.

---

## ❓ Questions?

Not sure about something? Ask in an issue, or dm me on discord at davedevils

---

<div align="center">

**Thank you for contributing mate !**

*Every contribution, no matter how small, helps preserve gaming history and educates others.*

</div>
