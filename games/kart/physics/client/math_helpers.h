#pragma once

// math and time helpers ported from TICK HELPERS and the vec3 mat3 quat leaves of RIGID BODY

#include <cstdint>

namespace KnC::Kart::Client {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// row major 3 by 3 index by row then column R times v rotates local to world
struct Mat3 {
    float m[3][3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
};

struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

// time frame delta 0x44D1A0 plain global read every tick seconds
extern float g_time_frame_delta;

// time now ms 0x44ED50 steady clock milliseconds monotonic since first
std::int64_t time_now_ms();

// math atan2 deg 0x44E240 bearing 0 to 360 y then x
float math_atan2_deg(float y, float x);

// math wrap angle open360 0x44DB60 steps of 360 into the open interval at most 64 each way
void math_wrap_angle_open360(float* inout);

// math wrap angle signed180 0x44DD00 the open360 wrap then the fold to minus 180 up 180
void math_wrap_angle_signed180(float* inout);

// math wrap angle 360 0x44D9C0 steps of 360 into 0 up 360 at most 64 each way
void math_wrap_angle_360(float* inout);

// math hypot2d 0x44D900 2D length 0 when both inputs zero
float math_hypot2d(float a, float b);

// body vec3 set 0x4ED3D0
void body_vec3_set(Vec3& out, float x, float y, float z);

// body vec3 add 0x4ED3F0 a gains b
void body_vec3_add(Vec3& a, const Vec3& b);

// body vec3 madd 0x4ED410 out base plus dir scaled by scale
void body_vec3_madd(Vec3& out, const Vec3& base, const Vec3& dir, float scale);

// body vec3 madd inplace 0x4ED440 a gains b scaled by s
void body_vec3_madd_inplace(Vec3& a, const Vec3& b, float s);

// body vec3 madd2 0x4ED470 out base plus b scaled by sb plus c scaled by sc
void body_vec3_madd2(Vec3& out, const Vec3& base, const Vec3& b, float sb, const Vec3& c, float sc);

// body vec3 sub 0x4ED4C0 out a minus b
void body_vec3_sub(Vec3& out, const Vec3& a, const Vec3& b);

// body vec3 negate 0x4ED4F0 v flipped in place
void body_vec3_negate(Vec3& v);

// body vec3 scaled 0x4ED5D0 out v scaled by s
void body_vec3_scaled(Vec3& out, const Vec3& v, float s);

// body vec3 scale 0x4ED600 v scaled in place by s
void body_vec3_scale(Vec3& v, float s);

// body vec3 normalize 0x4ED680 unit length in place false below the gate
bool body_vec3_normalize(Vec3& v);

// body vec3 dot 0x4ED6E0 plain dot product
float body_vec3_dot(const Vec3& a, const Vec3& b);

// body vec3 cross 0x4ED700 out a cross b
void body_vec3_cross(Vec3& out, const Vec3& a, const Vec3& b);

// body vec3 transform 0x4EE440 out mat times v plus translate
void body_vec3_transform(Vec3& out, const Vec3& v, const Mat3& mat, const Vec3& translate);

// body vec3 world to local 0x4EE4A0 out mat transposed times world minus origin
void body_vec3_world_to_local(Vec3& out, const Vec3& worldPoint, const Mat3& mat, const Vec3& origin);

// body mat3 set 0x4ED100 nine floats row by row
void body_mat3_set(Mat3& out, float m00, float m01, float m02, float m10, float m11, float m12,
                   float m20, float m21, float m22);

// body mat3 multiply 0x4ED150 out a times b 3 by 3
void body_mat3_multiply(Mat3& out, const Mat3& a, const Mat3& b);

// body mat3 transpose inplace 0x4ED240 swaps the off diagonal pairs
void body_mat3_transpose_inplace(Mat3& m);

// body mat3 transpose copy 0x4ED270 out is the transpose of m
void body_mat3_transpose_copy(Mat3& out, const Mat3& m);

// body mat3 transform vec 0x4ED510 mat times v column each row dotted with v
Vec3 body_mat3_transform_vec(const Mat3& mat, const Vec3& v);

// body mat3 transform vec transposed 0x4ED570 mat transposed times v each column dotted
Vec3 body_mat3_transform_vec_transposed(const Mat3& mat, const Vec3& v);

// body quat to matrix 0x4ED740 standard quaternion to row major
void body_quat_to_matrix(Mat3& out, const Quat& q);

// body quat to matrix scaled 0x4ED800 same with 2 over the given norm squared
void body_quat_to_matrix_scaled(Mat3& out, const Quat& q, float normSq);

// body quat norm sq 0x4EE410 x y z w squared and summed
float body_quat_norm_sq(const Quat& q);

// body mat3 to quat 0x4ED9C0 largest component first then normalize w kept positive
void body_mat3_to_quat(Quat& out, const Mat3& m);

// body quat from axis angle 0x4ED8E0 half angle sin cos axis
Quat body_quat_from_axis_angle(const Vec3& axis, float angleRad);

// body quat multiply 0x4ED930 a times b Hamilton product
Quat body_quat_multiply(const Quat& a, const Quat& b);

// d3dx quaternion slerp import slot 0xAFB628 thunk 0x5059F0 shortest path lerp under 0 dot 001
Quat body_quat_slerp_d3dx(const Quat& a, const Quat& b, float t);

// d3dx vec3 normalize thunk 0x502F86 zero vector stays zero
void body_vec3_normalize_d3dx(Vec3& v);

// body mat3 jacobi eigen 0x4EDBF0 numerical recipes jacobi columns of vectors are the axes
bool body_mat3_jacobi_eigen(const Mat3& tensor, Mat3& vectors, float values[3]);

} // namespace KnC Kart Client
