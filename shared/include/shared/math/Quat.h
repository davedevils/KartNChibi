/**
 * @file Quat.h
 * @brief Quaternion for 3D rotations
 */

#pragma once

#include "../core/Types.h"
#include "Vec3.h"
#include <cmath>

namespace KnC {
namespace Math {

/**
 * @brief Quaternion (x, y, z, w)
 * 
 * Used for 3D rotations. Avoids gimbal lock.
 */
struct Quat {
    float x, y, z, w;
    
    // Constructors
    Quat() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    Quat(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    
    // Operators
    Quat operator*(const Quat& other) const {
        return Quat(
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w,
            w * other.w - x * other.x - y * other.y - z * other.z
        );
    }
    
    Quat operator*(float scalar) const {
        return Quat(x * scalar, y * scalar, z * scalar, w * scalar);
    }
    
    Quat operator+(const Quat& other) const {
        return Quat(x + other.x, y + other.y, z + other.z, w + other.w);
    }
    
    bool operator==(const Quat& other) const {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }
    
    bool operator!=(const Quat& other) const {
        return !(*this == other);
    }
    
    // Methods
    float Length() const {
        return std::sqrt(x * x + y * y + z * z + w * w);
    }
    
    float LengthSquared() const {
        return x * x + y * y + z * z + w * w;
    }
    
    Quat Normalized() const {
        float len = Length();
        if (len == 0.0f) return Quat();
        return *this * (1.0f / len);
    }
    
    void Normalize() {
        float len = Length();
        if (len != 0.0f) {
            float invLen = 1.0f / len;
            x *= invLen;
            y *= invLen;
            z *= invLen;
            w *= invLen;
        }
    }
    
    Quat Conjugate() const {
        return Quat(-x, -y, -z, w);
    }
    
    Quat Inverse() const {
        float lenSq = LengthSquared();
        if (lenSq == 0.0f) return Quat();
        Quat conj = Conjugate();
        return conj * (1.0f / lenSq);
    }
    
    float Dot(const Quat& other) const {
        return x * other.x + y * other.y + z * other.z + w * other.w;
    }
    
    /**
     * @brief Rotate a vector by this quaternion
     */
    Vec3 RotateVector(const Vec3& v) const {
        // v' = q * v * q^-1
        Quat vecQuat(v.x, v.y, v.z, 0.0f);
        Quat result = (*this) * vecQuat * Conjugate();
        return Vec3(result.x, result.y, result.z);
    }
    
    // Static methods
    static Quat Identity() {
        return Quat(0.0f, 0.0f, 0.0f, 1.0f);
    }
    
    /**
     * @brief Create quaternion from axis-angle
     * @param axis Rotation axis (must be normalized)
     * @param angle Angle in radians
     */
    static Quat FromAxisAngle(const Vec3& axis, float angle) {
        float halfAngle = angle * 0.5f;
        float s = std::sin(halfAngle);
        return Quat(
            axis.x * s,
            axis.y * s,
            axis.z * s,
            std::cos(halfAngle)
        );
    }
    
    /**
     * @brief Create quaternion from Euler angles (in radians)
     * @param pitch Rotation around X axis
     * @param yaw Rotation around Y axis
     * @param roll Rotation around Z axis
     */
    static Quat FromEuler(float pitch, float yaw, float roll) {
        float cy = std::cos(yaw * 0.5f);
        float sy = std::sin(yaw * 0.5f);
        float cp = std::cos(pitch * 0.5f);
        float sp = std::sin(pitch * 0.5f);
        float cr = std::cos(roll * 0.5f);
        float sr = std::sin(roll * 0.5f);
        
        return Quat(
            sr * cp * cy - cr * sp * sy,
            cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy,
            cr * cp * cy + sr * sp * sy
        );
    }
    
    /**
     * @brief Spherical linear interpolation
     */
    static Quat Slerp(const Quat& a, const Quat& b, float t) {
        Quat qb = b;
        float cosTheta = a.Dot(b);
        
        // If negative dot, negate one quaternion to take shorter path
        if (cosTheta < 0.0f) {
            qb = Quat(-b.x, -b.y, -b.z, -b.w);
            cosTheta = -cosTheta;
        }
        
        // If very close, use linear interpolation
        if (cosTheta > 0.9995f) {
            return (a * (1.0f - t) + qb * t).Normalized();
        }
        
        // Calculate coefficients
        float theta = std::acos(cosTheta);
        float sinTheta = std::sin(theta);
        float wa = std::sin((1.0f - t) * theta) / sinTheta;
        float wb = std::sin(t * theta) / sinTheta;
        
        return a * wa + qb * wb;
    }
};

} // namespace Math
} // namespace KnC

