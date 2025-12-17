#!/bin/bash
# Download dependencies for KnC Server

set -e

echo "=== Downloading KnC Server Dependencies ==="

mkdir -p lib/asio/include lib/json/include/nlohmann lib/httplib

# ASIO standalone
ASIO_VERSION="asio-1-30-2"
if [ ! -f "lib/asio/include/asio.hpp" ]; then
    echo "Downloading ASIO..."
    curl -sL "https://github.com/chriskohlhoff/asio/archive/refs/tags/${ASIO_VERSION}.tar.gz" | \
        tar xz -C lib/asio/temp --strip-components=3 "asio-${ASIO_VERSION}/asio/include"
    mv lib/asio/temp/* lib/asio/include/
    rmdir lib/asio/temp
    echo "ASIO downloaded!"
else
    echo "ASIO already present"
fi

# nlohmann/json
if [ ! -f "lib/json/include/nlohmann/json.hpp" ]; then
    echo "Downloading nlohmann/json..."
    curl -sL "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp" \
        -o lib/json/include/nlohmann/json.hpp
    echo "nlohmann/json downloaded!"
else
    echo "nlohmann/json already present"
fi

# cpp-httplib
if [ ! -f "lib/httplib/httplib.h" ]; then
    echo "Downloading cpp-httplib..."
    curl -sL "https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h" \
        -o lib/httplib/httplib.h
    echo "cpp-httplib downloaded!"
else
    echo "cpp-httplib already present"
fi

echo ""
echo "=== All dependencies ready! ==="
echo "Run: cmake -B build && cmake --build build"

