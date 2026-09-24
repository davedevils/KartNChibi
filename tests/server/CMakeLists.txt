
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# prevents overriding the parent projects compiler and linker settings on windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(knc-tests
    test_protocol.cpp
    test_packet.cpp
    test_logger.cpp
    test_config.cpp
    test_rate_limiter.cpp
    test_track_catalog_rename.cpp
    test_kart_definition_wire.cpp
    test_session_frames.cpp
    test_room_wire.cpp
    test_room_bots.cpp
    test_drift_boost_observer.cpp
    test_stock_screens_wire.cpp
    test_kart_garage_rules.cpp
    test_craft_shop_catalog.cpp
    test_server_resilience.cpp
    test_shop_part_definitions.cpp
    test_motion_gate.cpp
    test_db_worker_pool.cpp
    test_reward_and_reauth_wire.cpp
    test_owned_kart_truth.cpp
    test_invite_option.cpp
    test_login_burst_offload.cpp
    test_pendant_mission_creation.cpp
    test_race_start_and_invite.cpp
    test_shop_catalogue.cpp
    test_client_reader_rules.cpp
    test_quest_mode.cpp
    test_mission_curve_items.cpp
    test_security_gates.cpp
    test_auto_ban.cpp
    # the game packet builders are not a library yet so pull the files each wire test needs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/packets/gen/SpawnPackets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/handlers/RaceBots.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/packets/gen/DriftBoostPackets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/packets/gen/MotionPackets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/packets/gen/CustomCarPackets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/packets/gen/RoomCraftPackets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/packets/gen/GachaPetPackets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/packets/gen/InventoryPackets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/packets/gen/SocialPackets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/packets/PacketBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/packets/KartDefinitionWire.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/util/DbRowWire.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/packets/gen/MissionPackets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/packets/gen/CharCreatePackets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/packets/gen/ScenarioPackets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/src/packets/gen/ShopPackets.cpp
)

target_include_directories(knc-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../server/game/include
)

# the recording and the track data the observer test replays sit in the repo
get_filename_component(KNC_TESTS_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
target_compile_definitions(knc-tests PRIVATE KNC_REPO_ROOT="${KNC_TESTS_REPO_ROOT}")

target_link_libraries(knc-tests PRIVATE
    knc-common
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(knc-tests)

