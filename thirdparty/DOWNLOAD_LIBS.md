# Third party libraries

What the tree needs and where each part comes from. Most are single headers,
MIT or public domain. They are checked in, this file says where to refresh them.

## Database

### MariaDB Connector/C 3.3.6 (LGPL 2.1)
- URL: https://mariadb.com/downloads/connectors/connectors-data-access/c-connector
- Checked in under `thirdparty/mariadb-connector-c/` with `include/`, `lib/libmariadb.lib` and `lib/libmariadb.dll`, the Windows x64 SDK
- The server build on Windows links it, the Docker image uses the distro package instead
- To refresh, take the MSI or zip for Windows x64, copy `include` and the two lib files

## Audio

### miniaudio (Public Domain / MIT-0)
- **URL**: https://github.com/mackron/miniaudio
- **File**: `miniaudio.h` (single header)
- **Location**: `thirdparty/miniaudio/miniaudio.h`
- **Download**: 
  ```bash
  curl -o thirdparty/miniaudio/miniaudio.h https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h
  ```

## Font Rendering

### stb_truetype (Public Domain / MIT)
- **URL**: https://github.com/nothings/stb
- **File**: `stb_truetype.h` (single header)
- **Location**: `thirdparty/stb/stb_truetype.h`
- **Download**:
  ```bash
  curl -o thirdparty/stb/stb_truetype.h https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h
  ```

## 3D Model Loading

### tinyobjloader (MIT)
- **URL**: https://github.com/tinyobjloader/tinyobjloader
- **File**: `tiny_obj_loader.h` (single header)
- **Location**: `thirdparty/tinyobj/tiny_obj_loader.h`
- **Download**:
  ```bash
  curl -o thirdparty/tinyobj/tiny_obj_loader.h https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/release/tiny_obj_loader.h
  ```

### tinygltf (MIT)
- **URL**: https://github.com/syoyo/tinygltf
- **File**: `tiny_gltf.h` (single header)
- **Location**: `thirdparty/tinygltf/tiny_gltf.h`
- **Download**:
  ```bash
  curl -o thirdparty/tinygltf/tiny_gltf.h https://raw.githubusercontent.com/syoyo/tinygltf/release/tiny_gltf.h
  ```

### ufbx (MIT)
- **URL**: https://github.com/ufbx/ufbx
- **File**: `ufbx.h` (single header) + `ufbx.c` (implementation)
- **Location**: `thirdparty/ufbx/`
- **Download**:
  ```bash
  curl -o thirdparty/ufbx/ufbx.h https://raw.githubusercontent.com/ufbx/ufbx/master/ufbx.h
  curl -o thirdparty/ufbx/ufbx.c https://raw.githubusercontent.com/ufbx/ufbx/master/ufbx.c
  ```

## Already Included

✅ **stb_image** - Public Domain (image loading)
✅ **glad** - Public Domain (OpenGL loader)
✅ **GLFW** - Zlib License (window/input)
✅ **niflib** - BSD License (NIF format)

## Download All (Windows PowerShell)

```powershell
# Create directories
New-Item -ItemType Directory -Force -Path thirdparty/miniaudio
New-Item -ItemType Directory -Force -Path thirdparty/tinyobj
New-Item -ItemType Directory -Force -Path thirdparty/tinygltf
New-Item -ItemType Directory -Force -Path thirdparty/ufbx

# Download miniaudio
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" -OutFile "thirdparty/miniaudio/miniaudio.h"

# Download stb_truetype
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h" -OutFile "thirdparty/stb/stb_truetype.h"

# Download tinyobjloader
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/release/tiny_obj_loader.h" -OutFile "thirdparty/tinyobj/tiny_obj_loader.h"

# Download tinygltf
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/syoyo/tinygltf/release/tiny_gltf.h" -OutFile "thirdparty/tinygltf/tiny_gltf.h"

# Download ufbx
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/ufbx/ufbx/master/ufbx.h" -OutFile "thirdparty/ufbx/ufbx.h"
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/ufbx/ufbx/master/ufbx.c" -OutFile "thirdparty/ufbx/ufbx.c"

Write-Host "All libraries downloaded successfully!"
```

## Download All (Linux/macOS)

```bash
#!/bin/bash
# Create directories
mkdir -p thirdparty/miniaudio
mkdir -p thirdparty/tinyobj
mkdir -p thirdparty/tinygltf
mkdir -p thirdparty/ufbx

# Download miniaudio
curl -o thirdparty/miniaudio/miniaudio.h https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h

# Download stb_truetype
curl -o thirdparty/stb/stb_truetype.h https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h

# Download tinyobjloader
curl -o thirdparty/tinyobj/tiny_obj_loader.h https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/release/tiny_obj_loader.h

# Download tinygltf
curl -o thirdparty/tinygltf/tiny_gltf.h https://raw.githubusercontent.com/syoyo/tinygltf/release/tiny_gltf.h

# Download ufbx
curl -o thirdparty/ufbx/ufbx.h https://raw.githubusercontent.com/ufbx/ufbx/master/ufbx.h
curl -o thirdparty/ufbx/ufbx.c https://raw.githubusercontent.com/ufbx/ufbx/master/ufbx.c

echo "All libraries downloaded successfully!"
```

## License Summary

All libraries are GitHub-compatible:
- **Public Domain**: miniaudio, stb_truetype, stb_image, glad
- **MIT**: tinyobjloader, tinygltf, ufbx
- **Zlib**: GLFW (very permissive, GitHub OK)
- **BSD**: niflib (very permissive, GitHub OK)

No GPL or restrictive licenses! ✅

