# =============================================================================
# Game Server Executable
# =============================================================================

add_executable(GameServer
    src/main.cpp
    src/GameServer.cpp
    src/Lobby.cpp
    src/Room.cpp
    src/handlers/LobbyHandler.cpp
    src/handlers/RoomHandler.cpp
    src/handlers/ChatHandler.cpp
    src/handlers/GameHandler.cpp
    src/handlers/InventoryHandler.cpp
)

target_include_directories(GameServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(GameServer PRIVATE
    knc-common
)

# Copy config file to output
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/game.json
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config/game.json
    COPYONLY
)

# Copy MariaDB DLL on Windows
if(WIN32 AND MARIADB_FOUND)
    get_filename_component(MARIADB_BIN_DIR "${MARIADB_LIB}" DIRECTORY)
    add_custom_command(TARGET GameServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MARIADB_BIN_DIR}/libmariadb.dll"
            "$<TARGET_FILE_DIR:GameServer>"
        COMMENT "Copying libmariadb.dll..."
    )
endif()

