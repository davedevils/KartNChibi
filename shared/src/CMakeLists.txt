# =============================================================================
# Common Library (knc-common)
# =============================================================================

add_library(knc-common STATIC
    # Net
    src/net/Packet.cpp
    src/net/Session.cpp
    
    # Game
    src/game/Player.cpp
    src/game/Room.cpp
    
    # Logging
    src/logging/Logger.cpp
    
    # Config
    src/config/Config.cpp
    
    # Database
    src/db/Database.cpp
    
    # Security
    src/security/RateLimiter.cpp
    src/security/PacketValidator.cpp
    src/security/BanManager.cpp
)

target_include_directories(knc-common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(knc-common PUBLIC
    asio
    nlohmann_json
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(knc-common PUBLIC ws2_32 wsock32)
else()
    target_link_libraries(knc-common PUBLIC pthread)
endif()

# MariaDB/MySQL detection
set(MARIADB_FOUND FALSE)

if(WIN32)
    # Windows: Check common MariaDB install locations
    set(MARIADB_SEARCH_PATHS
        "C:/Program Files/MariaDB 11.1"
        "C:/Program Files/MariaDB 11.0"
        "C:/Program Files/MariaDB 10.11"
        "C:/Program Files/MariaDB/MariaDB Connector C 64-bit"
        "C:/Program Files/MariaDB/MariaDB Connector C"
        "C:/MariaDB/MariaDB Connector C 64-bit"
        "$ENV{MARIADB_DIR}"
    )
    
    foreach(SEARCH_PATH ${MARIADB_SEARCH_PATHS})
        # Check for mysql.h in various locations
        if(EXISTS "${SEARCH_PATH}/include/mysql/mysql.h")
            set(MARIADB_INCLUDE_DIR "${SEARCH_PATH}/include/mysql")
        elseif(EXISTS "${SEARCH_PATH}/include/mysql.h")
            set(MARIADB_INCLUDE_DIR "${SEARCH_PATH}/include")
        endif()
        
        # Check for lib
        if(EXISTS "${SEARCH_PATH}/lib/libmariadb.lib")
            set(MARIADB_LIB "${SEARCH_PATH}/lib/libmariadb.lib")
        elseif(EXISTS "${SEARCH_PATH}/lib/mariadb/libmariadb.lib")
            set(MARIADB_LIB "${SEARCH_PATH}/lib/mariadb/libmariadb.lib")
        endif()
        
        if(MARIADB_INCLUDE_DIR AND MARIADB_LIB)
            set(MARIADB_FOUND TRUE)
            message(STATUS "Found MariaDB: ${SEARCH_PATH}")
            message(STATUS "  Include: ${MARIADB_INCLUDE_DIR}")
            message(STATUS "  Lib: ${MARIADB_LIB}")
            break()
        endif()
    endforeach()
    
    if(MARIADB_FOUND)
        target_include_directories(knc-common PUBLIC ${MARIADB_INCLUDE_DIR})
        target_link_libraries(knc-common PUBLIC ${MARIADB_LIB})
        target_compile_definitions(knc-common PUBLIC KNC_HAS_MARIADB)
    else()
        message(STATUS "MariaDB not found - database features disabled")
        message(STATUS "To enable: Install MariaDB Connector/C from https://mariadb.com/downloads/connectors/")
    endif()
else()
    # Linux/macOS: Use pkg-config
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(MARIADB QUIET libmariadb)
        if(MARIADB_FOUND)
            target_include_directories(knc-common PRIVATE ${MARIADB_INCLUDE_DIRS})
            target_link_libraries(knc-common PUBLIC ${MARIADB_LIBRARIES})
            target_compile_definitions(knc-common PUBLIC KNC_HAS_MARIADB)
            message(STATUS "Found MariaDB via pkg-config")
        else()
            message(STATUS "MariaDB not found - install libmariadb-dev")
        endif()
    endif()
endif()

