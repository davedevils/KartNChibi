#include "App.h"

#include "engine/render/png_screenshot.h"
#include "engine/render/scene_renderer.h"
#include "engine/render/texture_cache.h"
#include "net/Utf.h"
#include "screens/CharacterCreatePopup.h"
#include "screens/InvitePopup.h"
#include "ui/MessagePopup.h"

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>
#if defined(_WIN32)
#include <windows.h>
#endif

#include <bgfx/bgfx.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#ifndef KNC_CLIENT_SOURCE_DIR
#define KNC_CLIENT_SOURCE_DIR ""
#endif

namespace KnC::Client {

namespace {

void* nativeHandle(GLFWwindow* window) {
#if defined(_WIN32)
    return glfwGetWin32Window(window);
#elif defined(__APPLE__)
    return glfwGetCocoaWindow(window);
#else
    return reinterpret_cast<void*>(glfwGetX11Window(window));
#endif
}

// a hidden capture run paces itself so a frame count means a wall clock span
constexpr double kHiddenFrameSeconds = 0.04;
// the sprite pass sits after every view of the scene renderer so the hud lands on the world
constexpr bgfx::ViewId kSpriteView = 8;
// a frame longer than this prints its parts the 0x0040 reports stop while one runs
constexpr double kSlowFrameMs = 250.0;
// a staged capture run ends by itself after this many seconds past the race budget
constexpr double kRunGraceSeconds = 90.0;
// sub 46DB70 the kart channel plays at this share of its option value
constexpr float kKartChannelGain = 0.65f;

// the process clock every startup step is stamped against
const std::chrono::steady_clock::time_point kProcessStart = std::chrono::steady_clock::now();

// the windows font folder file or empty when it is not there
std::string windowsFont(const char* file) {
    std::string dir = "C:/Windows/Fonts";
#if defined(_WIN32)
    if (const char* windir = std::getenv("WINDIR")) dir = std::string(windir) + "/Fonts";
#endif
    const std::string path = dir + "/" + file;
    std::error_code ignored;
    return std::filesystem::is_regular_file(path, ignored) ? path : std::string();
}

}

App::App(Options options) : m_options(std::move(options)) {}

double App::uptimeMs() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - kProcessStart).count();
}

namespace {

// one line per startup step the span of the step and the process clock after it
struct StepClock {
    double last = App::uptimeMs();
    void mark(const char* step) {
        const double now = App::uptimeMs();
        std::printf("[time] %s %.0f ms (at %.0f ms)\n", step, now - last, now);
        last = now;
    }
};

}

App::~App() {
    m_stack.clear();
    m_held.clear();
    m_graveyard.clear();
    KnC::Render::set_texture_bytes_source(nullptr);
    if (m_initialised) {
        m_sound.shutdown();
        m_assets.destroyAll();
        m_font.shutdown();
        m_fontBold.shutdown();
        m_batch.shutdown();
        m_renderer->shutdown();
    }
    m_renderer.reset();
    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
    if (m_wireOpen) m_wireLog.close();
}

bool App::init() {
    StepClock clock;
    // the stock seeds rand with the clock at start the room title pick reads it
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    if (sampleMode()) {
        if (!m_sample.start(m_options.state)) {
            std::fprintf(stderr, "[app] the sample server could not listen\n");
            return false;
        }
        m_options.host = "127.0.0.1";
        m_options.port = m_sample.port();
    }
    if (!m_options.wireLog.empty()) {
        m_wireOpen = m_wireLog.open(m_options.wireLog);
        m_wireLog.setEcho(false);
        if (!m_wireOpen) std::fprintf(stderr, "[app] cannot open %s\n", m_options.wireLog.c_str());
    }
    if (m_wireOpen) m_session.setLog(&m_wireLog);
    wireSession();
    clock.mark("session");

    if (!glfwInit()) {
        std::fprintf(stderr, "[app] glfw init failed\n");
        return false;
    }
    // sub 406740 reads Option2 ini before the window the two graphic rows size it
    loadGameOptions();
    pickWindowMode();
    const bool showWindow = !captureMode();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, showWindow ? GLFW_TRUE : GLFW_FALSE);
    if (m_options.noFocus) {
        glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    }
    // sub 43DFA0 full screen is a frameless desktop sized window a window keeps its frame and no resize
    glfwWindowHint(GLFW_DECORATED, m_fullScreen ? GLFW_FALSE : GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, m_options.sizeGiven ? GLFW_TRUE : GLFW_FALSE);
    m_width = m_modeWidth;
    m_height = m_modeHeight;
    m_window = glfwCreateWindow(m_width, m_height, "Kart N'Chibi", nullptr, nullptr);
    if (m_window && m_fullScreen) glfwSetWindowPos(m_window, 0, 0);
    m_batch.setStretch(m_stretch);
    if (!m_window) {
        std::fprintf(stderr, "[app] window failed\n");
        return false;
    }
    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetCharCallback(m_window, charCallback);
    glfwSetCursorPosCallback(m_window, cursorCallback);
    glfwSetMouseButtonCallback(m_window, mouseCallback);
    clock.mark("window");

    // the scene renderer owns the bgfx boot the sprite batch draws on a view after its passes
    m_screenshot = std::make_unique<KnC::Render::PngScreenshotCallback>();
    m_renderer = std::make_unique<KnC::Render::SceneRenderer>();
    KnC::Render::RendererSetup setup;
    setup.native_window = nativeHandle(m_window);
    setup.width = m_width;
    setup.height = m_height;
    setup.callback = m_screenshot.get();
    setup.hour = m_options.hour;
    setup.vsync = showWindow;
    if (!m_renderer->init(setup)) {
        std::fprintf(stderr, "[app] the scene renderer found no usable backend\n");
        return false;
    }
    m_initialised = true;
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.f, 0);
    clock.mark("renderer");

    if (!m_batch.init()) return false;
    // the stock font table is Arial 14 to 29 px cells weights 400 600 and 700 GDI bolds over 550
    if (!m_font.init(24.f, windowsFont("arial.ttf"))) return false;
    if (!m_fontBold.init(24.f, windowsFont("arialbd.ttf"))) return false;
    std::printf("[font] %s and %s\n", m_font.faceName().c_str(), m_fontBold.faceName().c_str());
    clock.mark("font atlas");
    if (!m_options.gameDir.empty()) {
        m_assets.open(m_options.gameDir, m_options.language);
        // a texture no folder holds comes off the pak index read at the start never a second scan
        KnC::Render::set_texture_bytes_source([this](const std::string& path, std::vector<uint8_t>& out) {
            return m_assets.readPakByName(path, out);
        });
        clock.mark("pak open");
        m_text.load(m_assets, m_options.language);
        clock.mark("text table");
    } else {
        std::printf("[app] no game folder the screens draw placeholders\n");
    }
    m_batch.setWhite(m_assets.white()->handle);
    m_sound.init(m_assets, m_options.mute);
    clock.mark("sound engine");
    // the stock reads its two ini files from the working folder and keeps its defaults when they are not there
    if (m_bindings.load()) std::printf("[options] %s read\n", InputBindings::fileName());
    std::printf("[options] race keys up %s down %s left %s right %s item %s drift %s\n", InputBindings::keyName(m_bindings.keys[0]).c_str(),
                InputBindings::keyName(m_bindings.keys[1]).c_str(), InputBindings::keyName(m_bindings.keys[2]).c_str(),
                InputBindings::keyName(m_bindings.keys[3]).c_str(), InputBindings::keyName(m_bindings.keys[4]).c_str(),
                InputBindings::keyName(m_bindings.keys[5]).c_str());
    applySoundOptions();
    if (scripted() && !loadScript()) return false;
    clock.mark("options");
    return true;
}

namespace {

// the menu frame art of every lobby screen loaded while the logo the intro and the login are up
const char* const kWarmArt[] = {
    "Wallpaper/BackImage_00.png", "Menu/Common_back.png",
    "Menu/Common_Top_Lobby_00.png", "Menu/Common_Top_Lobby_01.png", "Menu/Common_Top_Lobby_02.png",
    "Menu/Common_Top_Ghost_00.png", "Menu/Common_Top_Ghost_01.png", "Menu/Common_Top_Ghost_02.png",
    "Menu/Common_Top_Quest_03.png", "Menu/Common_Top_Tutorial_00.png", "Menu/Common_Top_Tutorial_01.png",
    "Menu/Common_Top_Tutorial_02.png", "Menu/Common_Top_Mission_00.png", "Menu/Common_Top_Mission_01.png",
    "Menu/Common_Top_Mission_02.png", "Menu/Common_Bottom_Shop_00.png", "Menu/Common_Bottom_Shop_01.png",
    "Menu/Common_Bottom_Gacha_00.png", "Menu/Common_Bottom_Gacha_01.png", "Menu/Common_Bottom_Garage_00.png",
    "Menu/Common_Bottom_Garage_01.png", "Menu/Common_Bottom_CarCraft_00.png", "Menu/Common_Bottom_CarCraft_01.png",
    "Menu/Common_Bottom_RoomCraft_00.png", "Menu/Common_Bottom_RoomCraft_01.png", "Menu/Common_Option_00.png",
    "Menu/Common_Option_01.png", "Menu/Common_Community_00.png", "Menu/Common_Quit_00.png",
    "CharInfo/Common_Info_Bar.png", "UserList/Lobby_UserInfo_Back_00.png", "UserList/Lobby_Userlist_00.png",
    "UserList/Lobby_Userinfo_01.png", "Lobby/Lobby_Back.png", "Lobby/Lobby_Top.png",
    "ChannelSelect/channel_Back.PNG",
};
// one image a frame so the warm up never costs a visible hitch
constexpr size_t kWarmPerFrame = 1;

}

void App::warmAssets() {
    if (m_warmAt >= sizeof(kWarmArt) / sizeof(kWarmArt[0])) return;
    const std::string screen = baseScreenName();
    if (screen != "logo" && screen != "intro" && screen != "login" && screen != "channel") return;
    for (size_t i = 0; i < kWarmPerFrame && m_warmAt < sizeof(kWarmArt) / sizeof(kWarmArt[0]); ++i)
        m_assets.texture(kWarmArt[m_warmAt++]);
}

void App::finishRun() {
    if (m_finish || m_finishAt >= 0.0) return;
    if (m_options.linger > 0) {
        m_finishAt = m_time + static_cast<double>(m_options.linger);
        std::printf("[app] run lingers %d s\n", m_options.linger);
        return;
    }
    m_finish = true;
}

void App::click() {
    m_sound.play("button_01_click_snd", 0.8f);
}

void App::playMusic(const std::string& name) {
    m_musicName = name;
    const float level = m_gameOptions.bgmOff != 0.f ? 0.f : m_gameOptions.bgm;
    m_sound.music(name, 0.6f * level);
}

// sub 46CE90 reads the file sub 46D370 detects the first start detail sub 46DB70 maps the sound gains
void App::loadGameOptions() {
    const bool read = m_gameOptions.load();
    if (read) std::printf("[options] %s bgm %.2f effect %.2f car %.2f\n", GameOptions::fileName(), m_gameOptions.bgm, m_gameOptions.effect, m_gameOptions.car);
    if (m_gameOptions.detect == 0.f) {
        // over 200 MB of video memory is High every card this client runs on has that
        m_gameOptions.detect = 1.f;
        m_gameOptions.detail = 2.f;
        if (read && m_gameOptions.save()) std::printf("[options] detect ran detail High written to %s\n", GameOptions::fileName());
    }
    // option 11 goes out on the lobby tail with the value of the file
    m_session.sendOptionReport(m_gameOptions.randomInvite);
    std::printf("[options] window %s wide %s detail %d motion blur %s deny invite %s\n",
                m_gameOptions.windowMode > 0.f ? "on" : "off", m_gameOptions.wideMode > 0.f ? "on" : "off", detailLevel(),
                motionBlurOn() ? "on" : "off", m_gameOptions.randomInvite > 0.f ? "on" : "off");
}

// sub 43DFA0 window mode on frames 1024 by 768 or 1440 by 900 off takes the desktop size
void App::pickWindowMode() {
    const bool wide = m_gameOptions.wideMode > 0.f;
    m_fullScreen = false;
    m_stretch = !m_options.sizeGiven;
    if (m_options.sizeGiven) {
        m_modeWidth = m_options.width;
        m_modeHeight = m_options.height;
        std::printf("[options] size flag %ux%u letterboxed\n", m_modeWidth, m_modeHeight);
        return;
    }
    m_modeWidth = wide ? 1440 : 1024;
    m_modeHeight = wide ? 900 : 768;
    // a hidden capture run or a run beside a hand never covers the desktop
    const bool fullAllowed = !captureMode() && !m_options.noFocus;
    if (m_gameOptions.windowMode <= 0.f && fullAllowed) {
        if (const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor())) {
            m_fullScreen = true;
            m_modeWidth = static_cast<uint16_t>(mode->width);
            m_modeHeight = static_cast<uint16_t>(mode->height);
        }
    }
    std::printf("[options] window %ux%u %s\n", m_modeWidth, m_modeHeight, m_fullScreen ? "full screen" : "windowed");
}

// the frame loop resets bgfx after a window size or full screen change here
void App::applyGraphicOptions() {
    if (!m_window) return;
    const uint16_t oldW = m_modeWidth;
    const uint16_t oldH = m_modeHeight;
    const bool oldFull = m_fullScreen;
    pickWindowMode();
    m_batch.setStretch(m_stretch);
    if (oldW == m_modeWidth && oldH == m_modeHeight && oldFull == m_fullScreen) return;
    glfwSetWindowAttrib(m_window, GLFW_DECORATED, m_fullScreen ? GLFW_FALSE : GLFW_TRUE);
    if (m_fullScreen) glfwSetWindowPos(m_window, 0, 0);
    glfwSetWindowSize(m_window, m_modeWidth, m_modeHeight);
    std::printf("[options] applied %ux%u %s\n", m_modeWidth, m_modeHeight, m_fullScreen ? "full screen" : "windowed");
}

int App::detailLevel() const {
    const int d = static_cast<int>(m_gameOptions.detail + 0.5f);
    return d < 0 ? 0 : d > 2 ? 2 : d;
}

void App::canvasToPixels(float x, float y, float w, float h, float out[4]) const {
    const float fw = static_cast<float>(m_width);
    const float fh = static_cast<float>(m_height);
    float sx = std::min(fw / m_canvasWidth, fh / m_canvasHeight);
    float sy = sx;
    if (m_stretch) { sx = fw / m_canvasWidth; sy = fh / m_canvasHeight; }
    const float ox = std::floor((fw - m_canvasWidth * sx) * 0.5f);
    const float oy = std::floor((fh - m_canvasHeight * sy) * 0.5f);
    out[0] = ox + x * sx;
    out[1] = oy + y * sy;
    out[2] = w * sx;
    out[3] = h * sy;
}

void App::applySoundOptions() {
    const GameOptions& o = m_gameOptions;
    m_sound.setGroupGain(o.effectOff != 0.f ? 0.f : o.effect, o.carOff != 0.f ? 0.f : o.car * kKartChannelGain);
    if (m_musicName.empty()) return;
    // the bank keeps a named track playing so the volume lands through a stop and a start
    m_sound.music("");
    playMusic(m_musicName);
}

// the Input ini rows replace the stock defaults the VK of the row becomes the glfw key polled
bool App::raceKeyDown(RaceKey key) const {
    const int vk = m_bindings.keys[static_cast<int>(key)];
    const int glfwKey = InputBindings::glfwFromVk(vk);
    if (glfwKey < 0) return false;
    if (keyDown(glfwKey)) return true;
    // the stock reads the VK so both sides of a paired key count
    switch (glfwKey) {
    case GLFW_KEY_LEFT_SHIFT: return keyDown(GLFW_KEY_RIGHT_SHIFT);
    case GLFW_KEY_LEFT_CONTROL: return keyDown(GLFW_KEY_RIGHT_CONTROL);
    case GLFW_KEY_LEFT_ALT: return keyDown(GLFW_KEY_RIGHT_ALT);
    case GLFW_KEY_ENTER: return keyDown(GLFW_KEY_KP_ENTER);
    default: return false;
    }
}

void App::openCharacterCreate() {
    pushScreen(std::make_unique<CharacterCreatePopup>(*this));
}

void App::wireSession() {
    m_session.onChannelList = [this]() {
        dispatch(SessionEvent::ChannelList);
        // the list lands after the login and after the Channel button return both open the channel screen
        if (std::string(baseScreenName()) != "channel") go("channel");
    };
    m_session.onProfile = [this]() { dispatch(SessionEvent::Profile); };
    m_session.onMenuAck = [this]() {
        dispatch(SessionEvent::MenuAck);
        if (std::string(baseScreenName()) != "menu") go("menu");
    };
    m_session.onLobbyAck = [this]() {
        dispatch(SessionEvent::LobbyAck);
        if (std::string(baseScreenName()) != "lobby") go("lobby");
    };
    m_session.onGarageAck = [this]() {
        dispatch(SessionEvent::GarageAck);
        if (std::string(baseScreenName()) != "garage") go("garage");
    };
    m_session.onShopAck = [this]() {
        dispatch(SessionEvent::ShopAck);
        if (std::string(baseScreenName()) != "shop") go("shop");
    };
    m_session.onInventoryChanged = [this](uint32_t) { dispatch(SessionEvent::InventoryChanged); };
    m_session.onBuyOk = [this](uint32_t) { dispatch(SessionEvent::BuyOk); };
    m_session.onRoomsChanged = [this]() { dispatch(SessionEvent::RoomsChanged); };
    m_session.onChat = [this](const ChatLine&) { dispatch(SessionEvent::Chat); };
    m_session.onRoomEnter = [this]() {
        dispatch(SessionEvent::RoomEnter);
        if (std::string(baseScreenName()) != "room") go("room");
    };
    m_session.onRoomChanged = [this]() { dispatch(SessionEvent::RoomChanged); };
    m_session.onRoomTrack = [this]() { dispatch(SessionEvent::RoomTrack); };
    m_session.onRaceLaunch = [this]() {
        dispatch(SessionEvent::RaceLaunch);
        if (std::string(baseScreenName()) != "race") go("race");
    };
    m_session.onCharacterCreate = [this]() {
        dispatch(SessionEvent::CharacterCreate);
        openCharacterCreate();
    };
    m_session.onCharacterCreated = [this]() {
        dispatch(SessionEvent::CharacterCreated);
        const int32_t result = m_session.createResult();
        // sub 479230 a success shows MSG SAVE DONE with a close button the licence stage replaces it
        if (result == 0) { showMessage(tr("MSG_SAVE_DONE")); return; }
        // 1 and 3 are MSG INVALID NICK 2 is MSG ALREADY REGIST OK reopens a fresh popup Cancel quits
        if (result >= 1 && result <= 3) {
            const char* key = result == 2 ? "MSG_ALREADY_REGIST" : "MSG_INVALID_NICK";
            pushScreen(std::make_unique<MessagePopup>(*this, tr(key), [this]() { openCharacterCreate(); },
                                                      [this]() { quit(); }, true));
            return;
        }
        // any other code closes the socket and the game on the dismiss of MSG UNKNOWN ERROR
        showMessage(tr("MSG_UNKNOWN_ERROR"), [this]() {
            m_session.disconnect();
            quit();
        });
    };
    m_session.onLicenseAck = [this]() {
        dispatch(SessionEvent::LicenseAck);
        // the licence stage 14 opens on its ack the top bar and the fresh account path both land here
        if (std::string(baseScreenName()) != "licence") go("licence");
    };
    m_session.onRoomInvite = [this]() {
        dispatch(SessionEvent::RoomInvite);
        pushScreen(std::make_unique<InvitePopup>(*this, m_session.invite()));
    };
    m_session.onGachaResult = [this]() { dispatch(SessionEvent::GachaResult); };
    m_session.onGiftOk = [this]() { dispatch(SessionEvent::GiftOk); };
    m_session.onRoomCraftAck = [this]() {
        dispatch(SessionEvent::RoomCraftAck);
        if (std::string(baseScreenName()) != "roomcraft") go("roomcraft");
    };
    m_session.onRoomCraftSaved = [this]() { dispatch(SessionEvent::RoomCraftSaved); };
    m_session.onCarCraftAck = [this]() {
        dispatch(SessionEvent::CarCraftAck);
        if (std::string(baseScreenName()) != "carcraft") go("carcraft");
    };
    m_session.onCarCraftSaved = [this]() { dispatch(SessionEvent::CarCraftSaved); };
    m_session.onGhostMenuAck = [this]() { dispatch(SessionEvent::GhostMenuAck); };
    m_session.onGhostSession = [this]() { dispatch(SessionEvent::GhostSession); };
    m_session.onGhostResult = [this]() { dispatch(SessionEvent::GhostResult); };
    m_session.onLicenceTestAck = [this]() { dispatch(SessionEvent::LicenceTestAck); };
    m_session.onLicenceTestResult = [this]() { dispatch(SessionEvent::LicenceTestResult); };
    m_session.onMissionMenu = [this]() {
        dispatch(SessionEvent::MissionMenu);
        if (std::string(baseScreenName()) != "missions") go("missions");
    };
    m_session.onMissionStart = [this]() { dispatch(SessionEvent::MissionStart); };
    m_session.onMissionGo = [this]() { dispatch(SessionEvent::MissionGo); };
    m_session.onMissionComplete = [this]() { dispatch(SessionEvent::MissionComplete); };
    m_session.onPendantChanged = [this]() { dispatch(SessionEvent::PendantChanged); };
    m_session.onSocialChanged = [this]() { dispatch(SessionEvent::SocialChanged); };
    m_session.onQuickRoom = [this](uint32_t id) {
        showMessage("Quick match room " + std::to_string(id) + " acked with 0x0063, our server sends no 0x0013 context after it");
        if (m_options.quickMatch >= 0) {
            captureStage("quick");
            finishRun();
        }
        std::printf("[app] quick match room %u acked with 0x0063, no 0x0013 context follows on our server\n", id);
    };
    m_session.onMessage = [this](const ServerMessage& m) {
        const std::string text = m.wide ? u16ToUtf8(m.text) : m.key;
        std::printf("[app] server message type %u %s\n", m.boxType, text.c_str());
        showMessage(text);
    };
    m_session.onDisconnected = [this](const std::string& reason) {
        std::printf("[app] disconnected %s\n", reason.c_str());
        dispatch(SessionEvent::Disconnected);
        showMessage("Disconnected: " + reason, [this]() { if (std::string(baseScreenName()) != "login") go("login"); });
    };
    m_session.onFrame = [this](uint16_t op, Packet& pkt) {
        if (m_frameTap) {
            Packet copy = pkt;
            m_frameTap(op, copy);
        }
    };
}

void App::dispatch(SessionEvent event) {
    for (auto& screen : m_stack) screen->onSession(event);
}

const char* App::baseScreenName() const {
    return m_stack.empty() ? "" : m_stack.front()->name();
}

void App::go(const std::string& name) {
    m_pendingGo = name;
    m_hasPendingGo = true;
}

void App::pushScreen(std::unique_ptr<Screen> screen) {
    m_pendingPush.push_back(std::move(screen));
}

void App::popScreen() {
    ++m_pendingPop;
}

void App::showMessage(const std::string& utf8, std::function<void()> onClose) {
    pushScreen(std::make_unique<MessagePopup>(*this, utf8, std::move(onClose)));
}

void App::applyPending() {
    while (m_pendingPop > 0 && !m_stack.empty()) {
        // a popped owner leaves its scene behind the screen under it uploads again before its draw
        if (m_stack.back().get() == m_sceneOwner) { m_sceneOwner = nullptr; m_sceneTaken = true; }
        m_stack.back()->leave();
        m_graveyard.push_back(std::move(m_stack.back()));
        m_stack.pop_back();
        --m_pendingPop;
    }
    m_pendingPop = 0;
    if (m_hasPendingGo) {
        m_hasPendingGo = false;
        std::vector<std::unique_ptr<Screen>> previous;
        for (auto& s : m_stack) { s->leave(); previous.push_back(std::move(s)); }
        m_stack.clear();
        releaseHeld();
        // a fresh base uploads in its enter so nobody has to be told
        m_sceneOwner = nullptr;
        m_sceneTaken = false;
        std::unique_ptr<Screen> next = m_factory ? m_factory(*this, m_pendingGo) : nullptr;
        if (next) {
            std::printf("[app] screen %s\n", next->name());
            const double before = uptimeMs();
            m_stack.push_back(std::move(next));
            m_stack.back()->enter();
            // the stock holds the last frame of the stage it left while the next one loads
            if (m_stack.back()->holdsPrevious()) m_held = std::move(previous);
            const std::string screen = m_stack.back()->name();
            std::printf("[time] screen %s enter %.0f ms (at %.0f ms)\n", screen.c_str(), uptimeMs() - before, uptimeMs());
            m_firstFrameOf = screen;
            // the pak names of the stock loops the race and the mission pick their own theme track
            if (screen == "logo") playMusic("logo_bgm");
            else if (screen == "login" || screen == "channel" || screen == "menu") playMusic("title_bgm");
            else if (screen == "lobby" || screen == "room" || screen == "missions") playMusic("multiplay_lobby_bgm");
            else if (screen == "garage") playMusic("garage_bgm");
            else if (screen == "shop") playMusic("itemshop_bgm");
        } else {
            std::fprintf(stderr, "[app] no screen named %s\n", m_pendingGo.c_str());
        }
        for (auto& s : previous) if (s) m_graveyard.push_back(std::move(s));
    }
    for (auto& s : m_pendingPush) {
        m_stack.push_back(std::move(s));
        m_stack.back()->enter();
    }
    m_pendingPush.clear();
}

// the held screens go once the base stands their scene upload is gone with them
void App::releaseHeld() {
    if (m_held.empty()) return;
    for (auto& s : m_held) {
        if (s.get() == m_sceneOwner) { m_sceneOwner = nullptr; m_sceneTaken = true; }
        m_graveyard.push_back(std::move(s));
    }
    m_held.clear();
}

bool App::loadUiFile(const std::string& fileName, std::string& text) const {
    std::vector<std::string> dirs;
    if (!m_options.uiDir.empty()) dirs.push_back(m_options.uiDir);
    // release ships its layouts in clone beside the exe never in the game Data folder
#if defined(_WIN32)
    {
        char exe[MAX_PATH] = {};
        if (GetModuleFileNameA(nullptr, exe, MAX_PATH) > 0) {
            const std::string exeDir = std::filesystem::path(exe).parent_path().string();
            dirs.push_back(exeDir + "/clone/Data/Public/UI");
        }
    }
#endif
    if (!m_options.gameDir.empty()) dirs.push_back(m_options.gameDir + "/Data/Public/UI");
    if (KNC_CLIENT_SOURCE_DIR[0] != '\0') dirs.push_back(std::string(KNC_CLIENT_SOURCE_DIR) + "/Data/Public/UI");
    dirs.push_back("Data/Public/UI");
    for (const std::string& dir : dirs) {
        const std::filesystem::path path = std::filesystem::path(dir) / fileName;
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) continue;
        std::stringstream buffer;
        buffer << file.rdbuf();
        text = buffer.str();
        std::printf("[ui] %s\n", path.string().c_str());
        return true;
    }
    return false;
}

bool App::autoLeaves(const char* screenName) const {
    if (!m_options.autoWalk) return false;
    return m_options.stopAt != screenName;
}

bool App::keyDown(int key) const {
    // a key the script holds reads as down like a finger on it
    for (const auto& held : m_scriptHeld)
        if (held.first == key) return true;
    return m_window != nullptr && glfwGetKey(m_window, key) == GLFW_PRESS;
}

void App::captureStage(const std::string& stage) {
    if (!captureMode()) return;
    std::string name = m_options.screenshot;
    const size_t dot = name.rfind('.');
    const std::string suffix = "_" + stage;
    if (dot == std::string::npos || dot == 0) name += suffix;
    else name.insert(dot, suffix);
    m_pendingCaptures.push_back(name);
}

void App::keyCallback(GLFWwindow* window, int key, int, int action, int mods) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    app->key(key, action, mods);
}

void App::key(int key, int action, int mods) {
    if (key == GLFW_KEY_F12) {
        if (action == GLFW_PRESS) snapshot();
        return;
    }
    if (Screen* top = topScreen()) top->onKey(key, action, mods);
}

void App::charInput(unsigned codepoint) {
    if (Screen* top = topScreen()) top->onChar(codepoint);
}

void App::mouseMove(float cx, float cy) {
    if (Screen* top = topScreen()) top->onMouseMove(cx, cy);
}

void App::mouseButton(int button, int action, float cx, float cy) {
    if (Screen* top = topScreen()) top->onMouseButton(button, action, cx, cy);
}

// the png lands next to the exe as the base screen name a popup adds its own name
void App::snapshot() {
    std::string dir = ".";
#if defined(_WIN32)
    char exe[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exe, MAX_PATH) > 0) dir = std::filesystem::path(exe).parent_path().string();
#endif
    std::string name = baseScreenName();
    if (name.empty()) name = "screen";
    if (Screen* top = topScreen()) {
        if (std::string(top->name()) != name) name += std::string("_") + top->name();
    }
    const std::string path = (std::filesystem::path(dir) / (name + ".png")).string();
    m_pendingCaptures.push_back(path);
    std::printf("[app] F12 %s\n", path.c_str());
}

void App::charCallback(GLFWwindow* window, unsigned codepoint) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    app->charInput(codepoint);
}

void App::cursorCallback(GLFWwindow* window, double x, double y) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    float cx = 0.f, cy = 0.f;
    app->m_batch.toCanvas(x, y, cx, cy);
    app->mouseMove(cx, cy);
}

void App::mouseCallback(GLFWwindow* window, int button, int action, int) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    float cx = 0.f, cy = 0.f;
    app->m_batch.toCanvas(x, y, cx, cy);
    app->mouseButton(button, action, cx, cy);
}

namespace {

// a glfw key by its name a letter a digit or the decimal code minus one when unknown
int keyByName(const std::string& raw) {
    std::string n;
    for (char c : raw) n.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (n.empty()) return -1;
    if (n == "escape" || n == "esc") return GLFW_KEY_ESCAPE;
    if (n == "enter" || n == "return") return GLFW_KEY_ENTER;
    if (n == "tab") return GLFW_KEY_TAB;
    if (n == "space") return GLFW_KEY_SPACE;
    if (n == "backspace") return GLFW_KEY_BACKSPACE;
    if (n == "delete") return GLFW_KEY_DELETE;
    if (n == "up") return GLFW_KEY_UP;
    if (n == "down") return GLFW_KEY_DOWN;
    if (n == "left") return GLFW_KEY_LEFT;
    if (n == "right") return GLFW_KEY_RIGHT;
    if (n == "shift") return GLFW_KEY_LEFT_SHIFT;
    if (n == "ctrl") return GLFW_KEY_LEFT_CONTROL;
    if (n == "alt") return GLFW_KEY_LEFT_ALT;
    if (n.size() >= 2 && n[0] == 'f' && std::isdigit(static_cast<unsigned char>(n[1]))) {
        const int f = std::atoi(n.c_str() + 1);
        if (f >= 1 && f <= 25) return GLFW_KEY_F1 + f - 1;
    }
    if (n.size() == 1) {
        const char c = n[0];
        if (c >= 'a' && c <= 'z') return GLFW_KEY_A + (c - 'a');
        if (c >= '0' && c <= '9') return GLFW_KEY_0 + (c - '0');
    }
    bool digits = true;
    for (char c : n) if (!std::isdigit(static_cast<unsigned char>(c))) digits = false;
    if (digits) return std::atoi(n.c_str());
    return -1;
}

}

// one verb per line the words after the verb are its arguments a hash line is a comment
bool App::loadScript() {
    std::ifstream file(m_options.script);
    if (!file.is_open()) {
        std::fprintf(stderr, "[script] cannot open %s\n", m_options.script.c_str());
        return false;
    }
    std::string line;
    while (std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
        size_t start = 0;
        while (start < line.size() && line[start] == ' ') ++start;
        if (start >= line.size() || line[start] == '#') continue;
        std::istringstream in(line.substr(start));
        ScriptLine sl;
        in >> sl.verb;
        std::string word;
        while (in >> word) sl.args.push_back(word);
        const size_t space = line.find(' ', start);
        sl.rest = space == std::string::npos ? std::string() : line.substr(space + 1);
        m_script.push_back(sl);
    }
    std::printf("[script] %s %zu lines\n", m_options.script.c_str(), m_script.size());
    return true;
}

std::string App::shotPath(const std::string& name) const {
    std::filesystem::path dir = ".";
    if (!m_options.screenshot.empty()) {
        const std::filesystem::path shot(m_options.screenshot);
        if (shot.has_parent_path()) dir = shot.parent_path();
    }
    std::string file = name;
    if (file.size() < 4 || file.substr(file.size() - 4) != ".png") file += ".png";
    return (dir / file).string();
}

// a click is a press this frame and a release on the next one like a hand on the mouse
void App::runScript() {
    if (m_scriptDone) return;
    if (m_scriptReleaseButton >= 0) {
        mouseButton(m_scriptReleaseButton, GLFW_RELEASE, m_scriptX, m_scriptY);
        m_scriptReleaseButton = -1;
    }
    if (m_scriptReleaseKey >= 0) {
        key(m_scriptReleaseKey, GLFW_RELEASE, 0);
        m_scriptReleaseKey = -1;
    }
    // the held keys count down one a frame and release when spent
    for (size_t i = 0; i < m_scriptHeld.size();) {
        if (--m_scriptHeld[i].second > 0) { ++i; continue; }
        const int k = m_scriptHeld[i].first;
        m_scriptHeld.erase(m_scriptHeld.begin() + static_cast<std::ptrdiff_t>(i));
        key(k, GLFW_RELEASE, 0);
        std::printf("[script] release %d\n", k);
    }
    // the drag walks one step a frame so a screen that reads the run per move sees the path
    if (m_dragLeft > 0) {
        --m_dragLeft;
        const float t = 1.f - static_cast<float>(m_dragLeft) / static_cast<float>(std::max(m_dragSteps, 1));
        m_scriptX = m_dragFrom[0] + (m_dragTo[0] - m_dragFrom[0]) * t;
        m_scriptY = m_dragFrom[1] + (m_dragTo[1] - m_dragFrom[1]) * t;
        mouseMove(m_scriptX, m_scriptY);
        if (m_dragLeft == 0) mouseButton(m_dragButton, GLFW_RELEASE, m_scriptX, m_scriptY);
        return;
    }
    if (m_scriptWait > 0) { --m_scriptWait; return; }
    if (!m_scriptWaitScreen.empty()) {
        const Screen* top = topScreen();
        const std::string name = top ? top->name() : "";
        if (name == m_scriptWaitScreen && top->ready()) {
            std::printf("[script] waitfor %s ok\n", name.c_str());
            m_scriptWaitScreen.clear();
        } else if (--m_scriptWaitFor <= 0) {
            std::printf("[script] waitfor %s timed out on %s\n", m_scriptWaitScreen.c_str(), name.c_str());
            m_scriptWaitScreen.clear();
        } else {
            return;
        }
    }
    while (m_scriptAt < m_script.size()) {
        const ScriptLine& sl = m_script[m_scriptAt++];
        const std::string& v = sl.verb;
        auto arg = [&](size_t i) { return i < sl.args.size() ? sl.args[i] : std::string(); };
        if (v == "wait") {
            m_scriptWait = std::max(0, std::atoi(arg(0).c_str()));
            return;
        }
        if (v == "waitfor") {
            m_scriptWaitScreen = arg(0);
            m_scriptWaitFor = sl.args.size() > 1 ? std::atoi(arg(1).c_str()) : 250;
            return;
        }
        if (v == "click" || v == "rclick") {
            m_scriptX = static_cast<float>(std::atof(arg(0).c_str()));
            m_scriptY = static_cast<float>(std::atof(arg(1).c_str()));
            const int button = v == "click" ? GLFW_MOUSE_BUTTON_LEFT : GLFW_MOUSE_BUTTON_RIGHT;
            std::printf("[script] %s %.0f %.0f on %s\n", v.c_str(), m_scriptX, m_scriptY, topScreen() ? topScreen()->name() : "");
            mouseMove(m_scriptX, m_scriptY);
            mouseButton(button, GLFW_PRESS, m_scriptX, m_scriptY);
            m_scriptReleaseButton = button;
            return;
        }
        // drag X1 Y1 X2 Y2 with an optional button word and step count default left over ten frames
        if (v == "drag") {
            m_dragFrom[0] = static_cast<float>(std::atof(arg(0).c_str()));
            m_dragFrom[1] = static_cast<float>(std::atof(arg(1).c_str()));
            m_dragTo[0] = static_cast<float>(std::atof(arg(2).c_str()));
            m_dragTo[1] = static_cast<float>(std::atof(arg(3).c_str()));
            const std::string which = arg(4);
            m_dragButton = which == "right" ? GLFW_MOUSE_BUTTON_RIGHT
                         : which == "middle" ? GLFW_MOUSE_BUTTON_MIDDLE : GLFW_MOUSE_BUTTON_LEFT;
            m_dragSteps = sl.args.size() > 5 ? std::max(1, std::atoi(arg(5).c_str())) : 10;
            m_dragLeft = m_dragSteps;
            m_scriptX = m_dragFrom[0];
            m_scriptY = m_dragFrom[1];
            std::printf("[script] drag %s %.0f %.0f to %.0f %.0f on %s\n", which.empty() ? "left" : which.c_str(),
                        m_dragFrom[0], m_dragFrom[1], m_dragTo[0], m_dragTo[1],
                        topScreen() ? topScreen()->name() : "");
            mouseMove(m_scriptX, m_scriptY);
            mouseButton(m_dragButton, GLFW_PRESS, m_scriptX, m_scriptY);
            return;
        }
        if (v == "key") {
            const int k = keyByName(arg(0));
            if (k < 0) { std::printf("[script] unknown key %s\n", arg(0).c_str()); continue; }
            std::printf("[script] key %s on %s\n", arg(0).c_str(), topScreen() ? topScreen()->name() : "");
            key(k, GLFW_PRESS, 0);
            m_scriptReleaseKey = k;
            return;
        }
        // hold NAME N the key stays down N frames while the next lines run the race polls see it
        if (v == "hold") {
            const int k = keyByName(arg(0));
            if (k < 0) { std::printf("[script] unknown key %s\n", arg(0).c_str()); continue; }
            const int frames = std::max(1, std::atoi(arg(1).c_str()));
            std::printf("[script] hold %s %d on %s\n", arg(0).c_str(), frames, topScreen() ? topScreen()->name() : "");
            bool known = false;
            for (auto& held : m_scriptHeld)
                if (held.first == k) { held.second = frames; known = true; }
            if (!known) {
                m_scriptHeld.emplace_back(k, frames);
                key(k, GLFW_PRESS, 0);
            }
            continue;
        }
        if (v == "text") {
            // the token nick stands for the auto create nickname the gate makes one per fresh account
            std::string words = sl.rest;
            const std::string token = "{nick}";
            for (size_t at = words.find(token); at != std::string::npos; at = words.find(token))
                words.replace(at, token.size(), m_options.autoCreateCharacter);
            std::u16string wide = utf8ToU16(words);
            for (char16_t c : wide) charInput(static_cast<unsigned>(c));
            std::printf("[script] text %s\n", sl.rest.c_str());
            return;
        }
        if (v == "shot") {
            const std::string path = shotPath(arg(0));
            m_pendingCaptures.push_back(path);
            std::printf("[script] shot %s\n", path.c_str());
            return;
        }
        if (v == "expect") {
            const Screen* top = topScreen();
            const std::string name = top ? top->name() : "";
            const bool ok = name == arg(0);
            if (!ok) ++m_scriptFails;
            std::printf("[script] expect %s %s top %s\n", arg(0).c_str(), ok ? "PASS" : "FAIL", name.c_str());
            continue;
        }
        if (v == "quit") {
            std::printf("[script] quit with %d failed expects\n", m_scriptFails);
            m_scriptDone = true;
            finishRun();
            return;
        }
        std::printf("[script] unknown verb %s\n", v.c_str());
    }
    if (m_scriptAt >= m_script.size()) m_scriptDone = true;
}

int App::run() {
    const bool capture = captureMode();
    // one shot capture at frame N is the phase one proof the staged runs capture by name
    const bool staged = capture && (m_options.autoRaceTrack > 0 || m_options.autoCreateTrack > 0 || m_options.autoJoin ||
                        m_options.autoBuy || m_options.stopAt == "garage" || m_options.stopAt == "shop" ||
                        !m_options.autoCreateCharacter.empty() || m_options.autoMission >= 0 || !m_options.addFriend.empty() ||
                        m_options.acceptFriends || m_options.quickMatch >= 0 || m_options.stopAt == "missions" ||
                        m_options.stopAt == "mission" || m_options.stopAt == "friends" || !m_options.say.empty() ||
                        !m_options.note.empty() || m_options.stopAt == "licence" || m_options.stopAt == "carcraft" ||
                        m_options.stopAt == "roomcraft" || m_options.stopAt == "gacha" || m_options.stopAt == "escmenu" ||
                        m_options.stopAt == "intro" ||
                        m_options.stopAt == "trackpick" || m_options.stopAt == "inviting" || m_options.stopAt == "shopitem" ||
                        scripted());
    const double runBudget = static_cast<double>(m_options.raceSeconds) + kRunGraceSeconds;
    auto previous = std::chrono::steady_clock::now();
    const auto start = previous;
    int frame = 0;
    int finishFrames = -1;
    // the parts of the last frame a frame past the report span prints them
    double partPoll = 0.0, partNet = 0.0, partUpdate = 0.0, partDraw = 0.0, partSleep = 0.0;
    double frameFrom = uptimeMs();
    while (!glfwWindowShouldClose(m_window) && !m_quit) {
        {
            const double frameEnd = uptimeMs();
            if (frame > 0 && frameEnd - frameFrom > kSlowFrameMs)
                std::printf("[time] frame %.0f ms poll %.0f net %.0f update %.0f draw %.0f present %.0f sleep %.0f on %s\n",
                            frameEnd - frameFrom, partPoll, partNet, partUpdate, partDraw, m_presentMs, partSleep,
                            baseScreenName());
            frameFrom = frameEnd;
        }
        double partAt = uptimeMs();
        glfwPollEvents();
        if (scripted()) runScript();
        partPoll = uptimeMs() - partAt;
        const auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - previous).count();
        previous = now;
        m_time = std::chrono::duration<double>(now - start).count();
        if (dt > 0.25f) dt = 0.25f;

        partAt = uptimeMs();
        m_session.update();
        partNet = uptimeMs() - partAt;
        partAt = uptimeMs();
        applyPending();
        warmAssets();
        if (Screen* top = topScreen()) top->update(dt);
        applyPending();
        m_graveyard.clear();
        partUpdate = uptimeMs() - partAt;
        partAt = uptimeMs();

        int fbWidth = 0, fbHeight = 0;
        glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
        if (fbWidth > 0 && fbHeight > 0 && (fbWidth != m_width || fbHeight != m_height)) {
            m_width = static_cast<uint16_t>(fbWidth);
            m_height = static_cast<uint16_t>(fbHeight);
            m_renderer->resize(m_width, m_height);
        }

        // while the new base loads the screens it replaced draw in its place and get no update
        const bool holding = !m_held.empty() && !m_stack.empty() && m_stack.front()->holdsPrevious();
        if (!holding) releaseHeld();
        std::vector<std::unique_ptr<Screen>>& shown = holding ? m_held : m_stack;
        size_t first = 0;
        for (size_t i = shown.size(); i > 0; --i) {
            if (shown[i - 1]->opaque()) { first = i - 1; break; }
        }
        // top screen draws its 3D first a screen that lost the one scene upload is told
        bool drewScene = false;
        size_t sceneAt = first;
        for (size_t i = shown.size(); i > first; --i) {
            Screen* s = shown[i - 1].get();
            if (s != m_sceneOwner && (m_sceneOwner != nullptr || m_sceneTaken)) s->sceneLost();
            if (!s->drawScene()) continue;
            m_sceneOwner = s;
            m_sceneTaken = false;
            drewScene = true;
            sceneAt = i - 1;
            break;
        }
        // the scene renderer clears the frame itself a 2D screen clears view zero on its own
        if (!drewScene) {
            bgfx::setViewRect(0, 0, 0, m_width, m_height);
            bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.f, 0);
            bgfx::touch(0);
        }

        // a popup scene sits under every sprite so the screens below the popup leave its window open
        float hole[4] = {0.f, 0.f, 0.f, 0.f};
        const bool holed = drewScene && sceneAt > first && shown[sceneAt]->sceneHole(hole[0], hole[1], hole[2], hole[3]);
        m_batch.begin(kSpriteView, m_width, m_height, m_canvasWidth, m_canvasHeight);
        for (size_t i = first; i < shown.size(); ++i) {
            if (holed && i < sceneAt) m_batch.setHole(hole[0], hole[1], hole[2], hole[3]);
            else m_batch.clearHole();
            shown[i]->draw(m_batch);
        }
        m_batch.clearHole();
        m_batch.end();

        if (capture && !staged && frame == m_options.frames)
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, m_options.screenshot.c_str());
        if (!m_pendingCaptures.empty()) {
            std::printf("[app] capture %s\n", m_pendingCaptures.front().c_str());
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, m_pendingCaptures.front().c_str());
            m_pendingCaptures.erase(m_pendingCaptures.begin());
            ++m_capturesWritten;
        }
        const double presentFrom = uptimeMs();
        partDraw = presentFrom - partAt;
        bgfx::frame();
        m_presentMs = uptimeMs() - presentFrom;
        ++frame;
        if (!m_firstFrameOf.empty()) {
            std::printf("[time] screen %s first frame at %.0f ms\n", m_firstFrameOf.c_str(), uptimeMs());
            m_firstFrameOf.clear();
        }
        if (m_finishAt >= 0.0 && m_time >= m_finishAt) m_finish = true;
        if (capture && !staged && frame > m_options.frames + 2) break;
        if (m_finish && finishFrames < 0) finishFrames = frame + 3;
        if (finishFrames >= 0 && frame >= finishFrames) break;
        if (staged && m_time > runBudget) {
            std::printf("[app] run budget of %.0f s spent\n", runBudget);
            break;
        }
        partSleep = 0.0;
        if (capture) {
            const double spent = std::chrono::duration<double>(std::chrono::steady_clock::now() - now).count();
            if (spent < kHiddenFrameSeconds) {
                partAt = uptimeMs();
                std::this_thread::sleep_for(std::chrono::duration<double>(kHiddenFrameSeconds - spent));
                partSleep = uptimeMs() - partAt;
            }
        }
    }
    if (m_session.connected()) m_session.disconnect();
    // the captures encode on their own threads the files must stand before the run reports
    m_screenshot->finish();
    if (scripted()) {
        std::printf("[script] %s with %d failed expects\n", m_scriptFails == 0 ? "PASS" : "FAIL", m_scriptFails);
        return m_scriptFails == 0 ? 0 : 1;
    }
    if (capture) {
        if (!m_screenshot->wrote_file()) {
            std::fprintf(stderr, "[app] no screenshot was written\n");
            return 1;
        }
        std::printf("[app] wrote %s%s\n", m_options.screenshot.c_str(),
                    m_screenshot->wrote_one_colour() ? " (one flat colour)" : "");
    }
    return 0;
}

}
