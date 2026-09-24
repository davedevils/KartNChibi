# netcode lib always built so headless links it client itself needs render module and glfw present

add_subdirectory(net)

if(NOT (TARGET KnCRender AND TARGET glfw))
    message(STATUS "Client skipped, KnCRender or glfw is missing")
    return()
endif()

set(knc_client_bgfx_dir ${CMAKE_SOURCE_DIR}/thirdparty/bgfx/bgfx)

add_executable(knc_client
    app/main.cpp
    app/App.cpp
    app/Options.cpp
    app/Settings.cpp
    app/SampleServer.cpp
    assets/AssetStore.cpp
    assets/NifEmbeddedTextures.cpp
    assets/SoundBank.cpp
    assets/TextTable.cpp
    ui/SpriteBatch.cpp
    ui/FontAtlas.cpp
    ui/UiJson.cpp
    ui/Widgets.cpp
    ui/WidgetScreen.cpp
    ui/MessagePopup.cpp
    ui/MenuFrame.cpp
    ui/CharPanel.cpp
    race/TrackData.cpp
    race/RaceSim.cpp
    race/AutoDriver.cpp
    race/RaceSession.cpp
    race/RaceView.cpp
    race/PodiumScene.cpp
    race/GaugeScene.cpp
    race/RaceHud.cpp
    race/RaceEffects.cpp
    screens/LogoScreen.cpp
    screens/LoginScreen.cpp
    screens/ChannelScreen.cpp
    screens/MenuScreen.cpp
    screens/LobbyScreen.cpp
    screens/RoomScreen.cpp
    screens/RaceScreen.cpp
    screens/ShopCommon.cpp
    screens/GarageScreen.cpp
    screens/ShopScreen.cpp
    screens/CharacterCreatePopup.cpp
    screens/PendantPopup.cpp
    screens/MissionScreen.cpp
    screens/MessengerPopup.cpp
    screens/MenuPopup.cpp
    screens/HelpPopup.cpp
    screens/OptionPopup.cpp
    screens/InvitePopup.cpp
    screens/GachaPopup.cpp
    screens/LicenceScreen.cpp
    screens/GhostModeScreen.cpp
    screens/CarCraftScreen.cpp
    screens/RoomCraftScreen.cpp
)
set_target_properties(knc_client PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON FOLDER "client")
target_include_directories(knc_client BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/shared/src
    ${knc_client_bgfx_dir}/3rdparty
    ${knc_client_bgfx_dir}/examples/common/imgui
    ${CMAKE_SOURCE_DIR}/thirdparty/miniaudio
)
target_compile_definitions(knc_client PRIVATE KNC_CLIENT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
# track scene ghost car and physics port land later same configure pass cmake links them lazily
target_link_libraries(knc_client PRIVATE KnCClientNet KnCTrackScene KnCGhostReplay KnCKartClient KnCRender bgfx bimg bimg_decode bx glfw)
if(MSVC)
    target_compile_options(knc_client PRIVATE /wd4244 /wd4505)
    target_compile_definitions(knc_client PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

message(STATUS "Client configured: knc_client")
