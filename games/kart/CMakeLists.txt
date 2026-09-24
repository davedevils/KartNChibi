cmake_minimum_required(VERSION 3.15)

# the old game layer needs the old engine only builds when both are present
if(TARGET KnCEngine AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/race/RaceController.cpp)
    add_library(KnCGame STATIC
        # Race
        race/RaceController.cpp
        race/RaceHUD.cpp

        # Items
        items/ItemSystem.cpp
        items/ItemEffect.cpp

        # Gimmicks
        gimmicks/GimmickManager.cpp
        gimmicks/BoostPad.cpp
        gimmicks/ItemBox.cpp
        gimmicks/ItemDrum.cpp
        gimmicks/ItemBite.cpp
        gimmicks/ItemMoney.cpp
        gimmicks/TrackGimmick.cpp

        # network is disabled connection API not yet wired

        # States
        states/GameStateManager.cpp

        # Kart Physics
        physics/KartPhysics.cpp
    )

    target_include_directories(KnCGame PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/shared/include
        ${CMAKE_SOURCE_DIR}/thirdparty
    )

    target_compile_features(KnCGame PUBLIC cxx_std_17)

    if(WIN32)
        target_compile_definitions(KnCGame PRIVATE
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN
            NOMINMAX
        )
    endif()

    target_link_libraries(KnCGame PUBLIC
        shared
        KnCEngine
        KnCPhysics
        KnCNetwork
    )
endif()

# The one to one port of the client physics its own library no engine dependency
add_subdirectory(physics/client)
