# Recast/Detour Navigation

## Download

Clone or download recastnavigation from GitHub into this directory:

```bash
cd thirdparty/recast
git clone https://github.com/recastnavigation/recastnavigation .
```

Or download a release ZIP from:
https://github.com/recastnavigation/recastnavigation/releases

## Expected Layout

After cloning, the directory should contain:

```
thirdparty/recast/
  Recast/Include/Recast.h
  Recast/Source/*.cpp
  Detour/Include/DetourNavMesh.h
  Detour/Source/*.cpp
  DetourCrowd/Include/DetourCrowd.h
  DetourCrowd/Source/*.cpp
```

## Build

CMake will automatically detect and build Recast/Detour when the source is present.
The engine defines `KNC_HAS_RECAST=1` when available, enabling NavMesh/NavQuery/CrowdManager.
