# Kart N'Chibi - File Formats

**Version**: 1.0  
**Status**: Draft  
**Date**: December 2025

---

## 1. Overview

The KnC client uses a mix of standard and proprietary file formats.

---

## 2. Engine Formats (Gamebryo/NetImmerse)

### 2.1 NIF Files (`.nif`)

**Description**: NetImmerse File - 3D models, scenes, and scene graphs

**Status**: ✅ Well-documented (NifTools)

**Locations**:
```
Data/Public/Car/Body/       - Vehicle bodies
Data/Public/Driver/Body/    - Character models
Data/Public/World/          - Track geometry
Data/Public/Item/           - Item models
Data/Public/Effect/         - Visual effects
```

**Structure**:
- Header with version info
- Block type strings
- Object blocks (nodes, meshes, textures)
- Root references

**Tools**:
- NifSkope - View/edit NIF files
- PyNIFlib - Python library
- NifTools - Open-source toolset

**Loading**: Handled by EngineDLL via `NiStream`

### 2.2 KF Files (`.kf`)

**Description**: Keyframe animation files

**Status**: ✅ Well-documented (NifTools)

**Locations**:
```
Data/Public/Driver/Body/*/  - Character animations
Data/Public/Car/Effect/     - Vehicle animations
```

**Structure**:
- Animation sequences
- Keyframe data (position, rotation, scale)
- Controller links

**Tools**:
- NifSkope - View animations
- Blender + NIF plugin

### 2.3 KFM Files (`.kfm`)

**Description**: Keyframe Manager - animation sequence definitions

**Status**: ✅ Documented

**Usage**: Links multiple KF files to a base NIF

### 2.4 DDS Files (`.dds`)

**Description**: DirectDraw Surface - texture format

**Status**: ✅ Standard format

**Locations**:
```
Data/Public/Car/Body/       - Vehicle textures
Data/Public/Driver/Body/    - Character textures
Data/Public/World/          - Environment textures
Data/Public/Image/          - UI textures
```

**Compression**:
- DXT1 - RGB, 1-bit alpha
- DXT3 - RGBA, 4-bit alpha
- DXT5 - RGBA, interpolated alpha

**Tools**:
- NVIDIA Texture Tools
- DirectXTex
- Any image editor with DDS plugin

---

## 3. Game-Specific Formats

### 3.1 CAR Files (`.car`)

**Description**: Vehicle definition files

**Status**: 🔶 Partially documented

**Location**: `Data/Car/`

**Sample Files**: 48 vehicle definitions

**Structure Analysis**:

```
File: basic_1.car (example)
Offset  Size    Type        Description
------  ----    ----        -----------
0x0000  4       char[4]     Magic? / Version?
0x0004  4       int32       Unknown
0x0008  N       string      Model path (NIF)
...
[Physics parameters]
[Stats: speed, accel, handling, etc.]
[Visual customization slots]
```

**Reverse Strategy**:
1. Hex dump all 48 .car files
2. Compare similar vehicles
3. Identify common patterns
4. Map to decompiled vehicle loading code
5. Cross-reference with VehicleData structure (44 bytes)

**Tool to Build**: CAR file viewer/editor

### 3.2 REP Files (`.rep`)

**Description**: Replay/recording files

**Status**: 🔶 Partially documented

**Location**: `DevClient/` (License_Track_*.rep)

**Structure Hypothesis**:
```
Header:
  - Magic/version
  - Track ID
  - Player count
  - Duration
  
Data:
  - Timestamped position/rotation frames
  - Input events
  - Item usage
```

**Reverse Strategy**:
1. Capture new replays at known tracks
2. Compare binary differences
3. Identify position data patterns
4. Map timestamps to frame data

### 3.3 PAK Files (`.dat`)

**Description**: Packed resource archive

**Status**: ✅ Unpacker exists

**Location**: `DevClient/pak001.dat`

**Tool**: `KNC Un-Packer.exe` already extracts these

**Structure**:
- File table header
- File entries (name, offset, size)
- Compressed/raw file data

### 3.4 INI Files (`.ini`)

**Description**: Configuration files

**Status**: ✅ Standard format with quirks

**Files**:
| File | Purpose | Encryption |
|------|---------|------------|
| `Input.ini` | Keybindings | None |
| `Option2.ini` | Game options | None |
| `Network2.ini` | Server config | ⚠️ Encrypted |
| `launcher.ini` | Launcher config | None |

**Network2.ini Decryption**:
- Tool exists: `Network2 Decrypt.exe`
- Simple XOR or substitution cipher

---

## 4. Image Formats

### 4.1 PNG Files (`.png`)

**Status**: ✅ Standard

**Location**: `Data/Eng/Image/` - UI elements

**Usage**: UI buttons, icons, backgrounds

### 4.2 TGA Files (`.tga`)

**Status**: ✅ Standard

**Location**: Various effect directories

**Usage**: Alpha-channel textures for effects

### 4.3 BMP Files (`.bmp`)

**Status**: ✅ Standard

**Location**: Various

**Usage**: Legacy textures

### 4.4 IFL Files (`.ifl`)

**Description**: Image File List - animated texture sequences

**Status**: ✅ Simple text format

**Example**:
```
texture_frame_01.dds
texture_frame_02.dds
texture_frame_03.dds
```

---

## 5. Audio Formats

### 5.1 WAV Files (`.wav`)

**Status**: ✅ Standard PCM

**Location**: `Data/Public/Sound/High/`

**Count**: ~232 sound effects

**Usage**:
- Engine sounds
- Impact sounds
- UI sounds
- Item effects

---

## 6. Text/Data Formats

### 6.1 Definition Files (`def_*.txt`)

**Location**: `Define/Eng/`

**Files**:
| File | Purpose |
|------|---------|
| `def_emotion_*.txt` | Emote definitions |
| `def_quest_index.txt` | Quest ID mapping |
| `def_quest_message.txt` | Quest text |
| `def_title.txt` | Title definitions |
| `def_trans_index.txt` | Translation index |
| `def_trans_message.txt` | Localized text |

**Format**: Tab-separated or custom delimited

### 6.2 Driver Position Files (`driver_pos_*.ini`)

**Location**: `Define/Driver/`

**Purpose**: Character position/offset for different vehicles

**Format**: Standard INI with position vectors

### 6.3 Map Configuration Files

**Location**: `Data/Public/World/{Category}/{MapName}/`

| File | Format | Purpose |
|------|--------|---------|
| `regen.ini` | `x,y,z,rotation` (1 line) | Respawn/camera position |
| `start.ini` | `x,y,z,rotation` (8-16 lines) | Starting grid positions |
| `itembox.ini` | Groups of `x,y,z` | Item box positions |
| `boost.ini` | `x,y,z,w,h` | Boost zone positions |
| `follow_01.ini` | Waypoints | AI pathfinding |
| `minimap.ini` | Config | Minimap settings |
| `itembite.ini` | Positions | Item bite zones |
| `itemdrum.ini` | Positions | Item drum positions |

**Example `start.ini`**:
```
-78.3,43.442,-8.521,0.00
-78.141,49.484,-8.872,0.00
-77.983,55.525,-9.126,0.00
...
```

### 6.4 Gimmick Files

**Location**: `Data/Public/World/{Category}/{MapName}/Gimmick/`

**Contents**: Animated objects/monsters on the track

| File | Purpose |
|------|---------|
| `ant_01.nif` | Giant ant monster |
| `pierrot_01.nif` | Clown character |
| `*.dds` | Textures for gimmicks |

---

## 7. Collision Format (`.COL`)

### 7.1 Track Collision Files (`track.COL`)

**Status**: ✅ Reversed (December 2024)

**Location**: `Data/Public/World/{Category}/{MapName}/track.COL`

**Structure**:
```cpp
struct COLHeader {
    int32_t version;      // Usually 17
    int32_t vertexCount;  // Number of vertices
    int32_t faceCount;    // Number of triangles
    int32_t zoneCount;    // Number of zones
};

// After header:
// - Vertices: vertexCount * (float x, float y, float z) = 12 bytes each
// - Faces: faceCount * (uint16_t i0, i1, i2) = 6 bytes each
// - Zones: zoneCount * 56 bytes each (includes name like "START", "BOOST")
```

**Zone Structure** (56 bytes):
```cpp
struct COLZone {
    float data[12];       // Position/bounds data
    uint16_t flags[2];    // Type flags
    char name[8];         // Zone name (null-terminated)
    uint8_t padding[8];   // Padding to 56 bytes
};
```

**Zone Names**:
- `START` - Starting grid positions
- `BOOST` - Speed boost zones
- `ITEM` - Item pickup zones
- Other zones TBD

---

## Unknown/Proprietary Formats

### OGP Files (`.ogp`)

**File**: `clientinfo.ogp`

**Status**: Unknown (lazy dave)

**Hypothesis**: OGPlanet platform metadata

**Reverse Strategy**:
1. Hex dump and pattern analysis
2. Compare with other OGPlanet games
3. May not be needed for clone

---

## 8. File Format Tools to Build

### 8.1 CAR File Tools

**Priority**: High

```cpp
// car_viewer.cpp
struct CarFile {
    char magic[4];
    int32_t version;
    // ... fields to discover
};

bool LoadCarFile(const char* path);
void DumpCarFile(const CarFile& car);
void SaveCarFile(const CarFile& car, const char* path);
```

### 8.2 REP File Tools

**Priority**: Medium

```cpp
// rep_viewer.cpp
struct ReplayFile {
    // Header
    // Frame data
};

bool LoadReplayFile(const char* path);
void PlayReplay(const ReplayFile& replay);
```

### 8.3 Asset Browser

**Priority**: Medium

A unified tool to browse all game assets:
- Tree view of Data/ folder
- Preview panel (NIF viewer, texture viewer, audio player)
- Export functionality

---

## 9. Resource Loading Pipeline

### 9.1 Current Loading Flow

```
Game Startup
    │
    ├── Load PAK files (if packed)
    │   └── Extract to memory/cache
    │
    ├── Load configuration
    │   ├── Input.ini
    │   ├── Option2.ini
    │   └── Network2.ini (decrypt)
    │
    └── On-demand loading
        ├── NIF models (EngineDLL)
        ├── Textures (EngineDLL)
        ├── Sounds (EngineDLL or custom)
        └── UI images (D3D9 textures)
```

### 9.2 Clone Loading Strategy

```cpp
class ResourceManager {
public:
    // Pre-load essential resources
    void Initialize();
    
    // On-demand loading
    NiNode* LoadModel(const std::string& path);
    NiTexture* LoadTexture(const std::string& path);
    
    // Caching
    void PreloadCategory(ResourceCategory category);
    void FlushCache();
    
private:
    std::map<std::string, NiNode*> m_modelCache;
    std::map<std::string, NiTexture*> m_textureCache;
};
```

---

## 10. File Path Conventions

### 10.1 Model Paths

```
Vehicle body:     Data/Public/Car/Body/{name}/{name}.nif
Vehicle texture:  Data/Public/Car/Body/{name}/{name}.dds
Character body:   Data/Public/Driver/Body/{name}/{name}.nif
Character anim:   Data/Public/Driver/Body/{name}/*.kf
Track:            Data/Public/World/{track}/*.nif
Item:             Data/Public/Item/{item}.nif
Effect:           Data/Public/Effect/{effect}.nif
```

### 10.2 UI Paths

```
Buttons:          Data/Eng/Image/Buttons/*.png
Channel select:   Data/Eng/Image/ChannelSelect/*.png
Garage:           Data/Eng/Image/Garage/*.png
Lobby:            Data/Eng/Image/Lobby/*.png
Menu:             Data/Eng/Image/Menu/*.png
Popup:            Data/Eng/Image/Popup/*.png
Room:             Data/Eng/Image/Room/*.png
Shop:             Data/Eng/Image/Shop/*.png
```

### 10.3 Audio Paths

```
Sound effects:    Data/Public/Sound/High/*.wav
```

---

