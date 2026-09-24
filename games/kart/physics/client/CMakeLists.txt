# The one to one port of the client kart physics builds alone or inside the root solution
cmake_minimum_required(VERSION 3.15)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(KnCKartClient LANGUAGES CXX)
    if(WIN32)
        add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

# One folder per port the file list grows with the modules a glob keeps the owners apart
file(GLOB KNC_KART_CLIENT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
if(KNC_KART_CLIENT_SOURCES)
    add_library(KnCKartClient STATIC ${KNC_KART_CLIENT_SOURCES})
else()
    add_library(KnCKartClient INTERFACE)
endif()
target_include_directories(KnCKartClient INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
if(KNC_KART_CLIENT_SOURCES)
    target_include_directories(KnCKartClient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(KnCKartClient PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON FOLDER "games/kart")
endif()

# The ghost harness rides along in the standalone build tools replay builds ghost compare against this port
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tools/replay/CMakeLists.txt)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../tools/replay ${CMAKE_BINARY_DIR}/replay)
endif()

# One test program per module kartclient name test reads KNC CLIENT DATA
file(GLOB KNC_KART_CLIENT_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*_test.cpp)
foreach(test_source ${KNC_KART_CLIENT_TESTS})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(kartclient_${test_name} ${test_source})
    target_link_libraries(kartclient_${test_name} PRIVATE KnCKartClient)
    # the replay tests read the stock client recordings kept in tools replay recordings
    target_compile_definitions(kartclient_${test_name} PRIVATE KNC_RECORDINGS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../../tools/replay/recordings")
    set_target_properties(kartclient_${test_name} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON FOLDER "tests/kart_client")
endforeach()
