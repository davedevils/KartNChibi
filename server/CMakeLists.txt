# =============================================================================
# KnC Server Emulator - Root CMakeLists.txt
# =============================================================================
# Build: cmake -B build && cmake --build build
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(knc-emu VERSION 1.0.0 LANGUAGES CXX)

# -----------------------------------------------------------------------------
# C++ Standard
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_TOOLS "Build CLI tools" ON)
option(BUILD_WEB_ADMIN "Build web admin panel" ON)

# -----------------------------------------------------------------------------
# Platform Detection
# -----------------------------------------------------------------------------
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00)  # Windows 10+
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
endif()

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# -----------------------------------------------------------------------------
# Output directories
# -----------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# -----------------------------------------------------------------------------
# Dependencies (header-only libraries)
# -----------------------------------------------------------------------------
add_subdirectory(lib)

# -----------------------------------------------------------------------------
# Common library
# -----------------------------------------------------------------------------
add_subdirectory(common)

# -----------------------------------------------------------------------------
# Executables
# -----------------------------------------------------------------------------
add_subdirectory(login-server)
add_subdirectory(game-server)

if(BUILD_WEB_ADMIN)
    add_subdirectory(web-admin)
endif()

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# -----------------------------------------------------------------------------
# Tools
# -----------------------------------------------------------------------------
if(BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== KnC Server Emulator Configuration ===")
message(STATUS "Version:      ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "Tests:        ${BUILD_TESTS}")
message(STATUS "Tools:        ${BUILD_TOOLS}")
message(STATUS "Web Admin:    ${BUILD_WEB_ADMIN}")
message(STATUS "==========================================")
message(STATUS "")

