/**
 * @file Vec3.h
 * @brief 3D Vector math
 */

#pragma once

#include "../core/Types.h"
#include <cmath>

namespace KnC {
namespace Math {

/**
 * @brief 3D Vector
 */
struct Vec3 {
    float x, y, z;
    
    // Constructors
    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3(float scalar) : x(scalar), y(scalar), z(scalar) {}
    
    // Operators
    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }
    
    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }
    
    Vec3 operator*(float scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }
    
    Vec3 operator/(float scalar) const {
        return Vec3(x / scalar, y / scalar, z / scalar);
    }
    
    Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    
    Vec3& operator-=(const Vec3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
    
    Vec3& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }
    
    Vec3& operator/=(float scalar) {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }
    
    bool operator==(const Vec3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
    
    bool operator!=(const Vec3& other) const {
        return !(*this == other);
    }
    
    // Methods
    float Length() const {
        return std::sqrt(x * x + y * y + z * z);
    }
    
    float LengthSquared() const {
        return x * x + y * y + z * z;
    }
    
    Vec3 Normalized() const {
        float len = Length();
        if (len == 0.0f) return Vec3(0.0f, 0.0f, 0.0f);
        return *this / len;
    }
    
    void Normalize() {
        float len = Length();
        if (len != 0.0f) {
            x /= len;
            y /= len;
            z /= len;
        }
    }
    
    float Dot(const Vec3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }
    
    Vec3 Cross(const Vec3& other) const {
        return Vec3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }
    
    float Distance(const Vec3& other) const {
        return (*this - other).Length();
    }
    
    float DistanceSquared(const Vec3& other) const {
        return (*this - other).LengthSquared();
    }
    
    // Static methods
    static Vec3 Zero() { return Vec3(0.0f, 0.0f, 0.0f); }
    static Vec3 One() { return Vec3(1.0f, 1.0f, 1.0f); }
    static Vec3 UnitX() { return Vec3(1.0f, 0.0f, 0.0f); }
    static Vec3 UnitY() { return Vec3(0.0f, 1.0f, 0.0f); }
    static Vec3 UnitZ() { return Vec3(0.0f, 0.0f, 1.0f); }
    static Vec3 Up() { return Vec3(0.0f, 1.0f, 0.0f); }
    static Vec3 Down() { return Vec3(0.0f, -1.0f, 0.0f); }
    static Vec3 Forward() { return Vec3(0.0f, 0.0f, 1.0f); }
    static Vec3 Backward() { return Vec3(0.0f, 0.0f, -1.0f); }
    static Vec3 Left() { return Vec3(-1.0f, 0.0f, 0.0f); }
    static Vec3 Right() { return Vec3(1.0f, 0.0f, 0.0f); }
    
    static Vec3 Lerp(const Vec3& a, const Vec3& b, float t) {
        return a + (b - a) * t;
    }
};

} // namespace Math
} // namespace KnC

