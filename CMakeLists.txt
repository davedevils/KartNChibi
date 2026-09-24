cmake_minimum_required(VERSION 3.15)
project(KnC VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(KNC_PLATFORM "Android")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(KNC_PLATFORM "iOS")
elseif(EMSCRIPTEN)
    set(KNC_PLATFORM "Web")
elseif(WIN32)
    set(KNC_PLATFORM "Windows")
elseif(APPLE)
    set(KNC_PLATFORM "macOS")
else()
    set(KNC_PLATFORM "Linux")
endif()
message(STATUS "KnC Target Platform: ${KNC_PLATFORM}")

# Platform groups
if(KNC_PLATFORM STREQUAL "Windows" OR KNC_PLATFORM STREQUAL "macOS" OR KNC_PLATFORM STREQUAL "Linux")
    set(KNC_DESKTOP ON)
else()
    set(KNC_DESKTOP OFF)
endif()

if(KNC_PLATFORM STREQUAL "Android" OR KNC_PLATFORM STREQUAL "iOS")
    set(KNC_MOBILE ON)
else()
    set(KNC_MOBILE OFF)
endif()

# GLFW only on desktop
if(KNC_DESKTOP)
    set(KNC_HAS_GLFW ON)
else()
    set(KNC_HAS_GLFW OFF)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/debug)

foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOLOWER ${CONFIG} CONFIG_LOWER)
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_SOURCE_DIR}/${CONFIG_LOWER})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_SOURCE_DIR}/build/lib/${CONFIG_LOWER})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_SOURCE_DIR}/build/lib/${CONFIG_LOWER})
endforeach()

option(BUILD_CLIENT "Build client executable" ON)
option(BUILD_SERVER "Build server executables" ON)
option(BUILD_LAUNCHER "Build launcher" ON)
option(BUILD_TOOLS "Build development tools" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_LOADTEST "Build load testing tools" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(KNC_BUILD_RENDER "Build the bgfx render module and its tools" ON)

if(WIN32)
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/shared/include
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/thirdparty
)

if(KNC_HAS_GLFW)
    find_package(OpenGL REQUIRED)

    # GLFW - Build as STATIC library
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    add_subdirectory(thirdparty/glfw)

    message(STATUS "Thirdparty libraries configured (STATIC - no DLL)")
    message(STATUS "  GLFW: ${CMAKE_SOURCE_DIR}/thirdparty/glfw")
endif()

# STB - Image loading
add_subdirectory(thirdparty/stb)

# bgfx bimg and bx as static libraries the render module draws through them
if(KNC_BUILD_RENDER)
    set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(BGFX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
    set(BGFX_CUSTOM_TARGETS OFF CACHE BOOL "" FORCE)
    add_subdirectory(thirdparty/bgfx)
endif()

add_subdirectory(shared)
add_subdirectory(engine)
add_subdirectory(games/kart)
if(EXISTS ${CMAKE_SOURCE_DIR}/games/fps/CMakeLists.txt)
    add_subdirectory(games/fps)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/games/rpg/CMakeLists.txt)
    add_subdirectory(games/rpg)
endif()

if(BUILD_CLIENT)
    add_subdirectory(client)
endif()

if(BUILD_SERVER)
    add_subdirectory(server)
endif()

if(BUILD_LAUNCHER)
    if(EXISTS ${CMAKE_SOURCE_DIR}/launcher/CMakeLists.txt)
        add_subdirectory(launcher)
    endif()
endif()

if(KNC_DESKTOP AND BUILD_TOOLS)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tools/CMakeLists.txt)
        add_subdirectory(tools)
    endif()
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(KNC_DESKTOP)
    if(EXISTS ${CMAKE_SOURCE_DIR}/demos/CMakeLists.txt)
        add_subdirectory(demos)
    endif()
endif()

if(BUILD_LOADTEST AND EXISTS ${CMAKE_SOURCE_DIR}/loadtest/CMakeLists.txt)
    add_subdirectory(loadtest)
endif()

if(BUILD_BENCHMARKS AND EXISTS ${CMAKE_SOURCE_DIR}/benchmarks/CMakeLists.txt)
    add_subdirectory(benchmarks)
endif()

message(STATUS "")
message(STATUS "================================================================")
message(STATUS "KnC Build Configuration Summary")
message(STATUS "================================================================")
message(STATUS "  Platform:         ${KNC_PLATFORM}")
message(STATUS "  Desktop:          ${KNC_DESKTOP}")
message(STATUS "  Mobile:           ${KNC_MOBILE}")
message(STATUS "  GLFW:             ${KNC_HAS_GLFW}")
message(STATUS "  BUILD_CLIENT:     ${BUILD_CLIENT}")
message(STATUS "  BUILD_SERVER:     ${BUILD_SERVER}")
message(STATUS "  BUILD_LAUNCHER:   ${BUILD_LAUNCHER}")
message(STATUS "  BUILD_TOOLS:      ${BUILD_TOOLS}")
message(STATUS "  BUILD_TESTS:      ${BUILD_TESTS}")
message(STATUS "  BUILD_LOADTEST:   ${BUILD_LOADTEST}")
message(STATUS "  BUILD_BENCHMARKS: ${BUILD_BENCHMARKS}")
message(STATUS "================================================================")
