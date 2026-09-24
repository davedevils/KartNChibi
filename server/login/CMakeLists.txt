
# globbed see the note in shared src CMakeLists txt
file(GLOB_RECURSE LOGIN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
add_executable(LoginServer ${LOGIN_SOURCES})

target_include_directories(LoginServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(LoginServer PRIVATE
    knc-common
    httplib          # header only carries the signup page
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/login.json
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config/login.json
    COPYONLY
)

if(WIN32 AND MARIADB_FOUND)
    get_filename_component(MARIADB_BIN_DIR "${MARIADB_LIB}" DIRECTORY)
    add_custom_command(TARGET LoginServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MARIADB_BIN_DIR}/libmariadb.dll"
            "$<TARGET_FILE_DIR:LoginServer>"
        COMMENT "Copying libmariadb.dll..."
    )
endif()

