# ================================================================
# KnC Server - Gateway + Game + Services
# ================================================================
cmake_minimum_required(VERSION 3.15)
project(KnC_Server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ================================================================
# GATEWAY SERVER (ex-login)
# ================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gateway")
    file(GLOB_RECURSE GATEWAY_SOURCES
        "gateway/*.cpp"
    )
    file(GLOB_RECURSE GATEWAY_HEADERS
        "gateway/*.h"
    )
    
    if(GATEWAY_SOURCES)
        add_executable(gateway ${GATEWAY_SOURCES} ${GATEWAY_HEADERS})
    
    target_include_directories(gateway PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/shared/include
        ${CMAKE_SOURCE_DIR}/shared/src/include
    )
    
        target_link_libraries(gateway PRIVATE shared)
        
        message(STATUS "Gateway server configured")
    else()
        message(STATUS "Gateway server skipped (no sources)")
    endif()
endif()

# ================================================================
# GAME SERVER
# ================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/game")
    file(GLOB_RECURSE GAME_SOURCES
        "game/*.cpp"
    )
    file(GLOB_RECURSE GAME_HEADERS
        "game/*.h"
    )
    
    if(GAME_SOURCES)
        add_executable(game ${GAME_SOURCES} ${GAME_HEADERS})
        
        target_include_directories(game PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/shared/include
        )
        
        target_link_libraries(game PRIVATE shared)
        
        message(STATUS "Game server configured")
    else()
        message(STATUS "Game server skipped (no sources)")
    endif()
endif()

# ================================================================
# WEB ADMIN (optionnel)
# ================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/web-admin")
    file(GLOB_RECURSE WEBADMIN_SOURCES
        "web-admin/*.cpp"
    )
    file(GLOB_RECURSE WEBADMIN_HEADERS
        "web-admin/*.h"
    )
    
    if(WEBADMIN_SOURCES)
        add_executable(web-admin ${WEBADMIN_SOURCES} ${WEBADMIN_HEADERS})
        
        target_include_directories(web-admin PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/shared/include
        )
        
        target_link_libraries(web-admin PRIVATE shared)
        
        message(STATUS "Web admin configured")
    else()
        message(STATUS "Web admin skipped (no sources)")
    endif()
endif()

message(STATUS "Server components configured")
