cmake_minimum_required(VERSION 3.16)
project(knc-probe C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "knc-probe must be built for x86 (run vcvarsall.bat x86).")
endif()

enable_testing()

file(GLOB MINHOOK_SRC third_party/minhook/src/*.c third_party/minhook/src/hde/*.c)
add_library(minhook STATIC ${MINHOOK_SRC})
target_include_directories(minhook PUBLIC third_party/minhook/include)
if(MSVC)
    target_compile_options(minhook PRIVATE /W3)
endif()

add_library(knc_probe SHARED
    src/dllmain.cpp
    src/hook_engine.cpp
    src/capture.cpp
    src/safe_mem.cpp
    src/config.cpp
    src/ring_buffer.cpp
    src/sink.cpp
    src/jsonl.cpp
    src/poll.cpp
    src/bp_command.cpp
    src/bp_queue.cpp
    src/bp_serialize.cpp
    src/ghidra_client.cpp
    src/bp_engine.cpp
)
target_include_directories(knc_probe PRIVATE src)
target_link_libraries(knc_probe PRIVATE minhook ws2_32)
target_compile_options(knc_probe PRIVATE /W4 /WX)

add_executable(knc_probe_inject injector/main.cpp)
target_compile_options(knc_probe_inject PRIVATE /W4 /WX)

add_subdirectory(tests)
