# Console dump of a track collision folder over the kart client physics port
add_executable(col_tool main.cpp)
target_link_libraries(col_tool PRIVATE KnCKartClient)
set_target_properties(col_tool PROPERTIES FOLDER "tools/col_tool")
