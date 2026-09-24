#include "ui_sidebar.h"

#include "imgui_bgfx.h"
#include "ui_assets.h"
#include "ui_canvas.h"
#include "ui_dialogs.h"
#include "ui_json.h"
#include "ui_screens.h"

#include "tinyfiledialogs/tinyfiledialogs.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace KnC::Tools {
namespace {

const int kStateMapping[] = {STATE_LOGO, STATE_LOGIN, STATE_CHANNEL, STATE_MENU, STATE_GARAGE, STATE_LOBBY, STATE_ROOM, STATE_SHOP};
const char* const kStateItems[] = {"Logo (0)", "Login (2)", "Channel (3)", "Menu (4)", "Garage (5)", "Lobby (7)", "Room (8)", "Shop (6)"};
const char* const kLanguages[] = {"Eng", "Frn", "Ger", "Spn"};

// Edits a string through a fixed buffer and writes back on change
bool input_text(const char* label, std::string& value) {
    char buffer[512];
    std::strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    if (ImGui::InputText(label, buffer, sizeof(buffer))) {
        value = buffer;
        return true;
    }
    return false;
}

void save_current() {
    const UIScreen& screen = current_screen();
    export_screen_json(screen, g_editor.currentState, default_export_name(screen, g_editor.currentState));
}

void choose_game_folder() {
    const char* path = tinyfd_selectFolderDialog("Select KnC Game Folder", g_editor.gamePath.c_str());
    if (path == nullptr) return;
    std::printf("[INIT] game folder %s\n", path);
    reopen_game_folder(path);
}

void draw_element_list(UIScreen& screen) {
    ImGui::Text("Count: %d", static_cast<int>(screen.elements.size()));
    const float list_height = std::max(80.f, ImGui::GetContentRegionAvail().y - 110.f);
    ImGui::BeginChild("##elements", ImVec2(0.f, list_height), ImGuiChildFlags_Borders);
    int move_from = -1;
    int move_to = -1;
    for (int i = 0; i < static_cast<int>(screen.elements.size()); ++i) {
        UIElement& elem = screen.elements[i];
        ImGui::PushID(i);
        const std::string label = elem.name.empty() ? elem.id : elem.name;
        const float arrows = ImGui::GetFrameHeight() * 2.f + ImGui::GetStyle().ItemSpacing.x * 2.f;
        if (ImGui::Selectable(label.empty() ? "(unnamed)" : label.c_str(), elem.selected, 0,
                              ImVec2(ImGui::GetContentRegionAvail().x - arrows, 0.f)))
            select_element(i);
        ImGui::SameLine();
        ImGui::BeginDisabled(i == 0);
        if (ImGui::ArrowButton("##up", ImGuiDir_Up)) { move_from = i; move_to = i - 1; }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(i + 1 >= static_cast<int>(screen.elements.size()));
        if (ImGui::ArrowButton("##down", ImGuiDir_Down)) { move_from = i; move_to = i + 1; }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    ImGui::EndChild();

    if (move_from >= 0 && move_to >= 0 && move_to < static_cast<int>(screen.elements.size())) {
        std::swap(screen.elements[move_from], screen.elements[move_to]);
        screen.elements[move_from].zIndex = move_from * 10;
        screen.elements[move_to].zIndex = move_to * 10;
        if (g_editor.selectedElement == move_from) g_editor.selectedElement = move_to;
        else if (g_editor.selectedElement == move_to) g_editor.selectedElement = move_from;
    }

    if (ImGui::Button("Add Element", ImVec2(-1.f, 0.f))) open_add_element_dialog(g_editor.newElementType);
}

void draw_general_tab(UIElement& elem) {
    ImGui::Text("Type: %s", element_type_name(elem.type));
    ImGui::SetNextItemWidth(90.f);
    ImGui::InputFloat("X", &elem.bounds.x, 0.f, 0.f, "%.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.f);
    ImGui::InputFloat("Y", &elem.bounds.y, 0.f, 0.f, "%.0f");
    ImGui::SetNextItemWidth(90.f);
    if (ImGui::InputFloat("W", &elem.bounds.width, 0.f, 0.f, "%.0f")) elem.bounds.width = std::max(0.f, elem.bounds.width);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.f);
    if (ImGui::InputFloat("H", &elem.bounds.height, 0.f, 0.f, "%.0f")) elem.bounds.height = std::max(0.f, elem.bounds.height);
    ImGui::Checkbox("Visible", &elem.visible);
    ImGui::Checkbox("Enabled", &elem.enabled);
}

void draw_asset_tab(UIElement& elem) {
    if (elem.type != ELEM_IMAGE && elem.type != ELEM_BUTTON && elem.type != ELEM_CHECKBOX) {
        ImGui::TextDisabled("No asset for this type");
        return;
    }
    ImGui::TextUnformatted("Asset Path:");
    ImGui::SetNextItemWidth(-45.f);
    input_text("##asset", elem.assetPath);
    ImGui::SameLine();
    if (ImGui::Button("...##asset")) open_asset_browser(BrowserTarget::ElementOrBackground);

    if (elem.texture != nullptr && elem.texture->valid()) {
        ImGui::TextUnformatted("Preview:");
        const float width = ImGui::GetContentRegionAvail().x;
        const float scale = std::min(1.f, width / static_cast<float>(elem.texture->width));
        ImGui::Image(imgui_bgfx_texture(elem.texture->handle),
                     ImVec2(elem.texture->width * scale, elem.texture->height * scale));
        ImGui::Text("%dx%d", elem.texture->width, elem.texture->height);
    }
    if (ImGui::Button("Reload Asset", ImVec2(-1.f, 0.f))) elem.texture = load_texture(elem.assetPath);
}

void draw_state_tab(UIElement& elem) {
    if (elem.type != ELEM_BUTTON) {
        ImGui::TextDisabled("Only for buttons");
        return;
    }
    ImGui::TextUnformatted("Button States:");
    struct Field {
        const char* label;
        const char* id;
        std::string& asset;
        const LoadedTexture*& texture;
        BrowserTarget target;
    };
    Field fields[] = {
        {"Hover:", "##hover", elem.hoverAsset, elem.hoverTexture, BrowserTarget::Hover},
        {"Pressed:", "##pressed", elem.pressedAsset, elem.pressedTexture, BrowserTarget::Pressed},
        {"Disabled:", "##disabled", elem.disabledAsset, elem.disabledTexture, BrowserTarget::Disabled},
    };
    for (Field& field : fields) {
        ImGui::TextUnformatted(field.label);
        ImGui::SetNextItemWidth(-45.f);
        if (input_text(field.id, field.asset)) field.texture = load_texture(field.asset);
        ImGui::SameLine();
        ImGui::PushID(field.id);
        if (ImGui::Button("...")) open_asset_browser(field.target);
        ImGui::PopID();
    }
    ImGui::TextUnformatted("Action:");
    ImGui::SetNextItemWidth(-1.f);
    input_text("##action", elem.action);
}

void draw_text_tab(UIElement& elem) {
    if (elem.type == ELEM_TEXT || elem.type == ELEM_INPUT) {
        ImGui::TextUnformatted("Text:");
        ImGui::SetNextItemWidth(-1.f);
        input_text("##text", elem.text);
        ImGui::TextUnformatted("Font Size:");
        ImGui::SetNextItemWidth(90.f);
        if (ImGui::InputInt("##fontsize", &elem.fontSize)) elem.fontSize = std::clamp(elem.fontSize, 1, 199);
    }
    if (elem.type == ELEM_INPUT) {
        ImGui::TextUnformatted("Max Length:");
        ImGui::SetNextItemWidth(90.f);
        if (ImGui::InputInt("##maxlen", &elem.maxLength)) elem.maxLength = std::clamp(elem.maxLength, 1, 9999);
        ImGui::TextUnformatted("Placeholder:");
        ImGui::SetNextItemWidth(-1.f);
        input_text("##placeholder", elem.placeholder);
        ImGui::Checkbox("Password", &elem.isPassword);
    }
    if (elem.type == ELEM_CHECKBOX) ImGui::Checkbox("Checked", &elem.checked);
    if (elem.type != ELEM_TEXT && elem.type != ELEM_INPUT && elem.type != ELEM_CHECKBOX)
        ImGui::TextDisabled("No text for this type");
}

void draw_properties(UIScreen& screen) {
    UIElement* elem = selected_element();
    if (elem == nullptr) {
        ImGui::TextUnformatted("No element selected");
        ImGui::TextDisabled("Select an element to");
        ImGui::TextDisabled("edit its properties");
        return;
    }
    const char* display = elem->name.empty() ? elem->id.c_str() : elem->name.c_str();
    ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(50, 120, 200, 255));
    ImGui::Selectable(display, true);
    ImGui::PopStyleColor();

    if (ImGui::BeginTabBar("##props")) {
        if (ImGui::BeginTabItem("Gen")) { draw_general_tab(*elem); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Asset")) { draw_asset_tab(*elem); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("State")) { draw_state_tab(*elem); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Text")) { draw_text_tab(*elem); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImGui::Spacing();
    if (ImGui::Button("Delete Element", ImVec2(-1.f, 0.f))) {
        push_undo();
        screen.elements.erase(screen.elements.begin() + g_editor.selectedElement);
        select_element(-1);
    }
}

}

void draw_menu_bar() {
    if (!ImGui::BeginMainMenuBar()) return;
    UIScreen& screen = current_screen();
    const bool selection = has_selection();

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Save JSON", "Ctrl+S")) save_current();
        if (ImGui::MenuItem("Reload")) reload_screen(g_editor.currentState);
        ImGui::Separator();
        if (ImGui::MenuItem("Open PAK...")) choose_game_folder();
        if (ImGui::MenuItem("Set Background")) open_asset_browser(BrowserTarget::Background);
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) g_editor.quitRequested = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !g_editor.undoStack.empty())) undo();
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !g_editor.redoStack.empty())) redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, selection)) {
            g_editor.clipboard = *selected_element();
            g_editor.hasClipboard = true;
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, g_editor.hasClipboard)) {
            push_undo();
            UIElement pasted = g_editor.clipboard;
            pasted.bounds.x += 20.f;
            pasted.bounds.y += 20.f;
            pasted.name += " (copy)";
            pasted.selected = false;
            screen.elements.push_back(pasted);
            resolve_element_textures(screen.elements);
            select_element(static_cast<int>(screen.elements.size()) - 1);
        }
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, selection)) {
            push_undo();
            UIElement dup = *selected_element();
            dup.bounds.x += 10.f;
            dup.bounds.y += 10.f;
            dup.name += " (dup)";
            dup.selected = false;
            screen.elements.push_back(dup);
            select_element(static_cast<int>(screen.elements.size()) - 1);
        }
        if (ImGui::MenuItem("Delete", "Del", false, selection)) {
            push_undo();
            screen.elements.erase(screen.elements.begin() + g_editor.selectedElement);
            select_element(-1);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("To Front", nullptr, false, selection)) selected_element()->zIndex += 10;
        if (ImGui::MenuItem("To Back", nullptr, false, selection)) {
            int& z = selected_element()->zIndex;
            z = std::max(0, z - 10);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Grid", nullptr, &g_editor.showGrid);
        ImGui::MenuItem("Snap", nullptr, &g_editor.snapToGrid);
        ImGui::MenuItem("Bounds", nullptr, &g_editor.showBounds);
        ImGui::MenuItem("Preview", "P", &g_editor.previewMode);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset View", "Ctrl+0")) {
            g_editor.zoom = 1.f;
            g_editor.panX = 0.f;
            g_editor.panY = 0.f;
        }
        ImGui::EndMenu();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Lang:");
    int lang = 0;
    for (int i = 0; i < 4; ++i)
        if (g_editor.language == kLanguages[i]) lang = i;
    ImGui::SetNextItemWidth(60.f);
    if (ImGui::Combo("##lang", &lang, kLanguages, 4)) {
        g_editor.language = kLanguages[lang];
        std::printf("[LANG] %s\n", g_editor.language.c_str());
        reload_screen(g_editor.currentState);
    }

    ImGui::Separator();
    if (ImGui::SmallButton(g_editor.showGrid ? "[X] Grid" : "[ ] Grid")) g_editor.showGrid = !g_editor.showGrid;
    if (ImGui::SmallButton(g_editor.snapToGrid ? "[X] Snap" : "[ ] Snap")) g_editor.snapToGrid = !g_editor.snapToGrid;

    const char* title = "KnC UI Editor";
    const float title_w = ImGui::CalcTextSize(title).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - title_w - 12.f);
    ImGui::TextDisabled("%s", title);
    ImGui::EndMainMenuBar();
}

void draw_sidebar() {
    const Layout layout = compute_layout();
    ImGui::SetNextWindowPos(ImVec2(layout.sidebar_x, layout.sidebar_y));
    ImGui::SetNextWindowSize(ImVec2(layout.sidebar_w, layout.sidebar_h));
    ImGui::Begin("##sidebar", nullptr, kFixedWindow);

    ImGui::TextUnformatted("UI State:");
    int state_index = 0;
    for (int i = 0; i < 8; ++i)
        if (kStateMapping[i] == g_editor.currentState) state_index = i;
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::Combo("##state", &state_index, kStateItems, 8)) {
        g_editor.currentState = kStateMapping[state_index];
        select_element(-1);
        std::printf("[UI] state %d\n", g_editor.currentState);
    }

    UIScreen& screen = current_screen();
    if (ImGui::BeginTabBar("##mode")) {
        if (ImGui::BeginTabItem("Elements")) {
            draw_element_list(screen);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Properties")) {
            draw_properties(screen);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::Checkbox("Grid", &g_editor.showGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Bounds", &g_editor.showBounds);
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &g_editor.snapToGrid);
    if (ImGui::Button("Asset Browser", ImVec2(-1.f, 0.f))) open_asset_browser(BrowserTarget::ElementOrBackground);

    ImGui::End();
}

}
