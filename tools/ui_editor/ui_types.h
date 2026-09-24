// Shared editor types and the one global editor state
#pragma once

#include <bgfx/bgfx.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace KnC::Tools {

constexpr int   kGameWidth = 1024;
constexpr int   kGameHeight = 768;
constexpr float kSidebarWidth = 300.f;
constexpr float kToolbarWidth = 50.f;
constexpr float kStatusHeight = 22.f;
constexpr const char* kConfigFile = "ui_editor.ini";

enum UIStateID {
    STATE_LOGO = 0,
    STATE_TITLE = 1,
    STATE_LOGIN = 2,
    STATE_CHANNEL = 3,
    STATE_MENU = 4,
    STATE_GARAGE = 5,
    STATE_SHOP = 6,
    STATE_LOBBY = 7,
    STATE_ROOM = 8,
    STATE_GAME = 10,
    STATE_TUTORIAL_MENU = 13,
    STATE_CAR_FACTORY = 17,
    STATE_SCENARIO_MENU = 21,
    STATE_GHOST_MODE = 22,
    STATE_MISSION_MENU = 23,
    STATE_QUEST_MENU = 25,
    STATE_COUNT = 26
};

const char* state_name(int state);

enum ElementType {
    ELEM_IMAGE,
    ELEM_BUTTON,
    ELEM_TEXT,
    ELEM_INPUT,
    ELEM_LIST,
    ELEM_PANEL,
    ELEM_CHECKBOX,
    ELEM_TYPE_COUNT
};

// Display name with a capital and the lowercase word the client reads
const char* element_type_name(ElementType type);
const char* element_type_json(ElementType type);
ElementType element_type_from_string(const std::string& text);

struct Rect {
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;
};

struct Color8 {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;
};

// One GPU texture owned by the cache the elements only point at it
struct LoadedTexture {
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    int width = 0;
    int height = 0;
    bool valid() const { return bgfx::isValid(handle); }
};

struct UIElement {
    ElementType type = ELEM_IMAGE;
    std::string id;
    std::string name;
    std::string assetPath;
    Rect bounds{0.f, 0.f, 100.f, 30.f};

    const LoadedTexture* texture = nullptr;
    const LoadedTexture* hoverTexture = nullptr;
    const LoadedTexture* pressedTexture = nullptr;
    const LoadedTexture* disabledTexture = nullptr;

    bool visible = true;
    bool enabled = true;
    bool selected = false;

    std::string action;

    std::string normalAsset;
    std::string hoverAsset;
    std::string pressedAsset;
    std::string disabledAsset;

    enum ButtonState { BTN_NORMAL, BTN_HOVER, BTN_PRESSED, BTN_DISABLED } buttonState = BTN_NORMAL;

    std::string text;
    int fontSize = 20;
    Color8 textColor;

    int maxLength = 64;
    std::string placeholder;
    bool isPassword = false;

    bool checked = false;

    int zIndex = 0;

    std::map<std::string, std::string> properties;
};

struct UIScreen {
    int state = 0;
    std::string name;
    std::vector<UIElement> elements;
    std::string backgroundAsset;
    const LoadedTexture* background = nullptr;
    bool loaded = false;
};

// Which field the asset browser fills when a row is picked
enum class BrowserTarget { ElementOrBackground, Hover, Pressed, Disabled, NewElement, Background };

struct EditorState {
    int currentState = STATE_LOGO;
    UIScreen screens[STATE_COUNT];

    int selectedElement = -1;
    bool dragging = false;
    float dragOffsetX = 0.f;
    float dragOffsetY = 0.f;

    std::string gamePath;
    std::string uiDir;
    std::string language = "Eng";

    std::map<std::string, std::unique_ptr<LoadedTexture>> textureCache;

    float zoom = 1.f;
    float panX = 0.f;
    float panY = 0.f;
    bool previewMode = false;
    bool showGrid = true;
    bool showBounds = true;
    bool snapToGrid = true;
    int gridSize = 10;

    bool isResizing = false;
    int resizeHandle = -1;

    // The canvas rectangle of the last frame in window pixels
    float canvasX = 0.f;
    float canvasY = 0.f;
    float canvasW = 1.f;
    float canvasH = 1.f;

    bool showAssetBrowser = false;
    BrowserTarget browserTarget = BrowserTarget::ElementOrBackground;
    std::vector<std::string> availableAssets;
    char assetSearch[256] = {0};
    int assetFolderFilter = 0;

    bool showAddElementDialog = false;
    int newElementType = 0;
    char newElementName[256] = "New Element";
    char newElementAsset[512] = "";
    float newElementX = 100.f;
    float newElementY = 100.f;
    float newElementW = 200.f;
    float newElementH = 60.f;

    UIElement clipboard;
    bool hasClipboard = false;

    std::vector<std::vector<UIElement>> undoStack;
    std::vector<std::vector<UIElement>> redoStack;
    static const int kMaxUndo = 50;

    bool quitRequested = false;
};

extern EditorState g_editor;

UIScreen&  current_screen();
bool       has_selection();
UIElement* selected_element();
void       select_element(int index);

void push_undo();
void undo();
void redo();

}
