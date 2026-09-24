#include "motion_packet.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace KnC::Kart::Client {

namespace {

// named constant read memory from KnC exe raw 0x5a05e0 8 bit fraction to units
constexpr float kFracScale = 0.01f;
constexpr float kFracRound = 100.0f;  // 0x5a1648 units to centi units 0x59f450 predicted position lead factor
constexpr float kPredictDtScale = 50.0f;
constexpr float kSendIntervalMs = 100.0f;  // 0x1396e90 the 100 ms 10 Hz motion timer 0x5a164c drift lean subtracted from yaw
constexpr float kDriftGaugeYawScale = 0.6f;
constexpr float kDriftGaugeCap = 45.0f;  // 0x5eb700 drift gauge cap both signs 0x5a6b18 rpm nibble to units factor
constexpr float kRpmNibbleScale = 600.0f;
constexpr float kYawDegToByte = 0.70833331f;  // 0x5a6abc 255 over 360 the send yaw byte scale 0x5a3230 the gauge nibble spans 15 over twice the cap
constexpr float kGhostNibbleSpan = 15.0f;
constexpr float kGhostRpmFloor = 1000.0f;  // 0x5a3bf4 rpm under this sends nibble 0 0x5a6ab8 rpm over the floor clamps here
constexpr float kGhostRpmCeil = 9000.0f;
constexpr float kGhostRpmToNibble = 0.0016666667f; // 0x5a6ab4 one over 600

// FUN 0044E500 octant case 1 to 8 sign triple zero as plus
void octant_to_signs(int oct, float& sx, float& sy, float& sz)
{
    switch (oct) {
    case 1: sx = 1; sy = 1; sz = 1; return;
    case 2: sx = 1; sy = -1; sz = -1; return;
    case 3: sx = -1; sy = 1; sz = -1; return;
    case 4: sx = -1; sy = -1; sz = 1; return;
    case 5: sx = -1; sy = -1; sz = -1; return;
    case 6: sx = -1; sy = 1; sz = 1; return;
    case 7: sx = 1; sy = -1; sz = 1; return;
    case 8: sx = 1; sy = 1; sz = -1; return;
    default: sx = 1; sy = 1; sz = 1; return;
    }
}

// inverse octant to signs FUN 0044E370 case by case output
int signs_to_octant(float x, float y, float z)
{
    static const int table[8] = {1, 8, 7, 2, 6, 3, 4, 5};
    int idx = (x < 0.0f ? 4 : 0) | (y < 0.0f ? 2 : 0) | (z < 0.0f ? 1 : 0);
    return table[idx];
}

void write_u16_le(std::vector<uint8_t>& out, uint16_t v)
{
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void write_u32_le(std::vector<uint8_t>& out, uint32_t v)
{
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void write_f32_le(std::vector<uint8_t>& out, float f)
{
    uint32_t bits;
    static_assert(sizeof(bits) == sizeof(f), "float32 size");
    std::memcpy(&bits, &f, sizeof(bits));
    write_u32_le(out, bits);
}

uint32_t read_u32_le(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t read_u16_le(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

float read_f32_le(const uint8_t* p)
{
    uint32_t bits = read_u32_le(p);
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

void write_packed_vec3(std::vector<uint8_t>& out, float x, float y, float z)
{
    uint64_t packed = motion_pack_vec3(x, y, z);
    write_u32_le(out, static_cast<uint32_t>(packed & 0xFFFFFFFFu));
    write_u32_le(out, static_cast<uint32_t>(packed >> 32));
}

} // namespace

uint64_t motion_pack_vec3(float x, float y, float z)
{
    int oct = signs_to_octant(x, y, z);

    float mags[3] = {std::fabs(x), std::fabs(y), std::fabs(z)};
    int intPart[3];
    int fracPart[3];
    for (int i = 0; i < 3; ++i) {
        // round to nearest hundredth of a unit keeps int and frac consistent
        long centi = std::lround(mags[i] * kFracRound);
        if (centi < 0) centi = 0;
        long maxCenti = 4095L * 100L + 99L;
        if (centi > maxCenti) centi = maxCenti;
        intPart[i] = static_cast<int>(centi / 100);
        fracPart[i] = static_cast<int>(centi % 100);
        if (intPart[i] > 0xFFF) { intPart[i] = 0xFFF; fracPart[i] = 99; }
    }

    // low32 bits 0 to 3 octant 4 to 11 z frac
    uint32_t low = static_cast<uint32_t>(oct & 0xF);
    low |= static_cast<uint32_t>(fracPart[2] & 0xFF) << 4;
    low |= static_cast<uint32_t>(intPart[2] & 0xFFF) << 12;
    low |= static_cast<uint32_t>(fracPart[1] & 0xFF) << 24;

    // high32 bits 0 to 11 y int 12 to 19 x frac 20 to 31 x int
    uint32_t high = static_cast<uint32_t>(intPart[1] & 0xFFF);
    high |= static_cast<uint32_t>(fracPart[0] & 0xFF) << 12;
    high |= static_cast<uint32_t>(intPart[0] & 0xFFF) << 20;

    return (static_cast<uint64_t>(high) << 32) | static_cast<uint64_t>(low);
}

void motion_unpack_vec3(uint64_t packed, float outXYZ[3])
{
    uint32_t low = static_cast<uint32_t>(packed & 0xFFFFFFFFu);
    uint32_t high = static_cast<uint32_t>(packed >> 32);

    int oct = static_cast<int>(low & 0xF);
    float sx, sy, sz;
    octant_to_signs(oct, sx, sy, sz);

    float zFrac = static_cast<float>((low >> 4) & 0xFF);
    float zInt = static_cast<float>((low >> 12) & 0xFFF);
    float yFrac = static_cast<float>((low >> 24) & 0xFF);
    float yInt = static_cast<float>(high & 0xFFF);
    float xFrac = static_cast<float>((high >> 12) & 0xFF);
    float xInt = static_cast<float>((high >> 20) & 0xFFF);

    outXYZ[0] = (xFrac * kFracScale + xInt) * sx;
    outXYZ[1] = (yFrac * kFracScale + yInt) * sy;
    outXYZ[2] = (zFrac * kFracScale + zInt) * sz;
}

MotionSend0x40 motion_send_0x40(const MotionSendInputs& in)
{
    MotionSend0x40 out;
    out.pos[0] = in.pos[0];
    out.pos[1] = in.pos[1];
    out.pos[2] = in.pos[2];

    float dt50 = in.frameDt * kPredictDtScale;
    float zAsym = (in.vel[2] > 0.0f) ? 0.25f : 1.2f; // 0x5a32d4 rising 0x5a32b0 descending
    out.predPos[0] = in.pos[0] + dt50 * in.vel[0];
    out.predPos[1] = in.pos[1] + dt50 * in.vel[1];
    out.predPos[2] = in.pos[2] + dt50 * in.vel[2] * zAsym;

    // 0x49bfa0 math wrap angle 360 then 0x49bfa9 times 255 over 360 into crt ftol trunc
    float yawSrc = in.yawDeg - in.driftGaugeSmoothed * kDriftGaugeYawScale;
    yawSrc = std::fmod(yawSrc, 360.0f);
    if (yawSrc < 0.0f) yawSrc += 360.0f;
    out.yawByte = static_cast<uint8_t>(static_cast<int>(yawSrc * kYawDegToByte) & 0xFF);

    // 0x49bfb4 gauge plus 45 times 15 over 90 truncated then 0x49bfd9 rpm minus 1000 clamped 0 9000 over 600
    int speedNibble = static_cast<int>((in.driftGauge + kDriftGaugeCap) * (kGhostNibbleSpan / (kDriftGaugeCap + kDriftGaugeCap))) & 0xF;
    float rpmOver = in.rpm - kGhostRpmFloor;
    if (rpmOver < 0.0f) rpmOver = 0.0f;
    else if (rpmOver > kGhostRpmCeil) rpmOver = kGhostRpmCeil;
    int rpmNibble = static_cast<int>(rpmOver * kGhostRpmToNibble) & 0xF;

    int bits = (speedNibble & 0xF) << 12;
    bits |= (rpmNibble & 0xF) << 8;
    if (in.miniTurboBoost) bits |= 0x80;
    else if (in.itemBoost) bits |= 0x40;
    if (in.reverse) bits |= 0x20;
    if (in.drift) bits |= 0x10;
    if (in.miniTurboStage1) bits |= 0x08;
    if (in.turnState == 1) bits |= 0x04;
    else if (in.turnState == 2) bits |= 0x02;
    out.status = static_cast<uint16_t>(bits);

    return out;
}

bool motion_send_timer_fired(int64_t nowMs, int64_t& lastSendMs)
{
    if (nowMs - lastSendMs >= static_cast<int64_t>(kSendIntervalMs)) {
        lastSendMs = nowMs;
        return true;
    }
    return false;
}

std::vector<uint8_t> net_motion_pack_0x40(const MotionSend0x40& send)
{
    std::vector<uint8_t> out;
    out.reserve(19);
    write_packed_vec3(out, send.pos[0], send.pos[1], send.pos[2]);
    write_packed_vec3(out, send.predPos[0], send.predPos[1], send.predPos[2]);
    out.push_back(send.yawByte);
    write_u16_le(out, send.status);
    return out;
}

std::vector<uint8_t> net_motion_pack_0x40_raw(const MotionSend0x40& send)
{
    std::vector<uint8_t> out;
    out.reserve(28);
    write_f32_le(out, send.pos[0]);
    write_f32_le(out, send.pos[1]);
    write_f32_le(out, send.pos[2]);
    write_f32_le(out, send.predPos[0]);
    write_f32_le(out, send.predPos[1]);
    write_f32_le(out, send.predPos[2]);
    out.push_back(send.yawByte);
    out.push_back(send.pad);
    write_u16_le(out, send.status);
    return out;
}

std::vector<MotionRecvEntry> net_motion_recv_0x40(const uint8_t* data, size_t len)
{
    std::vector<MotionRecvEntry> out;
    if (data == nullptr || len < 1) return out;
    uint8_t n = data[0];
    size_t off = 1;
    for (uint8_t i = 0; i < n; ++i) {
        if (off + 25 > len) break;
        MotionRecvEntry e;
        e.id = static_cast<int32_t>(read_u32_le(data + off)); off += 4;
        e.hint = static_cast<int16_t>(read_u16_le(data + off)); off += 2;
        uint32_t posLow = read_u32_le(data + off); off += 4;
        uint32_t posHigh = read_u32_le(data + off); off += 4;
        motion_unpack_vec3((static_cast<uint64_t>(posHigh) << 32) | posLow, e.pos);
        uint32_t predLow = read_u32_le(data + off); off += 4;
        uint32_t predHigh = read_u32_le(data + off); off += 4;
        motion_unpack_vec3((static_cast<uint64_t>(predHigh) << 32) | predLow, e.predPos);
        e.yawByte = data[off]; off += 1;
        e.status = read_u16_le(data + off); off += 2;
        out.push_back(e);
    }
    return out;
}

std::vector<MotionRecvEntry> net_motion_recv_0x40_raw(const uint8_t* data, size_t len)
{
    std::vector<MotionRecvEntry> out;
    if (data == nullptr || len < 1) return out;
    uint8_t n = data[0];
    size_t off = 1;
    for (uint8_t i = 0; i < n; ++i) {
        if (off + 34 > len) break;
        MotionRecvEntry e;
        e.id = static_cast<int32_t>(read_u32_le(data + off)); off += 4;
        e.hint = static_cast<int16_t>(read_u16_le(data + off)); off += 2;
        for (int a = 0; a < 3; ++a) { e.pos[a] = read_f32_le(data + off); off += 4; }
        for (int a = 0; a < 3; ++a) { e.predPos[a] = read_f32_le(data + off); off += 4; }
        e.yawByte = data[off]; off += 1;
        off += 1; // pad byte not sent meaning
        e.status = read_u16_le(data + off); off += 2;
        out.push_back(e);
    }
    return out;
}

void net_motion_sample_push(MotionMailbox& mailbox, const MotionSample& sample)
{
    if (mailbox.present) {
        net_motion_queue_shift(mailbox); // 0x4a4200 drops the unconsumed sample first
    }
    mailbox.sample = sample;
    mailbox.present = true;
}

bool net_motion_sample_pop(MotionMailbox& mailbox, MotionSample& out)
{
    if (!mailbox.present) return false;
    out = mailbox.sample;
    net_motion_queue_shift(mailbox);
    return true;
}

void net_motion_queue_shift(MotionMailbox& mailbox)
{
    mailbox.present = false;
}

void net_motion_queue_clear(MotionMailbox& mailbox)
{
    mailbox.present = false; // 0x4a41f0 the count at queue 0x28 goes to 0
}

int net_player_id_to_slot(const int32_t* slotIds, const uint8_t* slotActive, int slotCount, int32_t playerId)
{
    int n = std::min(slotCount, 30);
    for (int i = 0; i < n; ++i) {
        if (slotActive[i] == 1 && slotIds[i] == playerId) return i;
    }
    return -1;
}

}
