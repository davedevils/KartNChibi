#pragma once

#include "../core/Types.h"

namespace KnC {
namespace Graphics {

struct Color {
    uint8 r = 0;
    uint8 g = 0;
    uint8 b = 0;
    uint8 a = 255;
    
    Color() = default;
    Color(uint8 red, uint8 green, uint8 blue, uint8 alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}
    
    // from packed RGBA as 0xRRGGBBAA
    static Color FromRGBA(uint32 rgba) {
        return Color(
            (rgba >> 24) & 0xFF,
            (rgba >> 16) & 0xFF,
            (rgba >> 8) & 0xFF,
            rgba & 0xFF
        );
    }
    
    uint32 ToRGBA() const {
        return (r << 24) | (g << 16) | (b << 8) | a;
    }

    static const Color White;
    static const Color Black;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Yellow;
    static const Color Magenta;
    static const Color Cyan;
    static const Color Transparent;
};

inline const Color Color::White = {255, 255, 255, 255};
inline const Color Color::Black = {0, 0, 0, 255};
inline const Color Color::Red = {255, 0, 0, 255};
inline const Color Color::Green = {0, 255, 0, 255};
inline const Color Color::Blue = {0, 0, 255, 255};
inline const Color Color::Yellow = {255, 255, 0, 255};
inline const Color Color::Magenta = {255, 0, 255, 255};
inline const Color Color::Cyan = {0, 255, 255, 255};
inline const Color Color::Transparent = {0, 0, 0, 0};

}}

