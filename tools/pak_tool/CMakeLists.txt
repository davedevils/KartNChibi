# Console pak archive tool needs only the header only pak reader and the standard library
add_executable(pak_tool main.cpp)
target_include_directories(pak_tool PRIVATE ${CMAKE_SOURCE_DIR}/shared/src)
set_target_properties(pak_tool PROPERTIES FOLDER "tools/pak_tool")
