# ================================================================
# KnC Project - Build System
# ================================================================
cmake_minimum_required(VERSION 3.15)
project(KnC VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# ================================================================
# OUTPUT DIRECTORIES - lowercase
# ================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/debug)

foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOLOWER ${CONFIG} CONFIG_LOWER)
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_SOURCE_DIR}/${CONFIG_LOWER})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_SOURCE_DIR}/build/lib/${CONFIG_LOWER})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_SOURCE_DIR}/build/lib/${CONFIG_LOWER})
endforeach()

# ================================================================
# OPTIONS
# ================================================================
option(BUILD_CLIENT "Build client executable" ON)
option(BUILD_SERVER "Build server executables" ON)
option(BUILD_LAUNCHER "Build launcher" ON)
option(BUILD_TOOLS "Build development tools" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_LOADTEST "Build load testing tools" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)

# ================================================================
# GLOBAL INCLUDE DIRECTORIES
# ================================================================
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/shared/include
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/thirdparty
)

# ================================================================
# THIRDPARTY LIBRARIES - STATIC BUILD (no DLL)
# ================================================================
find_package(OpenGL REQUIRED)

# GLFW - Build as STATIC library
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/glfw)

# GLAD - OpenGL Loader (static)
add_library(glad STATIC thirdparty/glad/src/gl.c)
target_include_directories(glad PUBLIC thirdparty/glad/include)
target_link_libraries(glad PUBLIC ${OPENGL_LIBRARIES})

message(STATUS "Thirdparty libraries configured (STATIC - no DLL)")
message(STATUS "  GLFW: ${CMAKE_SOURCE_DIR}/thirdparty/glfw")
message(STATUS "  GLAD: ${CMAKE_SOURCE_DIR}/thirdparty/glad")

# ================================================================
# SUBDIRECTORIES
# ================================================================
add_subdirectory(shared)
add_subdirectory(engine)

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

if(BUILD_TOOLS)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tools/CMakeLists.txt)
        add_subdirectory(tools)
    endif()
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_LOADTEST)
    add_subdirectory(loadtest)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# ================================================================
# SUMMARY
# ================================================================
message(STATUS "")
message(STATUS "================================================================")
message(STATUS "KnC Build Configuration Summary")
message(STATUS "================================================================")
message(STATUS "  BUILD_CLIENT:     ${BUILD_CLIENT}")
message(STATUS "  BUILD_SERVER:     ${BUILD_SERVER}")
message(STATUS "  BUILD_LAUNCHER:   ${BUILD_LAUNCHER}")
message(STATUS "  BUILD_TOOLS:      ${BUILD_TOOLS}")
message(STATUS "  BUILD_TESTS:      ${BUILD_TESTS}")
message(STATUS "  BUILD_LOADTEST:   ${BUILD_LOADTEST}")
message(STATUS "  BUILD_BENCHMARKS: ${BUILD_BENCHMARKS}")
message(STATUS "================================================================")
