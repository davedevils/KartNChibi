cmake_minimum_required(VERSION 3.15)

message(STATUS "Configuring tests...")

# Enable testing
enable_testing()

# Add test subdirectories
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/shared/CMakeLists.txt)
    add_subdirectory(shared)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/engine/CMakeLists.txt)
    add_subdirectory(engine)
endif()

message(STATUS "Tests configured")

