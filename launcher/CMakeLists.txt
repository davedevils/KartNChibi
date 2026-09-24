
cmake_minimum_required(VERSION 3.18)
project(KnCLauncher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(PLATFORM_LIBS ws2_32 ole32 oleaut32 uuid)
    add_definitions(-DWEBVIEW_EDGE)
elseif(APPLE)
    find_library(WEBKIT_LIB WebKit REQUIRED)
    set(PLATFORM_LIBS ${WEBKIT_LIB})
    add_definitions(-DWEBVIEW_COCOA)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.0)
    set(PLATFORM_LIBS ${GTK3_LIBRARIES} ${WEBKIT_LIBRARIES})
    set(PLATFORM_INCLUDES ${GTK3_INCLUDE_DIRS} ${WEBKIT_INCLUDE_DIRS})
    add_definitions(-DWEBVIEW_GTK)
endif()

set(WEBVIEW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/webview")
if(NOT EXISTS "${WEBVIEW_DIR}/webview.h")
    message(STATUS "Downloading webview...")
    file(DOWNLOAD 
        "https://raw.githubusercontent.com/webview/webview/master/webview.h"
        "${WEBVIEW_DIR}/webview.h"
        SHOW_PROGRESS
    )
endif()

# Windows WebView2 comes from a NuGet package packages is git ignored restored here
if(WIN32)
    set(WEBVIEW2_VERSION "1.0.2210.55")
    set(WEBVIEW2_PKG "${CMAKE_CURRENT_SOURCE_DIR}/packages/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")
    set(WEBVIEW2_INCLUDE "${WEBVIEW2_PKG}/build/native/include")
    set(WEBVIEW2_LIB "${WEBVIEW2_PKG}/build/native/x64")

    if(NOT EXISTS "${WEBVIEW2_INCLUDE}/WebView2.h")
        set(WEBVIEW2_NUPKG "${WEBVIEW2_PKG}/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}.nupkg")
        if(NOT EXISTS "${WEBVIEW2_NUPKG}")
            message(STATUS "restoring WebView2 nuget package")
            file(MAKE_DIRECTORY "${WEBVIEW2_PKG}")
            file(DOWNLOAD
                "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}"
                "${WEBVIEW2_NUPKG}"
                SHOW_PROGRESS
            )
        endif()
        message(STATUS "extracting WebView2 nuget package")
        file(ARCHIVE_EXTRACT INPUT "${WEBVIEW2_NUPKG}" DESTINATION "${WEBVIEW2_PKG}")
    endif()
endif()

if(WIN32)
    add_executable(KnCLauncher WIN32
        src/main.cpp
        src/Launcher.cpp
        resources/launcher.rc
    )
    
    # embeds a manifest for UAC elevation instead of the auto generated one
    set_target_properties(KnCLauncher PROPERTIES
        LINK_FLAGS "/MANIFESTUAC:NO /MANIFEST:EMBED /MANIFESTINPUT:\"${CMAKE_CURRENT_SOURCE_DIR}/resources/KnCLauncher.manifest\""
    )
else()
    add_executable(KnCLauncher
        src/main.cpp
        src/Launcher.cpp
    )
endif()

target_include_directories(KnCLauncher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/webview
    ${WEBVIEW2_INCLUDE}
    ${PLATFORM_INCLUDES}
)

if(WIN32)
    target_link_directories(KnCLauncher PRIVATE ${WEBVIEW2_LIB})
    target_link_libraries(KnCLauncher PRIVATE ${PLATFORM_LIBS} WebView2LoaderStatic.lib)
else()
    target_link_libraries(KnCLauncher PRIVATE ${PLATFORM_LIBS})
endif()

if(WIN32)
    add_custom_command(TARGET KnCLauncher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WEBVIEW2_PKG}/build/native/x64/WebView2Loader.dll"
            $<TARGET_FILE_DIR:KnCLauncher>
    )
endif()

add_custom_command(TARGET KnCLauncher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/web"
        "$<TARGET_FILE_DIR:KnCLauncher>/web"
)

install(TARGETS KnCLauncher DESTINATION bin)
install(DIRECTORY resources/web DESTINATION bin)
