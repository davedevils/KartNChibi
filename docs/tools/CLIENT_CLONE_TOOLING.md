# Tooling and Automation

All tools use KnC Engine for rendering and niflib for NIF loading.

---

## Completed Tools

### Model Viewer v4 (`tools/model_viewer/`)

Status: DONE.

3D model editor for karts, drivers, gimmicks.
- Auto-detect model type (Kart/Driver/Gimmick/Item)
- Kart assembly: Body + 4 wheels with attach points
- Color selection: BODYCOLOR variants
- Animation: load KF, timeline, scrub, play/pause/loop, frame step
- UI: menu bar, motions panel, timeline, part visibility
- Camera: orbit, pan, zoom
- Config persistence

Controls: Mouse orbit, WASD pan, Scroll zoom, Space play/pause, Arrows timeline.

Executable: `tools/model_viewer/build/release_v4/KnC_ModelViewer.exe`

### Map Viewer v2/v3 (`tools/map_viewer/`)

Status: DONE.

3D map visualization with collision data.
- PAK auto-discover
- Layers: geometry, track, sky
- Collision mesh display (.COL)
- Map browser, texture support (DDS with color key)
- FPS camera

Controls: WASD fly, Right-click look, Space/Ctrl up/down, Shift speed, 1-3 layer toggle, PageUp/Down map switch.

Executable: `tools/map_viewer/build/release_v3/KnC_MapViewer_v3.exe`

### DAT Manager (`tools/dat_manager/`)

Status: DONE.

PAK file browser and extractor.
- List/tree view
- Filter: images, models, audio
- Preview: DDS textures, NIF models (3D), WAV/OGG playback
- Extract: single or batch
- Search by filename

Executable: `tools/dat_manager/build/release/KnC_DatManager.exe`

### COL Analyzer (`tools/col_analyze.cpp`)

Status: DONE.

Collision file format analyzer. Parses header (version, vertex count, face count, zone count), searches for text markers (START, BOOST, ITEM, REGEN, GOAL).

Usage: `col_analyze.exe "path/to/track.COL"`

### Race Prototype (`tools/race_prototype/`)

Status: in progress. Structure defined, features pending.

### EngineDLL Proxy (`tools/proxy_dll/`)

Status: DONE.

DLL proxy for debugging EngineDLL calls. Intercepts NK_instantiateClass, logs calls, dumps vtables.

---

## Legacy Tools

| Tool | Location | Purpose |
|------|----------|---------|
| KNC Un-Packer | DevClient/ | Extract PAK files |
| Network2 Decrypt | DevClient/ | Decrypt Network2.ini |

## Third-Party

| Tool | Purpose |
|------|---------|
| NifSkope | NIF viewer/editor |
| IDA Pro | Disassembly |
| Ghidra | Free disassembly |
| x64dbg | Debugging (x32dbg) |
| Wireshark | Packet capture |
| HxD | Hex editor |

---

## Shared Components

All tools share:

- NifToRHI: NIF model loading via engine/nif_import/
- PakReader: PAK file access
- DDS loading via KnC Engine / stb_image
- Color key: RGB(0,0,255) = transparent
- INI config persistence

Build: CMake (tools/CMakeLists.txt). Deps: KnCEngine, niflib, tinyfiledialogs.

---

## Future Tools

### UI Editor (`tools/ui_editor/`)

Status: in progress. Multi-state UI editor, asset browser, visual placement, JSON export.

### Planned

- CAR File Editor: vehicle stat editing with 3D preview
- Track Editor: 3D, checkpoints, waypoints, item boxes, collision
- Packet Analyzer: real-time capture, protocol decode, replay
- Animation Editor: skeletal, keyframe, blending, KF export

---

## Test Programs

```
tests/intro/    test_intro_exact.cpp, test_intro_render.cpp
tests/misc/     test_engine.cpp, test_nif_load.cpp, test_pak_load.cpp
tests/speedo/   speedometer_demo.cpp
```

Analysis tools in `tools/dat_manager/`: pak_analyze, pak_dump, pak_scan, diagnose_nif, dump_nif.

---

## Tool Stats

| Tool | LOC | Status |
|------|-----|--------|
| Model Viewer v4 | ~2100 | DONE |
| Map Viewer v2 | ~2600 | DONE |
| Map Viewer v3 | ~680 | DONE |
| DAT Manager | ~1300 | DONE |
| Race Prototype | ~470 | In progress |
| COL Analyzer | ~106 | DONE |
| Proxy DLL | ~500 | DONE |

---

## Technical Notes

1. NIF Version: Gamebryo 10.2.0.0 (User Version 0). niflib works, nifly does not.
2. Textures: color key RGB(0,0,255) = transparent. DXT1/3/5. Alpha discard shader.
3. Kart assembly: BODY.nif + WHEEL1-4.nif. Attach points: O_WHEEL01-04. Colors in BODYCOLOR/{color}/.
4. Animations: .KF keyframe data, .KFM animation managers, target nodes must match NIF hierarchy.
5. PAK: NKZIP header, ~270+ bytes per entry, inline file data.

---

## Launch Commands

```bash
# Model Viewer
tools/model_viewer/build/release_v4/KnC_ModelViewer.exe

# Map Viewer
tools/map_viewer/build/release_v3/KnC_MapViewer_v3.exe

# DAT Manager
tools/dat_manager/build/release/KnC_DatManager.exe

# COL Analyzer
tools/col_analyze.exe "path/to/track.COL"
```

## Key Files

- `engine/nif_import/NifToRHI.h` - NIF loading
- `shared/src/PakReader.h` - PAK access
- `tools/model_viewer/model_viewer_v4.cpp` - UI/3D integration reference
- `tools/map_viewer/map_viewer_v2.cpp` - large scene handling reference
