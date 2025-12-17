# ================================================================
# KnC Tools - Development & Operations Tools
# ================================================================
cmake_minimum_required(VERSION 3.15)
project(KnC_Tools)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ================================================================
# COMMON UI LIBRARY (shared by tools)
# ================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ui_common")
    file(GLOB_RECURSE UI_COMMON_SOURCES
        "ui_common/src/*.cpp"
    )
    file(GLOB_RECURSE UI_COMMON_HEADERS
        "ui_common/include/*.h"
    )
    
    if(UI_COMMON_SOURCES)
        add_library(ui_common STATIC ${UI_COMMON_SOURCES} ${UI_COMMON_HEADERS})
    
    target_include_directories(ui_common PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/ui_common/include
        ${CMAKE_SOURCE_DIR}/engine/render/backends/raylib/include
        ${CMAKE_SOURCE_DIR}/engine/ui/backends/raygui
    )
    
    # Raylib
    if(EXISTS "${CMAKE_SOURCE_DIR}/engine/render/backends/raylib/lib")
        target_link_libraries(ui_common PUBLIC
            ${CMAKE_SOURCE_DIR}/engine/render/backends/raylib/lib/raylib.lib
        )
    endif()
    
        if(WIN32)
            target_link_libraries(ui_common PUBLIC winmm gdi32 opengl32)
        endif()
        
        message(STATUS "UI Common library configured")
    else()
        message(STATUS "UI Common library skipped (no sources)")
    endif()
endif()

# ================================================================
# TOOLS MACROS
# ================================================================
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
            ${CMAKE_SOURCE_DIR}/engine/render/backends/raylib/include
            ${CMAKE_SOURCE_DIR}/engine/ui/backends/raygui
            ${CMAKE_SOURCE_DIR}/thirdparty
        )
        
        target_link_libraries(${TOOL_NAME} PRIVATE
            shared
        )
        
        # Link ui_common if it exists
        if(TARGET ui_common)
            target_link_libraries(${TOOL_NAME} PRIVATE ui_common)
        endif()
        
        # Raylib
        if(EXISTS "${CMAKE_SOURCE_DIR}/engine/render/backends/raylib/lib")
            target_link_libraries(${TOOL_NAME} PRIVATE
                ${CMAKE_SOURCE_DIR}/engine/render/backends/raylib/lib/raylib.lib
            )
        endif()
        
        if(WIN32)
            target_link_libraries(${TOOL_NAME} PRIVATE winmm gdi32 opengl32)
        endif()
        
        message(STATUS "Tool configured: ${TOOL_NAME}")
    endif()
endfunction()

# ================================================================
# TOOLS
# ================================================================
add_knc_tool(ui_editor)
add_knc_tool(map_viewer)
add_knc_tool(model_viewer)
add_knc_tool(dat_manager)
add_knc_tool(packet_inspector)
add_knc_tool(network_decrypt)
add_knc_tool(pak_tool)
# add_knc_tool(replay_viewer)  # Future
# add_knc_tool(map_editor)     # Future

message(STATUS "Tools configured")

