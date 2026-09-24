# KnC Coding Standards

C++17. Windows-first (VS 2022). CMake 3.15+.

## General

- Simple, readable, predictable. No clever tricks.
- Data-oriented on hot paths.
- Measure before optimizing. Assume tight loops matter.

## Project Structure

```
engine/     Reusable core (RHI, graphics, physics, audio, input, math, models, camera, UI)
client/     Client app (game states, networking, entry)
server/     Server emulator (login, game, services)
tools/      Dev tools (packet inspector, model viewer, UI editor, map viewer, dat manager)
shared/     Shared lib (math, networking, types, security, logging)
tests/      All tests (unit, integration, engine)
demos/      Working demos
docs/       All documentation
thirdparty/ External deps
scripts/    Build and utility scripts
```

### Dependency Direction

```
Tools -> Engine -> shared
Client -> Engine -> shared
Server -> shared
Engine must NOT depend on Client/Server/Tools.
shared must NOT depend on Engine/Client/Server/Tools.
```

### File Placement

- No `.md` inside engine/client/server/tools/ source dirs. Use `docs/`.
- No test files inside engine/. Use `tests/`.
- No demo files inside engine/. Use `demos/`.
- No debug/temp files at root. Use `.gitignore`.
- No build artifacts in repo (*.exe, *.obj, *.dll, *.lib, *.pdb).

## Naming

### Files

| Type | Convention | Example |
|------|-----------|---------|
| C++ source | PascalCase | `Renderer.cpp` |
| C++ header | PascalCase | `Renderer.h` |
| Test files | `test_` prefix | `test_renderer.cpp` |
| CMake | `CMakeLists.txt` | - |
| Scripts | snake_case | `build_release.bat` |

### Code

```cpp
// Namespaces: PascalCase nested
namespace KnC {
namespace Engine {
namespace RHI {

// Classes/Structs: PascalCase
class PlayerManager {};
struct VertexData {};

// Functions: PascalCase
void UpdatePosition();
bool LoadModel(const std::string& path);

// Variables: camelCase
int playerCount;
float deltaTime;

// Members: m_ prefix
class Player {
private:
    int m_id;
    std::string m_name;
    float m_speed;
};

// Constants: UPPER_SNAKE_CASE
constexpr int MAX_PLAYERS = 12;
constexpr float GRAVITY = -20.0f;

// Enums: PascalCase type + values
enum class GraphicsAPI {
    OpenGL,
    DirectX11,
    DirectX12,
    Vulkan,
    Metal
};

// Templates: T prefix
template<typename TKey, typename TValue>
class HashMap {};

// Macros (avoid): KNC_ prefix
#define KNC_ASSERT(x) ...
```

## Formatting

### Braces: K&R

```cpp
void DoSomething() {
    if (condition) {
        Action();
    } else {
        OtherAction();
    }
}

class MyClass {
public:
    void Method();
private:
    int m_value;
};

namespace KnC {
namespace Engine {

class Renderer {
};

} // namespace Engine
} // namespace KnC
```

### Rules

- 4 spaces, no tabs.
- Namespace content NOT indented.
- Soft limit 100 chars, hard limit 120.

### Include Order

```cpp
// 1. Corresponding header
#include "MyClass.h"

// 2. C++ stdlib
#include <vector>
#include <string>
#include <memory>

// 3. Third-party
#include <glfw/glfw3.h>

// 4. Project (shared first, engine, local)
#include "shared/math/Vec3.h"
#include "rhi/RHI.h"
#include "graphics/Shader.h"
```

### Header Guards

```cpp
#pragma once
```

## Interfaces

- Minimal public API. Expose only what other modules need.
- Everything else `private` / anonymous namespace / internal headers.
- Pure interfaces only when needed (RHI backends, platform abstraction).
- No `friend class` unless documented why.

## Memory and Lifetime

- No raw owning pointers in public code.
- `std::unique_ptr` for ownership.
- `std::shared_ptr` only for shared ownership (rare, document why).
- References / raw pointers for non-owning access.
- No per-frame allocations. Pre-allocate, reuse buffers, use pools.
- GPU resources: explicit lifetime via RHI handles.

## Hot Path Rules

- Allocation-free.
- Branch-light.
- Cache-friendly (SoA where it matters).
- Contiguous storage (`std::vector`, packed arrays).
- No virtual calls in hot loops. Use function tables / IDs.

## Error Handling

- No exceptions in real-time runtime code.
- Return bool / error codes, log once, fail gracefully.
- Use `LOG_ERROR()` not `throw`.
- Tools: exceptions allowed if contained.
- Never silently fail.

## Threading

- Engine jobs deterministic when required.
- No locks in render loop hot sections. Double-buffer / command queues.
- Thread ownership documented.
- Lock-free preferred. `std::atomic` for simple shared state.

## Rendering (RHI)

```
Application -> Renderer API -> RHI abstraction -> Backend (DX12/Vulkan/WebGPU)
```

- All rendering through RHI. No direct API calls outside backends.
- Render thread gets immutable commands (no gameplay objects).
- Batching and state sorting mandatory in perf builds.

NIF coordinate conversion in `engine/nif_import/NifBridge.h`. Rest of codebase uses engine coords only.

## Asset Pipeline

- Runtime never parses heavy source formats. Tools convert.
- Runtime loads cooked/binary only (PAK archives, processed NIF).
- Stable asset IDs. No path strings in hot runtime code.
- PAK reader in shared/.

## Tools Rules

- Trade perf for simplicity. Must be stable and debuggable.
- No engine private headers unless allowed.
- Logs: input file, step name, elapsed time, output path.
- Tools depend on Engine public API. Engine never depends on Tools.

## Logging

| Level | Usage | Hot Loop |
|-------|-------|----------|
| `LOG_ERROR` | Failures | NO |
| `LOG_WARN` | Recoverable issues | NO |
| `LOG_INFO` | Lifecycle events (init, shutdown, load) | NO |
| `LOG_DEBUG` | Dev diagnostics | NO |
| `LOG_TRACE` | Verbose step-by-step (dev only) | NO |

Never log in hot loops. `LOG_INFO` only for major lifecycle events.
Static debug counters forbidden in render path (not thread-safe, never reset).

## Serialization

- Versioned (explicit version field).
- Dedicated data structures for save/load (no engine pointers).
- Network packet format compatible with original client.
- Packet structures in `docs/packets/`.

## Build Config

| Config | Asserts | Profiling | Optimization |
|--------|---------|-----------|-------------|
| Debug | Full | Full | None |
| Release | None | Minimal | Full |

### Platform Abstraction

- Platform-specific code behind abstraction layers.
- Windows: `#ifdef _WIN32` or platform headers.
- RHI backends: separate static libs, conditionally compiled.

### CMake

- One `CMakeLists.txt` per module.
- No hardcoded paths.
- `target_include_directories` and `target_link_libraries`.

## Comments

```cpp
// English only.
// Brief for simple logic.

/**
 * Handles position update packet from server.
 * Interpolates between server snapshots.
 *
 * @param packet Position update packet
 * @return true if update applied
 */
bool HandlePositionUpdate(const Packet& packet);

// TODO(dave): Migrate to RHI
// FIXME(dave): Leaks GPU memory on shutdown
```

- Public APIs: doc comments required.
- Complex algorithms: explain approach.
- Magic numbers: use named constants.
- No commented-out code older than 1 sprint. Delete it.

## Forbidden

| Forbidden | Why |
|-----------|-----|
| Allocations in per-frame hot loops | Performance |
| Logging in hot loops | Performance + spam |
| Engine depending on Client/Server/Tools | Architecture |
| Direct OpenGL/DX calls outside RHI | Portability |
| `using namespace std;` in headers | Namespace pollution |
| `#include` of .cpp files | Compilation |
| `friend class` on self | Nonsensical |
| Uncommented magic numbers | Readability |
| Silent failures | Debuggability |
| `.md` in source dirs | Organization |
| Build artifacts in git | Repo hygiene |
| Debug dump files in git | Repo hygiene |
| Marketing language in docs | Honesty |

## Commit Messages

```
type: brief description (50 chars max)

What changed and why.

- Bullet points for key changes
- Closes #123
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `chore`
