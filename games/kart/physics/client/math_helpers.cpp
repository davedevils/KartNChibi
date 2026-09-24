#include "math_helpers.h"

#include <chrono>
#include <cmath>
#include <cstring>

#include "constants.h"

namespace KnC::Kart::Client {

float g_time_frame_delta = 0.0f;

std::int64_t time_now_ms() {
    static const auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

void math_wrap_angle_360(float* inout) {
    // 0x44D9C0 unrolled by 8 the counter stops at 64 so a huge angle comes back only partly wrapped
    float v = *inout;
    for (int step = 0; step < kAngleWrapMaxSteps && v < kZero; ++step) v += kCollisionAngleWrap360;
    for (int step = 0; step < kAngleWrapMaxSteps && v >= kCollisionAngleWrap360; ++step) v -= kCollisionAngleWrap360;
    *inout = v;
}

void math_wrap_angle_open360(float* inout) {
    // 0x44DB60 the first test is at or under minus 360 so minus 360 itself steps up to 0
    float v = *inout;
    for (int step = 0; step < kAngleWrapMaxSteps && v <= -kCollisionAngleWrap360; ++step) v += kCollisionAngleWrap360;
    for (int step = 0; step < kAngleWrapMaxSteps && v >= kCollisionAngleWrap360; ++step) v -= kCollisionAngleWrap360;
    *inout = v;
}

float math_atan2_deg(float y, float x) {
    if (y == 0.0f && x == 0.0f) return 0.0f;
    if (y == 0.0f && x > 0.0f) return 0.0f;
    if (y == 0.0f && x <= 0.0f) return 180.0f;
    if (x == 0.0f && y > 0.0f) return 90.0f;
    if (x == 0.0f && y < 0.0f) return 270.0f;
    float deg = std::atan(y / x) * 57.2958f;
    if (x < 0.0f) deg += 180.0f;
    if (y < 0.0f) deg -= 360.0f;
    math_wrap_angle_360(&deg);
    return deg;
}

void math_wrap_angle_signed180(float* inout) {
    // 0x44DD00 the open360 wrap then under 0 add 360 when that is smaller else take minus 360 minus v
    math_wrap_angle_open360(inout);
    float v = *inout;
    if (v < kZero) {
        float up = v + kCollisionAngleWrap360;
        if (-v <= up) return;
        *inout = up;
        return;
    }
    if (v <= kCollisionAngleWrap360 - v) return;
    *inout = -(kCollisionAngleWrap360 - v);
}

float math_hypot2d(float a, float b) {
    if (a == 0.0f && b == 0.0f) return 0.0f;
    return std::sqrt(a * a + b * b);
}

void body_vec3_set(Vec3& out, float x, float y, float z) {
    out.x = x;
    out.y = y;
    out.z = z;
}

void body_vec3_add(Vec3& a, const Vec3& b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
}

void body_vec3_madd(Vec3& out, const Vec3& base, const Vec3& dir, float scale) {
    out.x = base.x + dir.x * scale;
    out.y = base.y + dir.y * scale;
    out.z = base.z + dir.z * scale;
}

void body_vec3_madd_inplace(Vec3& a, const Vec3& b, float s) {
    a.x = s * b.x + a.x;
    a.y = s * b.y + a.y;
    a.z = s * b.z + a.z;
}

void body_vec3_madd2(Vec3& out, const Vec3& base, const Vec3& b, float sb, const Vec3& c, float sc) {
    out.x = sc * c.x + sb * b.x + base.x;
    out.y = sc * c.y + sb * b.y + base.y;
    out.z = sc * c.z + sb * b.z + base.z;
}

void body_vec3_sub(Vec3& out, const Vec3& a, const Vec3& b) {
    out.x = a.x - b.x;
    out.y = a.y - b.y;
    out.z = a.z - b.z;
}

void body_vec3_negate(Vec3& v) {
    v.x = -v.x;
    v.y = -v.y;
    v.z = -v.z;
}

void body_vec3_scaled(Vec3& out, const Vec3& v, float s) {
    out.x = s * v.x;
    out.y = s * v.y;
    out.z = s * v.z;
}

void body_vec3_scale(Vec3& v, float s) {
    v.x *= s;
    v.y *= s;
    v.z *= s;
}

bool body_vec3_normalize(Vec3& v) {
    // the gate at 0x5A3C70 is a large negative float so it never rejects a real vector
    float lenSq = v.z * v.z + v.y * v.y + v.x * v.x;
    if (lenSq < kNormalizeGate) return false;
    if (lenSq <= 0.0f) return false; // port guard the client would divide by zero here
    float inv = kOne / std::sqrt(lenSq);
    v.x *= inv;
    v.y *= inv;
    v.z *= inv;
    return true;
}

float body_vec3_dot(const Vec3& a, const Vec3& b) {
    return b.x * a.x + b.y * a.y + b.z * a.z;
}

void body_vec3_cross(Vec3& out, const Vec3& a, const Vec3& b) {
    Vec3 r;
    r.x = b.z * a.y - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - b.x * a.y;
    out = r;
}

void body_vec3_transform(Vec3& out, const Vec3& v, const Mat3& mat, const Vec3& translate) {
    Vec3 r = body_mat3_transform_vec(mat, v);
    out.x = r.x + translate.x;
    out.y = r.y + translate.y;
    out.z = r.z + translate.z;
}

void body_vec3_world_to_local(Vec3& out, const Vec3& worldPoint, const Mat3& mat, const Vec3& origin) {
    Vec3 d{worldPoint.x - origin.x, worldPoint.y - origin.y, worldPoint.z - origin.z};
    out = body_mat3_transform_vec_transposed(mat, d);
}

void body_mat3_set(Mat3& out, float m00, float m01, float m02, float m10, float m11, float m12,
                   float m20, float m21, float m22) {
    out.m[0][0] = m00; out.m[0][1] = m01; out.m[0][2] = m02;
    out.m[1][0] = m10; out.m[1][1] = m11; out.m[1][2] = m12;
    out.m[2][0] = m20; out.m[2][1] = m21; out.m[2][2] = m22;
}

void body_mat3_multiply(Mat3& out, const Mat3& a, const Mat3& b) {
    Mat3 r;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            r.m[row][col] = a.m[row][0] * b.m[0][col] + a.m[row][1] * b.m[1][col] +
                             a.m[row][2] * b.m[2][col];
        }
    }
    out = r;
}

void body_mat3_transpose_inplace(Mat3& m) {
    float t;
    t = m.m[0][1]; m.m[0][1] = m.m[1][0]; m.m[1][0] = t;
    t = m.m[0][2]; m.m[0][2] = m.m[2][0]; m.m[2][0] = t;
    t = m.m[1][2]; m.m[1][2] = m.m[2][1]; m.m[2][1] = t;
}

void body_mat3_transpose_copy(Mat3& out, const Mat3& m) {
    Mat3 r;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) r.m[row][col] = m.m[col][row];
    }
    out = r;
}

Vec3 body_mat3_transform_vec(const Mat3& mat, const Vec3& v) {
    Vec3 r;
    r.x = mat.m[0][0] * v.x + mat.m[0][1] * v.y + mat.m[0][2] * v.z;
    r.y = mat.m[1][1] * v.y + mat.m[1][0] * v.x + mat.m[1][2] * v.z;
    r.z = mat.m[2][1] * v.y + mat.m[2][0] * v.x + mat.m[2][2] * v.z;
    return r;
}

Vec3 body_mat3_transform_vec_transposed(const Mat3& mat, const Vec3& v) {
    Vec3 r;
    r.x = mat.m[0][0] * v.x + mat.m[1][0] * v.y + mat.m[2][0] * v.z;
    r.y = mat.m[1][1] * v.y + mat.m[0][1] * v.x + mat.m[2][1] * v.z;
    r.z = mat.m[1][2] * v.y + mat.m[0][2] * v.x + mat.m[2][2] * v.z;
    return r;
}

void body_quat_to_matrix(Mat3& out, const Quat& q) {
    float x = q.x, y = q.y, z = q.z, w = q.w;
    out.m[0][0] = kOne - 2.0f * (y * y + z * z);
    out.m[0][1] = 2.0f * (x * y - z * w);
    out.m[0][2] = 2.0f * (x * z + y * w);
    out.m[1][0] = 2.0f * (x * y + z * w);
    out.m[1][1] = kOne - 2.0f * (x * x + z * z);
    out.m[1][2] = 2.0f * (y * z - x * w);
    out.m[2][0] = 2.0f * (x * z - y * w);
    out.m[2][1] = 2.0f * (y * z + x * w);
    out.m[2][2] = kOne - 2.0f * (x * x + y * y);
}

void body_quat_to_matrix_scaled(Mat3& out, const Quat& q, float normSq) {
    float x = q.x, y = q.y, z = q.z, w = q.w;
    float s = kQuatScaledTwo / normSq;
    out.m[0][0] = kOne - (z * z + y * y) * s;
    out.m[0][1] = (y * x - z * w) * s;
    out.m[0][2] = (y * w + z * x) * s;
    out.m[1][0] = (z * w + y * x) * s;
    out.m[1][1] = kOne - (z * z + x * x) * s;
    out.m[1][2] = (z * y - x * w) * s;
    out.m[2][0] = (z * x - y * w) * s;
    out.m[2][1] = (x * w + z * y) * s;
    out.m[2][2] = kOne - (y * y + x * x) * s;
}

float body_quat_norm_sq(const Quat& q) {
    return q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
}

void body_mat3_to_quat(Quat& out, const Mat3& m) {
    // weights trace plus 1 over 4 minus pair sums pick the largest jump table 0x4EDBDC
    float w2 = (m.m[1][1] + m.m[2][2] + m.m[0][0] + kOne) * kSmoothQuarter;
    float x2 = w2 - (m.m[1][1] + m.m[2][2]) * kHalf;
    float y2 = w2 - (m.m[0][0] + m.m[2][2]) * kHalf;
    float z2 = w2 - (m.m[1][1] + m.m[0][0]) * kHalf;
    int pick = 3;
    if (w2 <= x2) {
        if (x2 <= y2) {
            pick = (z2 < y2) ? 2 : 3;
        } else {
            pick = (z2 < x2) ? 1 : 3;
        }
    } else if (w2 <= y2) {
        pick = (z2 < y2) ? 2 : 3;
    } else if (z2 < w2) {
        pick = 0;
    }
    Quat q;
    bool fixSign = true;
    if (pick == 0) {
        float w = std::sqrt(w2);
        float s = kSmoothQuarter / w;
        q.w = w;
        q.x = (m.m[2][1] - m.m[1][2]) * s;
        q.y = (m.m[0][2] - m.m[2][0]) * s;
        q.z = (m.m[1][0] - m.m[0][1]) * s;
        fixSign = false;
    } else if (pick == 1) {
        float x = std::sqrt(x2);
        float s = kSmoothQuarter / x;
        q.x = x;
        q.w = (m.m[2][1] - m.m[1][2]) * s;
        q.y = (m.m[1][0] + m.m[0][1]) * s;
        q.z = (m.m[2][0] + m.m[0][2]) * s;
    } else if (pick == 2) {
        float y = std::sqrt(y2);
        float s = kSmoothQuarter / y;
        q.y = y;
        q.w = (m.m[0][2] - m.m[2][0]) * s;
        q.x = (m.m[1][0] + m.m[0][1]) * s;
        q.z = (m.m[2][1] + m.m[1][2]) * s;
    } else {
        float z = std::sqrt(z2);
        float s = kSmoothQuarter / z;
        q.z = z;
        q.w = (m.m[1][0] - m.m[0][1]) * s;
        q.x = (m.m[2][0] + m.m[0][2]) * s;
        q.y = (m.m[2][1] + m.m[1][2]) * s;
    }
    if (fixSign && q.w < kWheelAxisAngleTolerance) {
        q.w = -q.w;
        q.x = -q.x;
        q.y = -q.y;
        q.z = -q.z;
    }
    float inv = kOne / std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    q.w *= inv;
    q.x *= inv;
    q.y *= inv;
    q.z *= inv;
    out = q;
}

Quat body_quat_from_axis_angle(const Vec3& axis, float angleRad) {
    Vec3 n = axis;
    body_vec3_normalize(n);
    float half = angleRad * kHalf;
    float s = std::sin(half);
    float c = std::cos(half);
    Quat q;
    q.x = n.x * s;
    q.y = n.y * s;
    q.z = n.z * s;
    q.w = c;
    return q;
}

Quat body_quat_multiply(const Quat& a, const Quat& b) {
    Quat r;
    r.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    return r;
}

Quat body_quat_slerp_d3dx(const Quat& a, const Quat& b, float t) {
    // the exe imports the slerp from d3dx9 the semantic is the documented one not read from bytes
    float cosine = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    Quat bb = b;
    if (cosine < 0.0f) {
        cosine = -cosine;
        bb.x = -bb.x;
        bb.y = -bb.y;
        bb.z = -bb.z;
        bb.w = -bb.w;
    }
    float wa;
    float wb;
    if (kOne - cosine > kSlerpLinearGate) {
        float omega = std::acos(cosine);
        float sine = std::sin(omega);
        wa = std::sin((kOne - t) * omega) / sine;
        wb = std::sin(t * omega) / sine;
    } else {
        wa = kOne - t;
        wb = t;
    }
    Quat r;
    r.x = wa * a.x + wb * bb.x;
    r.y = wa * a.y + wb * bb.y;
    r.z = wa * a.z + wb * bb.z;
    r.w = wa * a.w + wb * bb.w;
    return r;
}

void body_vec3_normalize_d3dx(Vec3& v) {
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len <= 0.0f) return;
    v.x /= len;
    v.y /= len;
    v.z /= len;
}

bool body_mat3_jacobi_eigen(const Mat3& tensor, Mat3& vectors, float values[3]) {
    // numerical recipes jacobi 50 sweeps threshold 0x5A8360 hundred gate 0x5A1648
    float a[3][3];
    std::memcpy(a, tensor.m, sizeof(a));
    float v[3][3] = {{kOne, 0.0f, 0.0f}, {0.0f, kOne, 0.0f}, {0.0f, 0.0f, kOne}};
    float b[3];
    float z[3];
    float d[3];
    for (int i = 0; i < 3; ++i) {
        b[i] = d[i] = a[i][i];
        z[i] = 0.0f;
    }
    for (int sweep = 0; sweep < kJacobiSweeps; ++sweep) {
        float sm = kZero;
        for (int ip = 0; ip < 2; ++ip) {
            for (int iq = ip + 1; iq < 3; ++iq) sm = sm + a[ip][iq];
        }
        if (sm == kWheelAxisAngleTolerance) {
            std::memcpy(vectors.m, v, sizeof(v));
            values[0] = d[0];
            values[1] = d[1];
            values[2] = d[2];
            return true;
        }
        float tresh = kZero;
        if (sweep < 3) tresh = std::fabs(sm) * kJacobiThreshold;
        for (int ip = 0; ip < 2; ++ip) {
            for (int iq = ip + 1; iq < 3; ++iq) {
                float g = kJacobiHundred * std::fabs(a[ip][iq]);
                if (sweep >= 5 && std::fabs(d[ip]) == g + std::fabs(d[ip]) &&
                    std::fabs(d[iq]) == g + std::fabs(d[iq])) {
                    a[ip][iq] = 0.0f;
                } else if (tresh < std::fabs(a[ip][iq])) {
                    float h = d[iq] - d[ip];
                    float t;
                    if (std::fabs(h) == g + std::fabs(h)) {
                        t = a[ip][iq] / h;
                    } else {
                        float theta = (h / a[ip][iq]) * kHalf;
                        t = kOne / (std::fabs(theta) + std::sqrt(theta * theta + kOne));
                        if (theta < kWheelAxisAngleTolerance) t = -t;
                    }
                    float c = kOne / std::sqrt(t * t + kOne);
                    float s = c * t;
                    float tau = s / (c + kOne);
                    h = t * a[ip][iq];
                    z[ip] = z[ip] - h;
                    z[iq] = z[iq] + h;
                    d[ip] = d[ip] - h;
                    d[iq] = d[iq] + h;
                    a[ip][iq] = 0.0f;
                    auto rotate = [&](float& gg, float& hh) {
                        float g0 = gg;
                        float h0 = hh;
                        gg = g0 - (tau * g0 + h0) * s;
                        hh = (g0 - tau * h0) * s + h0;
                    };
                    for (int j = 0; j < ip; ++j) rotate(a[j][ip], a[j][iq]);
                    for (int j = ip + 1; j < iq; ++j) rotate(a[ip][j], a[j][iq]);
                    for (int j = iq + 1; j < 3; ++j) rotate(a[ip][j], a[iq][j]);
                    for (int j = 0; j < 3; ++j) rotate(v[j][ip], v[j][iq]);
                }
            }
        }
        for (int i = 0; i < 3; ++i) {
            b[i] = b[i] + z[i];
            d[i] = b[i];
            z[i] = 0.0f;
        }
        values[0] = d[0];
        values[1] = d[1];
        values[2] = d[2];
    }
    std::memcpy(vectors.m, v, sizeof(v));
    return false;
}

} // namespace KnC Kart Client
