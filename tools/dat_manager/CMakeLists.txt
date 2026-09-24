# The dat manager on bgfx glfw and dear imgui needs the render module and glfw
if(NOT (TARGET KnCRender AND TARGET glfw))
    return()
endif()

set(knc_bgfx_dir ${CMAKE_SOURCE_DIR}/thirdparty/bgfx/bgfx)
set(knc_imgui_dir ${knc_bgfx_dir}/3rdparty/dear-imgui)
set(knc_ui_editor_dir ${CMAKE_SOURCE_DIR}/tools/ui_editor)

# dear imgui from the bgfx tree plus the small bgfx backend of the ui editor
add_library(dat_manager_imgui STATIC
    ${knc_imgui_dir}/imgui.cpp
    ${knc_imgui_dir}/imgui_draw.cpp
    ${knc_imgui_dir}/imgui_tables.cpp
    ${knc_imgui_dir}/imgui_widgets.cpp
    ${knc_ui_editor_dir}/imgui_bgfx.cpp
)
set_target_properties(dat_manager_imgui PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON FOLDER "tools")
target_include_directories(dat_manager_imgui PUBLIC
    ${knc_bgfx_dir}/3rdparty
    ${knc_bgfx_dir}/examples/common/imgui
    ${knc_ui_editor_dir}
)
target_link_libraries(dat_manager_imgui PUBLIC bgfx bx glfw)
if(MSVC)
    target_compile_options(dat_manager_imgui PRIVATE /wd4244 /wd4505)
endif()

# The pak reading through PakReader the pack through the pak tool source the dialogs through tinyfiledialogs
add_executable(dat_manager
    main.cpp
    dat_archive.cpp
    dat_audio.cpp
    dat_preview.cpp
    dat_ui.cpp
    pak_pack.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/tinyfiledialogs/tinyfiledialogs.c
)
set_target_properties(dat_manager PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON FOLDER "tools")
target_include_directories(dat_manager PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/shared/src
    ${CMAKE_SOURCE_DIR}/thirdparty
)
target_link_libraries(dat_manager PRIVATE dat_manager_imgui KnCRender bgfx bimg bimg_decode bx glfw)
if(WIN32)
    target_link_libraries(dat_manager PRIVATE comdlg32 ole32 shell32 user32)
endif()
