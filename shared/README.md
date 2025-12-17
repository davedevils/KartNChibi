# Shared Library - Kart N'Chibi

**Cross-platform shared code** for client, server, and tools.

---

## 📦 **Contents**

### `core/` - Core Types
- **`Types.h`** - Base types (int8, uint32, PlayerId, etc.)
- **`Result.h`** - Rust-style error handling

### `math/` - Math Primitives
- **`Vec2.h`** - 2D vectors
- **`Vec3.h`** - 3D vectors  
- **`Quat.h`** - Quaternions (3D rotations)
- **`Matrix4.h`** - 4x4 matrices (transformations, camera)

### `net/` - Network Primitives
- **`Packet.h`** - Packet header, buffer, reader

---

## 🔧 **Usage**

### Quick Start

```cpp
#include <shared/Shared.h>

using namespace KnC;
using namespace KnC::Math;
using namespace KnC::Net;

// Math
Vec3 position(1.0f, 2.0f, 3.0f);
Vec3 velocity(0.0f, -9.8f, 0.0f);
position += velocity * deltaTime;

Quat rotation = Quat::FromEuler(0.0f, PI / 2, 0.0f);
Vec3 forward = rotation.RotateVector(Vec3::Forward());

// Networking
PacketBuffer packet;
packet.Init(0x01, 0x00); // CMD 0x01, Flag 0x00
packet.Write<uint32>(playerId);
packet.Write<Vec3>(position);
packet.WriteFixedString(playerName, 32);

// Send packet...
socket.Send(packet.GetData(), packet.GetTotalSize());

// Error Handling
Result<int, std::string> divide(int a, int b) {
    if (b == 0) {
        return Err<std::string>("Division by zero");
    }
    return Ok<int, std::string>(a / b);
}

auto result = divide(10, 2);
if (result.IsOk()) {
    printf("Result: %d\n", result.Unwrap());
}
```

---

## 🏗️ **Design Principles**

1. **Header-only where possible** - Fast compilation
2. **Zero dependencies** - Only STL
3. **Platform agnostic** - Works on Windows, Linux, macOS
4. **Game-oriented** - Types match network protocol
5. **Performance-first** - Inline, constexpr, no allocations

---

## 📊 **Type Sizes**

| Type | Size | Alignment |
|------|------|-----------|
| `PacketHeader` | 8 bytes | 1 byte |
| `Vec2` | 8 bytes | 4 bytes |
| `Vec3` | 12 bytes | 4 bytes |
| `Quat` | 16 bytes | 4 bytes |
| `Result<T,E>` | `sizeof(T)` or `sizeof(E)` + 1 | Varies |

---

## 🧪 **Testing**

```bash
cd tests
./run_shared_tests.bat
```

---

## 📚 **Documentation**

See `docs/shared/` for detailed API documentation.
