// Dear ImGui drawn through bgfx with glfw feeding the input
#pragma once

#include <bgfx/bgfx.h>
#include <dear-imgui/imgui.h>

struct GLFWwindow;

namespace KnC::Tools {

// Creates the context the font and the program and hooks the glfw callbacks
bool imgui_bgfx_create(GLFWwindow* window, float font_size);
void imgui_bgfx_destroy();
void imgui_bgfx_begin_frame(int width, int height, float delta_seconds);
void imgui_bgfx_end_frame(bgfx::ViewId view);

// Packs a bgfx texture into an ImGui texture id zero stays the invalid id
ImTextureID imgui_bgfx_texture(bgfx::TextureHandle handle);

}
