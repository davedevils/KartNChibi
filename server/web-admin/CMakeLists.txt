
# library that can be embedded in other servers
add_library(knc-web-admin STATIC
    src/WebServer.cpp
)

target_include_directories(knc-web-admin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(knc-web-admin PUBLIC
    knc-common
    httplib
)

add_executable(WebAdmin
    src/main.cpp
)

target_link_libraries(WebAdmin PRIVATE
    knc-web-admin
)

add_custom_command(TARGET WebAdmin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/static
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/static
    COMMENT "Copying static files..."
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/webadmin.json
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config/webadmin.json
    COPYONLY
)

if(WIN32 AND MARIADB_FOUND)
    get_filename_component(MARIADB_BIN_DIR "${MARIADB_LIB}" DIRECTORY)
    add_custom_command(TARGET WebAdmin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MARIADB_BIN_DIR}/libmariadb.dll"
            "$<TARGET_FILE_DIR:WebAdmin>"
        COMMENT "Copying libmariadb.dll..."
    )
endif()
