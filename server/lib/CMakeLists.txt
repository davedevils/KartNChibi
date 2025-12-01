# =============================================================================
# Dependencies - Header-only libraries
# =============================================================================

# -----------------------------------------------------------------------------
# ASIO (standalone, no Boost)
# -----------------------------------------------------------------------------
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/asio/include
)
target_compile_definitions(asio INTERFACE 
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
)

# -----------------------------------------------------------------------------
# nlohmann/json
# -----------------------------------------------------------------------------
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/json/include
)

# -----------------------------------------------------------------------------
# cpp-httplib
# -----------------------------------------------------------------------------
add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/httplib
)
# Enable SSL support if OpenSSL found
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    target_compile_definitions(httplib INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(httplib INTERFACE OpenSSL::SSL OpenSSL::Crypto)
endif()

