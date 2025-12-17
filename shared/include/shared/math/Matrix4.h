/**
 * @file Matrix4.h
 * @brief 4x4 Matrix for 3D transformations
 */

#pragma once

#include "../core/Types.h"
#include "Vec3.h"
#include "Quat.h"
#include <array>
#include <cmath>

namespace KnC {
namespace Math {

/**
 * @brief 4x4 Matrix (column-major order)
 * 
 * Layout:
 * [m00 m10 m20 m30]
 * [m01 m11 m21 m31]
 * [m02 m12 m22 m32]
 * [m03 m13 m23 m33]
 */
struct Matrix4 {
    std::array<float, 16> data; // Column-major: data[col * 4 + row]
    
    // Constructors
    Matrix4() {
        Identity();
    }
    
    Matrix4(float m00, float m01, float m02, float m03,
            float m10, float m11, float m12, float m13,
            float m20, float m21, float m22, float m23,
            float m30, float m31, float m32, float m33) {
        data[0]  = m00; data[1]  = m01; data[2]  = m02; data[3]  = m03;
        data[4]  = m10; data[5]  = m11; data[6]  = m12; data[7]  = m13;
        data[8]  = m20; data[9]  = m21; data[10] = m22; data[11] = m23;
        data[12] = m30; data[13] = m31; data[14] = m32; data[15] = m33;
    }
    
    // Element access
    float& operator()(int row, int col) {
        return data[col * 4 + row];
    }
    
    const float& operator()(int row, int col) const {
        return data[col * 4 + row];
    }
    
    // Matrix operations
    Matrix4 operator*(const Matrix4& other) const {
        Matrix4 result;
        for (int col = 0; col < 4; col++) {
            for (int row = 0; row < 4; row++) {
                result(row, col) = 
                    (*this)(row, 0) * other(0, col) +
                    (*this)(row, 1) * other(1, col) +
                    (*this)(row, 2) * other(2, col) +
                    (*this)(row, 3) * other(3, col);
            }
        }
        return result;
    }
    
    Vec3 operator*(const Vec3& v) const {
        float w = data[3] * v.x + data[7] * v.y + data[11] * v.z + data[15];
        if (w == 0.0f) w = 1.0f;
        
        return Vec3(
            (data[0] * v.x + data[4] * v.y + data[8]  * v.z + data[12]) / w,
            (data[1] * v.x + data[5] * v.y + data[9]  * v.z + data[13]) / w,
            (data[2] * v.x + data[6] * v.y + data[10] * v.z + data[14]) / w
        );
    }
    
    Matrix4 operator*(float scalar) const {
        Matrix4 result;
        for (int i = 0; i < 16; i++) {
            result.data[i] = data[i] * scalar;
        }
        return result;
    }
    
    Matrix4 operator+(const Matrix4& other) const {
        Matrix4 result;
        for (int i = 0; i < 16; i++) {
            result.data[i] = data[i] + other.data[i];
        }
        return result;
    }
    
    bool operator==(const Matrix4& other) const {
        for (int i = 0; i < 16; i++) {
            if (std::abs(data[i] - other.data[i]) > 0.0001f) {
                return false;
            }
        }
        return true;
    }
    
    bool operator!=(const Matrix4& other) const {
        return !(*this == other);
    }
    
    // Matrix operations
    void Identity() {
        data.fill(0.0f);
        data[0] = data[5] = data[10] = data[15] = 1.0f;
    }
    
    Matrix4 Transposed() const {
        return Matrix4(
            data[0], data[4], data[8],  data[12],
            data[1], data[5], data[9],  data[13],
            data[2], data[6], data[10], data[14],
            data[3], data[7], data[11], data[15]
        );
    }
    
    void Transpose() {
        *this = Transposed();
    }
    
    float Determinant() const {
        float a = data[0], b = data[1], c = data[2], d = data[3];
        float e = data[4], f = data[5], g = data[6], h = data[7];
        float i = data[8], j = data[9], k = data[10], l = data[11];
        float n = data[12], o = data[13], p = data[14], q = data[15];
        
        return a * (f * (k * q - l * p) - g * (j * q - l * o) + h * (j * p - k * o)) -
               b * (e * (k * q - l * p) - g * (i * q - l * n) + h * (i * p - k * n)) +
               c * (e * (j * q - l * o) - f * (i * q - l * n) + h * (i * o - j * n)) -
               d * (e * (j * p - k * o) - f * (i * p - k * n) + g * (i * o - j * n));
    }
    
    Matrix4 Inversed() const {
        float det = Determinant();
        if (std::abs(det) < 0.0001f) {
            return Matrix4(); // Return identity if not invertible
        }
        
        Matrix4 result;
        
        // Calculate cofactor matrix and transpose
        result(0, 0) =  (data[5] * (data[10] * data[15] - data[11] * data[14]) - data[9] * (data[6] * data[15] - data[7] * data[14]) + data[13] * (data[6] * data[11] - data[7] * data[10]));
        result(0, 1) = -(data[1] * (data[10] * data[15] - data[11] * data[14]) - data[9] * (data[2] * data[15] - data[3] * data[14]) + data[13] * (data[2] * data[11] - data[3] * data[10]));
        result(0, 2) =  (data[1] * (data[6]  * data[15] - data[7]  * data[14]) - data[5] * (data[2] * data[15] - data[3] * data[14]) + data[13] * (data[2] * data[7]  - data[3] * data[6]));
        result(0, 3) = -(data[1] * (data[6]  * data[11] - data[7]  * data[10]) - data[5] * (data[2] * data[11] - data[3] * data[10]) + data[9]  * (data[2] * data[7]  - data[3] * data[6]));
        
        result(1, 0) = -(data[4] * (data[10] * data[15] - data[11] * data[14]) - data[8] * (data[6] * data[15] - data[7] * data[14]) + data[12] * (data[6] * data[11] - data[7] * data[10]));
        result(1, 1) =  (data[0] * (data[10] * data[15] - data[11] * data[14]) - data[8] * (data[2] * data[15] - data[3] * data[14]) + data[12] * (data[2] * data[11] - data[3] * data[10]));
        result(1, 2) = -(data[0] * (data[6]  * data[15] - data[7]  * data[14]) - data[4] * (data[2] * data[15] - data[3] * data[14]) + data[12] * (data[2] * data[7]  - data[3] * data[6]));
        result(1, 3) =  (data[0] * (data[6]  * data[11] - data[7]  * data[10]) - data[4] * (data[2] * data[11] - data[3] * data[10]) + data[8]  * (data[2] * data[7]  - data[3] * data[6]));
        
        result(2, 0) =  (data[4] * (data[9]  * data[15] - data[11] * data[13]) - data[8] * (data[5] * data[15] - data[7] * data[13]) + data[12] * (data[5] * data[11] - data[7] * data[9]));
        result(2, 1) = -(data[0] * (data[9]  * data[15] - data[11] * data[13]) - data[8] * (data[1] * data[15] - data[3] * data[13]) + data[12] * (data[1] * data[11] - data[3] * data[9]));
        result(2, 2) =  (data[0] * (data[5]  * data[15] - data[7]  * data[13]) - data[4] * (data[1] * data[15] - data[3] * data[13]) + data[12] * (data[1] * data[7]  - data[3] * data[5]));
        result(2, 3) = -(data[0] * (data[5]  * data[11] - data[7]  * data[9])  - data[4] * (data[1] * data[11] - data[3] * data[9])  + data[8]  * (data[1] * data[7]  - data[3] * data[5]));
        
        result(3, 0) = -(data[4] * (data[9]  * data[14] - data[10] * data[13]) - data[8] * (data[5] * data[14] - data[6] * data[13]) + data[12] * (data[5] * data[10] - data[6] * data[9]));
        result(3, 1) =  (data[0] * (data[9]  * data[14] - data[10] * data[13]) - data[8] * (data[1] * data[14] - data[2] * data[13]) + data[12] * (data[1] * data[10] - data[2] * data[9]));
        result(3, 2) = -(data[0] * (data[5]  * data[14] - data[6]  * data[13]) - data[4] * (data[1] * data[14] - data[2] * data[13]) + data[12] * (data[1] * data[6]  - data[2] * data[5]));
        result(3, 3) =  (data[0] * (data[5]  * data[10] - data[6]  * data[9])  - data[4] * (data[1] * data[10] - data[2] * data[9])  + data[8]  * (data[1] * data[6]  - data[2] * data[5]));
        
        // Divide by determinant
        float invDet = 1.0f / det;
        for (int i = 0; i < 16; i++) {
            result.data[i] *= invDet;
        }
        
        return result;
    }
    
    void Invert() {
        *this = Inversed();
    }
    
    // Extract components
    Vec3 GetTranslation() const {
        return Vec3(data[12], data[13], data[14]);
    }
    
    Vec3 GetScale() const {
        return Vec3(
            Vec3(data[0], data[1], data[2]).Length(),
            Vec3(data[4], data[5], data[6]).Length(),
            Vec3(data[8], data[9], data[10]).Length()
        );
    }
    
    Quat GetRotation() const {
        Vec3 scale = GetScale();
        
        // Normalize to remove scale
        Matrix4 normalized = *this;
        normalized(0, 0) /= scale.x; normalized(1, 0) /= scale.x; normalized(2, 0) /= scale.x;
        normalized(0, 1) /= scale.y; normalized(1, 1) /= scale.y; normalized(2, 1) /= scale.y;
        normalized(0, 2) /= scale.z; normalized(1, 2) /= scale.z; normalized(2, 2) /= scale.z;
        
        // Convert to quaternion
        Quat q;
        float trace = normalized(0, 0) + normalized(1, 1) + normalized(2, 2);
        
        if (trace > 0.0f) {
            float s = std::sqrt(trace + 1.0f) * 2.0f;
            q.w = 0.25f * s;
            q.x = (normalized(2, 1) - normalized(1, 2)) / s;
            q.y = (normalized(0, 2) - normalized(2, 0)) / s;
            q.z = (normalized(1, 0) - normalized(0, 1)) / s;
        } else if (normalized(0, 0) > normalized(1, 1) && normalized(0, 0) > normalized(2, 2)) {
            float s = std::sqrt(1.0f + normalized(0, 0) - normalized(1, 1) - normalized(2, 2)) * 2.0f;
            q.w = (normalized(2, 1) - normalized(1, 2)) / s;
            q.x = 0.25f * s;
            q.y = (normalized(0, 1) + normalized(1, 0)) / s;
            q.z = (normalized(0, 2) + normalized(2, 0)) / s;
        } else if (normalized(1, 1) > normalized(2, 2)) {
            float s = std::sqrt(1.0f + normalized(1, 1) - normalized(0, 0) - normalized(2, 2)) * 2.0f;
            q.w = (normalized(0, 2) - normalized(2, 0)) / s;
            q.x = (normalized(0, 1) + normalized(1, 0)) / s;
            q.y = 0.25f * s;
            q.z = (normalized(1, 2) + normalized(2, 1)) / s;
        } else {
            float s = std::sqrt(1.0f + normalized(2, 2) - normalized(0, 0) - normalized(1, 1)) * 2.0f;
            q.w = (normalized(1, 0) - normalized(0, 1)) / s;
            q.x = (normalized(0, 2) + normalized(2, 0)) / s;
            q.y = (normalized(1, 2) + normalized(2, 1)) / s;
            q.z = 0.25f * s;
        }
        
        return q;
    }
    
    // Static factory methods
    static Matrix4 MakeIdentity() {
        return Matrix4();
    }
    
    static Matrix4 Translation(const Vec3& v) {
        Matrix4 result;
        result(0, 3) = v.x;
        result(1, 3) = v.y;
        result(2, 3) = v.z;
        return result;
    }
    
    static Matrix4 Scale(const Vec3& v) {
        Matrix4 result;
        result(0, 0) = v.x;
        result(1, 1) = v.y;
        result(2, 2) = v.z;
        return result;
    }
    
    static Matrix4 Scale(float s) {
        return Scale(Vec3(s, s, s));
    }
    
    static Matrix4 RotationX(float angle) {
        Matrix4 result;
        float c = std::cos(angle);
        float s = std::sin(angle);
        result(1, 1) = c;  result(1, 2) = -s;
        result(2, 1) = s;  result(2, 2) = c;
        return result;
    }
    
    static Matrix4 RotationY(float angle) {
        Matrix4 result;
        float c = std::cos(angle);
        float s = std::sin(angle);
        result(0, 0) = c;   result(0, 2) = s;
        result(2, 0) = -s;  result(2, 2) = c;
        return result;
    }
    
    static Matrix4 RotationZ(float angle) {
        Matrix4 result;
        float c = std::cos(angle);
        float s = std::sin(angle);
        result(0, 0) = c;  result(0, 1) = -s;
        result(1, 0) = s;  result(1, 1) = c;
        return result;
    }
    
    static Matrix4 Rotation(const Quat& q) {
        Matrix4 result;
        
        float xx = q.x * q.x;
        float yy = q.y * q.y;
        float zz = q.z * q.z;
        float xy = q.x * q.y;
        float xz = q.x * q.z;
        float yz = q.y * q.z;
        float wx = q.w * q.x;
        float wy = q.w * q.y;
        float wz = q.w * q.z;
        
        result(0, 0) = 1.0f - 2.0f * (yy + zz);
        result(0, 1) = 2.0f * (xy - wz);
        result(0, 2) = 2.0f * (xz + wy);
        
        result(1, 0) = 2.0f * (xy + wz);
        result(1, 1) = 1.0f - 2.0f * (xx + zz);
        result(1, 2) = 2.0f * (yz - wx);
        
        result(2, 0) = 2.0f * (xz - wy);
        result(2, 1) = 2.0f * (yz + wx);
        result(2, 2) = 1.0f - 2.0f * (xx + yy);
        
        return result;
    }
    
    static Matrix4 TRS(const Vec3& translation, const Quat& rotation, const Vec3& scale) {
        return Translation(translation) * Rotation(rotation) * Scale(scale);
    }
    
    static Matrix4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 f = (target - eye).Normalized();
        Vec3 r = f.Cross(up).Normalized();
        Vec3 u = r.Cross(f);
        
        Matrix4 result;
        result(0, 0) = r.x;  result(0, 1) = r.y;  result(0, 2) = r.z;  result(0, 3) = -r.Dot(eye);
        result(1, 0) = u.x;  result(1, 1) = u.y;  result(1, 2) = u.z;  result(1, 3) = -u.Dot(eye);
        result(2, 0) = -f.x; result(2, 1) = -f.y; result(2, 2) = -f.z; result(2, 3) = f.Dot(eye);
        result(3, 0) = 0;    result(3, 1) = 0;    result(3, 2) = 0;    result(3, 3) = 1.0f;
        return result;
    }
    
    static Matrix4 Perspective(float fov, float aspect, float near, float far) {
        float tanHalfFov = std::tan(fov / 2.0f);
        
        Matrix4 result;
        result.data.fill(0.0f);
        
        result(0, 0) = 1.0f / (aspect * tanHalfFov);
        result(1, 1) = 1.0f / tanHalfFov;
        result(2, 2) = -(far + near) / (far - near);
        result(2, 3) = -(2.0f * far * near) / (far - near);
        result(3, 2) = -1.0f;
        
        return result;
    }
    
    static Matrix4 Orthographic(float left, float right, float bottom, float top, float near, float far) {
        Matrix4 result;
        result.data.fill(0.0f);
        
        result(0, 0) = 2.0f / (right - left);
        result(1, 1) = 2.0f / (top - bottom);
        result(2, 2) = -2.0f / (far - near);
        result(0, 3) = -(right + left) / (right - left);
        result(1, 3) = -(top + bottom) / (top - bottom);
        result(2, 3) = -(far + near) / (far - near);
        result(3, 3) = 1.0f;
        
        return result;
    }
};

} // namespace Math
} // namespace KnC

