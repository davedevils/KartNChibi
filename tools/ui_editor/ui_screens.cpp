#include "ui_screens.h"

#include "ui_assets.h"
#include "ui_json.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>

namespace KnC::Tools {

EditorState g_editor;

const char* state_name(int state) {
    static const char* const kNames[] = {
        "Logo (0)", "Title (1)", "Login (2)", "Channel (3)",
        "Menu (4)", "Garage (5)", "Shop (6)", "Lobby (7)",
        "Room (8)", "Unknown (9)", "Game (10)", "Unknown (11)",
        "Unknown (12)", "TutorialMenu (13)", "Unknown (14)", "Unknown (15)",
        "Unknown (16)", "CarFactory (17)", "Unknown (18)", "Unknown (19)",
        "Unknown (20)", "ScenarioMenu (21)", "GhostMode (22)", "MissionMenu (23)",
        "Unknown (24)", "QuestMenu (25)"
    };
    if (state >= 0 && state < STATE_COUNT) return kNames[state];
    return "Unknown";
}

const char* element_type_name(ElementType type) {
    static const char* const kNames[] = {"Image", "Button", "Text", "Input", "List", "Panel", "Checkbox"};
    if (type < 0 || type >= ELEM_TYPE_COUNT) return "Image";
    return kNames[type];
}

const char* element_type_json(ElementType type) {
    static const char* const kNames[] = {"image", "button", "text", "input", "list", "panel", "checkbox"};
    if (type < 0 || type >= ELEM_TYPE_COUNT) return "image";
    return kNames[type];
}

ElementType element_type_from_string(const std::string& text) {
    std::string lower = text;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (int i = 0; i < ELEM_TYPE_COUNT; ++i)
        if (lower == element_type_json(static_cast<ElementType>(i))) return static_cast<ElementType>(i);
    return ELEM_IMAGE;
}

UIScreen& current_screen() { return g_editor.screens[g_editor.currentState]; }

bool has_selection() {
    const UIScreen& screen = current_screen();
    return g_editor.selectedElement >= 0 && g_editor.selectedElement < static_cast<int>(screen.elements.size());
}

UIElement* selected_element() { return has_selection() ? &current_screen().elements[g_editor.selectedElement] : nullptr; }

void select_element(int index) {
    UIScreen& screen = current_screen();
    for (UIElement& elem : screen.elements) elem.selected = false;
    if (index >= 0 && index < static_cast<int>(screen.elements.size())) {
        g_editor.selectedElement = index;
        screen.elements[index].selected = true;
    } else {
        g_editor.selectedElement = -1;
    }
}

namespace {

void nullify_textures(std::vector<UIElement>& elements) {
    for (UIElement& elem : elements) {
        elem.texture = nullptr;
        elem.hoverTexture = nullptr;
        elem.pressedTexture = nullptr;
        elem.disabledTexture = nullptr;
    }
}

}

void push_undo() {
    g_editor.undoStack.push_back(current_screen().elements);
    nullify_textures(g_editor.undoStack.back());
    g_editor.redoStack.clear();
    if (static_cast<int>(g_editor.undoStack.size()) > EditorState::kMaxUndo)
        g_editor.undoStack.erase(g_editor.undoStack.begin());
}

void undo() {
    if (g_editor.undoStack.empty()) return;
    UIScreen& screen = current_screen();
    g_editor.redoStack.push_back(screen.elements);
    nullify_textures(g_editor.redoStack.back());
    screen.elements = g_editor.undoStack.back();
    g_editor.undoStack.pop_back();
    resolve_element_textures(screen.elements);
    select_element(-1);
}

void redo() {
    if (g_editor.redoStack.empty()) return;
    UIScreen& screen = current_screen();
    g_editor.undoStack.push_back(screen.elements);
    nullify_textures(g_editor.undoStack.back());
    screen.elements = g_editor.redoStack.back();
    g_editor.redoStack.pop_back();
    resolve_element_textures(screen.elements);
    select_element(-1);
}

void save_config() {
    std::ofstream f(kConfigFile);
    if (!f.is_open()) return;
    f << "[Config]\n";
    f << "GamePath=" << g_editor.gamePath << "\n";
}

std::string load_config() {
    std::ifstream f(kConfigFile);
    if (!f.is_open()) return "";
    std::string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.rfind("GamePath=", 0) == 0) return line.substr(9);
    }
    return "";
}

namespace {

struct ButtonDef {
    const char* name;
    const char* asset;
    const char* hover;
    float x;
    float y;
    float w;
    float h;
};

void add_button(UIScreen& screen, const ButtonDef& def) {
    UIElement btn;
    btn.type = ELEM_BUTTON;
    btn.name = def.name;
    btn.assetPath = def.asset;
    btn.hoverAsset = def.hover;
    btn.bounds = {def.x, def.y, def.w, def.h};
    btn.texture = load_texture(btn.assetPath);
    btn.hoverTexture = load_texture(btn.hoverAsset);
    screen.elements.push_back(btn);
}

void set_background(UIScreen& screen, const char* asset) {
    screen.backgroundAsset = asset;
    screen.background = load_texture(asset);
}

}

void init_screen_fallback(int state) {
    UIScreen& screen = g_editor.screens[state];
    screen.state = state;
    screen.elements.clear();
    switch (state) {
    case STATE_LOGO: {
        screen.name = "Logo";
        set_background(screen, "./Data/Eng/Image/Login/Login01.png");
        UIElement logo;
        logo.type = ELEM_IMAGE;
        logo.name = "Logo";
        logo.assetPath = "./Data/Eng/Image/Logo/Logo.png";
        logo.bounds = {312.f, 200.f, 400.f, 200.f};
        logo.texture = load_texture(logo.assetPath);
        screen.elements.push_back(logo);
        break;
    }
    case STATE_LOGIN: {
        screen.name = "Login";
        set_background(screen, "./Data/Eng/Image/Login/Login01.png");
        UIElement username;
        username.type = ELEM_INPUT;
        username.name = "Username Input";
        username.bounds = {350.f, 300.f, 300.f, 40.f};
        screen.elements.push_back(username);
        UIElement password;
        password.type = ELEM_INPUT;
        password.name = "Password Input";
        password.bounds = {350.f, 350.f, 300.f, 40.f};
        screen.elements.push_back(password);
        add_button(screen, {"Login Button", "./Data/Eng/Image/Buttons/Common_OK_01.png",
                            "./Data/Eng/Image/Buttons/Common_OK_02.png", 450.f, 410.f, 100.f, 40.f});
        break;
    }
    case STATE_MENU: {
        screen.name = "Main Menu";
        set_background(screen, "./Data/Eng/Image/Menu/Menu_Back.png");
        add_button(screen, {"Play Button", "./Data/Eng/Image/Menu/Play_01.png", "./Data/Eng/Image/Menu/Play_02.png", 100.f, 300.f, 200.f, 60.f});
        add_button(screen, {"Garage Button", "./Data/Eng/Image/Menu/Garage_01.png", "./Data/Eng/Image/Menu/Garage_02.png", 100.f, 380.f, 200.f, 60.f});
        add_button(screen, {"Shop Button", "./Data/Eng/Image/Menu/Shop_01.png", "./Data/Eng/Image/Menu/Shop_02.png", 100.f, 460.f, 200.f, 60.f});
        break;
    }
    case STATE_LOBBY: {
        screen.name = "Lobby";
        set_background(screen, "./Data/Eng/Image/Lobby/Lobby_Back.png");
        UIElement rooms;
        rooms.type = ELEM_LIST;
        rooms.name = "Room List";
        rooms.bounds = {50.f, 100.f, 700.f, 500.f};
        screen.elements.push_back(rooms);
        add_button(screen, {"Create Room Button", "./Data/Eng/Image/Lobby/Create_01.png", "./Data/Eng/Image/Lobby/Create_02.png", 780.f, 100.f, 200.f, 60.f});
        add_button(screen, {"Refresh Button", "./Data/Eng/Image/Lobby/Refresh_01.png", "./Data/Eng/Image/Lobby/Refresh_02.png", 780.f, 180.f, 200.f, 60.f});
        break;
    }
    default:
        screen.name = state_name(state);
        break;
    }
    screen.loaded = true;
}

void load_all_screens() {
    const int states[] = {STATE_LOGO, STATE_TITLE, STATE_LOGIN, STATE_CHANNEL, STATE_MENU,
                          STATE_GARAGE, STATE_LOBBY, STATE_ROOM, STATE_SHOP};
    for (const int state : states) {
        const std::string path = json_path_for_state(state);
        if (!path.empty() && load_screen_from_json(g_editor.screens[state], path)) {
            std::printf("[INIT] %s loaded from %s\n", state_name(state), path.c_str());
        } else {
            init_screen_fallback(state);
            std::printf("[INIT] %s filled by the fallback\n", state_name(state));
        }
    }
}

bool reload_screen(int state) {
    const std::string path = json_path_for_state(state);
    if (path.empty()) return false;
    const bool ok = load_screen_from_json(g_editor.screens[state], path);
    if (ok && state == g_editor.currentState) select_element(-1);
    return ok;
}

void reopen_game_folder(const std::string& game_path) {
    g_editor.gamePath = game_path;
    save_config();
    destroy_all_textures();
    open_game_folder(game_path);
    for (UIScreen& screen : g_editor.screens) {
        if (!screen.loaded) continue;
        resolve_element_textures(screen.elements);
        screen.background = load_texture(screen.backgroundAsset);
    }
}

}
