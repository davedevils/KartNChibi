cmake_minimum_required(VERSION 3.15)
project(KnC-Headless CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# the netcode lives in client net now the headless links the same library as the client
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../client/net ${CMAKE_BINARY_DIR}/client_net)

add_executable(KnC-Headless
    main.cpp
    Drive.cpp
)

target_link_libraries(KnC-Headless PRIVATE KnCClientNet)
