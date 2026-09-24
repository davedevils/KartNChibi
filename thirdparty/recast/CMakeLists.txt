# Recast/Detour Navigation — static library
# Downloads: https://github.com/recastnavigation/recastnavigation
#
# Expected layout after cloning:
#   thirdparty/recast/Recast/Include/Recast.h
#   thirdparty/recast/Recast/Source/*.cpp
#   thirdparty/recast/Detour/Include/DetourNavMesh.h
#   thirdparty/recast/Detour/Source/*.cpp
#   thirdparty/recast/DetourCrowd/Include/DetourCrowd.h
#   thirdparty/recast/DetourCrowd/Source/*.cpp

cmake_minimum_required(VERSION 3.15)

# Guard: only build if source is present
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Recast/Include/Recast.h)
    message(STATUS "Recast/Detour: DISABLED (source not present)")
    return()
endif()

message(STATUS "Recast/Detour: ENABLED")

file(GLOB RECAST_SRC      Recast/Source/*.cpp)
file(GLOB DETOUR_SRC      Detour/Source/*.cpp)
file(GLOB DETOURCROWD_SRC DetourCrowd/Source/*.cpp)

add_library(recast STATIC
    ${RECAST_SRC}
    ${DETOUR_SRC}
    ${DETOURCROWD_SRC}
)

target_include_directories(recast PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Recast/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Detour/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/DetourCrowd/Include
)

# Suppress warnings in third-party code
if(MSVC)
    target_compile_options(recast PRIVATE /W0)
else()
    target_compile_options(recast PRIVATE -w)
endif()
