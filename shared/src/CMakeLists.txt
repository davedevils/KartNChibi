
# globbed on purpose since a hand written list was lost once and silently dropped half the server
file(GLOB_RECURSE KNC_COMMON_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
add_library(knc-common STATIC ${KNC_COMMON_SOURCES})

target_include_directories(knc-common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(knc-common PUBLIC
    asio
    nlohmann_json
)

# password hashing pulls libcrypto in so the linker needs it named
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    target_link_libraries(knc-common PUBLIC OpenSSL::Crypto OpenSSL::SSL)
    message(STATUS "Found OpenSSL for knc-common")
elseif(NOT WIN32)
    # windows hashes with CNG and pulls bcrypt through a pragma so it needs neither
    target_link_libraries(knc-common PUBLIC crypto ssl)
    message(STATUS "OpenSSL not found by cmake, linking crypto and ssl by name")
endif()

if(WIN32)
    target_link_libraries(knc-common PUBLIC ws2_32 wsock32)
else()
    target_link_libraries(knc-common PUBLIC pthread)
endif()

set(MARIADB_FOUND FALSE)

if(WIN32)
    set(MARIADB_SEARCH_PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparty/mariadb-connector-c"
        "C:/Program Files/MariaDB 11.1"
        "C:/Program Files/MariaDB 11.0"
        "C:/Program Files/MariaDB 10.11"
        "C:/Program Files/MariaDB/MariaDB Connector C 64-bit"
        "C:/Program Files/MariaDB/MariaDB Connector C"
        "C:/MariaDB/MariaDB Connector C 64-bit"
        "$ENV{MARIADB_DIR}"
    )
    
    foreach(SEARCH_PATH ${MARIADB_SEARCH_PATHS})
        if(EXISTS "${SEARCH_PATH}/include/mysql/mysql.h")
            set(MARIADB_INCLUDE_DIR "${SEARCH_PATH}/include/mysql")
        elseif(EXISTS "${SEARCH_PATH}/include/mysql.h")
            set(MARIADB_INCLUDE_DIR "${SEARCH_PATH}/include")
        endif()
        
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
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(MARIADB QUIET libmariadb)
        if(MARIADB_FOUND)
            # public since Database h is a public header and every consumer includes mysql h through it
            target_include_directories(knc-common PUBLIC ${MARIADB_INCLUDE_DIRS})
            target_link_directories(knc-common PUBLIC ${MARIADB_LIBRARY_DIRS})
            target_link_libraries(knc-common PUBLIC ${MARIADB_LIBRARIES})
            target_compile_definitions(knc-common PUBLIC KNC_HAS_MARIADB)
            message(STATUS "Found MariaDB via pkg-config")
        else()
            message(STATUS "MariaDB not found - install libmariadb-dev")
        endif()
    endif()
endif()

