# ================================================================
# KnC Client
# ================================================================
cmake_minimum_required(VERSION 3.15)
project(KnC_Client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ================================================================
# SOURCES
# ================================================================
file(GLOB_RECURSE CLIENT_SOURCES
    "app/*.cpp"
    "engine/*.cpp"
    "game/*.cpp"
    "net/*.cpp"
    "formats/*.cpp"
    "input/*.cpp"
    "render/*.cpp"
)

file(GLOB_RECURSE CLIENT_HEADERS
    "app/*.h"
    "engine/*.h"
    "game/*.h"
    "net/*.h"
)

# ================================================================
# EXECUTABLE
# ================================================================
add_executable(KnC-Clone ${CLIENT_SOURCES} ${CLIENT_HEADERS})

# Include directories
target_include_directories(KnC-Clone PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/shared/include
    ${CMAKE_SOURCE_DIR}/shared/src/include
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/engine/render/backends/raylib/include
    ${CMAKE_SOURCE_DIR}/engine/ui/backends/raygui
    ${CMAKE_SOURCE_DIR}/thirdparty
)

# Link libraries
target_link_libraries(KnC-Clone PRIVATE
    shared
)

# Raylib
if(EXISTS "${CMAKE_SOURCE_DIR}/engine/render/backends/raylib/lib")
    target_link_libraries(KnC-Clone PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/render/backends/raylib/lib/raylib.lib
    )
endif()

# Windows specific
if(WIN32)
    target_link_libraries(KnC-Clone PRIVATE
        winmm
        gdi32
        opengl32
    )
endif()

message(STATUS "Client executable configured")

