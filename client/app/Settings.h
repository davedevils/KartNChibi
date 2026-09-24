// the stock Option2 ini and Input ini read and written at the stock format in the working folder
#pragma once

#include <string>

namespace KnC::Client {

// the values of sub 46D370 the defaults and sub 46CE90 the reader sub 46C320 the writer
struct GameOptions {
    float detect = 0.f;
    float remember = 0.f;
    float randomInvite = 0.f;
    float motionBlur = 1.f;
    float windowMode = 0.f;
    float wideMode = 0.f;
    float detail = 1.f;
    float bgm = 0.7f;
    float effect = 1.f;
    float car = 1.f;
    float bgmOff = 0.f;
    float effectOff = 0.f;
    float carOff = 0.f;
    static const char* fileName() { return "./Option2.ini"; }
    // true when the file was there and every key read
    bool load(const std::string& path = fileName());
    bool save(const std::string& path = fileName()) const;
    // the stock defaults of the sound tab and the graphic tab of the Default button
    void defaultSound();
    void defaultGraphic();
};

// the eight rows of the Input ini in their file order
enum class RaceKey { Up = 0, Down, Left, Right, Item, Drift, Back, Slot };

// sub 45B000 the eight key rows as VK codes then the eight pad buttons
struct InputBindings {
    bool joystick = false;
    int keys[8] = {0x26, 0x28, 0x25, 0x27, 0x11, 0x10, 0x09, 0x12};
    int pad[8] = {2, 1, 0, 0, 3, 0, 7, 5};
    static const char* fileName() { return "./Input.ini"; }
    bool load(const std::string& path = fileName());
    bool save(const std::string& path = fileName()) const;
    void defaults();
    // the key name of the stock table sub 45AC40 letters arrows Ctrl Shift Space Alt Tab Back Return
    static std::string keyName(int vk);
    // true when the stock accepts that VK code on a row
    static bool keyAllowed(int vk);
    // the VK code of a glfw key minus one when the stock table has no name for it
    static int vkFromGlfw(int glfwKey);
    // the glfw key of a VK code of the table the left of a pair minus one when unknown
    static int glfwFromVk(int vk);
};

}
