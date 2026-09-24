# built by the Dockerfile via cmake -S server -B build -G Ninja shared sits beside server with its binary dir
cmake_minimum_required(VERSION 3.16)
project(KnC_Server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# header only third party libraries asio nlohmann and httplib
add_subdirectory(lib)

# knc-common holds shared net db game and logging code own binary dir since root project already uses build shared
if(NOT TARGET knc-common)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared/src ${CMAKE_CURRENT_BINARY_DIR}/knc-common)
endif()

add_subdirectory(login)
add_subdirectory(game)

# tests server holds the gtest suite wired off by default because gtest fetches over network at configure time
option(KNC_BUILD_TESTS "build the server unit tests" OFF)
if(KNC_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../tests/server/CMakeLists.txt")
    enable_testing()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tests/server ${CMAKE_BINARY_DIR}/tests-server)
endif()

# web-admin needs openssl which the slim image lacks so keep it opt in
option(KNC_BUILD_WEB_ADMIN "build the web admin service" OFF)
if(KNC_BUILD_WEB_ADMIN AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/web-admin/CMakeLists.txt")
    add_subdirectory(web-admin)
endif()
