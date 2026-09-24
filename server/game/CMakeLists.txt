
# globbed see the note in shared src cmakelists txt
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
add_executable(GameServer ${GAME_SOURCES})

target_include_directories(GameServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib   # crypto password hash h the 0x07 credential path
)

target_link_libraries(GameServer PRIVATE
    knc-common
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/game.json
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config/game.json
    COPYONLY
)

if(WIN32 AND MARIADB_FOUND)
    get_filename_component(MARIADB_BIN_DIR "${MARIADB_LIB}" DIRECTORY)
    add_custom_command(TARGET GameServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MARIADB_BIN_DIR}/libmariadb.dll"
            "$<TARGET_FILE_DIR:GameServer>"
        COMMENT "Copying libmariadb.dll..."
    )
endif()
