#include "dat_ui.h"

#include "imgui_bgfx.h"

#include "tinyfiledialogs/tinyfiledialogs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace KnC::Render;

namespace KnC::Tools {

namespace {

constexpr ImU32 kBackground = IM_COL32(30, 30, 35, 255);
constexpr ImU32 kPanel      = IM_COL32(40, 42, 48, 255);
constexpr ImU32 kAccent     = IM_COL32(80, 140, 200, 255);
constexpr ImU32 kFolder     = IM_COL32(220, 190, 90, 255);
constexpr ImU32 kDim        = IM_COL32(140, 140, 150, 255);
constexpr ImU32 kGood       = IM_COL32(100, 180, 100, 255);
constexpr ImU32 kBad        = IM_COL32(230, 110, 100, 255);

constexpr float kTopBar        = 44.f;
constexpr float kGap           = 10.f;
constexpr float kListHeader    = 34.f;
constexpr float kPreviewHeader = 34.f;
constexpr float kDetails       = 118.f;
constexpr float kPad           = 8.f;

ImVec4 colour(ImU32 packed) { return ImGui::ColorConvertU32ToFloat4(packed); }

const FolderNode* find_folder(const FolderNode& node, const std::string& path) {
    if (node.path == path) return &node;
    for (const auto& child : node.folders) {
        const FolderNode* found = find_folder(child.second, path);
        if (found != nullptr) return found;
    }
    return nullptr;
}

void refresh_listing(App& app) {
    app.listing.search = lower_text(app.search);
    app.listing.rebuild(app.archive);
}

void select_folder(App& app, const std::string& path) {
    app.selected = -1;
    app.selected_folder = path;
    app.status.clear();
    app.preview.clear();
}

const char* dialog_start(const App& app) {
    if (!app.last_dir.empty()) return app.last_dir.c_str();
    return app.archive.game_dir().empty() ? nullptr : app.archive.game_dir().c_str();
}

void extract_selected_file(App& app) {
    const char* dir = tinyfd_selectFolderDialog("Extract to folder", dialog_start(app));
    if (dir == nullptr) return;
    app.last_dir = dir;
    std::string error;
    const std::string name = app.archive.entries()[app.selected].filename;
    if (extract_entry(app.archive, app.selected, dir, error)) app.status = "Extracted " + name + " to " + dir;
    else app.status = "Extract failed " + error;
}

void extract_selected_folder(App& app, const FolderNode& folder) {
    const char* dir = tinyfd_selectFolderDialog("Extract the folder into", dialog_start(app));
    if (dir == nullptr) return;
    app.last_dir = dir;
    const int count = extract_folder(app.archive, folder, dir);
    app.status = "Extracted " + std::to_string(count) + " files to " + dir;
}

void pack_dialog(App& app) {
    const char* folder = tinyfd_selectFolderDialog("Folder to pack", dialog_start(app));
    if (folder == nullptr) return;
    app.last_dir = folder;
    const char* patterns[] = {"*.dat"};
    const char* out = tinyfd_saveFileDialog("Pak file to write", "pak001.dat", 1, patterns, "pak archive");
    if (out == nullptr) return;
    std::string error;
    if (pack_folder(out, folder, error)) app.status = std::string("Packed ") + out + " from " + folder;
    else app.status = "Pack failed " + error;
}

void change_folder_dialog(App& app) {
    const char* dir = tinyfd_selectFolderDialog("Select KnC Game Folder (with pak001.dat)", dialog_start(app));
    if (dir == nullptr) return;
    if (open_game_folder(app, dir)) save_game_dir_setting(dir);
}

// Play or pause whatever the preview holds the clip clock or the sound
void toggle_playback(App& app) {
    if (app.preview.kind() == PreviewKind::Audio) {
        if (app.audio.playing()) app.audio.stop();
        else app.audio.play();
    } else if (app.preview.kind() == PreviewKind::Model) {
        app.renderer->animation().toggle_running();
    }
}

void handle_keys(App& app) {
    if (ImGui::GetIO().WantTextInput) return;
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        app.selected = -1;
        app.selected_folder.clear();
        app.preview.clear();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Space)) toggle_playback(app);
    if (ImGui::IsKeyPressed(ImGuiKey_R)) app.preview.reset_camera();
}

void draw_top_bar(App& app, float width) {
    ImGui::SetCursorPos(ImVec2(15.f, 11.f));
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::TextUnformatted("KnC DAT Manager");
    ImGui::PopStyleColor();
    if (app.archive.opened()) {
        ImGui::SameLine(200.f);
        ImGui::TextColored(colour(kDim), "%d files", static_cast<int>(app.archive.entries().size()));
        ImGui::SameLine(290.f);
        ImGui::TextColored(colour(kDim), "%s", app.archive.game_dir().c_str());
    }
    ImGui::SameLine(width - 270.f);
    if (ImGui::Button("Pack Folder", ImVec2(120.f, 24.f))) pack_dialog(app);
    ImGui::SameLine(width - 140.f);
    if (ImGui::Button("Change Folder", ImVec2(125.f, 24.f))) change_folder_dialog(app);
}

void draw_file_row(App& app, int index) {
    const KnC::PakEntry& entry = app.archive.entries()[index];
    const FileKind kind = file_kind(entry.filename);
    ImGui::PushID(index);
    const ImVec2 mark = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(mark.x, mark.y + 4.f), ImVec2(mark.x + 8.f, mark.y + 15.f),
                                              kind_colour(kind));
    ImGui::Dummy(ImVec2(12.f, 0.f));
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, kind_colour(kind));
    if (ImGui::Selectable(entry.filename.c_str(), app.selected == index, ImGuiSelectableFlags_SpanAllColumns))
        select_entry(app, index);
    ImGui::PopStyleColor();
    if (app.scroll_to_selected && app.selected == index) {
        ImGui::SetScrollHereY(0.5f);
        app.scroll_to_selected = false;
    }
    ImGui::PopID();
}

void draw_tree(App& app, const FolderNode& node) {
    for (const auto& pair : node.folders) {
        const FolderNode& child = pair.second;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                   ImGuiTreeNodeFlags_SpanAvailWidth;
        if (app.selected < 0 && app.selected_folder == child.path) flags |= ImGuiTreeNodeFlags_Selected;
        if (!app.reveal.empty() && app.reveal.rfind(child.path + "/", 0) == 0)
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        ImGui::PushStyleColor(ImGuiCol_Text, kFolder);
        const bool open = ImGui::TreeNodeEx(child.name.c_str(), flags, "%s  (%d)", child.name.c_str(), child.total_files);
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) select_folder(app, child.path);
        if (open) {
            draw_tree(app, child);
            ImGui::TreePop();
        }
    }
    for (int index : node.files) draw_file_row(app, index);
}

void draw_flat_list(App& app) {
    if (!ImGui::BeginTable("flat", 2, ImGuiTableFlags_RowBg)) return;
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 72.f);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(app.listing.flat.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const int index = app.listing.flat[static_cast<size_t>(row)];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            draw_file_row(app, index);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(colour(kDim), "%u KB", app.archive.entries()[index].size / 1024u);
        }
    }
    ImGui::EndTable();
}

void draw_list_panel(App& app, ImVec2 pos, ImVec2 size) {
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kPanel);
    ImGui::BeginChild("list_panel", size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(ImVec2(kPad, 6.f));
    if (ImGui::RadioButton("List", !app.tree_view)) app.tree_view = false;
    ImGui::SameLine();
    if (ImGui::RadioButton("Tree", app.tree_view)) app.tree_view = true;
    ImGui::SameLine(0.f, 18.f);
    const char* names[] = {"All", "IMG", "NIF", "SND"};
    for (int i = 0; i < 4; ++i) {
        const FilterKind filter = static_cast<FilterKind>(i);
        if (i != 0) ImGui::SameLine();
        if (ImGui::RadioButton(names[i], app.listing.filter == filter) && app.listing.filter != filter) {
            app.listing.filter = filter;
            refresh_listing(app);
        }
    }
    ImGui::SameLine(0.f, 18.f);
    ImGui::SetNextItemWidth(std::max(80.f, ImGui::GetContentRegionAvail().x - kPad));
    if (ImGui::InputTextWithHint("##search", "Search...", app.search, sizeof(app.search))) refresh_listing(app);

    ImGui::SetCursorPos(ImVec2(kPad, kListHeader + 4.f));
    const ImVec2 list_size(size.x - 2.f * kPad, size.y - kListHeader - 30.f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBackground);
    ImGui::BeginChild("list", list_size, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleColor();
    if (app.tree_view) draw_tree(app, app.listing.root);
    else draw_flat_list(app);
    ImGui::EndChild();

    ImGui::SetCursorPos(ImVec2(kPad, size.y - 22.f));
    const int count = app.tree_view ? app.listing.root.total_files : static_cast<int>(app.listing.flat.size());
    ImGui::TextColored(colour(kDim), "%d items", count);
    ImGui::EndChild();
}

void draw_image_controls(App& app) {
    ImGui::TextColored(colour(kDim), "Zoom %.0f%%", app.preview.image_zoom * 100.f);
    ImGui::SameLine();
    if (ImGui::SmallButton("Fit")) app.preview.image_zoom = 0.f;
    ImGui::SameLine();
    if (ImGui::SmallButton("1:1")) {
        app.preview.image_zoom = 1.f;
        app.preview.image_pan[0] = app.preview.image_pan[1] = 0.f;
    }
    ImGui::SameLine();
    ImGui::TextColored(colour(kDim), "Wheel zooms  Drag pans");
}

void draw_model_controls(App& app) {
    AnimationClock& clock = app.renderer->animation();
    if (ImGui::SmallButton(clock.running() ? "Pause" : "Play")) clock.toggle_running();
    ImGui::SameLine();
    if (ImGui::SmallButton("Restart")) clock.set_seconds(0.f);
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset view")) app.preview.reset_camera();
    ImGui::SameLine();
    ImGui::TextColored(colour(kDim), "%.2f s", clock.seconds());
    const std::vector<CharacterClip>& clips = app.renderer->character_clips();
    if (app.preview.model().character && !clips.empty()) {
        ImGui::SameLine(0.f, 18.f);
        int current = app.renderer->character_clip();
        if (current < 0) current = 0;
        const std::string label = std::to_string(current) + " " + clips[static_cast<size_t>(current)].motion.name;
        ImGui::SetNextItemWidth(std::max(120.f, ImGui::GetContentRegionAvail().x - kPad));
        if (ImGui::BeginCombo("##clip", label.c_str())) {
            for (int i = 0; i < static_cast<int>(clips.size()); ++i) {
                char row[160];
                std::snprintf(row, sizeof(row), "%d  %s  %.2f s  id %d", i, clips[static_cast<size_t>(i)].motion.name.c_str(),
                              clips[static_cast<size_t>(i)].motion.duration(), clips[static_cast<size_t>(i)].sequence_id);
                if (ImGui::Selectable(row, i == current)) {
                    app.renderer->set_character_clip(i);
                    clock.set_seconds(0.f);
                    clock.set_running(true);
                }
            }
            ImGui::EndCombo();
        }
    }
}

void draw_audio_controls(App& app) {
    if (ImGui::SmallButton(app.audio.playing() ? "Stop" : "Play")) toggle_playback(app);
    ImGui::SameLine();
    ImGui::TextColored(colour(kDim), "%.2f / %.2f s", app.audio.position(), app.audio.duration());
    if (!app.audio.ready()) {
        ImGui::SameLine();
        ImGui::TextColored(colour(kBad), "no audio device");
    }
}

// The image sits centred at its zoom a zoom of zero fits the area
void draw_image_area(App& app, ImVec2 area) {
    const CachedTexture& image = app.preview.image();
    if (!bgfx::isValid(image.handle) || image.width == 0 || image.height == 0) return;
    if (app.preview.image_zoom <= 0.f) {
        const float fit = std::min((area.x - 16.f) / image.width, (area.y - 16.f) / image.height);
        app.preview.image_zoom = std::clamp(fit, 0.01f, 1.f);
        app.preview.image_pan[0] = app.preview.image_pan[1] = 0.f;
    }
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("image_area", area);
    const bool hovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();
    if (hovered && io.MouseWheel != 0.f)
        app.preview.image_zoom = std::clamp(app.preview.image_zoom * std::pow(1.15f, io.MouseWheel), 0.01f, 32.f);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        app.preview.image_pan[0] += io.MouseDelta.x;
        app.preview.image_pan[1] += io.MouseDelta.y;
    }
    const ImVec2 size(image.width * app.preview.image_zoom, image.height * app.preview.image_zoom);
    const ImVec2 top(origin.x + 0.5f * (area.x - size.x) + app.preview.image_pan[0],
                     origin.y + 0.5f * (area.y - size.y) + app.preview.image_pan[1]);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(origin, ImVec2(origin.x + area.x, origin.y + area.y), true);
    draw->AddImage(imgui_bgfx_texture(image.handle), top, ImVec2(top.x + size.x, top.y + size.y));
    draw->AddRect(ImVec2(top.x - 1.f, top.y - 1.f), ImVec2(top.x + size.x + 1.f, top.y + size.y + 1.f), kAccent);
    draw->PopClipRect();
}

// The scene draws under this hole the button only takes the orbit and the zoom
void draw_model_area(App& app, ImVec2 area) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("model_area", area);
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f))
        app.preview.camera().turn(io.MouseDelta.x * 0.005f, -io.MouseDelta.y * 0.005f);
    if (ImGui::IsItemHovered() && io.MouseWheel != 0.f)
        app.preview.camera().dolly(std::pow(0.85f, io.MouseWheel));
    ImGui::GetWindowDrawList()->AddText(ImVec2(origin.x + 6.f, origin.y + 4.f), IM_COL32(255, 255, 255, 128),
                                        "Drag: Rotate | Wheel: Zoom | R: Reset");
}

void draw_audio_area(App& app, ImVec2 area) {
    const ImVec2 origin = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(origin.x + 0.5f * area.x - 45.f, origin.y + 0.5f * area.y - 30.f));
    if (ImGui::Button(app.audio.playing() ? "Stop" : "Play", ImVec2(90.f, 36.f))) toggle_playback(app);
    ImGui::SetCursorPos(ImVec2(origin.x + 0.5f * area.x - 60.f, origin.y + 0.5f * area.y + 14.f));
    ImGui::TextColored(colour(kDim), "%.2f / %.2f s", app.audio.position(), app.audio.duration());
    if (app.audio.duration() > 0.f) {
        ImGui::SetCursorPos(ImVec2(origin.x + 0.5f * area.x - 100.f, origin.y + 0.5f * area.y + 40.f));
        ImGui::ProgressBar(app.audio.position() / app.audio.duration(), ImVec2(200.f, 6.f), "");
    }
}

void draw_text_area(App& app, ImVec2 area) {
    ImGui::InputTextMultiline("##text", app.preview.text_buffer(), app.preview.text_size(), area,
                              ImGuiInputTextFlags_ReadOnly);
}

void draw_details(App& app, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(35, 37, 43, 255));
    ImGui::BeginChild("details", size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(origin, ImVec2(origin.x + size.x, origin.y), kAccent);
    ImGui::SetCursorPos(ImVec2(kPad, 6.f));
    const FolderNode* folder = app.selected_folder.empty() ? nullptr : find_folder(app.listing.root, app.selected_folder);
    if (app.selected >= 0) {
        const KnC::PakEntry& entry = app.archive.entries()[app.selected];
        const FileKind kind = file_kind(entry.filename);
        ImGui::PushStyleColor(ImGuiCol_Text, kind_colour(kind));
        ImGui::TextUnformatted(entry.filename.c_str());
        ImGui::PopStyleColor();
        ImGui::TextColored(colour(kDim), "Path:");
        ImGui::SameLine(70.f);
        ImGui::TextUnformatted(Archive::clean_path(entry.path).c_str());
        ImGui::TextColored(colour(kDim), "Size:");
        ImGui::SameLine(70.f);
        ImGui::TextUnformatted(format_size(entry.size).c_str());
        ImGui::TextColored(colour(kDim), "Type:");
        ImGui::SameLine(70.f);
        ImGui::TextColored(colour(kind_colour(kind)), "%s", kind_name(kind));
        ImGui::SameLine(0.f, 24.f);
        ImGui::TextColored(colour(kDim), "pak%03d.dat", entry.pakIndex + 1);
        if (!app.preview.error().empty()) ImGui::TextColored(colour(kBad), "%s", app.preview.error().c_str());
        else if (!app.preview.info().empty()) ImGui::TextColored(colour(kDim), "%s", app.preview.info().c_str());
        ImGui::SetCursorPos(ImVec2(size.x - 115.f, 8.f));
        if (ImGui::Button("Extract", ImVec2(100.f, 28.f))) extract_selected_file(app);
    } else if (folder != nullptr) {
        ImGui::PushStyleColor(ImGuiCol_Text, kFolder);
        ImGui::TextUnformatted(folder->name.c_str());
        ImGui::PopStyleColor();
        ImGui::TextColored(colour(kDim), "Folder:");
        ImGui::SameLine(70.f);
        ImGui::TextUnformatted(folder->path.c_str());
        ImGui::TextColored(colour(kDim), "Files:");
        ImGui::SameLine(70.f);
        ImGui::Text("%d (including subfolders)", folder->total_files);
        ImGui::SetCursorPos(ImVec2(size.x - 145.f, 8.f));
        if (ImGui::Button("Extract Folder", ImVec2(130.f, 28.f))) extract_selected_folder(app, *folder);
    } else {
        ImGui::SetCursorPos(ImVec2(kPad, size.y * 0.5f - 8.f));
        ImGui::TextColored(colour(kDim), "Select a file or folder to see details");
    }
    if (!app.status.empty()) {
        ImGui::SetCursorPos(ImVec2(kPad, size.y - 22.f));
        ImGui::TextColored(colour(kGood), "%s", app.status.c_str());
    }
    ImGui::EndChild();
}

void draw_preview_panel(App& app, ImVec2 pos, ImVec2 size) {
    // The preview area is the hole the scene renderer draws into the panel paints around it
    const ImVec2 area(size.x - 2.f * kPad, size.y - kPreviewHeader - kDetails - 2.f * kPad);
    const ImVec2 area_pos(pos.x + kPad, pos.y + kPreviewHeader + kPad);
    const ImVec2 area_end(area_pos.x + area.x, area_pos.y + area.y);
    ImDrawList* panel = ImGui::GetWindowDrawList();
    panel->AddRectFilled(pos, ImVec2(pos.x + size.x, area_pos.y), kPanel);
    panel->AddRectFilled(ImVec2(pos.x, area_end.y), ImVec2(pos.x + size.x, pos.y + size.y), kPanel);
    panel->AddRectFilled(ImVec2(pos.x, area_pos.y), ImVec2(area_pos.x, area_end.y), kPanel);
    panel->AddRectFilled(ImVec2(area_end.x, area_pos.y), ImVec2(pos.x + size.x, area_end.y), kPanel);
    app.scene_rect.x = static_cast<uint16_t>(std::max(0.f, area_pos.x));
    app.scene_rect.y = static_cast<uint16_t>(std::max(0.f, area_pos.y));
    app.scene_rect.width = static_cast<uint16_t>(std::max(1.f, area.x));
    app.scene_rect.height = static_cast<uint16_t>(std::max(1.f, area.y));

    ImGui::SetCursorScreenPos(pos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::BeginChild("preview_panel", size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(ImVec2(kPad, 8.f));
    ImGui::TextUnformatted("Preview");
    ImGui::SameLine(0.f, 18.f);
    switch (app.preview.kind()) {
    case PreviewKind::Image: draw_image_controls(app); break;
    case PreviewKind::Model: draw_model_controls(app); break;
    case PreviewKind::Audio: draw_audio_controls(app); break;
    default: ImGui::TextColored(colour(kDim), "%s", app.preview.info().c_str()); break;
    }

    ImGui::SetCursorScreenPos(area_pos);
    ImGui::BeginChild("preview_area", area, ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    switch (app.preview.kind()) {
    case PreviewKind::Image: draw_image_area(app, area); break;
    case PreviewKind::Model: draw_model_area(app, area); break;
    case PreviewKind::Audio: draw_audio_area(app, area); break;
    case PreviewKind::Text: draw_text_area(app, area); break;
    case PreviewKind::Unsupported:
        ImGui::SetCursorPos(ImVec2(kPad, kPad));
        ImGui::TextColored(colour(kBad), "%s", app.preview.error().empty() ? "No preview" : app.preview.error().c_str());
        break;
    default: break;
    }
    ImGui::EndChild();
    ImGui::GetWindowDrawList()->AddRect(area_pos, area_end, kAccent);

    ImGui::SetCursorPos(ImVec2(1.f, size.y - kDetails));
    draw_details(app, ImVec2(size.x - 2.f, kDetails - 1.f));
    ImGui::EndChild();
}

}

bool open_game_folder(App& app, const std::string& game_dir) {
    app.selected = -1;
    app.selected_folder.clear();
    app.preview.clear();
    if (!app.archive.open(game_dir)) {
        app.status = "No pak001.dat under " + game_dir;
        app.listing.rebuild(app.archive);
        return false;
    }
    refresh_listing(app);
    app.preview.load_light_rig();
    app.status = "Opened " + game_dir;
    return true;
}

void select_entry(App& app, int index) {
    app.selected = index;
    app.selected_folder.clear();
    app.status.clear();
    app.preview.show(index);
}

void draw_ui(App& app, int width, int height) {
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    const ImVec2 list_pos(kGap, kTopBar + kGap);
    const ImVec2 list_size(std::floor(w * 0.42f), h - kTopBar - 2.f * kGap);
    const ImVec2 preview_pos(list_pos.x + list_size.x + kGap, list_pos.y);
    const ImVec2 preview_size(w - preview_pos.x - kGap, list_size.y);

    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("dat_manager", nullptr, flags);
    ImGui::PopStyleVar();

    // Everything but the preview hole gets the window colour behind it
    ImDrawList* back = ImGui::GetBackgroundDrawList();
    const float hx0 = app.scene_rect.x, hy0 = app.scene_rect.y;
    const float hx1 = hx0 + app.scene_rect.width, hy1 = hy0 + app.scene_rect.height;
    back->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(w, hy0), kBackground);
    back->AddRectFilled(ImVec2(0.f, hy1), ImVec2(w, h), kBackground);
    back->AddRectFilled(ImVec2(0.f, hy0), ImVec2(hx0, hy1), kBackground);
    back->AddRectFilled(ImVec2(hx1, hy0), ImVec2(w, hy1), kBackground);
    back->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(w, kTopBar + 6.f), kPanel);

    handle_keys(app);
    draw_top_bar(app, w);
    if (app.archive.opened()) {
        draw_list_panel(app, list_pos, list_size);
        draw_preview_panel(app, preview_pos, preview_size);
    } else {
        ImGui::SetCursorPos(ImVec2(w * 0.5f - 90.f, h * 0.5f - 10.f));
        ImGui::TextUnformatted("No PAK files loaded");
        if (!app.status.empty()) {
            ImGui::SetCursorPos(ImVec2(w * 0.5f - 90.f, h * 0.5f + 14.f));
            ImGui::TextColored(colour(kBad), "%s", app.status.c_str());
        }
        app.scene_rect = ViewportRect{0, 0, 1, 1};
    }
    ImGui::End();
    app.reveal.clear();
}

}
