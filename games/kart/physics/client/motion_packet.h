/// 0x40 motion channel port from net motion pack 0x4818a0 peers
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace KnC::Kart::Client {

/// 28 byte stack struct net motion pack 0x40 sends car 0x3374
struct MotionSend0x40 {
    float pos[3] = {0.0f, 0.0f, 0.0f};  // own position car 0x3244 0x3248 0x324c pos plus frame dt times 50 times velocity
    float predPos[3] = {0.0f, 0.0f, 0.0f};
    uint8_t yawByte = 0;  // 0 to 255 over 0 to 360 deg wire offset 0x18 unused byte not sent on the normal wire path
    uint8_t pad = 0;
    uint16_t status = 0;                     // bit7 miniturbo bit6 item bit5 reverse bit4 drift
};

/// decoded S2C 0x40 per player 25 byte normal 34 byte anti hook
struct MotionRecvEntry {
    int32_t id = 0;  // sender player id net player id to slot key ticks hint divisor for the anchor ease send 1000
    int16_t hint = 0;
    float pos[3] = {0.0f, 0.0f, 0.0f};
    float predPos[3] = {0.0f, 0.0f, 0.0f};
    uint8_t yawByte = 0;
    uint16_t status = 0;
};

/// mailbox payload car 0x3374 to 0x3390 one pending sample per
struct MotionSample {
    float pos[3] = {0.0f, 0.0f, 0.0f};  // car 0x3374 0x3378 0x337c car 0x3380 0x3384 0x3388
    float predPos[3] = {0.0f, 0.0f, 0.0f};
    uint8_t yawByte = 0;  // car 0x338c car 0x338e the bit flags byte
    uint8_t statusLo = 0;
    uint8_t statusHi = 0;  // car 0x338f hi nibble speed lo nibble rpm car 0x3390 widened wire ticks hint
    int16_t hint = 0;
};

/// car 0x3374 mailbox one deep push evicts pending sample first
struct MotionMailbox {
    MotionSample sample;
    bool present = false;   // car 0x3394 sample present flag stays set after the first push
};

/// pack x y z 8 byte form FUN 0x0044e7f0 low32 high32
uint64_t motion_pack_vec3(float x, float y, float z);

/// unpack 8 byte form inverse motion pack vec3 FUN 0044E7F0
void motion_unpack_vec3(uint64_t packed, float outXYZ[3]);

/// car fields motion send 0x40 0x49beb0 reads build 28 byte
struct MotionSendInputs {
    float pos[3] = {0.0f, 0.0f, 0.0f};  // car 0x3244 0x3248 0x324c car 0x3238 0x323c 0x3240
    float vel[3] = {0.0f, 0.0f, 0.0f};
    float yawDeg = 0.0f;  // car 0x3220 car 0x35ac subtracted from yaw before the byte
    float driftGaugeSmoothed = 0.0f;
    float driftGauge = 0.0f;  // car 0x35a8 the high nibble is the gauge plus 45 times 15 over 90 car 0x32f8
    float rpm = 0.0f;
    float frameDt = 0.0f;  // game 0x0034 seconds car 0x3300 nonzero and 0x3304 zero
    bool miniTurboBoost = false;
    bool itemBoost = false;  // car 0x3300 nonzero and 0x3304 nonzero car 0x332d equal to 1
    bool reverse = false;
    bool drift = false;  // car 0x35a4 nonzero car 0x35f0 equal to 1
    bool miniTurboStage1 = false;
    int turnState = 0;                        // car 0xa78e4 0 1 or 2
};

/// port motion send 0x40 0x49beb0 builds struct no socket
MotionSend0x40 motion_send_0x40(const MotionSendInputs& in);

/// 100 ms send timer dword 0x1396e90 returns true advances lastSendMs
bool motion_send_timer_fired(int64_t nowMs, int64_t& lastSendMs);

/// net motion pack 0x40 0x4818a0 normal path 19 bytes pos yaw
std::vector<uint8_t> net_motion_pack_0x40(const MotionSend0x40& send);

/// net motion pack 0x40 anti hook path 28 bytes FUN 0044E9C0
std::vector<uint8_t> net_motion_pack_0x40_raw(const MotionSend0x40& send);

/// net motion recv 0x40 0x47fd30 normal 1 plus 25 times n
std::vector<MotionRecvEntry> net_motion_recv_0x40(const uint8_t* data, size_t len);

/// net motion recv 0x40 anti hook 1 plus 34 times n
std::vector<MotionRecvEntry> net_motion_recv_0x40_raw(const uint8_t* data, size_t len);

/// net motion sample push 0x4a4260 evicts pending then stores new
void net_motion_sample_push(MotionMailbox& mailbox, const MotionSample& sample);

/// net motion sample pop 0x4a42b0 returns false leaves out untouched
bool net_motion_sample_pop(MotionMailbox& mailbox, MotionSample& out);

/// net motion queue shift 0x4a4200 drops pending sample mailbox depth
void net_motion_queue_shift(MotionMailbox& mailbox);

/// net motion queue clear 0x4a41f0 zeroes the count at queue 0x28 car 0x3370 the pending sample is gone
void net_motion_queue_clear(MotionMailbox& mailbox);

/// net player id to slot 0x48dea0 scans 30 slots car 0x743
int net_player_id_to_slot(const int32_t* slotIds, const uint8_t* slotActive, int slotCount, int32_t playerId);

}
