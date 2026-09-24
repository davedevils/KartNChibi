cmake_minimum_required(VERSION 3.15)
project(KartNChibi_Shared VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(shared_core INTERFACE)

target_include_directories(shared_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(shared_core INTERFACE
    $<$<CONFIG:Debug>:KNC_DEBUG>
    $<$<CONFIG:Release>:KNC_RELEASE>
)

set(SHARED_HEADERS
    include/shared/core/Types.h
    include/shared/core/Result.h
    
    include/shared/math/Vec2.h
    include/shared/math/Vec3.h
    include/shared/math/Quat.h
    
    include/shared/net/Packet.h
    
    # Master include
    include/shared/Shared.h
)

set(SHARED_SOURCES
)

if(SHARED_SOURCES)
    add_library(shared STATIC ${SHARED_SOURCES} ${SHARED_HEADERS})
    
    target_include_directories(shared PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    target_compile_definitions(shared PUBLIC
        $<$<CONFIG:Debug>:KNC_DEBUG>
        $<$<CONFIG:Release>:KNC_RELEASE>
    )
    
    if(WIN32)
        target_link_libraries(shared PUBLIC ws2_32)
    endif()
    
    message(STATUS "Shared library configured (compiled)")
else()
    # Alias for compatibility
    add_library(shared ALIAS shared_core)
    message(STATUS "Shared library configured (header-only)")
endif()

source_group("Core" FILES 
    include/shared/core/Types.h
    include/shared/core/Result.h
)

source_group("Math" FILES 
    include/shared/math/Vec2.h
    include/shared/math/Vec3.h
    include/shared/math/Quat.h
)

source_group("Network" FILES 
    include/shared/net/Packet.h
)

install(DIRECTORY include/shared
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

message(STATUS "  Include dir: ${CMAKE_CURRENT_SOURCE_DIR}/include")
