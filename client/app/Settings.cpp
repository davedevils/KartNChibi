#include "Settings.h"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

namespace KnC::Client {

namespace {

// section then key to value trimmed of spaces and tabs the stock reads with GetPrivateProfileString
std::map<std::string, std::string> readIni(const std::string& path) {
    std::map<std::string, std::string> out;
    std::ifstream file(path);
    if (!file.is_open()) return out;
    std::string line;
    std::string section;
    auto trim = [](std::string s) {
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        return s.substr(i);
    };
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';') continue;
        if (line[0] == '[') {
            const size_t end = line.find(']');
            section = line.substr(1, end == std::string::npos ? std::string::npos : end - 1);
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        out[section + "/" + trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return out;
}

bool readFloat(const std::map<std::string, std::string>& ini, const char* key, float& out) {
    auto it = ini.find(key);
    if (it == ini.end() || it->second.empty()) return false;
    out = static_cast<float>(std::atof(it->second.c_str()));
    return true;
}

bool readInt(const std::map<std::string, std::string>& ini, const char* key, int& out) {
    auto it = ini.find(key);
    if (it == ini.end() || it->second.empty()) return false;
    out = std::atoi(it->second.c_str());
    return true;
}

float clamp01(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }

const char* const kKeyIniNames[8] = {"up", "down", "left", "right", "item", "drift", "back", "slot"};

}

// sub 46CE90 every key must be there or the stock keeps its defaults and rewrites the file
bool GameOptions::load(const std::string& path) {
    const auto ini = readIni(path);
    if (ini.empty()) return false;
    GameOptions o = *this;
    const bool ok = readFloat(ini, "system/detect", o.detect) && readFloat(ini, "system/remember", o.remember) &&
                    readFloat(ini, "system/random_invite", o.randomInvite) && readFloat(ini, "system/motion_blur", o.motionBlur) &&
                    readFloat(ini, "graphic/window_mode", o.windowMode) && readFloat(ini, "graphic/wide_mode", o.wideMode) &&
                    readFloat(ini, "graphic/detail", o.detail) && readFloat(ini, "sound/bgmOff", o.bgmOff) &&
                    readFloat(ini, "sound/effectOff", o.effectOff) && readFloat(ini, "sound/carOff", o.carOff) &&
                    readFloat(ini, "sound/bgm", o.bgm) && readFloat(ini, "sound/effect", o.effect) && readFloat(ini, "sound/car", o.car);
    if (!ok) return false;
    o.bgm = clamp01(o.bgm);
    o.effect = clamp01(o.effect);
    o.car = clamp01(o.car);
    *this = o;
    return true;
}

// sub 46C320 the same lines and tabs as the stock file a muted channel writes a hundredth at least
bool GameOptions::save(const std::string& path) const {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    auto floor01 = [](float off, float v) { return off == 0.f && v < 0.01f ? 0.01f : v; };
    std::fprintf(f, "[system]\r\n");
    std::fprintf(f, "detect\t\t= %.2f\r\n", detect);
    std::fprintf(f, "remember\t  = %.2f\r\n", remember);
    std::fprintf(f, "random_invite = %.2f\r\n", randomInvite);
    std::fprintf(f, "motion_blur = %.2f\r\n", motionBlur);
    std::fprintf(f, "\r\n");
    std::fprintf(f, "[graphic]\r\n");
    std::fprintf(f, "window_mode   = %.2f\r\n", windowMode);
    std::fprintf(f, "wide_mode\t = %.2f\r\n", wideMode);
    std::fprintf(f, "detail\t\t= %.2f\r\n", detail);
    std::fprintf(f, "\r\n");
    std::fprintf(f, "[sound]\r\n");
    std::fprintf(f, "bgm\t\t   = %.2f\r\n", floor01(bgmOff, bgm));
    std::fprintf(f, "effect\t\t= %.2f\r\n", floor01(effectOff, effect));
    std::fprintf(f, "car\t\t   = %.2f\r\n", floor01(carOff, car));
    std::fprintf(f, "\r\n");
    std::fprintf(f, "bgmOff\t\t= %.2f\r\n", bgmOff);
    std::fprintf(f, "effectOff\t = %.2f\r\n", effectOff);
    std::fprintf(f, "carOff\t\t= %.2f\r\n", carOff);
    std::fprintf(f, "\r\n");
    std::fclose(f);
    return true;
}

// sub 46D2A0 tab 1 the three mutes off bgm at seven tenths the two others full
void GameOptions::defaultSound() {
    bgmOff = 0.f;
    effectOff = 0.f;
    carOff = 0.f;
    bgm = 0.7f;
    effect = 1.f;
    car = 1.f;
}

// sub 46D2A0 tab 0 window mode off normal width detail medium
void GameOptions::defaultGraphic() {
    windowMode = 0.f;
    wideMode = 0.f;
    detail = 1.f;
}

// sub 45B000 the joystick flag then the eight keyboard rows then the eight pad rows
bool InputBindings::load(const std::string& path) {
    const auto ini = readIni(path);
    if (ini.empty()) return false;
    InputBindings b = *this;
    int joy = 0;
    b.joystick = readInt(ini, "system/joystick", joy) && joy == 1;
    for (int i = 0; i < 8; ++i) {
        if (!readInt(ini, (std::string("keyboard/") + kKeyIniNames[i]).c_str(), b.keys[i])) return false;
    }
    for (int i = 0; i < 8; ++i) {
        if (!readInt(ini, (std::string("joystick/") + kKeyIniNames[i]).c_str(), b.pad[i])) return false;
    }
    *this = b;
    return true;
}

// sub 45B240 the same lines as the stock file
bool InputBindings::save(const std::string& path) const {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    static const char* const kLine[8] = {"up\t   = %d\r\n", "down\t = %d\r\n", "left\t = %d\r\n", "right\t= %d\r\n",
                                         "item\t = %d\r\n", "drift\t= %d\r\n", "back\t = %d\r\n", "slot\t = %d\r\n"};
    std::fprintf(f, "[system]\r\n");
    std::fprintf(f, "joystick = %d\r\n", joystick ? 1 : 0);
    std::fprintf(f, "\r\n");
    std::fprintf(f, "[keyboard]\r\n");
    for (int i = 0; i < 8; ++i) std::fprintf(f, kLine[i], keys[i]);
    std::fprintf(f, "\r\n");
    std::fprintf(f, "[joystick]\r\n");
    for (int i = 0; i < 8; ++i) std::fprintf(f, kLine[i], pad[i]);
    std::fprintf(f, "\r\n");
    std::fclose(f);
    return true;
}

// sub 45AF60
void InputBindings::defaults() {
    *this = InputBindings();
}

std::string InputBindings::keyName(int vk) {
    if (vk >= 'a' && vk <= 'z') vk = vk - 'a' + 'A';
    if (vk >= 'A' && vk <= 'Z') return std::string(1, static_cast<char>(vk));
    switch (vk) {
    case 0x26: return "Up";
    case 0x28: return "Down";
    case 0x25: return "Left";
    case 0x27: return "Right";
    case 0x11: return "Ctrl";
    case 0x10: return "Shift";
    case 0x20: return "Space";
    case 0x12: return "Alt";
    case 0x09: return "Tab";
    case 0x08: return "Back";
    case 0x0D: return "Return";
    default: return std::string();
    }
}

bool InputBindings::keyAllowed(int vk) {
    return !keyName(vk).empty();
}

int InputBindings::glfwFromVk(int vk) {
    if (vk >= 'a' && vk <= 'z') vk = vk - 'a' + 'A';
    if (vk >= 'A' && vk <= 'Z') return GLFW_KEY_A + (vk - 'A');
    switch (vk) {
    case 0x26: return GLFW_KEY_UP;
    case 0x28: return GLFW_KEY_DOWN;
    case 0x25: return GLFW_KEY_LEFT;
    case 0x27: return GLFW_KEY_RIGHT;
    case 0x11: return GLFW_KEY_LEFT_CONTROL;
    case 0x10: return GLFW_KEY_LEFT_SHIFT;
    case 0x20: return GLFW_KEY_SPACE;
    case 0x12: return GLFW_KEY_LEFT_ALT;
    case 0x09: return GLFW_KEY_TAB;
    case 0x08: return GLFW_KEY_BACKSPACE;
    case 0x0D: return GLFW_KEY_ENTER;
    default: return -1;
    }
}

int InputBindings::vkFromGlfw(int k) {
    if (k >= GLFW_KEY_A && k <= GLFW_KEY_Z) return 'A' + (k - GLFW_KEY_A);
    switch (k) {
    case GLFW_KEY_UP: return 0x26;
    case GLFW_KEY_DOWN: return 0x28;
    case GLFW_KEY_LEFT: return 0x25;
    case GLFW_KEY_RIGHT: return 0x27;
    case GLFW_KEY_LEFT_CONTROL: case GLFW_KEY_RIGHT_CONTROL: return 0x11;
    case GLFW_KEY_LEFT_SHIFT: case GLFW_KEY_RIGHT_SHIFT: return 0x10;
    case GLFW_KEY_SPACE: return 0x20;
    case GLFW_KEY_LEFT_ALT: case GLFW_KEY_RIGHT_ALT: return 0x12;
    case GLFW_KEY_TAB: return 0x09;
    case GLFW_KEY_BACKSPACE: return 0x08;
    case GLFW_KEY_ENTER: case GLFW_KEY_KP_ENTER: return 0x0D;
    default: return -1;
    }
}

}
