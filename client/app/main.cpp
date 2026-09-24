// Kart N'Chibi client on glfw and bgfx from the window to the lobby the room and the race
#include "App.h"
#include "Options.h"
#include "screens/CarCraftScreen.h"
#include "screens/ChannelScreen.h"
#include "screens/LicenceScreen.h"
#include "screens/RoomCraftScreen.h"
#include "screens/GarageScreen.h"
#include "screens/LobbyScreen.h"
#include "screens/LoginScreen.h"
#include "screens/LogoScreen.h"
#include "screens/MenuScreen.h"
#include "screens/MissionScreen.h"
#include "screens/RaceScreen.h"
#include "screens/RoomScreen.h"
#include "screens/ShopScreen.h"

#include <cstdio>
#include <memory>

#if defined(_WIN32)
#include <windows.h>
#endif

using namespace KnC::Client;

namespace {

// a run without debug writes no line the console a double click opened goes away
void silenceLogs() {
#if defined(_WIN32)
    DWORD attached[2] = {};
    if (GetConsoleProcessList(attached, 2) == 1) FreeConsole();
    const char* sink = "NUL";
#else
    const char* sink = "/dev/null";
#endif
    std::freopen(sink, "w", stdout);
    std::freopen(sink, "w", stderr);
    std::setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
}

}

int main(int argc, char** argv) {
    Options options;
    if (!parseOptions(argc, argv, options)) {
        printUsage();
        return 2;
    }
    // the capture runs read the log through a pipe an unbuffered stdout keeps its last line on a crash
    if (options.debug) std::setvbuf(stdout, nullptr, _IONBF, 0);
    else silenceLogs();
    std::printf("=== KnC client ===\n");
    App app(options);
    app.setScreenFactory([](App& a, const std::string& name) -> std::unique_ptr<Screen> {
        if (name == "logo") return std::make_unique<LogoScreen>(a);
        if (name == "login") return std::make_unique<LoginScreen>(a);
        if (name == "channel") return std::make_unique<ChannelScreen>(a);
        if (name == "menu") return std::make_unique<MenuScreen>(a);
        if (name == "lobby") return std::make_unique<LobbyScreen>(a);
        if (name == "room") return std::make_unique<RoomScreen>(a);
        if (name == "race") return std::make_unique<RaceScreen>(a);
        if (name == "garage") return std::make_unique<GarageScreen>(a);
        if (name == "shop") return std::make_unique<ShopScreen>(a);
        if (name == "missions") return std::make_unique<MissionMenuScreen>(a);
        if (name == "mission") return std::make_unique<MissionRunScreen>(a);
        if (name == "licence") return std::make_unique<LicenceScreen>(a);
        if (name == "carcraft") return std::make_unique<CarCraftScreen>(a);
        if (name == "roomcraft") return std::make_unique<RoomCraftScreen>(a);
        return nullptr;
    });
    if (!app.init()) return 1;
    app.go("logo");
    return app.run();
}
