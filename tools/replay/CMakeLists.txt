# ghost replay reader plus a dump tool builds as a subdirectory of the root solution
cmake_minimum_required(VERSION 3.15)

add_library(KnCGhostReplay STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ghost_replay.cpp
)

target_include_directories(KnCGhostReplay PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(KnCGhostReplay PUBLIC cxx_std_17)

if(WIN32)
    target_compile_definitions(KnCGhostReplay PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# the port sample type comes straight from the kart client physics library
if(TARGET KnCKartClient)
    target_link_libraries(KnCGhostReplay PUBLIC KnCKartClient)
else()
    target_include_directories(KnCGhostReplay PUBLIC
        ${CMAKE_SOURCE_DIR}/games/kart/physics/client
    )
endif()

# database read path needs the vendored mariadb connector headers and import lib
option(KNC_REPLAY_DB "build the ghost replay database read path" ON)

# relative to this file so a standalone kart client configure still finds it
set(KNC_REPLAY_MARIADB_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../thirdparty/mariadb-connector-c)
set(KNC_REPLAY_MARIADB_INCLUDE ${KNC_REPLAY_MARIADB_ROOT}/include/mysql.h)
set(KNC_REPLAY_MARIADB_LIB ${KNC_REPLAY_MARIADB_ROOT}/lib/libmariadb.lib)
set(KNC_REPLAY_MARIADB_DLL ${KNC_REPLAY_MARIADB_ROOT}/lib/libmariadb.dll)

if(KNC_REPLAY_DB AND EXISTS "${KNC_REPLAY_MARIADB_INCLUDE}" AND EXISTS "${KNC_REPLAY_MARIADB_LIB}")
    target_include_directories(KnCGhostReplay PRIVATE ${KNC_REPLAY_MARIADB_ROOT}/include)
    target_link_libraries(KnCGhostReplay PRIVATE ${KNC_REPLAY_MARIADB_LIB})
    target_compile_definitions(KnCGhostReplay PUBLIC KNC_REPLAY_DB=1)
    message(STATUS "ghost replay database read path enabled")
else()
    target_compile_definitions(KnCGhostReplay PUBLIC KNC_REPLAY_DB=0)
    message(STATUS "ghost replay database read path skipped connector not found")
endif()

add_executable(ghost_dump ${CMAKE_CURRENT_SOURCE_DIR}/ghost_dump.cpp)
target_link_libraries(ghost_dump PRIVATE KnCGhostReplay)

if(WIN32)
    target_compile_definitions(ghost_dump PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# the dll travels next to every exe that can reach the database path at run time
if(WIN32 AND KNC_REPLAY_DB AND EXISTS "${KNC_REPLAY_MARIADB_DLL}")
    add_custom_command(TARGET ghost_dump POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${KNC_REPLAY_MARIADB_DLL}" "$<TARGET_FILE_DIR:ghost_dump>"
        COMMENT "copying libmariadb dll next to ghost dump"
    )
endif()

# drives the physics port with a recorded ghost and prints the gap needs the port library
if(TARGET KnCKartClient)
    add_executable(ghost_compare ${CMAKE_CURRENT_SOURCE_DIR}/ghost_compare.cpp)
    target_link_libraries(ghost_compare PRIVATE KnCGhostReplay KnCKartClient)
    if(WIN32)
        target_compile_definitions(ghost_compare PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
    endif()
    set_target_properties(ghost_compare PROPERTIES FOLDER "tools/replay")
endif()

set_target_properties(KnCGhostReplay ghost_dump PROPERTIES FOLDER "tools/replay")
