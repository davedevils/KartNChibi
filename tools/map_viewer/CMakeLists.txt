# opens one track folder with the bgfx renderer track scene and ghost replay link in later during configure
if(TARGET KnCRender AND TARGET glfw)
    add_executable(map_viewer main.cpp)
    set_target_properties(map_viewer PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON FOLDER "tools")
    target_link_libraries(map_viewer PRIVATE KnCTrackScene KnCGhostReplay KnCRender glfw)
endif()
