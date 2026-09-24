#include "ui_dialogs.h"

#include "imgui_bgfx.h"
#include "ui_assets.h"
#include "ui_screens.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace KnC::Tools {
namespace {

const char* const kAddTitle = "Add New Element";
const char* const kBrowserTitle = "Asset Browser";
const char* const kTypeItems[] = {"Image", "Button", "Text", "Input", "List", "Panel", "Checkbox"};
const char* const kFolderItems[] = {"All", "Login", "Menu", "Lobby", "Room", "Garage", "Shop", "Channel", "Common", "Icon", "Popup", "UI_"};
const char* const kFolderFilters[] = {"", "Login/", "Menu/", "Lobby/", "Room/", "Garage/", "Shop/", "ChannelSelect/", "Common/", "Icon/", "Popup/", "UI_"};
constexpr int kFolderCount = 12;
constexpr float kRowHeight = 60.f;

std::string lower_copy(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

void reset_new_element() {
    std::strncpy(g_editor.newElementName, "New Element", sizeof(g_editor.newElementName) - 1);
    g_editor.newElementName[sizeof(g_editor.newElementName) - 1] = '\0';
    g_editor.newElementAsset[0] = '\0';
    g_editor.newElementX = 100.f;
    g_editor.newElementY = 100.f;
    g_editor.newElementW = 200.f;
    g_editor.newElementH = 60.f;
    g_editor.newElementType = 0;
}

void add_new_element() {
    push_undo();
    UIScreen& screen = current_screen();
    UIElement elem;
    elem.type = static_cast<ElementType>(std::clamp(g_editor.newElementType, 0, ELEM_TYPE_COUNT - 1));
    elem.name = g_editor.newElementName;
    elem.assetPath = g_editor.newElementAsset;
    elem.bounds = {g_editor.newElementX, g_editor.newElementY, g_editor.newElementW, g_editor.newElementH};
    if (!elem.assetPath.empty()) elem.texture = load_texture(elem.assetPath);
    screen.elements.push_back(elem);
    std::printf("[EDITOR] added %s at (%.0f, %.0f)\n", elem.name.c_str(), elem.bounds.x, elem.bounds.y);
    select_element(static_cast<int>(screen.elements.size()) - 1);
    reset_new_element();
}

void draw_add_element_dialog() {
    if (g_editor.showAddElementDialog && !ImGui::IsPopupOpen(kAddTitle)) ImGui::OpenPopup(kAddTitle);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500.f, 0.f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(kAddTitle, &g_editor.showAddElementDialog, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextUnformatted("Type:");
    ImGui::SetNextItemWidth(-1.f);
    ImGui::Combo("##type", &g_editor.newElementType, kTypeItems, ELEM_TYPE_COUNT);

    ImGui::TextUnformatted("Name:");
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText("##name", g_editor.newElementName, sizeof(g_editor.newElementName));

    const int type = g_editor.newElementType;
    if (type == ELEM_IMAGE || type == ELEM_BUTTON || type == ELEM_CHECKBOX) {
        ImGui::TextUnformatted("Asset Path:");
        ImGui::SetNextItemWidth(-80.f);
        ImGui::InputText("##newasset", g_editor.newElementAsset, sizeof(g_editor.newElementAsset));
        ImGui::SameLine();
        if (ImGui::Button("Browse", ImVec2(70.f, 0.f))) {
            g_editor.showAddElementDialog = false;
            open_asset_browser(BrowserTarget::NewElement);
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::TextUnformatted("Position:");
    ImGui::SetNextItemWidth(100.f);
    ImGui::InputFloat("X", &g_editor.newElementX, 0.f, 0.f, "%.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.f);
    ImGui::InputFloat("Y", &g_editor.newElementY, 0.f, 0.f, "%.0f");

    ImGui::TextUnformatted("Size:");
    ImGui::SetNextItemWidth(100.f);
    ImGui::InputFloat("W", &g_editor.newElementW, 0.f, 0.f, "%.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.f);
    ImGui::InputFloat("H", &g_editor.newElementH, 0.f, 0.f, "%.0f");

    ImGui::Spacing();
    if (ImGui::Button("Cancel", ImVec2(100.f, 30.f))) {
        g_editor.showAddElementDialog = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add", ImVec2(100.f, 30.f))) {
        add_new_element();
        g_editor.showAddElementDialog = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// Applies the picked asset to the field the browser was opened for
void apply_pick(const std::string& asset) {
    std::printf("[BROWSER] picked %s\n", asset.c_str());
    UIScreen& screen = current_screen();
    UIElement* elem = selected_element();
    switch (g_editor.browserTarget) {
    case BrowserTarget::NewElement:
        std::strncpy(g_editor.newElementAsset, asset.c_str(), sizeof(g_editor.newElementAsset) - 1);
        g_editor.newElementAsset[sizeof(g_editor.newElementAsset) - 1] = '\0';
        g_editor.showAddElementDialog = true;
        return;
    case BrowserTarget::Hover:
        if (elem != nullptr) { elem->hoverAsset = asset; elem->hoverTexture = load_texture(asset); }
        return;
    case BrowserTarget::Pressed:
        if (elem != nullptr) { elem->pressedAsset = asset; elem->pressedTexture = load_texture(asset); }
        return;
    case BrowserTarget::Disabled:
        if (elem != nullptr) { elem->disabledAsset = asset; elem->disabledTexture = load_texture(asset); }
        return;
    case BrowserTarget::Background:
        screen.backgroundAsset = asset;
        screen.background = load_texture(asset);
        std::printf("[BACKGROUND] %s\n", asset.c_str());
        return;
    case BrowserTarget::ElementOrBackground:
        break;
    }
    if (elem != nullptr) {
        elem->assetPath = short_asset_path(asset);
        elem->texture = load_texture(asset);
        if (elem->bounds.width < 10.f && elem->texture != nullptr && elem->texture->valid()) {
            elem->bounds.width = static_cast<float>(elem->texture->width);
            elem->bounds.height = static_cast<float>(elem->texture->height);
        }
    } else {
        screen.backgroundAsset = asset;
        screen.background = load_texture(asset);
        std::printf("[BACKGROUND] %s\n", asset.c_str());
    }
}

std::vector<const std::string*> filtered_assets() {
    std::vector<const std::string*> filtered;
    const std::string search = lower_copy(g_editor.assetSearch);
    const int folder = std::clamp(g_editor.assetFolderFilter, 0, kFolderCount - 1);
    const std::string folder_filter = lower_copy(kFolderFilters[folder]);
    for (const std::string& asset : g_editor.availableAssets) {
        const std::string lower = lower_copy(asset);
        if (folder > 0 && lower.find(folder_filter) == std::string::npos) continue;
        if (!search.empty()) {
            const size_t slash = lower.find_last_of('/');
            const std::string file = slash == std::string::npos ? lower : lower.substr(slash + 1);
            if (file.find(search) == std::string::npos) continue;
        }
        filtered.push_back(&asset);
    }
    return filtered;
}

void draw_asset_row(const std::string& asset) {
    const ImVec2 row_start = ImGui::GetCursorScreenPos();
    const float row_width = ImGui::GetContentRegionAvail().x;
    ImGui::PushID(asset.c_str());
    if (ImGui::Selectable("##row", false, 0, ImVec2(row_width, kRowHeight))) {
        apply_pick(asset);
        g_editor.showAssetBrowser = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopID();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const LoadedTexture* thumb = load_texture(asset);
    if (thumb != nullptr && thumb->valid()) {
        const float scale = 50.f / static_cast<float>(std::max(thumb->width, thumb->height));
        const ImVec2 a(row_start.x + 5.f, row_start.y + 5.f);
        dl->AddImage(imgui_bgfx_texture(thumb->handle), a, ImVec2(a.x + thumb->width * scale, a.y + thumb->height * scale));
    }
    const size_t slash = asset.find_last_of("/\\");
    const std::string file = slash == std::string::npos ? asset : asset.substr(slash + 1);
    dl->AddText(ImVec2(row_start.x + 65.f, row_start.y + 6.f), IM_COL32(255, 255, 255, 255), file.c_str());
    dl->AddText(ImVec2(row_start.x + 65.f, row_start.y + 24.f), IM_COL32(120, 120, 130, 255), asset.c_str());
    if (thumb != nullptr && thumb->valid()) {
        char dims[32];
        std::snprintf(dims, sizeof(dims), "%dx%d", thumb->width, thumb->height);
        dl->AddText(ImVec2(row_start.x + 65.f, row_start.y + 42.f), IM_COL32(100, 150, 100, 255), dims);
    }
}

void draw_asset_browser() {
    if (g_editor.showAssetBrowser && !ImGui::IsPopupOpen(kBrowserTitle)) ImGui::OpenPopup(kBrowserTitle);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(1000.f, vp->WorkSize.x - 40.f), std::min(700.f, vp->WorkSize.y - 40.f)), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(kBrowserTitle, &g_editor.showAssetBrowser)) return;

    ImGui::TextUnformatted("Search:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText("##search", g_editor.assetSearch, sizeof(g_editor.assetSearch));
    ImGui::TextUnformatted("Folder:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.f);
    ImGui::Combo("##folder", &g_editor.assetFolderFilter, kFolderItems, kFolderCount);
    ImGui::SameLine();
    if (ImGui::Button("Close")) {
        g_editor.showAssetBrowser = false;
        ImGui::CloseCurrentPopup();
    }

    const std::vector<const std::string*> filtered = filtered_assets();
    const float footer = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("##assetlist", ImVec2(0.f, -footer), ImGuiChildFlags_Borders);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(filtered.size()), kRowHeight);
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) draw_asset_row(*filtered[i]);
    ImGui::EndChild();
    ImGui::TextDisabled("Showing %d of %d assets", static_cast<int>(filtered.size()), static_cast<int>(g_editor.availableAssets.size()));
    ImGui::EndPopup();
}

}

void open_add_element_dialog(int element_type) {
    g_editor.newElementType = std::clamp(element_type, 0, ELEM_TYPE_COUNT - 1);
    g_editor.showAddElementDialog = true;
}

void open_asset_browser(BrowserTarget target) {
    g_editor.browserTarget = target;
    g_editor.showAssetBrowser = true;
}

void draw_dialogs() {
    draw_add_element_dialog();
    draw_asset_browser();
}

}
