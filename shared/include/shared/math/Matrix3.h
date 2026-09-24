/// 3x3 matrix for rotations and inertia tensors

#pragma once

#include "Vec3.h"
#include <cmath>

namespace KnC {
namespace Math {

/// 3x3 matrix row major
struct Matrix3 {
    float m[3][3];
    
    Matrix3() {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                m[i][j] = (i == j) ? 1.0f : 0.0f;
    }
    
    Matrix3(float m00, float m01, float m02,
            float m10, float m11, float m12,
            float m20, float m21, float m22) {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m12;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m22;
    }
    
    static Matrix3 Identity() {
        return Matrix3();
    }
    
    static Matrix3 Zero() {
        return Matrix3(0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    
    static Matrix3 Diagonal(float d0, float d1, float d2) {
        return Matrix3(d0, 0, 0, 0, d1, 0, 0, 0, d2);
    }
    
    Matrix3 operator+(const Matrix3& other) const {
        Matrix3 result;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                result.m[i][j] = m[i][j] + other.m[i][j];
        return result;
    }
    
    Matrix3 operator-(const Matrix3& other) const {
        Matrix3 result;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                result.m[i][j] = m[i][j] - other.m[i][j];
        return result;
    }
    
    Matrix3 operator*(float scalar) const {
        Matrix3 result;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                result.m[i][j] = m[i][j] * scalar;
        return result;
    }
    
    Matrix3 operator*(const Matrix3& other) const {
        Matrix3 result;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                result.m[i][j] = 0;
                for (int k = 0; k < 3; k++)
                    result.m[i][j] += m[i][k] * other.m[k][j];
            }
        }
        return result;
    }
    
    Vec3 operator*(const Vec3& v) const {
        return Vec3(
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        );
    }
    
    Matrix3 Transpose() const {
        return Matrix3(
            m[0][0], m[1][0], m[2][0],
            m[0][1], m[1][1], m[2][1],
            m[0][2], m[1][2], m[2][2]
        );
    }
    
    float Determinant() const {
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
             - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
             + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }
    
    Matrix3 Inverse() const {
        float det = Determinant();
        if (std::abs(det) < 1e-6f) {
            return Identity();  // singular matrix returns identity
        }
        
        float invDet = 1.0f / det;
        Matrix3 result;
        
        result.m[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
        result.m[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet;
        result.m[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;
        
        result.m[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet;
        result.m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
        result.m[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet;
        
        result.m[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
        result.m[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet;
        result.m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;
        
        return result;
    }
};

}
} // namespace Math KnC

