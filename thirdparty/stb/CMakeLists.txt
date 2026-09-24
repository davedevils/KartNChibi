# STB Image Library
cmake_minimum_required(VERSION 3.15)

# Create a simple CPP file to wrap stb_image (C++ compatible)
add_library(stb_image STATIC stb_image_impl.cpp)

target_include_directories(stb_image PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# No PCH for this library
set_target_properties(stb_image PROPERTIES 
    SKIP_PRECOMPILE_HEADERS ON
)

