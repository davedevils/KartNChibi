// Test motion packet module pack unpack state position round trip yaw byte status bit nibble S2C

#include "../motion_packet.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace KnC::Kart::Client;

int main() {
    int failures = 0;

    float x = 1234.56f;
    float y = -987.12f;
    float z = 45.0f;
    uint64_t packed = motion_pack_vec3(x, y, z);
    float unpacked[3];
    motion_unpack_vec3(packed, unpacked);
    std::printf("vec3 round trip x %f y %f z %f\n", static_cast<double>(unpacked[0]),
                static_cast<double>(unpacked[1]), static_cast<double>(unpacked[2]));
    if (std::fabs(unpacked[0] - x) > 0.011f) { std::printf("vec3 x out of packed precision\n"); ++failures; }
    if (std::fabs(unpacked[1] - y) > 0.011f) { std::printf("vec3 y out of packed precision\n"); ++failures; }
    if (std::fabs(unpacked[2] - z) > 0.011f) { std::printf("vec3 z out of packed precision\n"); ++failures; }

    MotionSendInputs in;
    in.pos[0] = 100.0f; in.pos[1] = 200.0f; in.pos[2] = 5.0f;
    in.vel[0] = 10.0f; in.vel[1] = 0.0f; in.vel[2] = -2.0f;
    in.yawDeg = 90.0f;
    in.driftGaugeSmoothed = 0.0f;
    in.driftGauge = 30.0f;
    in.rpm = 3000.0f;
    in.frameDt = 0.016f;
    in.miniTurboBoost = true;
    in.reverse = true;
    in.drift = true;
    in.miniTurboStage1 = true;
    in.turnState = 1;

    MotionSend0x40 send = motion_send_0x40(in);
    std::printf("send yaw byte %d status %d\n", static_cast<int>(send.yawByte), static_cast<int>(send.status));

    if ((send.status & 0x80) == 0) { std::printf("status bit7 mini turbo boost not set\n"); ++failures; }
    if ((send.status & 0x40) != 0) { std::printf("status bit6 item boost set when it should not be\n"); ++failures; }
    if ((send.status & 0x20) == 0) { std::printf("status bit5 reverse not set\n"); ++failures; }
    if ((send.status & 0x10) == 0) { std::printf("status bit4 drift not set\n"); ++failures; }
    if ((send.status & 0x08) == 0) { std::printf("status bit3 mini turbo stage1 not set\n"); ++failures; }
    if ((send.status & 0x04) == 0) { std::printf("status bit2 turn state one not set\n"); ++failures; }
    if ((send.status & 0x02) != 0) { std::printf("status bit1 turn state two set when it should not be\n"); ++failures; }

    int speedNibble = (send.status >> 12) & 0xF;
    int rpmNibble = (send.status >> 8) & 0xF;
    std::printf("speed nibble %d rpm nibble %d\n", speedNibble, rpmNibble);
    // 0x49bfd9 rpm minus 1000 clamped 0 to 9000 over 600 truncated 3000 gives 3
    if (rpmNibble != 3) { std::printf("rpm nibble did not match rpm minus 1000 over 600 truncated\n"); ++failures; }
    // 0x49bfb4 gauge plus 45 times 15 over 90 truncated 30 gives 12
    if (speedNibble != 12) { std::printf("gauge nibble did not match gauge plus 45 times 15 over 90\n"); ++failures; }

    // 0x49bfa9 the yaw times 255 over 360 truncated 90 gives 63
    int expectedYawByte = static_cast<int>(90.0f * 0.70833331f);
    if (static_cast<int>(send.yawByte) != expectedYawByte) {
        std::printf("yaw byte did not match the 255 over 360 scale truncated\n");
        ++failures;
    }

    std::vector<uint8_t> c2s = net_motion_pack_0x40(send);
    if (c2s.size() != 19) { std::printf("normal C2S payload was not 19 bytes\n"); ++failures; }

    std::vector<uint8_t> wire;
    wire.push_back(1);
    uint32_t id = 7;
    wire.push_back(static_cast<uint8_t>(id & 0xFF));
    wire.push_back(static_cast<uint8_t>((id >> 8) & 0xFF));
    wire.push_back(static_cast<uint8_t>((id >> 16) & 0xFF));
    wire.push_back(static_cast<uint8_t>((id >> 24) & 0xFF));
    uint16_t hint = 1000;
    wire.push_back(static_cast<uint8_t>(hint & 0xFF));
    wire.push_back(static_cast<uint8_t>((hint >> 8) & 0xFF));
    for (uint8_t b : c2s) wire.push_back(b);
    if (wire.size() != 26) { std::printf("hand built S2C wire was not 26 bytes\n"); ++failures; }

    std::vector<MotionRecvEntry> entries = net_motion_recv_0x40(wire.data(), wire.size());
    if (entries.size() != 1) {
        std::printf("decoded entry count was not one\n");
        ++failures;
    } else {
        const MotionRecvEntry& e = entries[0];
        std::printf("decoded id %d hint %d yaw byte %d status %d\n", e.id, e.hint,
                    static_cast<int>(e.yawByte), static_cast<int>(e.status));
        if (e.id != 7) { std::printf("decoded id did not match\n"); ++failures; }
        if (e.hint != 1000) { std::printf("decoded hint did not match\n"); ++failures; }
        if (std::fabs(e.pos[0] - in.pos[0]) > 0.02f) { std::printf("decoded pos x out of range\n"); ++failures; }
        if (std::fabs(e.pos[1] - in.pos[1]) > 0.02f) { std::printf("decoded pos y out of range\n"); ++failures; }
        if (std::fabs(e.pos[2] - in.pos[2]) > 0.02f) { std::printf("decoded pos z out of range\n"); ++failures; }
        if (e.yawByte != send.yawByte) { std::printf("decoded yaw byte did not match the sent byte\n"); ++failures; }
        if (e.status != send.status) { std::printf("decoded status word did not match the sent word\n"); ++failures; }
    }

    if (failures == 0) {
        std::printf("motion_packet_test PASS\n");
        return 0;
    }
    std::printf("motion_packet_test FAIL %d\n", failures);
    return 1;
}
