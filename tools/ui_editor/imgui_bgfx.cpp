#include "imgui_bgfx.h"

#include <bgfx/embedded_shader.h>
#include <bx/math.h>

#include <GLFW/glfw3.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "vs_ocornut_imgui.bin.h"
#include "fs_ocornut_imgui.bin.h"
#include "roboto_regular.ttf.h"

namespace KnC::Tools {
namespace {

const bgfx::EmbeddedShader kShaders[] = {
    BGFX_EMBEDDED_SHADER(vs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),
    BGFX_EMBEDDED_SHADER_END()
};

struct Backend {
    GLFWwindow*         window = nullptr;
    bgfx::VertexLayout  layout;
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle sampler = BGFX_INVALID_HANDLE;
    bgfx::ViewId        view = 0;
    ImFont*             font = nullptr;
};

Backend g_backend;

void* imgui_alloc(size_t size, void*) { return std::malloc(size); }
void  imgui_free(void* ptr, void*) { std::free(ptr); }

bgfx::TextureHandle unpack_texture(ImTextureID id) {
    bgfx::TextureHandle handle;
    handle.idx = static_cast<uint16_t>(id - 1);
    return handle;
}

ImGuiKey map_key(int key) {
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) return static_cast<ImGuiKey>(ImGuiKey_A + (key - GLFW_KEY_A));
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) return static_cast<ImGuiKey>(ImGuiKey_0 + (key - GLFW_KEY_0));
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) return static_cast<ImGuiKey>(ImGuiKey_F1 + (key - GLFW_KEY_F1));
    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + (key - GLFW_KEY_KP_0));
    switch (key) {
    case GLFW_KEY_TAB: return ImGuiKey_Tab;
    case GLFW_KEY_LEFT: return ImGuiKey_LeftArrow;
    case GLFW_KEY_RIGHT: return ImGuiKey_RightArrow;
    case GLFW_KEY_UP: return ImGuiKey_UpArrow;
    case GLFW_KEY_DOWN: return ImGuiKey_DownArrow;
    case GLFW_KEY_PAGE_UP: return ImGuiKey_PageUp;
    case GLFW_KEY_PAGE_DOWN: return ImGuiKey_PageDown;
    case GLFW_KEY_HOME: return ImGuiKey_Home;
    case GLFW_KEY_END: return ImGuiKey_End;
    case GLFW_KEY_INSERT: return ImGuiKey_Insert;
    case GLFW_KEY_DELETE: return ImGuiKey_Delete;
    case GLFW_KEY_BACKSPACE: return ImGuiKey_Backspace;
    case GLFW_KEY_SPACE: return ImGuiKey_Space;
    case GLFW_KEY_ENTER: return ImGuiKey_Enter;
    case GLFW_KEY_ESCAPE: return ImGuiKey_Escape;
    case GLFW_KEY_APOSTROPHE: return ImGuiKey_Apostrophe;
    case GLFW_KEY_COMMA: return ImGuiKey_Comma;
    case GLFW_KEY_MINUS: return ImGuiKey_Minus;
    case GLFW_KEY_PERIOD: return ImGuiKey_Period;
    case GLFW_KEY_SLASH: return ImGuiKey_Slash;
    case GLFW_KEY_SEMICOLON: return ImGuiKey_Semicolon;
    case GLFW_KEY_EQUAL: return ImGuiKey_Equal;
    case GLFW_KEY_LEFT_BRACKET: return ImGuiKey_LeftBracket;
    case GLFW_KEY_BACKSLASH: return ImGuiKey_Backslash;
    case GLFW_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
    case GLFW_KEY_GRAVE_ACCENT: return ImGuiKey_GraveAccent;
    case GLFW_KEY_CAPS_LOCK: return ImGuiKey_CapsLock;
    case GLFW_KEY_SCROLL_LOCK: return ImGuiKey_ScrollLock;
    case GLFW_KEY_NUM_LOCK: return ImGuiKey_NumLock;
    case GLFW_KEY_PRINT_SCREEN: return ImGuiKey_PrintScreen;
    case GLFW_KEY_PAUSE: return ImGuiKey_Pause;
    case GLFW_KEY_KP_DECIMAL: return ImGuiKey_KeypadDecimal;
    case GLFW_KEY_KP_DIVIDE: return ImGuiKey_KeypadDivide;
    case GLFW_KEY_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
    case GLFW_KEY_KP_SUBTRACT: return ImGuiKey_KeypadSubtract;
    case GLFW_KEY_KP_ADD: return ImGuiKey_KeypadAdd;
    case GLFW_KEY_KP_ENTER: return ImGuiKey_KeypadEnter;
    case GLFW_KEY_KP_EQUAL: return ImGuiKey_KeypadEqual;
    case GLFW_KEY_LEFT_SHIFT: return ImGuiKey_LeftShift;
    case GLFW_KEY_LEFT_CONTROL: return ImGuiKey_LeftCtrl;
    case GLFW_KEY_LEFT_ALT: return ImGuiKey_LeftAlt;
    case GLFW_KEY_LEFT_SUPER: return ImGuiKey_LeftSuper;
    case GLFW_KEY_RIGHT_SHIFT: return ImGuiKey_RightShift;
    case GLFW_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
    case GLFW_KEY_RIGHT_ALT: return ImGuiKey_RightAlt;
    case GLFW_KEY_RIGHT_SUPER: return ImGuiKey_RightSuper;
    case GLFW_KEY_MENU: return ImGuiKey_Menu;
    default: return ImGuiKey_None;
    }
}

bool key_down(int key) { return glfwGetKey(g_backend.window, key) == GLFW_PRESS; }

// Modifiers are polled so a combo works whatever order the keys arrive
void update_modifiers() {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl, key_down(GLFW_KEY_LEFT_CONTROL) || key_down(GLFW_KEY_RIGHT_CONTROL));
    io.AddKeyEvent(ImGuiMod_Shift, key_down(GLFW_KEY_LEFT_SHIFT) || key_down(GLFW_KEY_RIGHT_SHIFT));
    io.AddKeyEvent(ImGuiMod_Alt, key_down(GLFW_KEY_LEFT_ALT) || key_down(GLFW_KEY_RIGHT_ALT));
    io.AddKeyEvent(ImGuiMod_Super, key_down(GLFW_KEY_LEFT_SUPER) || key_down(GLFW_KEY_RIGHT_SUPER));
}

void on_mouse_button(GLFWwindow*, int button, int action, int) {
    if (button < 0 || button > 2) return;
    ImGui::GetIO().AddMouseButtonEvent(button, action == GLFW_PRESS);
}

void on_cursor(GLFWwindow*, double x, double y) {
    ImGui::GetIO().AddMousePosEvent(static_cast<float>(x), static_cast<float>(y));
}

void on_scroll(GLFWwindow*, double x, double y) {
    ImGui::GetIO().AddMouseWheelEvent(static_cast<float>(x), static_cast<float>(y));
}

void on_key(GLFWwindow*, int key, int, int action, int) {
    if (action == GLFW_REPEAT) return;
    update_modifiers();
    const ImGuiKey mapped = map_key(key);
    if (mapped != ImGuiKey_None) ImGui::GetIO().AddKeyEvent(mapped, action == GLFW_PRESS);
}

void on_char(GLFWwindow*, unsigned int codepoint) { ImGui::GetIO().AddInputCharacter(codepoint); }

void on_focus(GLFWwindow*, int focused) { ImGui::GetIO().AddFocusEvent(focused != 0); }

const char* get_clipboard(ImGuiContext*) { return glfwGetClipboardString(g_backend.window); }
void        set_clipboard(ImGuiContext*, const char* text) { glfwSetClipboardString(g_backend.window, text); }

// The font atlas arrives as texture requests since ImGui 1 92
void update_textures(ImDrawData* data) {
    if (data->Textures == nullptr) return;
    for (ImTextureData* tex : *data->Textures) {
        if (tex->Status == ImTextureStatus_WantCreate) {
            const bgfx::TextureHandle handle = bgfx::createTexture2D(
                static_cast<uint16_t>(tex->Width), static_cast<uint16_t>(tex->Height), false, 1,
                bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
                bgfx::copy(tex->GetPixels(), static_cast<uint32_t>(tex->GetSizeInBytes())));
            bgfx::setName(handle, "ImGui font atlas");
            tex->SetTexID(imgui_bgfx_texture(handle));
            tex->SetStatus(ImTextureStatus_OK);
        } else if (tex->Status == ImTextureStatus_WantUpdates) {
            const bgfx::TextureHandle handle = unpack_texture(tex->GetTexID());
            for (const ImTextureRect& rect : tex->Updates) {
                const uint32_t bpp = static_cast<uint32_t>(tex->BytesPerPixel);
                const uint32_t row_bytes = rect.w * bpp;
                const bgfx::Memory* memory = bgfx::alloc(row_bytes * rect.h);
                for (int row = 0; row < rect.h; ++row)
                    std::memcpy(memory->data + row * row_bytes, tex->GetPixelsAt(rect.x, rect.y + row), row_bytes);
                bgfx::updateTexture2D(handle, 0, 0, rect.x, rect.y, rect.w, rect.h, memory);
            }
            tex->SetStatus(ImTextureStatus_OK);
        } else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0) {
            bgfx::destroy(unpack_texture(tex->GetTexID()));
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }
    }
}

void render(ImDrawData* data) {
    update_textures(data);
    const int width = static_cast<int>(data->DisplaySize.x * data->FramebufferScale.x);
    const int height = static_cast<int>(data->DisplaySize.y * data->FramebufferScale.y);
    if (width <= 0 || height <= 0) return;

    const bgfx::ViewId view = g_backend.view;
    bgfx::setViewName(view, "ImGui");
    bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
    const bgfx::Caps* caps = bgfx::getCaps();
    float ortho[16];
    const float x = data->DisplayPos.x;
    const float y = data->DisplayPos.y;
    bx::mtxOrtho(ortho, x, x + data->DisplaySize.x, y + data->DisplaySize.y, y, 0.f, 1000.f, 0.f, caps->homogeneousDepth);
    bgfx::setViewTransform(view, nullptr, ortho);
    bgfx::setViewRect(view, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));

    const ImVec2 clip_pos = data->DisplayPos;
    const ImVec2 clip_scale = data->FramebufferScale;
    for (int list_index = 0; list_index < data->CmdListsCount; ++list_index) {
        const ImDrawList* list = data->CmdLists[list_index];
        const uint32_t vertex_count = static_cast<uint32_t>(list->VtxBuffer.size());
        const uint32_t index_count = static_cast<uint32_t>(list->IdxBuffer.size());
        if (vertex_count != bgfx::getAvailTransientVertexBuffer(vertex_count, g_backend.layout)) break;
        if (index_count != 0 && index_count != bgfx::getAvailTransientIndexBuffer(index_count)) break;

        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer tib;
        bgfx::allocTransientVertexBuffer(&tvb, vertex_count, g_backend.layout);
        bgfx::allocTransientIndexBuffer(&tib, index_count, sizeof(ImDrawIdx) == 4);
        std::memcpy(tvb.data, list->VtxBuffer.begin(), vertex_count * sizeof(ImDrawVert));
        std::memcpy(tib.data, list->IdxBuffer.begin(), index_count * sizeof(ImDrawIdx));

        bgfx::Encoder* encoder = bgfx::begin();
        for (const ImDrawCmd& cmd : list->CmdBuffer) {
            if (cmd.UserCallback != nullptr) {
                cmd.UserCallback(list, &cmd);
                continue;
            }
            if (cmd.ElemCount == 0) continue;
            const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA |
                                   BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
            bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
            const ImTextureID id = cmd.GetTexID();
            if (id != ImTextureID_Invalid) texture = unpack_texture(id);

            const float clip_x0 = (cmd.ClipRect.x - clip_pos.x) * clip_scale.x;
            const float clip_y0 = (cmd.ClipRect.y - clip_pos.y) * clip_scale.y;
            const float clip_x1 = (cmd.ClipRect.z - clip_pos.x) * clip_scale.x;
            const float clip_y1 = (cmd.ClipRect.w - clip_pos.y) * clip_scale.y;
            if (clip_x0 >= width || clip_y0 >= height || clip_x1 < 0.f || clip_y1 < 0.f) continue;
            const uint16_t sx = static_cast<uint16_t>(bx::max(clip_x0, 0.f));
            const uint16_t sy = static_cast<uint16_t>(bx::max(clip_y0, 0.f));
            encoder->setScissor(sx, sy, static_cast<uint16_t>(bx::min(clip_x1, 65535.f) - sx),
                                static_cast<uint16_t>(bx::min(clip_y1, 65535.f) - sy));
            encoder->setState(state);
            encoder->setTexture(0, g_backend.sampler, texture);
            encoder->setVertexBuffer(0, &tvb, cmd.VtxOffset, vertex_count);
            encoder->setIndexBuffer(&tib, cmd.IdxOffset, cmd.ElemCount);
            encoder->submit(view, g_backend.program);
        }
        bgfx::end(encoder);
    }
}

} // namespace

bool imgui_bgfx_create(GLFWwindow* window, float font_size) {
    g_backend.window = window;
    IMGUI_CHECKVERSION();
    ImGui::SetAllocatorFunctions(imgui_alloc, imgui_free, nullptr);
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280.f, 800.f);
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
    io.BackendPlatformName = "glfw";
    io.BackendRendererName = "bgfx";

    ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
    platform.Platform_GetClipboardTextFn = get_clipboard;
    platform.Platform_SetClipboardTextFn = set_clipboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 3.f;
    style.WindowRounding = 0.f;
    style.WindowBorderSize = 0.f;

    const bgfx::RendererType::Enum type = bgfx::getRendererType();
    g_backend.program = bgfx::createProgram(bgfx::createEmbeddedShader(kShaders, type, "vs_ocornut_imgui"),
                                            bgfx::createEmbeddedShader(kShaders, type, "fs_ocornut_imgui"), true);
    if (!bgfx::isValid(g_backend.program)) return false;
    g_backend.sampler = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
    g_backend.layout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    g_backend.font = io.Fonts->AddFontFromMemoryTTF(const_cast<uint8_t*>(s_robotoRegularTtf),
                                                    static_cast<int>(sizeof(s_robotoRegularTtf)), font_size, &config);

    glfwSetMouseButtonCallback(window, on_mouse_button);
    glfwSetCursorPosCallback(window, on_cursor);
    glfwSetScrollCallback(window, on_scroll);
    glfwSetKeyCallback(window, on_key);
    glfwSetCharCallback(window, on_char);
    glfwSetWindowFocusCallback(window, on_focus);
    return true;
}

void imgui_bgfx_destroy() {
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
        if (tex->RefCount != 1 || tex->GetTexID() == ImTextureID_Invalid) continue;
        bgfx::destroy(unpack_texture(tex->GetTexID()));
        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
    ImGui::DestroyContext();
    if (bgfx::isValid(g_backend.sampler)) bgfx::destroy(g_backend.sampler);
    if (bgfx::isValid(g_backend.program)) bgfx::destroy(g_backend.program);
    g_backend = Backend();
}

void imgui_bgfx_begin_frame(int width, int height, float delta_seconds) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    io.DeltaTime = delta_seconds > 0.f ? delta_seconds : 1.f / 60.f;
    update_modifiers();
    ImGui::NewFrame();
}

void imgui_bgfx_end_frame(bgfx::ViewId view) {
    g_backend.view = view;
    ImGui::Render();
    render(ImGui::GetDrawData());
}

ImTextureID imgui_bgfx_texture(bgfx::TextureHandle handle) {
    if (!bgfx::isValid(handle)) return ImTextureID_Invalid;
    return static_cast<ImTextureID>(handle.idx) + 1;
}

}

// The bgfx imgui user header declares this overload for its widgets one font serves both slots
namespace ImGui {
void PushFont(Font::Enum, float font_size_base_unscaled) {
    PushFont(KnC::Tools::g_backend.font, font_size_base_unscaled);
}
}

// The bgfx imconfig turns the stb implementations off inside imgui so this file carries them
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4244 4505)
#endif
#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
