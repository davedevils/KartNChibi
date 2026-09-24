#!/bin/bash

echo "Downloading third-party libraries..."

mkdir -p thirdparty/miniaudio
mkdir -p thirdparty/tinyobj
mkdir -p thirdparty/tinygltf
mkdir -p thirdparty/ufbx

success=0
failed=0

echo "Downloading miniaudio (Public Domain)..."
if curl -s -o thirdparty/miniaudio/miniaudio.h https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h; then
    echo "  ✓ Downloaded to thirdparty/miniaudio/miniaudio.h"
    ((success++))
else
    echo "  ✗ Failed to download miniaudio"
    ((failed++))
fi

echo "Downloading stb_truetype (Public Domain)..."
if curl -s -o thirdparty/stb/stb_truetype.h https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h; then
    echo "  ✓ Downloaded to thirdparty/stb/stb_truetype.h"
    ((success++))
else
    echo "  ✗ Failed to download stb_truetype"
    ((failed++))
fi

echo "Downloading tinyobjloader (MIT)..."
if curl -s -o thirdparty/tinyobj/tiny_obj_loader.h https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/release/tiny_obj_loader.h; then
    echo "  ✓ Downloaded to thirdparty/tinyobj/tiny_obj_loader.h"
    ((success++))
else
    echo "  ✗ Failed to download tinyobjloader"
    ((failed++))
fi

echo "Downloading tinygltf (MIT)..."
if curl -s -o thirdparty/tinygltf/tiny_gltf.h https://raw.githubusercontent.com/syoyo/tinygltf/release/tiny_gltf.h; then
    echo "  ✓ Downloaded to thirdparty/tinygltf/tiny_gltf.h"
    ((success++))
else
    echo "  ✗ Failed to download tinygltf"
    ((failed++))
fi

echo "Downloading ufbx header (MIT)..."
if curl -s -o thirdparty/ufbx/ufbx.h https://raw.githubusercontent.com/ufbx/ufbx/master/ufbx.h; then
    echo "  ✓ Downloaded to thirdparty/ufbx/ufbx.h"
    ((success++))
else
    echo "  ✗ Failed to download ufbx header"
    ((failed++))
fi

echo "Downloading ufbx implementation (MIT)..."
if curl -s -o thirdparty/ufbx/ufbx.c https://raw.githubusercontent.com/ufbx/ufbx/master/ufbx.c; then
    echo "  ✓ Downloaded to thirdparty/ufbx/ufbx.c"
    ((success++))
else
    echo "  ✗ Failed to download ufbx implementation"
    ((failed++))
fi

echo ""
echo "Download complete!"
echo "  Success: $success"
echo "  Failed: $failed"

if [ $failed -eq 0 ]; then
    echo ""
    echo "All libraries downloaded successfully! ✓"
    echo "You can now build the engine with full functionality."
else
    echo ""
    echo "Some downloads failed. Please check your internet connection."
    echo "You can manually download missing libraries from:"
    echo "  https://github.com/mackron/miniaudio"
    echo "  https://github.com/nothings/stb"
    echo "  https://github.com/tinyobjloader/tinyobjloader"
    echo "  https://github.com/syoyo/tinygltf"
    echo "  https://github.com/ufbx/ufbx"
fi

