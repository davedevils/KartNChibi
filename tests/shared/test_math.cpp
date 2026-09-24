
#include "shared/math/Vec3.h"
#include "shared/math/Matrix4.h"
#include "shared/math/Quat.h"
#include <iostream>
#include <cmath>
#include <cstdlib>

static int g_pass = 0, g_fail = 0;

#define CHECK(expr) do { if (expr) { g_pass++; } else { \
    std::cerr << "  \033[31m[FAIL]\033[0m " << #expr << " (" << __FILE__ << ":" << __LINE__ << ")\n"; g_fail++; \
} } while(0)

#define CHECK_NEAR(a, b, eps) do { if (std::abs((a)-(b)) <= (eps)) { g_pass++; } else { \
    std::cerr << "  \033[31m[FAIL]\033[0m " << #a << " !~ " << #b << " (got " << (a) << " vs " << (b) << ")\n"; g_fail++; \
} } while(0)

using namespace KnC::Math;

void test_Vec3_Operations() {
    std::cout << "--- Vec3_Operations ---\n";
    Vec3 a(1, 2, 3), b(4, 5, 6);

    auto c = a + b;
    CHECK_NEAR(c.x, 5.0f, 0.001f);
    CHECK_NEAR(c.y, 7.0f, 0.001f);
    CHECK_NEAR(c.z, 9.0f, 0.001f);

    auto d = a - b;
    CHECK_NEAR(d.x, -3.0f, 0.001f);
    CHECK_NEAR(d.y, -3.0f, 0.001f);

    auto e = a * 2.0f;
    CHECK_NEAR(e.x, 2.0f, 0.001f);
    CHECK_NEAR(e.z, 6.0f, 0.001f);

    // dot product 1 times 4 plus 2 times 5 plus 3 times 6 equals 32
    float dot = a.Dot(b);
    CHECK_NEAR(dot, 32.0f, 0.001f);

    // cross product UnitX cross UnitY equals UnitZ
    Vec3 cross = Vec3::UnitX().Cross(Vec3::UnitY());
    CHECK_NEAR(cross.z, 1.0f, 0.001f);
    CHECK_NEAR(cross.x, 0.0f, 0.001f);

    Vec3 norm = Vec3(3, 0, 0).Normalized();
    CHECK_NEAR(norm.x, 1.0f, 0.001f);
    CHECK_NEAR(norm.Length(), 1.0f, 0.001f);

    float dist = Vec3::Zero().Distance(Vec3(3, 4, 0));
    CHECK_NEAR(dist, 5.0f, 0.001f);

    Vec3 lerp = Vec3::Lerp(Vec3::Zero(), Vec3::One(), 0.5f);
    CHECK_NEAR(lerp.x, 0.5f, 0.001f);
    CHECK_NEAR(lerp.y, 0.5f, 0.001f);
}

void test_Matrix4_Identity() {
    std::cout << "--- Matrix4_Identity ---\n";
    auto id = Matrix4::MakeIdentity();
    CHECK_NEAR(id.data[0], 1.0f, 0.001f);
    CHECK_NEAR(id.data[5], 1.0f, 0.001f);
    CHECK_NEAR(id.data[10], 1.0f, 0.001f);
    CHECK_NEAR(id.data[15], 1.0f, 0.001f);
    CHECK_NEAR(id.data[1], 0.0f, 0.001f);
    CHECK_NEAR(id.data[4], 0.0f, 0.001f);
}

void test_Matrix4_Multiply() {
    std::cout << "--- Matrix4_Multiply ---\n";
    auto id = Matrix4::MakeIdentity();

    auto mul = id * id;
    CHECK_NEAR(mul.data[0], 1.0f, 0.001f);
    CHECK_NEAR(mul.data[15], 1.0f, 0.001f);
    CHECK_NEAR(mul.data[1], 0.0f, 0.001f);

    auto t = Matrix4::Translation(Vec3(10, 20, 30));
    auto p = t.TransformPoint(Vec3::Zero());
    CHECK_NEAR(p.x, 10.0f, 0.001f);
    CHECK_NEAR(p.y, 20.0f, 0.001f);
    CHECK_NEAR(p.z, 30.0f, 0.001f);

    auto s = Matrix4::Scale(Vec3(2, 3, 4));
    auto sp = s.TransformPoint(Vec3(1, 1, 1));
    CHECK_NEAR(sp.x, 2.0f, 0.001f);
    CHECK_NEAR(sp.y, 3.0f, 0.001f);
    CHECK_NEAR(sp.z, 4.0f, 0.001f);
}

void test_Matrix4_Inverse() {
    std::cout << "--- Matrix4_Inverse ---\n";
    auto id = Matrix4::MakeIdentity();

    auto inv = id.Inverse();
    CHECK_NEAR(inv.data[0], 1.0f, 0.001f);
    CHECK_NEAR(inv.data[5], 1.0f, 0.001f);

    // Inverse of translation recovers origin
    auto t = Matrix4::Translation(Vec3(10, 20, 30));
    auto tinv = t.Inverse();
    auto recover = tinv.TransformPoint(Vec3(10, 20, 30));
    CHECK_NEAR(recover.x, 0.0f, 0.05f);
    CHECK_NEAR(recover.y, 0.0f, 0.05f);
    CHECK_NEAR(recover.z, 0.0f, 0.05f);
}

void test_Quat_FromAxisAngle() {
    std::cout << "--- Quat_FromAxisAngle ---\n";
    auto id = Quat::Identity();
    CHECK_NEAR(id.w, 1.0f, 0.001f);
    CHECK_NEAR(id.x, 0.0f, 0.001f);

    float pi = 3.14159265f;
    auto q = Quat::FromAxisAngle(Vec3::UnitY(), pi / 2.0f);
    CHECK_NEAR(q.Length(), 1.0f, 0.001f);

    // Rotate UnitX by 90 around Y
    Vec3 rotated = q.RotateVector(Vec3::UnitX());
    CHECK_NEAR(std::abs(rotated.x), 0.0f, 0.01f);
    CHECK_NEAR(std::abs(rotated.z), 1.0f, 0.01f);
}

void test_Quat_Slerp() {
    std::cout << "--- Quat_Slerp ---\n";
    float pi = 3.14159265f;
    auto q = Quat::FromAxisAngle(Vec3::UnitY(), pi / 2.0f);

    auto s0 = Quat::Slerp(Quat::Identity(), q, 0.0f);
    CHECK_NEAR(s0.w, 1.0f, 0.01f);

    auto s1 = Quat::Slerp(Quat::Identity(), q, 1.0f);
    CHECK_NEAR(s1.x, q.x, 0.01f);
    CHECK_NEAR(s1.y, q.y, 0.01f);
    CHECK_NEAR(s1.z, q.z, 0.01f);
    CHECK_NEAR(s1.w, q.w, 0.01f);

    auto s5 = Quat::Slerp(Quat::Identity(), q, 0.5f);
    CHECK_NEAR(s5.Length(), 1.0f, 0.01f);
}

int main() {
    std::cout << "\n========================================\n";
    std::cout << "  KnC Test Suite: math\n";
    std::cout << "========================================\n\n";

    test_Vec3_Operations();
    test_Matrix4_Identity();
    test_Matrix4_Multiply();
    test_Matrix4_Inverse();
    test_Quat_FromAxisAngle();
    test_Quat_Slerp();

    std::cout << "\n========================================\n";
    if (g_fail == 0) {
        std::cout << "  \033[32mResults: " << g_pass << " passed, 0 failed\033[0m\n";
    } else {
        std::cout << "  \033[31mResults: " << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    }
    std::cout << "========================================\n";

    return g_fail > 0 ? 1 : 0;
}
