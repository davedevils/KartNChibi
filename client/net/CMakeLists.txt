# netcode client and headless share byte exact with server wire works standalone for tools headless own build folder
set(knc_client_shared_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/src)

add_library(KnCClientNet STATIC
    NetClient.cpp
    CatalogDecode.cpp
    Catalog.cpp
    Session.cpp
    ${knc_client_shared_dir}/src/net/Packet.cpp
)
set_target_properties(KnCClientNet PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON FOLDER "client")
target_include_directories(KnCClientNet PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${knc_client_shared_dir}/include
)
target_compile_definitions(KnCClientNet PRIVATE _CRT_SECURE_NO_WARNINGS)
if(WIN32)
    target_link_libraries(KnCClientNet PUBLIC ws2_32)
endif()
