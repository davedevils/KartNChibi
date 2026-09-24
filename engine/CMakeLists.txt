
cmake_minimum_required(VERSION 3.15)

# the old engine only builds when its source tree is present kept until games kart moves off it
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/graphics/Shader.cpp)

# Add xinput library for gamepad vibration on Windows
if(WIN32)
    set(XINPUT_LIBRARY "xinput")
else()
    set(XINPUT_LIBRARY "")
endif()

add_library(KnCEngine STATIC
    core/debug/Log.cpp
    core/debug/Profiler.cpp

    core/platform/Window.cpp
    core/platform/TimeSystem.cpp
    core/platform/ResolutionManager.cpp

    core/io/BinaryStream.cpp

    core/services/ServiceManager.cpp

    core/jobs/TaskSystem.cpp

    scene/SceneSerializer.cpp

    graphics/Shader.cpp
    graphics/Texture.cpp
    graphics/Material.cpp
    graphics/Mesh.cpp
    graphics/RenderTexture.cpp
    graphics/ImmediateRenderer2D.cpp
    graphics/ShaderManager.cpp
    graphics/FullScreenQuad.cpp
    graphics/InstanceBatch.cpp

    renderer/Renderer.cpp
    renderer/Viewport.cpp
    renderer/culling/Frustum.cpp
    renderer/culling/Octree.cpp
    renderer/culling/HZBOcclusion.cpp
    renderer/shadows/ShadowMap.cpp
    renderer/shadows/CascadedShadowMap.cpp
    renderer/lighting/EnvironmentMap.cpp
    renderer/lighting/EnvironmentProbe.cpp
    renderer/postfx/PostFXPipeline.cpp
    renderer/postfx/ToneMappingPass.cpp
    renderer/postfx/FXAAPass.cpp
    renderer/postfx/BloomPass.cpp
    renderer/postfx/SSAOPass.cpp
    renderer/postfx/SSRPass.cpp
    renderer/postfx/AutoExposurePass.cpp
    renderer/postfx/DOFPass.cpp
    renderer/postfx/VolumetricFogPass.cpp

    # Scene graph
    scene/SceneNode.cpp

    # Models
    models/Model.cpp

    animation/Animation.cpp
    animation/SkeletalAnimator.cpp
    animation/AnimSequence.cpp
    animation/AnimBlender.cpp
    animation/MorphTarget.cpp
    animation/AnimStateMachine.cpp

    # Camera
    camera/Camera3D.cpp
    camera/Camera2D.cpp
    camera/CameraController.cpp

    # Input
    input/Keyboard.cpp
    input/Mouse.cpp
    input/Gamepad.cpp
    input/KeyboardLayout.cpp
    input/GamepadMapping.cpp
    input/VibrationSettings.cpp
    input/AndroidVibration.cpp

    # Audio
    audio/Sound.cpp
    audio/Music.cpp
    audio/Audio3D.cpp
    audio/ReverbNode.cpp
    audio/MotorSound.cpp

    # UI
    ui/Font.cpp
    ui/Draw2D.cpp
    ui/ToolkitUI.cpp

    rhi/RHI.cpp
    rhi/RHITypes.cpp
    rhi/PipelineCache.cpp

    # Shader registry
    shaders/ShaderRegistry.cpp
    shaders/compiler/ShaderCompiler.cpp

    nif_import/NifToRHI.cpp

    # Particles
    particles/ParticleSystem.cpp
    particles/PSEmitter.cpp
    particles/PSParticleSystem.cpp
    particles/PSForce.cpp
    particles/PSCollider.cpp
    particles/PSSpawner.cpp
    particles/PSBillboardRenderer.cpp

    # Terrain
    terrain/Terrain.cpp
    terrain/TerrainBlock.cpp
    terrain/TerrainMaterial.cpp
    terrain/TerrainSector.cpp
    terrain/TerrainPager.cpp
    terrain/TerrainDecal.cpp

    # Environment
    environment/Sky.cpp
    environment/Atmosphere.cpp
    environment/TimeOfDay.cpp
    environment/EnvironmentService.cpp

    # Scripting
    scripting/LuaState.cpp
    scripting/LuaBindings.cpp
    scripting/LuaService.cpp

    # ECS
    ecs/EntityService.cpp

    core/platform/Time.cpp
    core/platform/Network.cpp
    core/platform/FileSystem.cpp
    core/platform/Platform.cpp
    core/platform/impl/NullInput.cpp
    core/platform/impl/DesktopHaptics.cpp

    ai/NavMesh.cpp
    ai/NavQuery.cpp
    ai/CrowdManager.cpp
    ai/NavMeshService.cpp

    # Messaging
    messaging/MessageBus.cpp
    messaging/MessageBusService.cpp

    replication/EntitySnapshot.cpp
    replication/NetChannel.cpp
    replication/ReplicationService.cpp

    framework/GameStateMachine.cpp
    framework/SaveSystem.cpp
)

if(NOT ANDROID AND NOT EMSCRIPTEN)
    target_sources(KnCEngine PRIVATE
        core/platform/impl/GLFWWindow.cpp
        core/platform/impl/GLFWInput.cpp
    )
endif()

if(ANDROID)
    target_sources(KnCEngine PRIVATE
        core/platform/impl/AndroidWindow.cpp
        core/platform/impl/AndroidInput.cpp
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_sources(KnCEngine PRIVATE
        core/platform/impl/iOSWindow.mm
        core/platform/impl/iOSInput.mm
    )
endif()

if(EMSCRIPTEN)
    target_sources(KnCEngine PRIVATE
        core/platform/impl/WebWindow.cpp
        core/platform/impl/WebInput.cpp
    )
endif()

# asio is header only and standalone used by core platform Network cpp
target_include_directories(KnCEngine PRIVATE ${CMAKE_SOURCE_DIR}/server/lib/asio/include)

target_include_directories(KnCEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/shared/include
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${CMAKE_SOURCE_DIR}/thirdparty/stb
    ${CMAKE_SOURCE_DIR}/thirdparty/miniaudio
    ${CMAKE_SOURCE_DIR}/thirdparty/tinyddsloader
    ${CMAKE_SOURCE_DIR}/thirdparty/libjpeg-turbo/include
    ${CMAKE_SOURCE_DIR}/thirdparty/tinyobjloader
    ${CMAKE_SOURCE_DIR}/thirdparty/tinygltf
    ${CMAKE_SOURCE_DIR}/thirdparty/ufbx
    ${CMAKE_SOURCE_DIR}/engine/niflib/include
    ${CMAKE_SOURCE_DIR}/engine/rhi
)

if(KNC_HAS_GLFW)
    target_include_directories(KnCEngine PUBLIC
        ${CMAKE_SOURCE_DIR}/thirdparty/glfw/include
    )
endif()

target_link_libraries(KnCEngine PUBLIC
    shared
    stb_image
    niflib
    ${XINPUT_LIBRARY}
)

if(KNC_HAS_GLFW)
    target_link_libraries(KnCEngine PUBLIC glfw)
endif()

# Platform-specific RHI backends
if(WIN32)
    target_link_libraries(KnCEngine PUBLIC RHI_DirectX12)
endif()

# Platform-specific settings
if(WIN32)
    target_compile_definitions(KnCEngine PUBLIC
        _CRT_SECURE_NO_WARNINGS
        USE_STB_TRUETYPE
        # NOMINMAX and WIN32 settings are handled by WindowsLean h
    )
    # ws2 32 is needed by asio in core platform Network cpp
    target_link_libraries(KnCEngine PUBLIC
        winmm
        ws2_32
        gdi32
        opengl32
    )
endif()

# PCH is disabled for stb image impl and Texture cpp to avoid macro conflicts
set_source_files_properties(stb_image_impl.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(graphics/Texture.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# Add subdirectories
add_subdirectory(niflib)
add_subdirectory(physics)
add_subdirectory(network)

if(WIN32)
    add_subdirectory(rhi/dx12)
endif()
# vulkan is cross platform across windows linux and android
add_subdirectory(rhi/vulkan)
if(EMSCRIPTEN)
    add_subdirectory(rhi/webgpu)
endif()

if(TARGET RHI_Vulkan)
    target_link_libraries(KnCEngine PUBLIC RHI_Vulkan)
    target_compile_definitions(KnCEngine PUBLIC KNC_HAS_VULKAN=1)
endif()

if(TARGET RHI_WebGPU)
    target_link_libraries(KnCEngine PUBLIC RHI_WebGPU)
    target_compile_definitions(KnCEngine PUBLIC KNC_HAS_WEBGPU=1)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/CMakeLists.txt)
    add_subdirectory(resources)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ui/CMakeLists.txt)
    add_subdirectory(ui)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/thirdparty/lua54/lua.h)
    add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/lua54 ${CMAKE_BINARY_DIR}/thirdparty/lua54)
    if(TARGET lua54)
        target_link_libraries(KnCEngine PUBLIC lua54)
        target_compile_definitions(KnCEngine PUBLIC KNC_HAS_LUA=1)
        target_include_directories(KnCEngine PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/lua54)
        message(STATUS "Lua 5.4 scripting: ENABLED")
    endif()
else()
    message(STATUS "Lua 5.4 scripting: DISABLED (place source in thirdparty/lua54/)")
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/thirdparty/recast/Recast/Include/Recast.h)
    add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/recast ${CMAKE_BINARY_DIR}/thirdparty/recast)
    if(TARGET recast)
        target_link_libraries(KnCEngine PUBLIC recast)
        target_compile_definitions(KnCEngine PUBLIC KNC_HAS_RECAST=1)
        target_include_directories(KnCEngine PUBLIC
            ${CMAKE_SOURCE_DIR}/thirdparty/recast/Recast/Include
            ${CMAKE_SOURCE_DIR}/thirdparty/recast/Detour/Include
            ${CMAKE_SOURCE_DIR}/thirdparty/recast/DetourCrowd/Include
        )
        message(STATUS "Recast/Detour navigation: ENABLED")
    endif()
else()
    message(STATUS "Recast/Detour navigation: DISABLED (place source in thirdparty/recast/)")
endif()

endif()

# The asset readers and the bgfx renderer the base of the client rewrite and the public build
add_subdirectory(formats)
if(KNC_BUILD_RENDER)
    add_subdirectory(render)
endif()
