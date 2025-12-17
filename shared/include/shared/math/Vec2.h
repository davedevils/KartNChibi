/**
 * @file Vec2.h
 * @brief 2D Vector math
 */

#pragma once

#include "../core/Types.h"
#include <cmath>

namespace KnC {
namespace Math {

/**
 * @brief 2D Vector
 */
struct Vec2 {
    float x, y;
    
    // Constructors
    Vec2() : x(0.0f), y(0.0f) {}
    Vec2(float x, float y) : x(x), y(y) {}
    Vec2(float scalar) : x(scalar), y(scalar) {}
    
    // Operators
    Vec2 operator+(const Vec2& other) const {
        return Vec2(x + other.x, y + other.y);
    }
    
    Vec2 operator-(const Vec2& other) const {
        return Vec2(x - other.x, y - other.y);
    }
    
    Vec2 operator*(float scalar) const {
        return Vec2(x * scalar, y * scalar);
    }
    
    Vec2 operator/(float scalar) const {
        return Vec2(x / scalar, y / scalar);
    }
    
    Vec2& operator+=(const Vec2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
    
    Vec2& operator-=(const Vec2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    
    Vec2& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }
    
    Vec2& operator/=(float scalar) {
        x /= scalar;
        y /= scalar;
        return *this;
    }
    
    bool operator==(const Vec2& other) const {
        return x == other.x && y == other.y;
    }
    
    bool operator!=(const Vec2& other) const {
        return !(*this == other);
    }
    
    // Methods
    float Length() const {
        return std::sqrt(x * x + y * y);
    }
    
    float LengthSquared() const {
        return x * x + y * y;
    }
    
    Vec2 Normalized() const {
        float len = Length();
        if (len == 0.0f) return Vec2(0.0f, 0.0f);
        return *this / len;
    }
    
    void Normalize() {
        float len = Length();
        if (len != 0.0f) {
            x /= len;
            y /= len;
        }
    }
    
    float Dot(const Vec2& other) const {
        return x * other.x + y * other.y;
    }
    
    float Cross(const Vec2& other) const {
        return x * other.y - y * other.x;
    }
    
    float Distance(const Vec2& other) const {
        return (*this - other).Length();
    }
    
    float DistanceSquared(const Vec2& other) const {
        return (*this - other).LengthSquared();
    }
    
    // Static methods
    static Vec2 Zero() { return Vec2(0.0f, 0.0f); }
    static Vec2 One() { return Vec2(1.0f, 1.0f); }
    static Vec2 UnitX() { return Vec2(1.0f, 0.0f); }
    static Vec2 UnitY() { return Vec2(0.0f, 1.0f); }
    
    static Vec2 Lerp(const Vec2& a, const Vec2& b, float t) {
        return a + (b - a) * t;
    }
};

} // namespace Math
} // namespace KnC

