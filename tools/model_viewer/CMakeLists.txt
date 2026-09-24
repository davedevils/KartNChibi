add_executable(model_viewer main.cpp)
set_target_properties(model_viewer PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON FOLDER "tools")
target_link_libraries(model_viewer PRIVATE KnCRender glfw)
