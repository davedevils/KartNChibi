# Changelog

## Unreleased, 2026-09

### Repository
- Every source comment is one plain line, at most twenty words, letters digits and a few signs. Reverse notes that lived only in deleted comments are in docs/packets/RECOVERED_RE_NOTES.md
- Engine solution generated for Visual Studio 2022, toolset v143, whole solution builds. Build scripts moved to scripts/build and find MSBuild with vswhere
- CMakePresets.json, vs2022 and ninja
- Old plans, audits and session notes cleaned out, generated parity pages and json dumps dropped
- Test run bitmaps dropped from the tree, only tests/output/golden is versioned
- MariaDB Connector/C 3.3.6 checked in for the Windows server build

### Engine
- The bgfx renderer and the NIF reader of the X-Legend editor imported as `engine/render` and `engine/formats`, bgfx bimg bx as pinned submodules, shaders compiled by shaderc at configure time, `tools/model_viewer` draws a KnC NIF, `tools/nif` checks the reader against a client tree. The home RHI, shaders, niflib, nif_import and models stay until their consumers move
- The client light rig: `Light.nif` read once, its ambient and its directional light applied in the vertex shaders, cars draw in their colours
- Client physics reverse: rigid body layer, collision file and BSP, every tick helper, remote car and motion channel, constants, ground and respawn, input and stats, effects and gear, suspension and teleport, one file each under `docs/reverse/physics/`, checked claim by claim against Ghidra
- Client physics port started in `games/kart/physics/client`, its own C++17 project: collision file and BSP, body layer with the gear and tyre model, stats and input, gimmick files, remote car and the 0x40 channel, the 22 step tick with drift, boosts, effects, collision response and respawn, nine tests on the Cookie 01 data, the body motion integration is settled too now
- Root pages README, BUILD, CONTRIBUTING, AGENTS, STATUS, LICENSE back in their original format, docs/formats and docs/packets carry the reverse, every doc checked against the tree, the data, the server code and the binary

### Server
- Login to game handoff by ticket in the profile blob, one account one game
- CPU cars that drive the racing line, take item boxes, fire and get hit, seated on quick match when nobody comes
- Race: places follow the CPU cars, twenty second rest counter ends the race after the first finisher, hit reports echoed to the reporter, race exit lands in the lobby, random track draws a real one
- Shop: everything on sale with the game's own names, three price rows, pets under their real keys, gacha coins, room craft prices
- Room Craft save is the waiting room decor, mission menu and quests filled, ghost of the record just ahead
- Chat commands for a local server, /getmoney /getastro /getexp
- release package, Launch Server.bat with migrations, admin only fresh database

---

## [1.1.0] - 2026-03-01

### Multi-Backend Shaders
- Translated all GLSL shaders to HLSL (DX11/DX12), MSL (Metal), and Vulkan GLSL 450
- Shadow buffer + cbuffer architecture for uniform upload
- 3 constant buffers: PerDraw (b0), PerScene (b1), BoneMatrices (b2)
- Vulkan: shaderc runtime GLSL 450 to SPIR-V compilation
- Auto-detection: backends detect GLSL markers and substitute native shaders

### Gamebryo NIF Rendering (7 Phases + Audit)
- Vertex colors (baked AO from NIF)
- Alpha test wired to shader (cutout transparency)
- Dynamic lighting (4 lights, Blinn-Phong)
- Normal maps, specular maps, tangent generation
- Skeletal animation (NiSkinInstance, GPU skinning, 4 weights/vertex)
- NiMaterialProperty wired to Material (ambient, diffuse, specular, emissive)
- NiTexturingProperty multi-layer (dark, detail, glow, normal, specular)
- UV offset per mesh

### Modern Rendering Pipeline (24 Tasks)
- Post-FX pipeline: HDR ping-pong, tone mapping (ACES), FXAA, bloom
- Shadow maps: directional PCF, cascaded shadow maps (4 cascades, PSSM)
- PBR: Cook-Torrance, metallic/roughness workflow, IBL, BRDF LUT, environment probes
- Optimization: depth pre-pass, frustum culling, octree, instanced drawing, pipeline cache, shader permutations
- Advanced: SSAO, auto-exposure, depth of field, SSR, volumetric fog, god rays
- RenderSettings struct for central feature toggles

### Safety Audit (60+ Fixes)
- All server SQL queries migrated to prepared statements
- All RHI::GetDevice() call sites null-guarded
- NIF loader: bounds checks on all file-read indices
- Model loaders (OBJ/GLTF): buffer bounds validation
- Network: shift overflow, division by zero, partial send guards
- D3D12: fence/swap chain null guards, frame index modulo

### 15-Item Engine Audit
- Fog system (NiFogProperty to shader uniforms)
- Dark/detail/glow texture maps (slots 3-5)
- Shader destructor (shared_ptr reference counting)
- Render states via PipelineDesc
- Collision NIF (bhkCollisionObject), LOD (NiLODNode)
- Billboard nodes (NiBillboardNode), environment mapping (NiTextureEffect)
- Particle system detection (NiParticleSystem skip)
- Network WinSock fixes, delta compression bug fix
- UI PopColor/PopFloat fix

### PAL Abstraction Layer
- Platform init/shutdown orchestration
- PAL::FileSystem (migrated ShaderManager off stat())
- Android/iOS/Web window and input stubs
- WebGPU backend stub for Emscripten
- Platform detection, conditional GLFW, cross-compilation toolchains

### Animation System
- Node transform animation with animBase precomputation
- NIF Z-up to Engine Y-up coordinate conversion (translation + quaternion)
- Parent-matched animation pivot fix (use parentWorld, not meshWorld)
- Blend-to-cutout transparency conversion for non-additive meshes
- Material useBlackKey auto-enable for additive blend

### Project Cleanup
- Organized demos into subfolders (driving/, playable/, legacy/)
- Removed dead tools (nif_debug, ui_extractor, proxy_dll, dat_tool, network_decrypt, replay_viewer, map_editor)
- Removed orphaned code (DdsLoader.cpp)
- Deleted screenshots, one-off PS1 scripts from root
- Updated .gitignore, README.md, STATUS.md
- Updated engine documentation (NIF_PIPELINE.md)

### Bug Fixes
- Matrix4::Inversed(): cofactor matrix was not transposed before dividing by determinant
- Mesh Rule-of-Five: added move ctor/assignment to prevent GPU resource destruction
- OpenGL BC compressed textures: added glCompressedTexImage2D for BC1-BC7
- NIF texture search: FindTextureFile() searches basePath subdirectories (2 levels)
- NIF match group limit raised from 10000 to 65535

---

## [1.0.1] - 2025-12-19

### Changed
- Moved engine tests from root to `tests/engine/` directory
- Renamed test files: `*_demo.cpp` → `*Test.cpp`
- Cleaned up documentation structure
- Archived development session logs to `docs/archive/`

### Files Moved
- `physics_demo.cpp` → `tests/engine/PhysicsTest.cpp`
- `di_demo.cpp` → `tests/engine/DITest.cpp`
- `logger_demo.cpp` → `tests/engine/LoggerTest.cpp`
- `optimizations_demo.cpp` → `tests/engine/OptimizationsTest.cpp`
- `knc_full_demo.cpp` → `tests/engine/IntegrationTest.cpp`

### Removed
- Removed `examples/` directory (not needed)
- Removed excessive marketing documentation
- Cleaned up emoji usage

---

## [1.0.0] - 2025-12-19

### Engine Development Complete

#### Physics Engine
- Custom arcade-style kart physics
- AABB Tree for collision detection (43x performance improvement over O(n²))
- Projectile system (rockets with homing, area damage)
- Dynamic objects (rolling balls)
- Stick-to-ground for looping tracks

#### Networking
- Client-server architecture
- Client prediction with server reconciliation
- Lag compensation (server rewind, client prediction)
- Delta compression (65% bandwidth reduction)
- Automatic reconnection with exponential backoff
- Packet prioritization and reliability

#### Resource Management
- LRU cache system
- Async loading (non-blocking)
- Hot-reload support
- Memory budgets with automatic eviction

#### Architecture
- Full dependency injection (ServiceContainer)
- Interface-based design
- Smart pointer migration (memory safety)
- Result<T, E> error handling
- Zero-cost logging system (6 levels, 9 categories)

#### Game Systems
- GameStateManager (state machine)
- NetworkGameLoop (integrated server/client)
- 4 player support
- Splitscreen rendering

### Performance Metrics
- AABB Tree: 43x faster than brute force
- Delta Compression: 65% bandwidth savings
- Resource Cache: 1000-10000x faster than disk
- Quaternion Compression: 3-5x smaller
- Frame time: 8-10ms (60 FPS stable)

### Documentation
- Complete API reference
- Architecture documentation
- Feature list with benchmarks
- Build instructions
- Phase documentation (archived)

---

## Known Issues

- Pre-existing linker: shaderc_combined.lib CRT mismatch, missing OpenSSL (web-admin)

See `STATUS.md` for what is open right now.
