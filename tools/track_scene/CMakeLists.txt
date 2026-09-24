# Loads a track folder into a MapScene - build needs render module and kart physics port
if(TARGET KnCRender AND TARGET KnCKartClient)
    add_library(KnCTrackScene STATIC track_scene.cpp ghost_car.cpp)
    set_target_properties(KnCTrackScene PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON FOLDER "tools")
    target_include_directories(KnCTrackScene PUBLIC ${CMAKE_SOURCE_DIR})
    target_link_libraries(KnCTrackScene PUBLIC KnCRender KnCKartClient)
endif()
