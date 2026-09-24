// the window the frame loop the screen stack and the session wiring
#pragma once

#include "Options.h"
#include "SampleServer.h"
#include "Settings.h"
#include "assets/AssetStore.h"
#include "assets/SoundBank.h"
#include "assets/TextTable.h"
#include "net/Session.h"
#include "ui/FontAtlas.h"
#include "ui/Screen.h"
#include "ui/SpriteBatch.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

namespace KnC::Render {
class PngScreenshotCallback;
class SceneRenderer;
}

namespace KnC::Client {

class App {
public:
    explicit App(Options options);
    ~App();

    bool init();
    int run();

    AssetStore& assets() { return m_assets; }
    const FontAtlas& font() const { return m_font; }
    const FontAtlas& fontBold() const { return m_fontBold; }
    const TextTable& text() const { return m_text; }
    SoundBank& sound() { return m_sound; }
    Session& session() { return m_session; }
    KnC::Render::SceneRenderer& renderer() { return *m_renderer; }
    const Options& options() const { return m_options; }
    float canvasWidth() const { return m_canvasWidth; }
    float canvasHeight() const { return m_canvasHeight; }
    double time() const { return m_time; }
    uint16_t width() const { return m_width; }
    uint16_t height() const { return m_height; }

    // the screen factory main registers names logo login channel menu lobby room race
    using ScreenFactory = std::function<std::unique_ptr<Screen>(App&, const std::string&)>;
    void setScreenFactory(ScreenFactory factory) { m_factory = std::move(factory); }
    // replaces the base screen at the next frame boundary popups above it go away
    void go(const std::string& name);
    void pushScreen(std::unique_ptr<Screen> screen);
    void popScreen();
    Screen* topScreen() const { return m_stack.empty() ? nullptr : m_stack.back().get(); }
    const char* baseScreenName() const;
    // a modal text box the session messages and the disconnects land here
    void showMessage(const std::string& utf8, std::function<void()> onClose = nullptr);
    void quit() { m_quit = true; }

    // finds a ui state file in the ui dir the game folder or the source tree
    bool loadUiFile(const std::string& fileName, std::string& text) const;
    // true when the auto walk may leave this screen
    bool autoLeaves(const char* screenName) const;
    // the def trans line of a catalogue key or the key itself
    const std::string& tr(const std::string& key) const { return m_text.text(key); }
    // every inbound frame after the session saw it the race side reads its own opcodes here
    void setFrameTap(std::function<void(uint16_t opcode, Packet& pkt)> tap) { m_frameTap = std::move(tap); }
    // a held key as glfw reports it the race polls the arrows this way
    bool keyDown(int key) const;
    // a held race key through the Input ini rows up down left right item drift back slot
    bool raceKeyDown(RaceKey key) const;
    // writes a png of the next frame the name is the screenshot option with the stage before the extension
    void captureStage(const std::string& stage);
    // true when the screenshot option is set the window is hidden and frames are paced
    bool captureMode() const { return !m_options.screenshot.empty(); }
    // the auto stages leave by themselves the last one calls this to end the run
    void finishRun();
    // the button click of the pak plays on every widget click and the popup confirms
    void click();
    // the looping track of a screen name empty stops the music
    void playMusic(const std::string& name);
    // opens the creation popup over the current screen a fresh random driver each time
    void openCharacterCreate();
    // true when the sample server answers the wire the auto walks then leave the cars alone
    bool sampleMode() const { return !m_options.state.empty(); }
    // writes a png of the next frame next to the exe named after the base screen
    void snapshot();
    // the input entry points the glfw callbacks and the script share the canvas coordinates
    void mouseMove(float cx, float cy);
    void mouseButton(int button, int action, float cx, float cy);
    void key(int key, int action, int mods);
    void charInput(unsigned codepoint);
    // true while a script file drives the run
    bool scripted() const { return !m_options.script.empty(); }
    // the Option2 ini values and the Input ini rows read from the working folder at start
    GameOptions& gameOptions() { return m_gameOptions; }
    InputBindings& bindings() { return m_bindings; }
    // the bgm option reaches the music the effect and kart options the bank group gains
    void applySoundOptions();
    // window mode and wide mode of Option2 ini the stock reads them at start ours on OK too
    void applyGraphicOptions();
    // the detail of Option2 ini 0 low 1 medium 2 high the race loaders read it
    int detailLevel() const;
    // true while the motion blur row is on the race stages draw the boost blur with it
    bool motionBlurOn() const { return m_gameOptions.motionBlur > 0.f; }
    // a canvas rect in framebuffer pixels x y w h letterboxed on a free size stretched on the stock sizes
    void canvasToPixels(float x, float y, float w, float h, float out[4]) const;
    // milliseconds since the process started the startup log stamps every step with it
    static double uptimeMs();

private:
    void applyPending();
    void dispatch(SessionEvent event);
    void wireSession();
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void charCallback(GLFWwindow* window, unsigned codepoint);
    static void cursorCallback(GLFWwindow* window, double x, double y);
    static void mouseCallback(GLFWwindow* window, int button, int action, int mods);
    // loads a few menu frame images per frame while the way in screens are up
    void warmAssets();
    // reads the script file one verb per line
    bool loadScript();
    // runs the script for this frame after the glfw events
    void runScript();
    // the png path of a shot verb next to the screenshot option or in the cwd
    std::string shotPath(const std::string& name) const;

    Options m_options;
    SampleServer m_sample;
    GLFWwindow* m_window = nullptr;
    std::unique_ptr<KnC::Render::PngScreenshotCallback> m_screenshot;
    std::unique_ptr<KnC::Render::SceneRenderer> m_renderer;
    WireLog m_wireLog;
    bool m_wireOpen = false;
    Session m_session;
    AssetStore m_assets;
    SoundBank m_sound;
    TextTable m_text;
    SpriteBatch m_batch;
    FontAtlas m_font;
    FontAtlas m_fontBold;
    GameOptions m_gameOptions;
    InputBindings m_bindings;
    std::string m_musicName;
    ScreenFactory m_factory;
    std::function<void(uint16_t, Packet&)> m_frameTap;
    std::vector<std::unique_ptr<Screen>> m_stack;
    // the screen whose upload the renderer holds and whether a popped one left it behind
    Screen* m_sceneOwner = nullptr;
    bool m_sceneTaken = false;
    std::string m_pendingGo;
    bool m_hasPendingGo = false;
    std::vector<std::unique_ptr<Screen>> m_pendingPush;
    int m_pendingPop = 0;
    std::vector<std::unique_ptr<Screen>> m_graveyard;
    // the stack the base screen replaced drawn with no update while the new base still loads
    std::vector<std::unique_ptr<Screen>> m_held;
    // the span of the last bgfx frame call the slow frame line names it
    double m_presentMs = 0.0;
    void releaseHeld();
    std::vector<std::string> m_pendingCaptures;
    int m_capturesWritten = 0;
    float m_canvasWidth = 1024.f;
    float m_canvasHeight = 768.f;
    uint16_t m_width = 1024;
    uint16_t m_height = 768;
    // the window size the options pick and whether it is the frameless full screen
    uint16_t m_modeWidth = 1024;
    uint16_t m_modeHeight = 768;
    bool m_fullScreen = false;
    // the stock stretches its canvas on the sizes the options pick a free size letterboxes it
    bool m_stretch = false;
    // picks the window size from the options the explicit size flag and the capture mode
    void pickWindowMode();
    // reads Option2 ini runs the detect row and hands option 11 to the session
    void loadGameOptions();
    double m_time = 0.0;
    // the screen whose first frame is still due the time log stamps it after the frame
    std::string m_firstFrameOf;
    // how far the frame art warm up has walked its list
    size_t m_warmAt = 0;
    bool m_quit = false;
    bool m_finish = false;
    double m_finishAt = -1.0;
    bool m_initialised = false;
    // the script lines the cursor the wait and the release due on the next frame
    struct ScriptLine {
        std::string verb;
        std::vector<std::string> args;
        std::string rest;
    };
    std::vector<ScriptLine> m_script;
    size_t m_scriptAt = 0;
    int m_scriptWait = 0;
    int m_scriptWaitFor = 0;
    std::string m_scriptWaitScreen;
    int m_scriptReleaseButton = -1;
    int m_scriptReleaseKey = -1;
    // the hold verb the glfw key and the frames it stays down
    std::vector<std::pair<int, int>> m_scriptHeld;
    // the drag verb one mouse move a frame between the two points with the button held
    int m_dragLeft = 0;
    int m_dragSteps = 0;
    int m_dragButton = 0;
    float m_dragFrom[2] = {0.f, 0.f};
    float m_dragTo[2] = {0.f, 0.f};
    float m_scriptX = 0.f;
    float m_scriptY = 0.f;
    int m_scriptFails = 0;
    bool m_scriptDone = false;
};

}
