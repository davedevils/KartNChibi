# The UI layout editor on bgfx glfw and dear imgui needs the bgfx and glfw targets
if(NOT (TARGET bgfx AND TARGET glfw))
    return()
endif()

set(knc_bgfx_dir ${CMAKE_SOURCE_DIR}/thirdparty/bgfx/bgfx)
set(knc_imgui_dir ${knc_bgfx_dir}/3rdparty/dear-imgui)

# dear imgui from the bgfx tree plus the small bgfx backend of this tool
add_library(ui_editor_imgui STATIC
    ${knc_imgui_dir}/imgui.cpp
    ${knc_imgui_dir}/imgui_draw.cpp
    ${knc_imgui_dir}/imgui_tables.cpp
    ${knc_imgui_dir}/imgui_widgets.cpp
    imgui_bgfx.cpp
)
set_target_properties(ui_editor_imgui PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON FOLDER "tools")
target_include_directories(ui_editor_imgui PUBLIC
    ${knc_bgfx_dir}/3rdparty
    ${knc_bgfx_dir}/examples/common/imgui
)
target_link_libraries(ui_editor_imgui PUBLIC bgfx bx glfw)
if(MSVC)
    target_compile_options(ui_editor_imgui PRIVATE /wd4244 /wd4505)
endif()

add_executable(ui_editor
    main.cpp
    ui_assets.cpp
    ui_canvas.cpp
    ui_dialogs.cpp
    ui_json.cpp
    ui_screens.cpp
    ui_screenshot.cpp
    ui_sidebar.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/tinyfiledialogs/tinyfiledialogs.c
)
set_target_properties(ui_editor PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON FOLDER "tools")
target_include_directories(ui_editor PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/shared/src
    ${CMAKE_SOURCE_DIR}/thirdparty
)
target_link_libraries(ui_editor PRIVATE ui_editor_imgui bgfx bimg bimg_decode bx glfw)
if(WIN32)
    target_link_libraries(ui_editor PRIVATE comdlg32 ole32 shell32 user32)
endif()
