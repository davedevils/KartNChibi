# =============================================================================
# Unit Tests (Google Test)
# =============================================================================

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(knc-tests
    test_protocol.cpp
    test_packet.cpp
    test_logger.cpp
    test_config.cpp
    test_rate_limiter.cpp
)

target_link_libraries(knc-tests PRIVATE
    knc-common
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(knc-tests)

