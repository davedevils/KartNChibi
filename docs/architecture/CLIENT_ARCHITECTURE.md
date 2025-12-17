# Client Architecture

**Project**: Kart N'Chibi  
**Version**: 0.2.0  
**Last Updated**: December 17, 2025  
**Status**: Active Development

---

## Overview

The Kart N'Chibi client is a modern C++17 reimplementation of the original kart racing MMO client, featuring a state-based architecture with modular subsystems.

**Key Design Principles**:
- State-based architecture for game flow management
- Modular system design with clear separation of concerns
- Event-driven networking with packet dispatch
- Modern C++17 with RAII and smart pointers
- Cross-platform ready (Windows primary target)

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      KartNChibi Client                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐   │
│  │   Core       │   │   Engine     │   │   Network    │   │
│  │              │   │              │   │              │   │
│  │ • Application│   │ • Renderer   │   │ • Connection │   │
│  │ • StateMan.  │   │ • Resources  │   │ • Packet     │   │
│  │ • Config     │   │ • Audio      │   │ • Dispatcher │   │
│  └──────────────┘   └──────────────┘   └──────────────┘   │
│                                                              │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐   │
│  │   Input      │   │   UI         │   │   Game       │   │
│  │              │   │              │   │              │   │
│  │ • Keyboard   │   │ • Widgets    │   │ • Lobby      │   │
│  │ • Mouse      │   │ • Screens    │   │ • Room       │   │
│  │ • Gamepad    │   │ • Rendering  │   │ • Race       │   │
│  └──────────────┘   └──────────────┘   └──────────────┘   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              Game States (State Machine)              │  │
│  │  Logo → Login → Channel → Menu → Lobby → Room → Race │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## Directory Structure

```
client/
├── app/
│   ├── bootstrap/          # Application initialization
│   │   ├── Application.cpp # Main application class
│   │   └── StateManager.cpp # State machine
│   └── main.cpp            # Entry point
│
├── engine/
│   ├── EngineWrapper.cpp   # KnC Engine interface
│   ├── input/              # Input handling
│   ├── render/             # Rendering
│   └── ui/                 # UI system
│
├── net/
│   ├── Connection.cpp      # TCP network connection
│   ├── Packet.cpp          # Packet serialization/deserialization
│   └── PacketDispatcher.cpp # Packet routing
│
├── game/
│   ├── StateChannel.cpp    # Channel selection state
│   ├── StateLobby.cpp      # Lobby state
│   ├── StateLogo.cpp       # Logo screen state
│   ├── camera/             # Camera system
│   ├── items/              # Item system
│   ├── kart/               # Kart physics
│   └── tracks/             # Track management
│
├── formats/
│   ├── DdsLoader.cpp       # DDS texture loading
│   ├── NifLoader.cpp       # NIF 3D model loading
│   └── PakReader.cpp       # PAK archive reading
│
└── ui/                     # UI components
```

---

## Core Systems

### 1. Application

**Purpose**: Main application entry point and lifecycle management

**Location**: `client/app/bootstrap/Application.cpp`

**Responsibilities**:
- Window creation and management
- Main game loop
- Subsystem initialization
- Shutdown handling

**Key Methods**:
```cpp
class Application {
public:
    bool Initialize();       // Initialize all subsystems
    int Run();              // Main game loop
    void Shutdown();        // Cleanup
    
private:
    void ProcessEvents();   // Handle window/input events
    void Update(float dt);  // Update game logic
    void Render();          // Render frame
};
```

---

### 2. State Manager

**Purpose**: Manage game states and transitions

**Location**: `client/app/bootstrap/StateManager.cpp`

**Game States**:
```cpp
enum class GameState {
    Logo = 0,        // Logo/splash screen
    Title = 1,       // Title screen
    Login = 2,       // Login screen (if no launcher)
    Channel = 3,     // Channel selection
    Menu = 4,        // Main menu
    Garage = 5,      // Vehicle garage
    Shop = 6,        // Item shop
    Lobby = 7,       // Room lobby
    Room = 8,        // Waiting room
    Loading = 9,     // Loading screen
    Racing = 10,     // In race
    Results = 11,    // Race results
    Tutorial = 12    // Tutorial mode
};
```

**State Interface**:
```cpp
class IGameState {
public:
    virtual ~IGameState() = default;
    
    // Lifecycle
    virtual bool Initialize() = 0;
    virtual void Cleanup() = 0;
    
    // Per-frame
    virtual void Update(float deltaTime) = 0;
    virtual void Render() = 0;
    
    // Events
    virtual void OnEnter() {}
    virtual void OnExit() {}
    virtual void OnPacket(uint8_t cmd, const Packet& packet) {}
};
```

**State Flow**:
```
Logo → Channel → Menu → Lobby → Room → Race → Results
                   ↓       ↓       ↓
                Garage   Shop   (Back to Menu)
```

---

### 3. Network System

**Purpose**: Handle server communication

**Location**: `client/net/`

**Components**:

#### Connection
- TCP socket management
- Send/receive packet data
- Connection state handling
- Heartbeat mechanism

#### Packet
- Binary packet serialization
- Type-safe reading/writing
- Header management (size, CMD, flag)

**Packet Structure**:
```
┌────────────────────────────────────┐
│ Header (8 bytes)                   │
├────────────────────────────────────┤
│ [0-1]  Size (uint16_t)             │
│ [2]    CMD (uint8_t)               │
│ [3]    Flag (uint8_t)              │
│ [4-7]  Reserved                    │
├────────────────────────────────────┤
│ Payload (variable)                 │
└────────────────────────────────────┘
```

#### PacketDispatcher
- Route packets to handlers by CMD
- Handler registration system
- Type-safe packet dispatch

**Usage**:
```cpp
// Register handlers
dispatcher.RegisterHandler(0x01, HandleLoginResponse);
dispatcher.RegisterHandler(0x12, HandleShowLobby);

// Dispatch packet
void OnReceive(Packet& packet) {
    dispatcher.Dispatch(packet);
}
```

---

### 4. Engine Integration

**Purpose**: Interface with KnC Engine

**Location**: `client/engine/EngineWrapper.cpp`

**Features**:
- 3D rendering via KnC Engine
- Model loading (NIF format)
- Texture management
- Audio playback
- Camera control

**Key Methods**:
```cpp
class EngineWrapper {
public:
    bool Initialize();
    void Update(float dt);
    void Render();
    
    // Model management
    ModelHandle LoadModel(const std::string& path);
    void DrawModel(ModelHandle handle, const Transform& transform);
    
    // Camera
    void SetCamera(const Camera& camera);
    
    // Audio
    void PlaySound(const std::string& path);
    void PlayMusic(const std::string& path, bool loop);
};
```

---

### 5. Input System

**Purpose**: Handle user input

**Location**: `client/engine/input/`

**Input Sources**:
- **Keyboard**: Key states, text input
- **Mouse**: Position, buttons, scroll
- **Gamepad**: Buttons, axes, vibration

**Usage**:
```cpp
// Keyboard
if (Input::IsKeyPressed(Key::SPACE)) {
    Jump();
}

// Mouse
if (Input::IsMouseButtonDown(MouseButton::LEFT)) {
    Vec2 pos = Input::GetMousePosition();
    OnClick(pos);
}

// Gamepad
float leftX = Input::GetGamepadAxis(0, GamepadAxis::LEFT_X);
MovePlayer(leftX);
```

---

### 6. UI System

**Purpose**: User interface rendering and interaction

**Location**: `client/ui/`

**Components**:
- **Widgets**: Button, Label, Input, List, Panel
- **Screens**: Pre-built UI screens for each state
- **Layout**: Position, size, anchoring
- **Events**: Click, hover, focus

**Widget Hierarchy**:
```
UIWidget (base)
├── UIButton
├── UILabel
├── UIImage
├── UIInput
├── UIList
│   ├── UIChannelList
│   ├── UIRoomList
│   └── UIPlayerList
└── UIPanel
    └── UIDialog
```

**Example UI Screen**:
```cpp
class LobbyScreen {
public:
    void Initialize() {
        // Room list
        m_roomList = CreateList({50, 100, 600, 400});
        
        // Buttons
        m_createBtn = CreateButton({700, 100, 150, 40}, "Create Room");
        m_refreshBtn = CreateButton({700, 150, 150, 40}, "Refresh");
        
        // Chat
        m_chatInput = CreateInput({50, 550, 500, 30});
    }
    
    void Update(float dt) {
        if (m_createBtn->IsClicked()) {
            OnCreateRoom();
        }
    }
};
```

---

## Data Flow

### Network Receive Flow

```
Server
  │
  ▼
┌──────────────┐
│ TCP Socket   │ recv() raw bytes
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Packet       │ Parse header + payload
│ Parser       │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Packet       │ Route by CMD byte
│ Dispatcher   │
└──────┬───────┘
       │
       ├─────► CMD 0x01: LoginResponse
       ├─────► CMD 0x12: ShowLobby
       ├─────► CMD 0x3E: PlayerJoin
       └─────► etc.
```

### Render Flow

```
┌──────────────┐
│ StateManager │ Current state's Render()
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Engine       │ 3D scene rendering
│ Wrapper      │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ KnC Engine   │ Model/texture rendering
│ (OpenGL)     │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ UI System    │ 2D overlay (HUD, menus)
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Present      │ Swap buffers
└──────────────┘
```

---

## State Implementations

### Channel Selection State

**File**: `client/game/StateChannel.cpp`

**Purpose**: Display channel list and handle selection

**UI Elements**:
- Channel list with server status
- Join button
- Back to menu button

**Network Packets**:
- **Receive**: `0x0E` Channel List
- **Send**: `0x19` Server Query (join)

---

### Lobby State

**File**: `client/game/StateLobby.cpp`

**Purpose**: Display room list and allow joining/creating rooms

**UI Elements**:
- Room list table (name, map, mode, players)
- Create room button
- Quick join button
- Chat area
- Player list

**Network Packets**:
- **Receive**: `0x12` Show Lobby, `0x3F` Room List
- **Send**: `0x63` Create Room, Room join request

---

### Room State

**Purpose**: Waiting room before race starts

**UI Elements**:
- 8 player slots (2x4 grid)
- Track preview
- Ready button
- Start button (host only)
- Chat

**Network Packets**:
- **Receive**: `0x3E` Player Join, `0x23` Player Update, `0x14` Game Mode
- **Send**: `0x23` Ready state

---

### Racing State

**Purpose**: In-game racing

**HUD Elements**:
- Speedometer
- Position display (1st, 2nd, etc.)
- Lap counter
- Item slot
- Minimap
- Timer

**Network Packets**:
- **Receive**: `0x31` Position updates, `0x35` Score, `0x45` Item usage
- **Send**: `0x31` Position, `0x45` Item use, `0x39` Finish

---

## File Format Handlers

### NIF Loader

**Purpose**: Load Gamebryo NIF 3D models

**Location**: `client/formats/NifLoader.cpp`

**Features**:
- Mesh loading with vertices, normals, UVs
- Material parsing
- Texture reference extraction
- Transform hierarchy

### PAK Reader

**Purpose**: Extract files from game archives

**Location**: `client/formats/PakReader.cpp`

**Features**:
- Read PAK file headers
- Extract individual files
- Directory listing

### DDS Loader

**Purpose**: Load DirectDraw Surface textures

**Location**: `client/formats/DdsLoader.cpp`

**Features**:
- DXT1/DXT3/DXT5 compression
- Mipmap support
- Convert to OpenGL format

---

## Threading Model

The client is primarily **single-threaded**:

```
Main Thread
├── Window events (GLFW)
├── Network recv/send
├── State update
├── Rendering
└── Present
```

**Future Considerations**:
- Background thread for resource loading
- Audio streaming thread (handled by engine)
- Network thread for non-blocking I/O

---

## Memory Management

**Strategy**: Modern C++ RAII with smart pointers

**Guidelines**:
- Use `std::unique_ptr` for exclusive ownership
- Use `std::shared_ptr` for shared resources
- Use `std::weak_ptr` to break cycles
- Avoid raw `new`/`delete`
- RAII for all resources (files, sockets, GPU objects)

**Example**:
```cpp
// Resource loading
class ResourceManager {
public:
    std::shared_ptr<Texture> LoadTexture(const std::string& path) {
        auto it = m_textures.find(path);
        if (it != m_textures.end()) {
            return it->second; // Return cached
        }
        
        auto texture = std::make_shared<Texture>();
        if (texture->Load(path)) {
            m_textures[path] = texture;
            return texture;
        }
        return nullptr;
    }
    
private:
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
};
```

---

## Error Handling

**Strategy**: Exceptions for critical errors, return codes for recoverable errors

**Examples**:
```cpp
// Network error (critical)
if (!connection.Connect(ip, port)) {
    throw NetworkException("Failed to connect to server");
}

// Resource loading (recoverable)
auto model = LoadModel("kart.nif");
if (!model) {
    Log::Error("Failed to load model, using default");
    model = GetDefaultModel();
}
```

---

## Configuration

**Files**:
- `config/client.json` - Client settings
- `launcher.ini` - Launcher config (if used)
- `input.ini` - Key bindings

**Example `client.json`**:
```json
{
    "window": {
        "width": 1024,
        "height": 768,
        "fullscreen": false,
        "vsync": true
    },
    "network": {
        "server": "127.0.0.1",
        "port": 50017,
        "timeout": 5000
    },
    "graphics": {
        "renderAPI": "OpenGL",
        "antialiasing": 4,
        "shadows": true
    }
}
```

---

## Testing

**Unit Tests**: `client/tests/`

**Test Categories**:
- Packet serialization/deserialization
- State transitions
- Input handling
- UI widget behavior

**Integration Tests**:
- Connect to test server
- Full game flow (login → lobby → room → race)
- Network protocol compliance

---

## Build System

**CMake Configuration**: `client/CMakeLists.txt`

**Dependencies**:
- KnC Engine (internal)
- GLFW (window/input)
- OpenGL 3.3+
- stb_image (texture loading)

**Build**:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## Future Enhancements

**Planned Features**:
- Async resource loading
- Replay system
- Screenshot/recording
- Mod support
- Linux/macOS ports

---

## References

- [Network Protocol](../protocol/PROTOCOL_OVERVIEW.md) - Complete packet reference
- [Engine Quick Start](../ENGINE_QUICKSTART.md) - Engine usage guide
- [Reverse Engineering Guide](../reverse/IDA_GUIDE.md) - Original client analysis
- [Build Guide](../../BUILD.md) - How to build the client

---

**Version**: 0.2.0  
**Last Updated**: December 17, 2025  
**Status**: Active Development

