/*******************************************************************************************
*   KnC Shared Library - Vec4 (4D Vector)
*   
*   Copyright (c) 2025 Kart N'Chibi Team
*******************************************************************************************/

#pragma once

#include <cmath>

namespace KnC {
namespace Math {

/**
 * @brief 4D Vector (x, y, z, w)
 */
struct Vec4 {
    float x, y, z, w;
    
    // Constructors
    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vec4(float value) : x(value), y(value), z(value), w(value) {}
    
    // Operators
    Vec4 operator+(const Vec4& other) const { return Vec4(x + other.x, y + other.y, z + other.z, w + other.w); }
    Vec4 operator-(const Vec4& other) const { return Vec4(x - other.x, y - other.y, z - other.z, w - other.w); }
    Vec4 operator*(float scalar) const { return Vec4(x * scalar, y * scalar, z * scalar, w * scalar); }
    Vec4 operator/(float scalar) const { return Vec4(x / scalar, y / scalar, z / scalar, w / scalar); }
    
    Vec4& operator+=(const Vec4& other) { x += other.x; y += other.y; z += other.z; w += other.w; return *this; }
    Vec4& operator-=(const Vec4& other) { x -= other.x; y -= other.y; z -= other.z; w -= other.w; return *this; }
    Vec4& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this; }
    Vec4& operator/=(float scalar) { x /= scalar; y /= scalar; z /= scalar; w /= scalar; return *this; }
    
    bool operator==(const Vec4& other) const { 
        return std::abs(x - other.x) < 1e-6f && 
               std::abs(y - other.y) < 1e-6f && 
               std::abs(z - other.z) < 1e-6f &&
               std::abs(w - other.w) < 1e-6f;
    }
    bool operator!=(const Vec4& other) const { return !(*this == other); }
    
    // Length
    float Length() const { return std::sqrt(x * x + y * y + z * z + w * w); }
    float LengthSquared() const { return x * x + y * y + z * z + w * w; }
    
    // Normalize
    Vec4 Normalized() const {
        float len = Length();
        return (len > 0) ? (*this / len) : Vec4(0, 0, 0, 0);
    }
    
    void Normalize() {
        *this = Normalized();
    }
    
    // Dot product
    static float Dot(const Vec4& a, const Vec4& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }
    
    // Lerp
    static Vec4 Lerp(const Vec4& a, const Vec4& b, float t) {
        return a + (b - a) * t;
    }
};

}} // namespace KnC::Math

