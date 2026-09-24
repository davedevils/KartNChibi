cmake_minimum_required(VERSION 3.20)
project(knc_clientpatch CXX)

# client is x86 32 bit so the proxy must match or it will not load
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "configure with -A Win32, client is 32 bit")
endif()

add_library(dinput8 SHARED dllmain.cpp spy.cpp pilot.cpp dinput8.def)

target_link_libraries(dinput8 PRIVATE ws2_32 gdiplus)

# agents drive the client through this since they only have a shell
add_executable(knc-pilot knc_pilot_cli.cpp)
set_target_properties(knc-pilot PROPERTIES CXX_STANDARD 17)

set_target_properties(dinput8 PROPERTIES
    OUTPUT_NAME "dinput8"
    PREFIX ""
    CXX_STANDARD 17
)

if(MSVC)
    target_compile_definitions(dinput8 PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_link_options(dinput8 PRIVATE "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/dinput8.def")
    # map file turns a crash rva from the event log into a symbol
    target_link_options(dinput8 PRIVATE "/MAP")
endif()
