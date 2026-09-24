cmake_minimum_required(VERSION 3.15)
project(KnC_Tools)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Builds one tool folder into an executable when its sources exist
function(add_knc_tool TOOL_NAME)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${TOOL_NAME}")
        file(GLOB_RECURSE TOOL_SOURCES
            "${TOOL_NAME}/*.cpp"
        )
        file(GLOB_RECURSE TOOL_HEADERS
            "${TOOL_NAME}/*.h"
        )

        add_executable(${TOOL_NAME} ${TOOL_SOURCES} ${TOOL_HEADERS})

        target_include_directories(${TOOL_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/shared/include
            ${CMAKE_SOURCE_DIR}/shared/src/include
            ${CMAKE_SOURCE_DIR}/engine
            ${CMAKE_SOURCE_DIR}/thirdparty
        )

        target_link_libraries(${TOOL_NAME} PRIVATE
            shared
        )

        if(WIN32)
            target_link_libraries(${TOOL_NAME} PRIVATE winmm gdi32 opengl32)
        endif()

        message(STATUS "Tool configured: ${TOOL_NAME}")
    endif()
endfunction()

# The map viewer ships its own project file on the new render module
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/map_viewer/CMakeLists.txt)
    add_subdirectory(map_viewer)
endif()

add_knc_tool(network_decrypt)

add_subdirectory(nif)

# Lists extracts and packs a pak archive needs only the standard library
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/pak_tool/CMakeLists.txt)
    add_subdirectory(pak_tool)
endif()

# Dumps a track collision folder needs the kart client physics port
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/col_tool/CMakeLists.txt AND TARGET KnCKartClient)
    add_subdirectory(col_tool)
endif()

# Opens one nif with the new render module needs KnCRender and glfw
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/model_viewer/CMakeLists.txt AND TARGET KnCRender AND TARGET glfw)
    add_subdirectory(model_viewer)
endif()

# The dat manager browses the pak files with a preview on the render module needs KnCRender and glfw
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dat_manager/CMakeLists.txt AND TARGET KnCRender AND TARGET glfw)
    add_subdirectory(dat_manager)
endif()

# The UI editor draws with bgfx glfw and dear imgui needs the bgfx and glfw targets
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ui_editor/CMakeLists.txt AND TARGET bgfx AND TARGET glfw)
    add_subdirectory(ui_editor)
endif()

# The track scene library and its viewer land here once the other agent finishes
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/track_scene/CMakeLists.txt)
    add_subdirectory(track_scene)
endif()

# The ghost replay tool lands here once the other agent finishes
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/replay/CMakeLists.txt)
    add_subdirectory(replay)
endif()

message(STATUS "Tools configured")
