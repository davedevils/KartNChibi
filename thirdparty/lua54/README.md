# Lua 5.4 Source

This directory should contain the Lua 5.4.7 source files.

## Setup

1. Download Lua 5.4.7 from https://www.lua.org/ftp/lua-5.4.7.tar.gz
2. Extract `src/*.c` and `src/*.h` into this directory (flat, no subdirectories)
3. Do NOT include `lua.c` or `luac.c` (standalone executables) — they are excluded by CMake but best not to copy them

The CMakeLists.txt will automatically build a `lua54` static library from all `.c` files found here.

When no `.c` files are present, the engine compiles without Lua support (`KNC_HAS_LUA` is not defined).
