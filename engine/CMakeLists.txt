# ================================================================
# KnC Engine - Game Engine for Kart N'Chibi
# Core, Graphics, Camera, Models, Input, UI, Audio
# ================================================================
cmake_minimum_required(VERSION 3.15)
project(KnC_Engine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ================================================================
# ENGINE SOURCES - Phase 1: Core Module
# ================================================================

set(ENGINE_SOURCES
    # Precompiled header
    pch.cpp
    
    # RHI (Render Hardware Interface)
    rhi/RHI.cpp
    rhi/backends/OpenGL/OpenGLDevice.cpp
    
    # Core module (Window, Context, Time, Log)
    core/Window.cpp
    core/Context.cpp
    core/TimeSystem.cpp
    core/Log.cpp
    
    # Graphics module (Renderer, Shader, Texture, Mesh, Material)
    graphics/Renderer.cpp
    graphics/Shader.cpp
    graphics/Texture.cpp
    graphics/Mesh.cpp
    graphics/Material.cpp
    
    # Camera module (Camera3D, Camera2D)
    camera/Camera3D.cpp
    camera/Camera2D.cpp
    
    # Models module (Model, Animation)
    models/Model.cpp
    models/Animation.cpp
    
    # Input module (Keyboard, Mouse, Gamepad)
    input/Keyboard.cpp
    input/Mouse.cpp
    input/Gamepad.cpp
    
    # UI module (Draw2D, Font)
    ui/Draw2D.cpp
    ui/Font.cpp
    
    # Audio module (Sound, Music)
    audio/Sound.cpp
    audio/Music.cpp
)

set(ENGINE_HEADERS
    # RHI (Render Hardware Interface)
    rhi/RHI.h
    rhi/IRHIDevice.h
    rhi/RHITypes.h
    rhi/backends/OpenGL/OpenGLDevice.h
    
    # Core module
    core/Window.h
    core/Context.h
    core/TimeSystem.h
    core/Log.h
    
    # Graphics module
    graphics/Renderer.h
    graphics/Shader.h
    graphics/Texture.h
    graphics/Mesh.h
    graphics/Material.h
    
    # Camera module
    camera/Camera3D.h
    camera/Camera2D.h
)

# ================================================================
# ENGINE LIBRARY - Static Library
# ================================================================

add_library(engine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Precompiled Headers (Enterprise-Grade: WindowsLean.h + STL + time headers)
if(MSVC)
    target_precompile_headers(engine PRIVATE pch.h)
    
    # Disable PCH for RHI backend (explicit includes to avoid conflicts)
    set_source_files_properties(
        rhi/backends/OpenGL/OpenGLDevice.cpp
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
    )
endif()

# Set C++17 standard
target_compile_features(engine PUBLIC cxx_std_17)

# Include directories
target_include_directories(engine 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/rhi/backends
)

# Link dependencies
target_link_libraries(engine 
    PUBLIC
        shared
        glfw
        glad
)

# Add GLAD include path explicitly
target_include_directories(engine 
    PUBLIC
        ${CMAKE_SOURCE_DIR}/thirdparty/glad/include
)

message(STATUS "KnC Engine configured (RHI + Phase 1+2: Core + Graphics)")
message(STATUS "  Engine dir: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "  Modules: RHI (1 backend), Core (4), Graphics (5), Camera (2)")

