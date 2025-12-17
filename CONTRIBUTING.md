# 🤝 Contributing to Kart N'Chibi

**Thank you for considering contributing to Kart N'Chibi!**

This project was made in **free time for fun**, so code can be messy in places. We welcome contributions to improve it =D YAP!

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
3. Test with DevClient to verify protocol compatibility

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
- OS: Windows 11
- Build: Release/Debug
- Version: [commit hash or version]

### Logs
```
Paste relevant logs here
```
```

### 💡 Suggesting Features

**Before suggesting:**
- Check if it's already proposed in [Issues](https://github.com/davedevils/KartNChibi/issues) or [Discussions](https://github.com/davedevils/KartNChibi/discussions)
- Consider if it fits the project's educational/preservation goals

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
- **New features** ✨
- **Test coverage** 🧪
- **Documentation** 📚
- **Tooling improvements** 🛠️

### 📝 Documentation Contributions

Help improve:
- API documentation
- Protocol documentation
- User guides
- Code comments
- README files
- Tutorial content

### 🌍 Translations

Help translate:
- UI text
- Documentation
- Error messages
- Comments (optional)

---

## 🛠️ Development Setup

### Prerequisites

- **Windows** (or Linux )
- **Visual Studio 2022/2019** with C++ Desktop Development
- **CMake 3.15+**
- **Git**

### Clone and Build

```bash
# Clone your fork
git clone https://github.com/YOUR_USERNAME/KartNChibi.git
cd knc-clone

# Add upstream remote
git remote add upstream https://github.com/davedevils/KartNChibi.git

# Build
scripts\build.bat release
```

### Project Structure

```
📦 Kart N'Chibi
├── client/         # Client code
├── server/         # Server code
├── engine/         # Engine (NIF, render, UI)
├── shared/         # Shared library
├── tools/          # Development tools
├── docs/           # Documentation
├── tests/          # Tests
└── scripts/        # Build scripts
```

### Running Tests

```bash
# Build with tests enabled
cmake .. -DBUILD_TESTS=ON
cmake --build . --config Release

# Run tests
ctest -C Release
```

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

- Write clean, readable code
- Follow existing code style
- Add comments for complex logic
- Update documentation if needed

### 3. Test Thoroughly

- ✅ Build succeeds (Release and Debug)
- ✅ No new compiler warnings
- ✅ Existing tests pass
- ✅ New features have tests
- ✅ **Test with official client** (protocol compatibility!)

### 4. Commit

```bash
# Stage your changes
git add .

# Commit with a descriptive message
git commit -m "feat: add replay recording system

- Implement packet capture to file
- Add replay playback functionality
- Update documentation
- Add unit tests

Closes #123"
```

**Commit message format:**
```
type: brief description (50 chars max)

Detailed description of what changed and why.
Can be multiple paragraphs.

- Bullet points for key changes
- Reference issues: Closes #123, Fixes #456

Breaking Changes: (if any)
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation only
- `style`: Code style (formatting, no logic change)
- `refactor`: Code restructuring
- `perf`: Performance improvement
- `test`: Adding tests
- `chore`: Maintenance tasks

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
- [ ] Built successfully (Release/Debug)
- [ ] Tested with Official Client (protocol compatibility)
- [ ] Added/updated tests
- [ ] All tests pass

## Checklist
- [ ] Code follows project style
- [ ] Self-reviewed the code
- [ ] Commented complex parts
- [ ] Updated documentation
- [ ] No new warnings
- [ ] Added tests

## Screenshots (if applicable)

## Related Issues
Closes #123
```

### 6. Code Review

- Maintainers will review your PR
- Address feedback promptly
- Be open to suggestions
- Keep discussions respectful

### 7. Merge

Once approved:
- Maintainer will merge
- Your contribution is live!
- You're added to contributors list 🎉

---

## 💻 Coding Standards

### C++ Style Guide

**General:**
- Use **C++17** features
- Prefer `const` and `constexpr`
- Use RAII for resource management
- Avoid raw pointers (use smart pointers)

**Naming:**
```cpp
// Classes: PascalCase
class PlayerManager { };

// Functions: PascalCase or camelCase (be consistent)
void UpdatePosition();
void handlePacket();

// Variables: camelCase
int playerCount;
float velocityX;

// Constants: UPPER_SNAKE_CASE
const int MAX_PLAYERS = 12;

// Members: m_ prefix (optional but preferred)
class Player {
private:
    int m_id;
    std::string m_name;
};
```

**Formatting:**
```cpp
// Braces on new line (Allman style preferred)
if (condition)
{
    DoSomething();
}

// Or K&R style (acceptable)
if (condition) {
    DoSomething();
}

// Indentation: 4 spaces (no tabs)
void Function()
{
    if (something)
    {
        DoThis();
    }
}
```

**Headers:**
```cpp
// Use header guards
#pragma once

// Or include guards
#ifndef PLAYER_H
#define PLAYER_H
// ...
#endif

// Include order:
// 1. Corresponding header
// 2. C system headers
// 3. C++ system headers
// 4. Third-party headers
// 5. Project headers
```

**Comments:**
```cpp
// Use English for comments
// Brief comments for simple logic
int count = 0;  // Player count

// Detailed comments for complex logic
/**
 * Handles position update packet from server.
 * Updates local player position and interpolates
 * between server snapshots for smooth movement.
 * 
 * @param packet The position update packet
 * @return true if update successful
 */
bool HandlePositionUpdate(const Packet& packet);
```

### Documentation

- **Public APIs**: Must have Doxygen comments
- **Complex algorithms**: Explain the approach
- **Magic numbers**: Use named constants
- **TODOs**: Format as `// TODO(name): description`

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

**Critical:** Always test with real client
```bash
# 1. Run your server
release\gateway.exe
release\game.exe

# 2. Run real client
cd Client
KnC.exe

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
/**
 * @brief Sends a packet to the server
 * @param packet The packet to send
 * @param reliable Whether to use reliable delivery
 * @return true if sent successfully
 * 
 * @throws NetworkException if connection is lost
 * 
 * @note This is a blocking operation
 * @warning Not thread-safe, use mutex if multi-threaded
 * 
 * @example
 * Packet p = CreateLoginPacket(username, password);
 * if (SendPacket(p, true)) {
 *     // Wait for response
 * }
 */
bool SendPacket(const Packet& packet, bool reliable = false);
```

### Markdown Documentation

- Use **clear headings**
- Include **code examples**
- Add **diagrams** where helpful
- Keep **tables** for structured data
- Update **related docs** when changing code

---

## 🌐 Community

### Where to Get Help

- 💬 **[GitHub Discussions](https://github.com/davedevils/KartNChibi/discussions)** - General questions
- 🐛 **[GitHub Issues](https://github.com/davedevils/KartNChibi/issues)** - Bugs and features
- 📖 **[Documentation](docs/)** - Technical reference

### Communication Guidelines

- Be kind and respectful
- Help others when you can
- Search before asking (it might be answered)
- Provide context when asking questions
- Thank people who help you

---

## 🎁 Recognition

Contributors are recognized in:
- **CONTRIBUTORS.md** file
- **GitHub Contributors** page
- Release notes (for significant contributions)
- Project documentation (for doc contributors)

---

## 📝 License

By contributing, you agree that your contributions will be licensed under the same license as the project: **CC BY-NC-SA 4.0**.

See [LICENSE.md](LICENSE.md) for details.

---

## ❓ Questions?

Not sure about something? Ask in an issue ! or dm me on discord at davedevils ? 

---

<div align="center">

**Thank you for contributing mate !**

*Every contribution, no matter how small, helps preserve gaming history and educates others.*

</div>

