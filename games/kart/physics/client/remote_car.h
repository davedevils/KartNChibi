/// remote car mover port car remote update 0x49ed90 from REMOTE AND INPUT
#pragma once

#include "constants.h"
#include "motion_packet.h"
#include "world_collision.h"

#include <array>
#include <cstdint>
#include <vector>

namespace KnC::Kart::Client {

/// ghost or replay sample car 0x3754 ring stride 0x1c 24000 deep wraps at 2400 outside race mode 0xd
struct GhostSample {
    float pos[3] = {0.0f, 0.0f, 0.0f};  // ring entry 0x00 0x04 0x08 x y ground z height ring entry 0x0c
    uint8_t yawByte = 0;
    uint16_t flags = 0;  // ring entry 0x10 boost reverse drift stage turn bits ring entry 0x14 low nibble drift gauge hi nibble rpm
    uint8_t nibbles = 0;
    uint8_t inputMask = 0;             // ring entry 0x18 slots 0 1 2 3 5 4 7 on bits 7 6 5 4 3 2 1
};

/// ring two indices record blend tick counter the memory holds the race mode 0xd depth
struct GhostRing {
    std::vector<GhostSample> samples = std::vector<GhostSample>(static_cast<size_t>(kGhostRingWrapMode0xD));
    int writeIndex = 0;  // car 0xa7858 car 0xa785c
    int readIndex = 0;
    int tickCounter = 0;  // car 0xa7864 ticks since the last write or blend step
};

/// remote mover car fields reads writes struct per car offsets
struct RemoteCarState {
    // position and motion car 0x3244 0x3248 0x324c
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    float velX = 0.0f, velY = 0.0f, velZ = 0.0f;  // car 0x3238 0x323c 0x3240 car 0x3220
    float yawDeg = 0.0f;
    float yawRateLean = 0.0f;  // car 0x32e4 cosmetic turn state lean car 0x32f8
    float rpm = 0.0f;

    // drift and boost car 0x35a8
    float driftGauge = 0.0f;
    float driftGaugeSmoothed = 0.0f;  // car 0x35ac car 0x35a4 0 none 1 or 2 while remote
    int driftState = 0;
    int miniTurboStage = 0;  // car 0x35f0 car 0x3300 0 idle else active
    int boostState = 0;
    int boostKind = 0;  // car 0x3304 0 miniturbo 1 item car 0x332d
    uint8_t reverseFlag = 0;
    int turnState = 0;                // car 0xa78e4 0 1 or 2

    // active effect mirrored from car record tick owner copies before update car 0x36a8 code 300 pins position to snapshot
    int effectCode = 0;
    float effectWobble = 0.0f;  // car 0x36ac added to the yaw before the ease unless code 300 car 0x36d0 0x36d4 0x36d8
    float effectSnapX = 0.0f, effectSnapY = 0.0f, effectSnapZ = 0.0f;
    float effectEndCondition = 0.0f;  // car 0x36c8 added to the snapshot z for code 300

    // interpolation anchor chain car 0xa7884 0xa7888 0xa788c
    float anchorStart[3] = {0.0f, 0.0f, 0.0f};
    float anchorTarget[3] = {0.0f, 0.0f, 0.0f};  // car 0xa7890 0xa7894 0xa7898 car 0xa789c 0xa78a0 0xa78a4
    float lastKnownPos[3] = {0.0f, 0.0f, 0.0f};
    int mismatchCount = 0;  // car 0xa78a8 car 0xa78ac
    float divergenceAccum = 0.0f;

    float posEaseCountdown = 1.0f;  // car 0x33a0 car 0x339c
    float yawEaseCountdown = 1.0f;

    int watchdogState = 0;  // car 0xa78c0 0 1 or 2 car 0xa78c8
    int64_t watchdogSavedMs = 0;
    int64_t watchdogThresholdMs = 0;    // car 0xa78c4 the i16 delay of S2C 0x6A MotionBlock no default only the packet

    // collision recovery world gates through the probe context below car 0xa78b0 0 1 2 or 3
    int collisionRecoveryState = 0;
    float collisionRecoveryScale = 1.0f;  // car 0xa78b4 car 0xa78b8 the time state 2 holds against no writer found stays 0
    int64_t collisionRecoveryMs = 0;
    int64_t nowMs = 0;  // tick time the update sets for the ease car 0xa7874 one after first ground miss until next hit
    uint8_t groundLostLatch = 0;

    // status decode gate car 0x3598 against DAT 005C8320 both 0 in static image gate passes per car distance visual fills
    float lodDistance = 0.0f;
    float lodGate = 0.0f;       // DAT 005C8320 a runtime threshold 0 in the static image

    // anchor hint debug knob no writer in exe both stay 0 DAT 00B23154 over 0 subtracts offset from wire hint
    int32_t hintDebugEnable = 0;
    float hintDebugOffset = 0.0f;  // DAT 00B23168 the offset truncated to ticks

    // the mailbox and the ever received flag car 0x3348 the queue object car 0x3374 the popped sample
    MotionMailbox mailbox;
    bool everReceivedSample = false;  // car 0x3398 stands in for car 0x3394 once set car 0x338c to 0x3390 kept after pop for yaw ease
    MotionSample lastSample;

    // the ghost or replay ring car 0x3754 0xa7858 0xa785c 0xa7864
    GhostRing ghost;

    // frame time seconds set by the caller before each tick game 0x0034
    float frameDt = 0.0f;

    // world the ground snap reads tick owner sets both before update null skips snap car 0x3608 probe context of car
    BspQuery* groundQuery = nullptr;
    const ColTrack* track = nullptr;   // the loaded pieces for the edge vertices
};

/// sample apply first sample init divergence mismatch 0x49ed90 mailbox
void car_remote_sample_apply(RemoteCarState& car, const MotionSample& sample);

/// per tick ease pos 50 tick yaw 6 tick 0x49ed90 with the ground snap when the probe is set
void car_remote_ease_tick(RemoteCarState& car);

/// pop mailbox pending run per tick ease call once
void car_remote_update_tick(RemoteCarState& car, int64_t nowMs);

/// net remote watchdog 0x49ecd0 coast sample threshold 700 ms
void net_remote_watchdog(RemoteCarState& car, int64_t nowMs);

/// net remote watchdog arm 0x49e8a0 S2C 0x6A sets the delay state 1 and the saved time
void net_remote_watchdog_arm(RemoteCarState& car, int64_t nowMs, int32_t delayMs);

/// car remote coast extrapolate 0x49eae0 dead reckon 8 steps
void car_remote_coast_extrapolate(RemoteCarState& car);

/// car ghost sample record 0x49fad0 every 10 ticks or every tick in mode 0xd true when the ring wrapped
bool car_ghost_sample_record(RemoteCarState& car, bool isRaceMode0xD, uint8_t inputMask);

/// car ghost sample apply 0x49ff90 blends next ring 10 ticks
void car_ghost_sample_apply(RemoteCarState& car, bool isRaceMode0xD);

}
