# Lua 5.4 static library
# Place Lua 5.4.7 source files (*.c, *.h) in this directory.
# Download from: https://www.lua.org/ftp/lua-5.4.7.tar.gz
#
# Exclude lua.c and luac.c (standalone interpreter/compiler executables).

cmake_minimum_required(VERSION 3.15)

file(GLOB LUA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

# Remove standalone executables from the library build
list(FILTER LUA_SOURCES EXCLUDE REGEX "lua\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX "luac\\.c$")

if(LUA_SOURCES)
    add_library(lua54 STATIC ${LUA_SOURCES})
    target_include_directories(lua54 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

    # Lua is C code — ensure it compiles as C even in a C++ project
    set_target_properties(lua54 PROPERTIES LINKER_LANGUAGE C)

    if(WIN32)
        # Suppress CRT warnings in Lua source
        target_compile_definitions(lua54 PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
else()
    message(STATUS "Lua 5.4 source not found in ${CMAKE_CURRENT_SOURCE_DIR} — lua54 target NOT created")
endif()
