# ================================================================
# Shared Library - Kart N'Chibi
# Cross-platform code for client, server, and tools
# ================================================================
cmake_minimum_required(VERSION 3.15)
project(KartNChibi_Shared VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ================================================================
# HEADER-ONLY LIBRARY (Core Types)
# ================================================================

add_library(shared_core INTERFACE)

target_include_directories(shared_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(shared_core INTERFACE
    $<$<CONFIG:Debug>:KNC_DEBUG>
    $<$<CONFIG:Release>:KNC_RELEASE>
)

# ================================================================
# COMPILED LIBRARY (Optional - for implementations)
# ================================================================

set(SHARED_HEADERS
    # Core (header-only)
    include/shared/core/Types.h
    include/shared/core/Result.h
    
    # Math (header-only)
    include/shared/math/Vec2.h
    include/shared/math/Vec3.h
    include/shared/math/Quat.h
    
    # Network (header-only)
    include/shared/net/Packet.h
    
    # Master include
    include/shared/Shared.h
)

# Optional: Add compiled sources if needed in the future
set(SHARED_SOURCES
    # Add .cpp files here if needed
    # Example: src/net/Socket.cpp
)

# Only create compiled library if there are source files
if(SHARED_SOURCES)
    add_library(shared STATIC ${SHARED_SOURCES} ${SHARED_HEADERS})
    
    target_include_directories(shared PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    target_compile_definitions(shared PUBLIC
        $<$<CONFIG:Debug>:KNC_DEBUG>
        $<$<CONFIG:Release>:KNC_RELEASE>
    )
    
    # Windows sockets
    if(WIN32)
        target_link_libraries(shared PUBLIC ws2_32)
    endif()
    
    message(STATUS "Shared library configured (compiled)")
else()
    # Alias for compatibility
    add_library(shared ALIAS shared_core)
    message(STATUS "Shared library configured (header-only)")
endif()

# ================================================================
# IDE Organization
# ================================================================

# Group headers for Visual Studio / IDE
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

# ================================================================
# Installation
# ================================================================

install(DIRECTORY include/shared
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

message(STATUS "  Include dir: ${CMAKE_CURRENT_SOURCE_DIR}/include")
