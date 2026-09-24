#!/bin/bash

set -e
cd "$(dirname "$0")/../.."

if ! command -v doxygen &>/dev/null; then
    echo "ERROR: Doxygen not found. Install with your package manager."
    exit 1
fi

echo "=== KnC Engine Documentation Generator ==="
echo ""

if [ -d "docs/engine/html" ]; then
    echo "Cleaning previous output..."
    rm -rf "docs/engine/html"
fi

echo "Generating documentation..."
doxygen Doxyfile

if [ -f "docs/engine/html/index.html" ]; then
    echo ""
    echo "Documentation generated successfully!"
    echo "Output: docs/engine/html/"
    echo "Open: docs/engine/html/index.html"
else
    echo "ERROR: Generation failed"
    exit 1
fi
