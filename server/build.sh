#!/bin/bash

echo "==============================================="
echo "          KnC Server Build"
echo "==============================================="
echo ""

mkdir -p build
cd build

echo "Running CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "[ERROR] CMake failed!"
    exit 1
fi

echo "Building..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "[ERROR] Build failed!"
    exit 1
fi

echo "Copying to dist..."
cd ..
mkdir -p dist
cp build/bin/LoginServer dist/
cp build/bin/GameServer dist/

echo ""
echo "==============================================="
echo "  Build successful!"
echo "  Output: dist/LoginServer"
echo "          dist/GameServer"
echo "==============================================="

