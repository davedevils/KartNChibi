# =============================================================================
# Login Server Executable
# =============================================================================

add_executable(LoginServer
    src/main.cpp
    src/LoginServer.cpp
    src/handlers/AuthHandler.cpp
    src/handlers/HandshakeHandler.cpp
)

target_include_directories(LoginServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(LoginServer PRIVATE
    knc-common
)

# Copy config file to output
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/login.json
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config/login.json
    COPYONLY
)

# Copy MariaDB DLL on Windows (post-build)
if(WIN32 AND MARIADB_FOUND)
    get_filename_component(MARIADB_BIN_DIR "${MARIADB_LIB}" DIRECTORY)
    add_custom_command(TARGET LoginServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MARIADB_BIN_DIR}/libmariadb.dll"
            "$<TARGET_FILE_DIR:LoginServer>"
        COMMENT "Copying libmariadb.dll..."
    )
endif()

