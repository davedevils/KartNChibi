cmake_minimum_required(VERSION 3.15)
project(SharedTests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_shared_types
    test_types.cpp
)

target_link_libraries(test_shared_types PRIVATE shared_core)

enable_testing()
add_test(NAME SharedTypes COMMAND test_shared_types)

message(STATUS "Shared tests configured")

add_executable(test_math test_math.cpp)
target_include_directories(test_math PRIVATE
    ${CMAKE_SOURCE_DIR}/shared/include
    ${CMAKE_SOURCE_DIR}/tests/framework
)
target_link_libraries(test_math PRIVATE shared)
set_target_properties(test_math PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/release/tests"
)
add_test(NAME test_math COMMAND test_math --auto)

