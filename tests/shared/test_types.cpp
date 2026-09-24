/// tests for shared core Types header

#include <shared/Shared.h>
#include <cstdio>
#include <cassert>

using namespace KnC;
using namespace KnC::Math;
using namespace KnC::Net;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    printf("Running %s...\n", #name); \
    test_##name(); \
    printf("  PASS\n"); \
} while(0)

#define ASSERT_EQ(a, b) assert((a) == (b))
#define ASSERT_TRUE(x) assert(x)
#define ASSERT_FALSE(x) assert(!(x))
#define ASSERT_NEAR(a, b, eps) assert(std::abs((a) - (b)) < (eps))

TEST(types_basic) {
    ASSERT_EQ(sizeof(int8), 1);
    ASSERT_EQ(sizeof(int16), 2);
    ASSERT_EQ(sizeof(int32), 4);
    ASSERT_EQ(sizeof(int64), 8);
    
    ASSERT_EQ(sizeof(uint8), 1);
    ASSERT_EQ(sizeof(uint16), 2);
    ASSERT_EQ(sizeof(uint32), 4);
    ASSERT_EQ(sizeof(uint64), 8);
    
    ASSERT_EQ(sizeof(float32), 4);
    ASSERT_EQ(sizeof(float64), 8);
}

TEST(types_game) {
    PlayerId p1 = 123;
    RoomId r1 = 456;
    
    ASSERT_EQ(p1, 123);
    ASSERT_EQ(r1, 456);
    ASSERT_EQ(INVALID_PLAYER_ID, 0);
    ASSERT_EQ(INVALID_ROOM_ID, 0);
}

TEST(vec2_basic) {
    Vec2 v1(1.0f, 2.0f);
    Vec2 v2(3.0f, 4.0f);
    
    Vec2 sum = v1 + v2;
    ASSERT_EQ(sum.x, 4.0f);
    ASSERT_EQ(sum.y, 6.0f);
    
    Vec2 diff = v2 - v1;
    ASSERT_EQ(diff.x, 2.0f);
    ASSERT_EQ(diff.y, 2.0f);
    
    Vec2 scaled = v1 * 2.0f;
    ASSERT_EQ(scaled.x, 2.0f);
    ASSERT_EQ(scaled.y, 4.0f);
}

TEST(vec2_length) {
    Vec2 v(3.0f, 4.0f);
    ASSERT_NEAR(v.Length(), 5.0f, 0.001f);
    ASSERT_NEAR(v.LengthSquared(), 25.0f, 0.001f);
}

TEST(vec3_basic) {
    Vec3 v1(1.0f, 2.0f, 3.0f);
    Vec3 v2(4.0f, 5.0f, 6.0f);
    
    Vec3 sum = v1 + v2;
    ASSERT_EQ(sum.x, 5.0f);
    ASSERT_EQ(sum.y, 7.0f);
    ASSERT_EQ(sum.z, 9.0f);
}

TEST(vec3_cross) {
    Vec3 x = Vec3::UnitX();
    Vec3 y = Vec3::UnitY();
    Vec3 z = x.Cross(y);
    
    ASSERT_NEAR(z.x, 0.0f, 0.001f);
    ASSERT_NEAR(z.y, 0.0f, 0.001f);
    ASSERT_NEAR(z.z, 1.0f, 0.001f);
}

TEST(quat_identity) {
    Quat q = Quat::Identity();
    ASSERT_EQ(q.x, 0.0f);
    ASSERT_EQ(q.y, 0.0f);
    ASSERT_EQ(q.z, 0.0f);
    ASSERT_EQ(q.w, 1.0f);
}

TEST(quat_rotation) {
    // 90 degree rotation around Y axis
    Quat q = Quat::FromAxisAngle(Vec3::UnitY(), 3.14159f / 2.0f);
    
    Vec3 forward = Vec3::Forward();
    Vec3 rotated = q.RotateVector(forward);
    
    // forward 0 0 1 rotated 90 degrees around Y should give right 1 0 0
    ASSERT_NEAR(rotated.x, 1.0f, 0.01f);
    ASSERT_NEAR(rotated.y, 0.0f, 0.01f);
    ASSERT_NEAR(rotated.z, 0.0f, 0.01f);
}

Result<int, const char*> divide(int a, int b) {
    if (b == 0) {
        return Err<const char*, int>("Division by zero");
    }
    return Ok<int, const char*>(a / b);
}

TEST(result_ok) {
    auto result = divide(10, 2);
    ASSERT_TRUE(result.IsOk());
    ASSERT_FALSE(result.IsErr());
    ASSERT_EQ(result.Unwrap(), 5);
}

TEST(result_err) {
    auto result = divide(10, 0);
    ASSERT_FALSE(result.IsOk());
    ASSERT_TRUE(result.IsErr());
}

TEST(result_unwrap_or) {
    auto ok = divide(10, 2);
    auto err = divide(10, 0);
    
    ASSERT_EQ(ok.UnwrapOr(999), 5);
    ASSERT_EQ(err.UnwrapOr(999), 999);
}

TEST(packet_header) {
    PacketHeader header(0x01, 0x02, 100);
    
    ASSERT_EQ(header.cmd, 0x01);
    ASSERT_EQ(header.flag, 0x02);
    ASSERT_EQ(header.size, 100);
    ASSERT_EQ(sizeof(PacketHeader), 8);
}

TEST(packet_buffer_write) {
    PacketBuffer buffer;
    buffer.Init(0x01, 0x00);
    
    uint32 playerId = 12345;
    buffer.Write(playerId);
    
    Vec3 position(1.0f, 2.0f, 3.0f);
    buffer.Write(position);
    
    buffer.WriteFixedString("TestPlayer", 32);
    
    ASSERT_EQ(buffer.GetHeader()->cmd, 0x01);
    ASSERT_EQ(buffer.GetHeader()->flag, 0x00);
    ASSERT_TRUE(buffer.GetPayloadSize() > 0);
}

TEST(packet_reader) {
    PacketBuffer buffer;
    buffer.Init(0x05, 0x01);
    buffer.Write<uint32>(9999);
    buffer.Write<float>(42.5f);

    PacketReader reader(buffer.GetData(), buffer.GetTotalSize());
    
    ASSERT_EQ(reader.GetHeader()->cmd, 0x05);
    ASSERT_EQ(reader.GetHeader()->flag, 0x01);
    
    uint32 value1;
    float value2;
    
    ASSERT_TRUE(reader.Read(value1));
    ASSERT_TRUE(reader.Read(value2));
    
    ASSERT_EQ(value1, 9999);
    ASSERT_NEAR(value2, 42.5f, 0.001f);
}

int main() {
    printf("=== Kart N'Chibi - Shared Types Tests ===\n\n");

    RUN_TEST(types_basic);
    RUN_TEST(types_game);

    RUN_TEST(vec2_basic);
    RUN_TEST(vec2_length);
    RUN_TEST(vec3_basic);
    RUN_TEST(vec3_cross);
    RUN_TEST(quat_identity);
    RUN_TEST(quat_rotation);

    RUN_TEST(result_ok);
    RUN_TEST(result_err);
    RUN_TEST(result_unwrap_or);

    RUN_TEST(packet_header);
    RUN_TEST(packet_buffer_write);
    RUN_TEST(packet_reader);
    
    printf("\n=== ALL TESTS PASSED ===\n");
    return 0;
}

