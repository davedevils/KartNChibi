#include "ui_canvas.h"

#include "imgui_bgfx.h"
#include "ui_assets.h"
#include "ui_dialogs.h"
#include "ui_json.h"
#include "ui_screens.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace KnC::Tools {
namespace {

constexpr ImU32 kColWhite = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kColYellow = IM_COL32(253, 249, 0, 255);
constexpr ImU32 kColGreen = IM_COL32(0, 228, 48, 255);
constexpr ImU32 kColGold = IM_COL32(255, 203, 0, 255);
constexpr float kHandleSize = 8.f;
constexpr float kHandleHit = 6.f;

struct View {
    float origin_x = 0.f;
    float origin_y = 0.f;
    float scale_x = 1.f;
    float scale_y = 1.f;
};

View canvas_view() {
    View view;
    view.origin_x = g_editor.canvasX + g_editor.panX;
    view.origin_y = g_editor.canvasY + g_editor.panY;
    view.scale_x = g_editor.canvasW / static_cast<float>(kGameWidth) * g_editor.zoom;
    view.scale_y = g_editor.canvasH / static_cast<float>(kGameHeight) * g_editor.zoom;
    return view;
}

ImTextureID texture_id(const LoadedTexture* texture) {
    return texture != nullptr && texture->valid() ? imgui_bgfx_texture(texture->handle) : ImTextureID_Invalid;
}

float element_width(const UIElement& elem) {
    float width = elem.bounds.width;
    if (width < 1.f && elem.texture != nullptr && elem.texture->valid()) width = static_cast<float>(elem.texture->width);
    return width < 1.f ? 100.f : width;
}

float element_height(const UIElement& elem) {
    float height = elem.bounds.height;
    if (height < 1.f && elem.texture != nullptr && elem.texture->valid()) height = static_cast<float>(elem.texture->height);
    return height < 1.f ? 30.f : height;
}

// The element rectangle in window pixels
Rect screen_rect(const UIElement& elem, const View& view) {
    return {view.origin_x + elem.bounds.x * view.scale_x, view.origin_y + elem.bounds.y * view.scale_y,
            element_width(elem) * view.scale_x, element_height(elem) * view.scale_y};
}

void game_coords(const View& view, float mouse_x, float mouse_y, float& game_x, float& game_y) {
    game_x = (mouse_x - view.origin_x) / view.scale_x;
    game_y = (mouse_y - view.origin_y) / view.scale_y;
}

void draw_image_or_fallback(ImDrawList* dl, const UIElement& elem, const Rect& sb) {
    const LoadedTexture* tex = elem.texture;
    if (!elem.enabled && elem.disabledTexture != nullptr && elem.disabledTexture->valid()) tex = elem.disabledTexture;
    else if (elem.buttonState == UIElement::BTN_PRESSED && elem.pressedTexture != nullptr && elem.pressedTexture->valid()) tex = elem.pressedTexture;
    else if (elem.buttonState == UIElement::BTN_HOVER && elem.hoverTexture != nullptr && elem.hoverTexture->valid()) tex = elem.hoverTexture;

    const ImVec2 a(sb.x, sb.y);
    const ImVec2 b(sb.x + sb.width, sb.y + sb.height);
    if (tex != nullptr && tex->valid()) {
        dl->AddImage(texture_id(tex), a, b);
        return;
    }
    const ImU32 fill = elem.type == ELEM_BUTTON ? IM_COL32(100, 100, 150, 255) : IM_COL32(80, 80, 100, 255);
    dl->AddRectFilled(a, b, fill);
    dl->AddRect(a, b, kColYellow, 0.f, 0, 2.f);
    char label[32];
    std::strncpy(label, elem.name.c_str(), 24);
    label[24] = '\0';
    const ImVec2 size = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(sb.x + sb.width * 0.5f - size.x * 0.5f, sb.y + sb.height * 0.5f - size.y * 0.5f), kColWhite, label);
}

void draw_element(ImDrawList* dl, const UIElement& elem, const View& view) {
    const Rect sb = screen_rect(elem, view);
    const ImVec2 a(sb.x, sb.y);
    const ImVec2 b(sb.x + sb.width, sb.y + sb.height);
    switch (elem.type) {
    case ELEM_IMAGE:
    case ELEM_BUTTON:
        draw_image_or_fallback(dl, elem, sb);
        break;
    case ELEM_INPUT:
        dl->AddRectFilled(a, b, IM_COL32(50, 50, 60, 255));
        dl->AddRect(a, b, kColWhite, 0.f, 0, 2.f);
        break;
    case ELEM_LIST:
        dl->AddRectFilled(a, b, IM_COL32(40, 40, 50, 200));
        dl->AddRect(a, b, IM_COL32(100, 100, 150, 255), 0.f, 0, 2.f);
        break;
    case ELEM_PANEL:
        dl->AddRectFilled(a, b, IM_COL32(50, 50, 60, 200));
        break;
    case ELEM_CHECKBOX:
        if (elem.texture != nullptr && elem.texture->valid()) {
            dl->AddImage(texture_id(elem.texture), a, b);
        } else {
            dl->AddRectFilled(a, b, IM_COL32(80, 80, 100, 255));
            dl->AddRect(a, b, kColWhite, 0.f, 0, 2.f);
            if (elem.checked) {
                dl->AddLine(a, b, kColGreen, 1.f);
                dl->AddLine(ImVec2(b.x, a.y), ImVec2(a.x, b.y), kColGreen, 1.f);
            }
        }
        break;
    case ELEM_TEXT: {
        const float size = std::max(4.f, static_cast<float>(elem.fontSize) * view.scale_y);
        const ImU32 colour = IM_COL32(elem.textColor.r, elem.textColor.g, elem.textColor.b, elem.textColor.a);
        dl->AddText(ImGui::GetFont(), size, a, colour, elem.text.c_str());
        break;
    }
    default:
        break;
    }

    if (!g_editor.previewMode && (elem.selected || g_editor.showBounds)) {
        dl->AddRect(a, b, elem.selected ? kColGold : IM_COL32(100, 255, 100, 100), 0.f, 0, 2.f);
        if (elem.selected) {
            const float hs = kHandleSize * 0.5f;
            const ImVec2 corners[4] = {a, ImVec2(b.x, a.y), b, ImVec2(a.x, b.y)};
            for (const ImVec2& c : corners)
                dl->AddRectFilled(ImVec2(c.x - hs, c.y - hs), ImVec2(c.x + hs, c.y + hs), kColGold);
        }
    }
}

void draw_grid(ImDrawList* dl, const View& view) {
    const ImU32 colour = g_editor.snapToGrid ? IM_COL32(255, 255, 255, 25) : IM_COL32(255, 255, 255, 15);
    const float x0 = g_editor.canvasX;
    const float y0 = g_editor.canvasY;
    const float x1 = x0 + g_editor.canvasW;
    const float y1 = y0 + g_editor.canvasH;
    const int step = std::max(1, g_editor.gridSize);
    for (int x = 0; x <= kGameWidth; x += step) {
        const float sx = std::floor(view.origin_x + x * view.scale_x);
        if (sx < x0 || sx > x1) continue;
        dl->AddLine(ImVec2(sx, y0), ImVec2(sx, y1), colour, 1.f);
    }
    for (int y = 0; y <= kGameHeight; y += step) {
        const float sy = std::floor(view.origin_y + y * view.scale_y);
        if (sy < y0 || sy > y1) continue;
        dl->AddLine(ImVec2(x0, sy), ImVec2(x1, sy), colour, 1.f);
    }
}

std::vector<size_t> sorted_visible(const UIScreen& screen) {
    std::vector<size_t> order;
    for (size_t i = 0; i < screen.elements.size(); ++i)
        if (screen.elements[i].visible) order.push_back(i);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        if (screen.elements[a].zIndex != screen.elements[b].zIndex) return screen.elements[a].zIndex < screen.elements[b].zIndex;
        return a < b;
    });
    return order;
}

float snapped(float value) {
    if (!g_editor.snapToGrid) return value;
    const float step = static_cast<float>(std::max(1, g_editor.gridSize));
    return std::round(value / step) * step;
}

int hit_handle(const UIElement& sel, const View& view, float mouse_x, float mouse_y) {
    const Rect sb = screen_rect(sel, view);
    const float hx[4] = {sb.x, sb.x + sb.width, sb.x + sb.width, sb.x};
    const float hy[4] = {sb.y, sb.y, sb.y + sb.height, sb.y + sb.height};
    for (int c = 0; c < 4; ++c)
        if (std::fabs(mouse_x - hx[c]) <= kHandleHit && std::fabs(mouse_y - hy[c]) <= kHandleHit) return c;
    return -1;
}

void apply_resize(UIElement& elem, float gx, float gy) {
    Rect& b = elem.bounds;
    switch (g_editor.resizeHandle) {
    case 0: b.width += b.x - gx; b.height += b.y - gy; b.x = gx; b.y = gy; break;
    case 1: b.width = gx - b.x; b.height += b.y - gy; b.y = gy; break;
    case 2: b.width = gx - b.x; b.height = gy - b.y; break;
    case 3: b.width += b.x - gx; b.x = gx; b.height = gy - b.y; break;
    default: break;
    }
    b.width = std::max(10.f, snapped(b.width));
    b.height = std::max(10.f, snapped(b.height));
}

void handle_canvas_input(bool hovered, bool active) {
    if (g_editor.previewMode) return;
    ImGuiIO& io = ImGui::GetIO();
    UIScreen& screen = current_screen();
    const View view = canvas_view();
    float gx = 0.f;
    float gy = 0.f;
    game_coords(view, io.MousePos.x, io.MousePos.y, gx, gy);
    const bool inside = hovered || active;

    if (hovered) {
        for (UIElement& elem : screen.elements) {
            if (elem.type != ELEM_BUTTON || !elem.enabled) continue;
            const bool over = gx >= elem.bounds.x && gx <= elem.bounds.x + element_width(elem) &&
                              gy >= elem.bounds.y && gy <= elem.bounds.y + element_height(elem);
            elem.buttonState = !over ? UIElement::BTN_NORMAL : io.MouseDown[0] ? UIElement::BTN_PRESSED : UIElement::BTN_HOVER;
        }
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        bool started_resize = false;
        if (UIElement* sel = selected_element()) {
            const int handle = hit_handle(*sel, view, io.MousePos.x, io.MousePos.y);
            if (handle >= 0) {
                push_undo();
                g_editor.isResizing = true;
                g_editor.resizeHandle = handle;
                g_editor.dragging = false;
                started_resize = true;
            }
        }
        if (!started_resize) {
            int found = -1;
            for (int i = static_cast<int>(screen.elements.size()) - 1; i >= 0; --i) {
                const UIElement& elem = screen.elements[i];
                if (!elem.visible) continue;
                if (gx >= elem.bounds.x && gx <= elem.bounds.x + element_width(elem) &&
                    gy >= elem.bounds.y && gy <= elem.bounds.y + element_height(elem)) {
                    found = i;
                    break;
                }
            }
            select_element(found);
            if (found >= 0) {
                push_undo();
                g_editor.dragging = true;
                g_editor.dragOffsetX = gx - screen.elements[found].bounds.x;
                g_editor.dragOffsetY = gy - screen.elements[found].bounds.y;
            }
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        g_editor.dragging = false;
        g_editor.isResizing = false;
    }

    if (UIElement* elem = selected_element()) {
        if (g_editor.dragging && inside) {
            elem->bounds.x = snapped(gx - g_editor.dragOffsetX);
            elem->bounds.y = snapped(gy - g_editor.dragOffsetY);
        }
        if (g_editor.isResizing && inside) apply_resize(*elem, gx, gy);
    }

    if (hovered && std::fabs(io.MouseWheel) > 0.01f) {
        if (io.KeyCtrl) {
            g_editor.gridSize += io.MouseWheel > 0.f ? 5 : -5;
            g_editor.gridSize = std::clamp(g_editor.gridSize, 5, 100);
        } else {
            g_editor.zoom = std::clamp(g_editor.zoom * (1.f + io.MouseWheel * 0.1f), 0.25f, 4.f);
        }
    }

    if (inside && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        g_editor.panX += io.MouseDelta.x;
        g_editor.panY += io.MouseDelta.y;
    }
}

}

Layout compute_layout() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float x0 = vp->WorkPos.x;
    const float y0 = vp->WorkPos.y;
    const float w = std::max(vp->WorkSize.x, kSidebarWidth + kToolbarWidth + 1.f);
    const float h = std::max(vp->WorkSize.y, kStatusHeight + 1.f);
    Layout layout;
    layout.sidebar_x = x0;
    layout.sidebar_y = y0;
    layout.sidebar_w = kSidebarWidth;
    layout.sidebar_h = h - kStatusHeight;
    layout.canvas_x = x0 + kSidebarWidth;
    layout.canvas_y = y0;
    layout.canvas_w = w - kSidebarWidth - kToolbarWidth;
    layout.canvas_h = h - kStatusHeight;
    layout.toolbar_x = x0 + w - kToolbarWidth;
    layout.toolbar_y = y0;
    layout.toolbar_w = kToolbarWidth;
    layout.toolbar_h = h - kStatusHeight;
    layout.status_x = x0;
    layout.status_y = y0 + h - kStatusHeight;
    layout.status_w = w;
    layout.status_h = kStatusHeight;
    return layout;
}

void draw_canvas() {
    const Layout layout = compute_layout();
    ImGui::SetNextWindowPos(ImVec2(layout.canvas_x, layout.canvas_y));
    ImGui::SetNextWindowSize(ImVec2(layout.canvas_w, layout.canvas_h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("##canvas", nullptr, kFixedWindow | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    size.x = std::max(size.x, 1.f);
    size.y = std::max(size.y, 1.f);
    g_editor.canvasX = origin.x;
    g_editor.canvasY = origin.y;
    g_editor.canvasW = size.x;
    g_editor.canvasH = size.y;

    ImGui::InvisibleButton("##canvas_hit", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 end(origin.x + size.x, origin.y + size.y);
    dl->PushClipRect(origin, end, true);
    dl->AddRectFilled(origin, end, IM_COL32(35, 35, 42, 255));

    UIScreen& screen = current_screen();
    if (screen.background != nullptr && screen.background->valid()) dl->AddImage(texture_id(screen.background), origin, end);

    const View view = canvas_view();
    if (g_editor.showGrid && !g_editor.previewMode) draw_grid(dl, view);

    const std::vector<size_t> order = sorted_visible(screen);
    for (int pass = 0; pass < 2; ++pass)
        for (const size_t index : order) {
            const UIElement& elem = screen.elements[index];
            if ((pass == 0) == elem.selected) continue;
            draw_element(dl, elem, view);
        }

    dl->AddRect(origin, end, IM_COL32(60, 60, 70, 255), 0.f, 0, 1.f);
    dl->PopClipRect();

    handle_canvas_input(hovered, active);

    ImGui::End();
    ImGui::PopStyleVar();
}

void draw_toolbar() {
    const Layout layout = compute_layout();
    ImGui::SetNextWindowPos(ImVec2(layout.toolbar_x, layout.toolbar_y));
    ImGui::SetNextWindowSize(ImVec2(layout.toolbar_w, layout.toolbar_h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.f, 10.f));
    ImGui::Begin("##toolbar", nullptr, kFixedWindow | ImGuiWindowFlags_NoScrollbar);

    static const char* const kLabels[] = {"Img", "Btn", "Txt", "Inp", "Lst", "Pnl", "Chk"};
    for (int i = 0; i < ELEM_TYPE_COUNT; ++i) {
        if (ImGui::Button(kLabels[i], ImVec2(40.f, 40.f))) open_add_element_dialog(i);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", element_type_name(static_cast<ElementType>(i)));
    }

    ImGui::Spacing();
    if (g_editor.previewMode) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 140, 60, 255));
    if (ImGui::Button("Prev", ImVec2(40.f, 20.f))) g_editor.previewMode = !g_editor.previewMode;
    if (g_editor.previewMode) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Preview [P]");

    ImGui::End();
    ImGui::PopStyleVar();
}

void draw_status_bar() {
    const Layout layout = compute_layout();
    ImGui::SetNextWindowPos(ImVec2(layout.status_x, layout.status_y));
    ImGui::SetNextWindowSize(ImVec2(layout.status_w, layout.status_h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 3.f));
    ImGui::Begin("##status", nullptr, kFixedWindow | ImGuiWindowFlags_NoScrollbar);

    const UIScreen& screen = current_screen();
    const UIElement* sel = selected_element();
    const char* sel_name = sel != nullptr ? sel->name.c_str() : "None";

    const ImGuiIO& io = ImGui::GetIO();
    const View view = canvas_view();
    float gx = 0.f;
    float gy = 0.f;
    game_coords(view, io.MousePos.x, io.MousePos.y, gx, gy);
    const bool in_canvas = io.MousePos.x > g_editor.canvasX && io.MousePos.x < g_editor.canvasX + g_editor.canvasW &&
                           io.MousePos.y > g_editor.canvasY && io.MousePos.y < g_editor.canvasY + g_editor.canvasH;
    char cursor[64];
    if (in_canvas) std::snprintf(cursor, sizeof(cursor), "Cursor: %d, %d", static_cast<int>(gx), static_cast<int>(gy));
    else std::snprintf(cursor, sizeof(cursor), "Cursor: ---");

    if (g_editor.previewMode) {
        ImGui::TextColored(ImVec4(0.4f, 0.86f, 0.4f, 1.f), "State: %s | Elements: %d | Zoom: %.0f%% | %s | PREVIEW MODE - P to exit",
                           state_name(g_editor.currentState), static_cast<int>(screen.elements.size()), g_editor.zoom * 100.f, cursor);
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), "State: %s | Elements: %d | Zoom: %.0f%% | Selected: %s | %s",
                           state_name(g_editor.currentState), static_cast<int>(screen.elements.size()), g_editor.zoom * 100.f,
                           sel_name, cursor);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void handle_shortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) return;

    if (g_editor.previewMode) {
        if (ImGui::IsKeyPressed(ImGuiKey_P, false)) g_editor.previewMode = false;
        return;
    }

    UIScreen& screen = current_screen();
    const bool ctrl = io.KeyCtrl;
    const bool shift = io.KeyShift;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) { undo(); return; }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) { redo(); return; }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        export_screen_json(screen, g_editor.currentState, default_export_name(screen, g_editor.currentState));
        return;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        if (const UIElement* sel = selected_element()) {
            g_editor.clipboard = *sel;
            g_editor.hasClipboard = true;
            std::printf("[COPY] %s\n", sel->name.c_str());
        }
        return;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
        if (g_editor.hasClipboard) {
            push_undo();
            UIElement pasted = g_editor.clipboard;
            pasted.bounds.x += 20.f;
            pasted.bounds.y += 20.f;
            pasted.name += " (copy)";
            pasted.selected = false;
            screen.elements.push_back(pasted);
            resolve_element_textures(screen.elements);
            select_element(static_cast<int>(screen.elements.size()) - 1);
            std::printf("[PASTE] %s\n", pasted.name.c_str());
        }
        return;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
        if (const UIElement* sel = selected_element()) {
            push_undo();
            UIElement dup = *sel;
            dup.bounds.x += 10.f;
            dup.bounds.y += 10.f;
            dup.name += " (dup)";
            dup.selected = false;
            screen.elements.push_back(dup);
            select_element(static_cast<int>(screen.elements.size()) - 1);
        }
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        if (has_selection()) {
            push_undo();
            std::printf("[DELETE] %s\n", screen.elements[g_editor.selectedElement].name.c_str());
            screen.elements.erase(screen.elements.begin() + g_editor.selectedElement);
            select_element(-1);
        }
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        select_element(-1);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_P, false)) g_editor.previewMode = !g_editor.previewMode;

    if (UIElement* elem = selected_element()) {
        const float dx = (ImGui::IsKeyPressed(ImGuiKey_RightArrow) ? 1.f : 0.f) - (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) ? 1.f : 0.f);
        const float dy = (ImGui::IsKeyPressed(ImGuiKey_DownArrow) ? 1.f : 0.f) - (ImGui::IsKeyPressed(ImGuiKey_UpArrow) ? 1.f : 0.f);
        if (ctrl) {
            elem->bounds.x += dx;
            elem->bounds.y += dy;
        }
        if (shift) {
            elem->bounds.width += dx;
            elem->bounds.height += dy;
        }
    }

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_0, false)) {
        g_editor.zoom = 1.f;
        g_editor.panX = 0.f;
        g_editor.panY = 0.f;
    }
}

}
