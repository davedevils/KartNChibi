cmake_minimum_required(VERSION 3.15)
project(KnC-MITM CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# reuse the server Packet so the injected direct login is byte exact with the wire
set(SHARED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/src)

add_executable(mitmprivategg
    main.cpp
    ${SHARED_DIR}/src/net/Packet.cpp
)

target_include_directories(mitmprivategg PRIVATE ${SHARED_DIR}/include)
target_link_libraries(mitmprivategg ws2_32)
