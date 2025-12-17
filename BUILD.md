# KnC - Build Guide

## Build Structure

```
@git/
├── build/              # Intermediate files (.obj, .lib, CMake cache)
├── Release/            # Release executables (.exe, .dll)
├── Debug/              # Debug executables (.exe, .dll)
└── data/               # Runtime assets
    └── ui/             # 55 UI JSON files
```

---

## Quick Start

### Build Release (Recommended)
```batch
scripts\build.bat Release
```

### Build Debug
```batch
scripts\build.bat Debug
```

### Clean + Build
```batch
scripts\build.bat Release clean
```

---

## Output Files

After compilation, all executables will be in:

- **Release/** - Optimized builds (production)
- **Debug/** - Builds with debug symbols

### Generated Executables

| Executable | Description | Path |
|------------|-------------|------|
| `KartNChibi.exe` | Main client | release/KartNChibi.exe |
| `ui_editor.exe` | UI editor | Release/ui_editor.exe |
| `map_viewer.exe` | Map viewer | Release/map_viewer.exe |
| `model_viewer.exe` | 3D model viewer | Release/model_viewer.exe |
| `dat_manager.exe` | PAK manager | Release/dat_manager.exe |

---

## Manual Build (CMake)

If you want to configure manually:

```batch
# 1. Create build directory
mkdir build
cd build

# 2. Configure with CMake
cmake .. -G "Visual Studio 17 2022" -A Win32

# 3. Compile
cmake --build . --config Release

# Executables will be in Release/ at root
```

---

## Build Options

You can disable certain components in `CMakeLists.txt`:

```cmake
option(BUILD_CLIENT "Build client executable" ON)
option(BUILD_SERVER "Build server executables" ON)
option(BUILD_LAUNCHER "Build launcher" ON)
option(BUILD_TOOLS "Build development tools" ON)
option(BUILD_TESTS "Build tests" OFF)
```

---

## Cleanup

To clean completely:

```batch
rmdir /s /q build
rmdir /s /q Release
rmdir /s /q Debug
```

Or using the script:

```batch
scripts\build.bat clean
```

---

## Prerequisites

- **Visual Studio 2022** or **2019** (with C++ desktop development)
- **CMake 3.15+** (included with VS)
- **Windows SDK**

---

## Common Issues

### "vcvars32.bat not found"
→ Install Visual Studio with "Desktop development with C++"

### "CMake not found"
→ Verify CMake is in PATH or use CMake included with VS

### "Raylib errors"
→ Raylib is now integrated in `engine/render/backends/raylib/`

### "UI JSON not found"
→ UI JSON files must be in `data/ui/` (55 files)

---

## Project Structure

```
@git/
├── client/             # Client source code
├── server/             # Server source code
├── engine/             # Engine (render, ui, nif, animation)
├── common/             # Shared code (network, packets, parsers)
├── tools/              # Development tools
├── include/            # Public headers
├── thirdparty/         # External dependencies
├── data/               # Runtime assets
├── tests/              # Unit tests
├── scripts/            # Build/deploy scripts
└── CMakeLists.txt      # Root CMake configuration
```

---

## Development Workflow

1. **Modify code** in `client/`, `engine/`, `tools/`, etc.
2. **Recompile** with `scripts\build.bat`
3. **Test** executable in `Release/` or `Debug/`
4. **Repeat**

---

## Notes

- `.obj`, `.lib` files go in `build/`
- `.exe`, `.dll` files go in `Release/` or `Debug/`
- UI JSON files are in `data/ui/` (centralized)

