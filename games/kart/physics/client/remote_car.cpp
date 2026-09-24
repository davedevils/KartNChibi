#include "remote_car.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "body.h"
#include "math_helpers.h"

namespace KnC::Kart::Client {

namespace {

// named constant read memory KnC exe raw 0x5a6b20 yaw byte to degrees
constexpr float kYawByteToDeg = 360.0f / 255.0f;
constexpr float kYawDegToByte = 0.70833331f;  // 0x5a6abc 255 over 360 record and coast yaw byte scale 0x5a3230 drift nibble spans 15 over twice gauge cap
constexpr float kGhostNibbleSpan = 15.0f;
constexpr float kGhostRpmFloor = 1000.0f;  // 0x5a3bf4 rpm below this records nibble 0 0x5a6ab8 rpm above the floor clamps here
constexpr float kGhostRpmCeil = 9000.0f;
constexpr float kGhostRpmToNibble = 0.0016666667f;  // 0x5a6ab4 1 over 600 the rpm nibble scale 0x5a6b18 rpm nibble to units factor
constexpr float kRpmNibbleScale = 600.0f;
constexpr float kEaseBlend = 0.125f;  // 0x5a32ac rpm and lean per tick blend 0x5a6acc turn state cosmetic lean plus or minus
constexpr float kTurnLean = 0.5236f;
constexpr float kDivergenceThreshold = 0.5f;  // 0x59f414 mismatch counter distance factor 0x5a3bf4 anchor ease scale over the hint
constexpr float kHintDivisor = 1000.0f;
constexpr float kRecoveryGrowCap = 4.0f;  // 0x5a0054 collision recovery state 1 to 2 cap 0x5a69e4 coast magnitude decay per step
constexpr float kCoastDecay = 0.94f;
constexpr float kCoastStartMag = 9.4f;  // 0x41166666 immediate at 0x49eb17 the coast start magnitude 0x59f450 predicted position lead factor
constexpr float kPredictDtScale = 50.0f;
constexpr float kGhostBlendStep = 0.1f;  // 0x5a2494 ghost apply per tick blend 0x5a164c the smoothed gauge scale in the coast yaw byte
constexpr float kGaugeYawScale = 0.6f;
constexpr float kPushGaugeYawScale = 1.2f;        // 0x5a32b0 the smoothed gauge scale of the PUSH fan heading

// math wrap angle 360 0x44d9c0 folds into 0 to 360
void wrap_angle_360(float& deg)
{
    deg = std::fmod(deg, 360.0f);
    if (deg < 0.0f) deg += 360.0f;
}

// math wrap angle signed180 0x44dd00 folds minus 180 to 180
void wrap_angle_signed180(float& deg)
{
    wrap_angle_360(deg);
    if (deg > 180.0f) deg -= 360.0f;
}

// math point along heading 0x44dec0 the point dist along the heading from x y
void point_along_heading(float x, float y, float dist, float headingDeg, float& outX, float& outY)
{
    float a = -(headingDeg - kAxisCorrection90Deg) * kDegToRad;
    outX = std::cos(a) * dist + x;
    outY = std::sin(a) * dist + y;
}

// bit7 miniturbo bit6 item bit5 reverse bit3 stage1 bit2 bit1 turn shared by both decoders
void decode_boost_reverse_turn(RemoteCarState& car, uint8_t bits)
{
    if ((bits & 0x80) != 0) {
        car.boostState = 1;
        car.boostKind = 0;
    } else if ((bits & 0x40) != 0) {
        car.boostState = 1;
        car.boostKind = 1;
    } else {
        car.boostState = 0;
    }
    car.reverseFlag = static_cast<uint8_t>((bits >> 5) & 1);
    car.miniTurboStage = (bits >> 3) & 1;
    if ((bits & 0x04) != 0) car.turnState = 1;
    else if ((bits & 0x02) != 0) car.turnState = 2;
    else car.turnState = 0;
}

void ease_turn_lean(RemoteCarState& car)
{
    float leanTarget = 0.0f;
    if (car.turnState == 1) leanTarget = kTurnLean;
    else if (car.turnState == 2) leanTarget = -kTurnLean;
    float leanDelta = leanTarget - car.yawRateLean;
    wrap_angle_signed180(leanDelta);
    car.yawRateLean += leanDelta * kEaseBlend;
}

// 0x49fb69 and 0x49ec13 the yaw byte source wrap 360 of the yaw times 255 over 360 truncated
uint8_t encode_yaw_byte(float yawDeg)
{
    wrap_angle_360(yawDeg);
    return static_cast<uint8_t>(static_cast<int>(yawDeg * kYawDegToByte) & 0xFF);
}

// 0x49fba1 and 0x49ec18 the high nibble is the gauge plus the cap times 15 over twice the cap truncated
uint8_t encode_gauge_nibble(float gauge)
{
    float scaled = (gauge + kDriftGaugeCap) * (kGhostNibbleSpan / (kDriftGaugeCap + kDriftGaugeCap));
    return static_cast<uint8_t>(static_cast<int>(scaled) & 0xF);
}

// 0x49fbd9 and 0x49ec3d the low nibble is rpm minus 1000 clamped 0 to 9000 over 600 truncated
uint8_t encode_rpm_nibble(float rpm)
{
    float rpmOver = rpm - kGhostRpmFloor;
    if (rpmOver < 0.0f) rpmOver = 0.0f;
    else if (rpmOver > kGhostRpmCeil) rpmOver = kGhostRpmCeil;
    return static_cast<uint8_t>(static_cast<int>(rpmOver * kGhostRpmToNibble) & 0xF);
}

// 0x49f2c8 the ground snap of the eased position the PUSH fan and the wall slide need the probe
void remote_ground_snap(RemoteCarState& car, float oldX, float oldY, float oldZ)
{
    BspQuery& ctx = *car.groundQuery;
    float ground[3] = {car.posX, car.posY, car.posZ};
    bool onGround = world_ground_height_at(ctx, ground);
    bool fell = !onGround;
    if (onGround) {
        // 0x49f30f z floors at the plane plus 0 34 a falling anchor over 0 8 above it snaps
        if (car.posZ < kRemoteGroundSnapBand + ground[2] + kRemoteGroundClearance) {
            car.posZ = ground[2] + kRemoteGroundClearance;
        }
        if (car.anchorTarget[2] < car.anchorStart[2] && car.anchorTarget[2] - ground[2] > kRemoteAnchorDropGate) {
            car.anchorTarget[2] = ground[2];
        }
        // 0x49f37d a PUSH cell fans 0 to 30 ahead in 0 5 steps any miss counts as a fall
        const char* name = world_query_surface_name(ctx);
        if (name != nullptr && std::strcmp(name, "PUSH") == 0) {
            float heading = car.yawDeg - car.driftGaugeSmoothed * kPushGaugeYawScale;
            for (float d = 0.0f; d < kRemotePushScanRange; d += kRemotePushScanStep) {
                float px = 0.0f;
                float py = 0.0f;
                point_along_heading(car.posX, car.posY, d, heading, px, py);
                if (!world_ground_test_point(ctx, px, py)) {
                    fell = true;
                    break;
                }
            }
        }
        if (!fell) car.groundLostLatch = 0;
    }
    if (fell) {
        // 0x49f4fc the first miss slides the car back along the blocking edge toward the anchor
        if (car.groundLostLatch == 0) {
            car.groundLostLatch = 1;
            const ColEdge* edge = ctx.lastEdge;
            const ColPiece* piece = ctx.piece;
            if (edge != nullptr && piece != nullptr) {
                Vec3 pA;
                Vec3 pB;
                body_get_shape_point(*piece, pA, static_cast<int>(edge->reserved & 0xffffu));
                body_get_shape_point(*piece, pB, static_cast<int>((edge->reserved >> 16) & 0xffffu));
                Vec3 normal{pB.y - pA.y, pA.x - pB.x, pA.z - pB.z};
                body_vec3_normalize_d3dx(normal);
                float refX = normal.x * kRemoteEdgeSlideReach + car.posX;
                float refY = normal.y * kRemoteEdgeSlideReach + car.posY;
                float bearingNow = math_atan2_deg(car.posX - refX, car.posY - refY);
                float bearingOld = math_atan2_deg(oldX - refX, oldY - refY);
                float slideBearing = (bearingNow - bearingOld) + bearingNow;
                float toAnchorX = car.anchorTarget[0] - car.posX;
                float toAnchorY = car.anchorTarget[1] - car.posY;
                float slideLength = math_hypot2d(toAnchorX, toAnchorY) * kRemoteEdgeSlideScale;
                float slideStep = slideLength * car.frameDt;
                car.posX = oldX;
                car.posY = oldY;
                car.posZ = oldZ;
                // 0x49f6a0 the car backs 0 2 along the bearing then the anchor walks the slide while on ground
                float bx = 0.0f;
                float by = 0.0f;
                point_along_heading(car.posX, car.posY, kRemoteEdgeSlideStep, bearingNow, bx, by);
                car.posX = bx;
                car.posY = by;
                if (slideLength > 0.0f && slideStep > 0.0f) {
                    for (float d = 0.0f; d < slideLength; d += slideStep) {
                        float px = 0.0f;
                        float py = 0.0f;
                        point_along_heading(car.posX, car.posY, d, slideBearing, px, py);
                        if (!world_ground_test_point(ctx, px, py)) break;
                        car.anchorTarget[0] = px;
                        car.anchorTarget[1] = py;
                    }
                }
            }
        }
        // 0x49f78c recovery state 0 arms state 1 off the mesh or more than one frame above the plane
        if (car.collisionRecoveryState == 0) {
            float probe[3] = {car.posX, car.posY, car.posZ};
            if (!world_ground_height_at(ctx, probe) || car.posZ - probe[2] > car.frameDt) {
                car.collisionRecoveryState = 1;
            }
        }
    }
}

} // namespace

void car_remote_sample_apply(RemoteCarState& car, const MotionSample& sample)
{
    if (!car.everReceivedSample) {
        car.posEaseCountdown = 1.0f;
        car.yawEaseCountdown = 1.0f;
        for (int a = 0; a < 3; ++a) {
            car.anchorStart[a] = sample.pos[a];
            car.anchorTarget[a] = sample.pos[a];
        }
        car.lastKnownPos[0] = car.posX;
        car.lastKnownPos[1] = car.posY;
        car.lastKnownPos[2] = car.posZ;
        car.everReceivedSample = true;
        car.mismatchCount = 0;
    } else {
        float dx = car.posX - car.lastKnownPos[0];
        float dy = car.posY - car.lastKnownPos[1];
        float dist = std::sqrt(dx * dx + dy * dy); // math hypot2d 0x44d900 x and y only
        bool forceReanchor = false;
        if (car.divergenceAccum * kDivergenceThreshold <= dist) {
            car.mismatchCount = 0;
        } else {
            car.mismatchCount += 1;
            if (car.mismatchCount > 3) {
                forceReanchor = true;
                car.mismatchCount = 0;
            }
        }
        if (forceReanchor) {
            car.posEaseCountdown = 1.0f;
            car.yawEaseCountdown = 1.0f;
            for (int a = 0; a < 3; ++a) {
                car.anchorStart[a] = sample.pos[a];
                car.anchorTarget[a] = sample.pos[a];
            }
            net_motion_queue_clear(car.mailbox); // 0x49f18b net motion queue clear 0x4a41f0
        } else {
            int hint = sample.hint;
            // 0x49f1c3 DAT 00B23154 over 0 subtracts DAT 00B23168 truncated floored at 1 no writer sets them
            if (car.hintDebugEnable > 0) {
                hint -= static_cast<int>(car.hintDebugOffset);
                if (hint < 1) hint = 1;
            }
            car.posEaseCountdown = 50.0f;
            car.yawEaseCountdown = 6.0f;
            float deltaZ = sample.predPos[2] - sample.pos[2];
            float ease = kHintDivisor / static_cast<float>(hint);
            car.anchorStart[0] = sample.pos[0];
            car.anchorStart[1] = sample.pos[1];
            car.anchorStart[2] = sample.pos[2];
            car.anchorTarget[0] = sample.pos[0] + (sample.predPos[0] - sample.pos[0]) * ease;
            car.anchorTarget[1] = sample.pos[1] + (sample.predPos[1] - sample.pos[1]) * ease;
            car.anchorTarget[2] = sample.pos[2] + deltaZ * ease;
        }
        car.lastKnownPos[0] = car.posX;
        car.lastKnownPos[1] = car.posY;
        car.lastKnownPos[2] = car.posZ;
    }
    car.divergenceAccum = 0.0f;
    car.lastSample = sample;
}

void car_remote_ease_tick(RemoteCarState& car)
{
    if (!car.everReceivedSample) return; // nothing to ease toward yet

    // 0x49f2a8 the status decode gate car 0x3598 at or under DAT 005C8320 both 0 by default
    bool statusGate = car.lodDistance <= car.lodGate;

    if (car.posEaseCountdown > 0.0f) {
        float oldX = car.posX;
        float oldY = car.posY;
        float oldZ = car.posZ;
        float deltaZ = car.anchorTarget[2] - car.posZ;
        if (car.collisionRecoveryState != 0) {
            deltaZ *= car.collisionRecoveryScale;
        }
        float frac = 1.0f / car.posEaseCountdown;
        car.posX += frac * (car.anchorTarget[0] - car.posX);
        car.posY += frac * (car.anchorTarget[1] - car.posY);
        car.posZ += frac * deltaZ;
        car.posZ -= car.frameDt; // 0x49f2f1 subtracts the raw frame time global 0x5a3be4
        float dx = car.posX - oldX;
        float dy = car.posY - oldY;
        car.divergenceAccum += std::sqrt(dx * dx + dy * dy);
        if (statusGate && car.groundQuery != nullptr && car.groundQuery->active) {
            remote_ground_snap(car, oldX, oldY, oldZ);
        }
        car.posEaseCountdown -= 1.0f;
    }

    // 0x49f7d6 the effect wobble car 0x36ac joins the yaw before the delta unless code 300 runs
    float yawBase = car.yawDeg;
    if (car.effectCode != 300) yawBase += car.effectWobble;
    float yawByteDeg = static_cast<float>(car.lastSample.yawByte) * kYawByteToDeg;
    float yawDelta = yawByteDeg - yawBase;
    wrap_angle_signed180(yawDelta);
    car.yawDeg += yawDelta / car.yawEaseCountdown;
    wrap_angle_360(car.yawDeg);

    if (statusGate) {
        // 0x49f83c the gauge takes 90 times the unit times the nibble minus 45 then one more unit 6
        int speedNibble = car.lastSample.statusHi >> 4;
        float gauge = (kDriftGaugeCap + kDriftGaugeCap) * kRemoteGaugeNibbleUnit * static_cast<float>(speedNibble) -
                      kDriftGaugeCap;
        car.driftGauge = (kDriftGaugeCap + kDriftGaugeCap) * kRemoteGaugeNibbleUnit + gauge;
        int rpmNibble = car.lastSample.statusHi & 0xF;
        car.rpm += (static_cast<float>(rpmNibble) * kRpmNibbleScale - car.rpm) * kEaseBlend;
        decode_boost_reverse_turn(car, car.lastSample.statusLo);
        car.driftState = (car.lastSample.statusLo >> 4) & 1; // remote status word 0 or 1 only
        ease_turn_lean(car);
    }

    car.yawEaseCountdown -= 1.0f;
    if (car.yawEaseCountdown < 1.0f) car.yawEaseCountdown = 1.0f;

    if (car.collisionRecoveryState == 1) {
        car.collisionRecoveryScale *= kRemoteRecoveryGrow;
        if (car.collisionRecoveryScale > kRecoveryGrowCap) {
            car.collisionRecoveryScale = 4.0f;
            car.collisionRecoveryState = 2;
        }
    }
    if (car.collisionRecoveryState == 2) {
        // 0x49f9c4 state 2 holds while 0 64 or more over the plane inside 1501 ms else state 3
        bool hold = false;
        if (car.groundQuery != nullptr && car.groundQuery->active) {
            float probe[3] = {car.posX, car.posY, car.posZ};
            if (world_ground_height_at(*car.groundQuery, probe) && car.posZ - probe[2] >= kRemoteRecoveryHeightGate) {
                int64_t since = car.nowMs - car.collisionRecoveryMs;
                hold = since >= 0 && since < kRemoteRecoveryHoldMs;
            }
        }
        if (!hold) car.collisionRecoveryState = 3;
    }
    if (car.collisionRecoveryState == 3) {
        car.collisionRecoveryScale *= kRemoteRecoveryDecay;
        if (car.collisionRecoveryScale < 1.0f) {
            car.collisionRecoveryScale = 1.0f;
            car.collisionRecoveryState = 0;
        }
    }
}

void car_remote_update_tick(RemoteCarState& car, int64_t nowMs)
{
    net_remote_watchdog(car, nowMs);
    // 0x49edd0 effect 300 pins the position to the snapshot plus the end condition height
    if (car.effectCode == 300) {
        car.posX = car.effectSnapX;
        car.posY = car.effectSnapY;
        car.posZ = car.effectSnapZ + car.effectEndCondition;
    }
    MotionSample sample;
    if (net_motion_sample_pop(car.mailbox, sample)) {
        car_remote_sample_apply(car, sample);
    }
    car.nowMs = nowMs;
    car_remote_ease_tick(car);
}

void net_remote_watchdog(RemoteCarState& car, int64_t nowMs)
{
    if (car.watchdogState == 1) {
        if (nowMs - car.watchdogSavedMs >= car.watchdogThresholdMs) {
            car_remote_coast_extrapolate(car);
            car.watchdogSavedMs = nowMs;
            car.watchdogState = 2;
        }
    } else if (car.watchdogState == 2) {
        if (nowMs - car.watchdogSavedMs >= 700) { // the 699 to 700 literal from the decompile
            car.watchdogState = 0;
        }
    }
}

void net_remote_watchdog_arm(RemoteCarState& car, int64_t nowMs, int32_t delayMs)
{
    // net remote watchdog arm 0x49e8a0 S2C 0x6A player id then i16 delay ms
    car.watchdogThresholdMs = delayMs;
    car.watchdogState = 1;
    car.watchdogSavedMs = nowMs;
}

void car_remote_coast_extrapolate(RemoteCarState& car)
{
    net_motion_queue_clear(car.mailbox); // 0x49eb12 on the queue at car 0x3348
    float x = car.posX;
    float y = car.posY;
    float z = car.posZ;
    float mag = kCoastStartMag;
    for (int step = 0; step < 8; ++step) {
        // math dir from heading 0x44dddd yaw minus 90 gives the forward of the wire yaw
        float rad = -((car.yawDeg - kAxisCorrection90Deg) - kAxisCorrection90Deg) * kDegToRad;
        float dx = std::cos(rad) * mag;
        float dy = std::sin(rad) * mag;
        mag *= kCoastDecay;
        float dt50 = car.frameDt * kPredictDtScale;
        float newX = x + dx * dt50;
        float newY = y + dy * dt50;
        float newZ = z; // the height axis scale is always zero while coasting

        MotionSample s;
        s.pos[0] = x; s.pos[1] = y; s.pos[2] = z;
        s.predPos[0] = newX; s.predPos[1] = newY; s.predPos[2] = newZ;
        // 0x49ebe1 yaw minus the smoothed gauge times 0 6 wrapped then the same nibbles as the recorder
        s.yawByte = encode_yaw_byte(car.yawDeg - car.driftGaugeSmoothed * kGaugeYawScale);
        s.statusLo = 0x80;  // 0x49eca0 the mini turbo bit is forced on
        s.statusHi = static_cast<uint8_t>((encode_gauge_nibble(car.driftGauge) << 4) | encode_rpm_nibble(car.rpm));
        s.hint = 1000;
        net_motion_sample_push(car.mailbox, s);

        x = newX; y = newY; z = newZ;
    }
}

bool car_ghost_sample_record(RemoteCarState& car, bool isRaceMode0xD, uint8_t inputMask)
{
    car.ghost.tickCounter += 1;
    // 0x49fb0e the divider is 10 or 0 in race mode 0xd which records every tick
    int interval = isRaceMode0xD ? 0 : kGhostRecordIntervalTicks;
    if (car.ghost.writeIndex > 0 && car.ghost.tickCounter < interval) return false;
    car.ghost.tickCounter = 0;

    int wrap = isRaceMode0xD ? kGhostRingWrapMode0xD : kGhostRingWrapNormal;
    int idx = car.ghost.writeIndex % kGhostRingWrapMode0xD;
    GhostSample g;
    g.pos[0] = car.posX;
    g.pos[1] = car.posY;
    g.pos[2] = car.posZ;
    // 0x49fb69 the yaw wrapped to 360 times 255 over 360 truncated by crt ftol trunc
    g.yawByte = encode_yaw_byte(car.yawDeg);
    // 0x49fba1 gauge plus cap times 15 over twice the cap 0x49fbd9 rpm minus 1000 over 600
    g.nibbles = static_cast<uint8_t>(encode_gauge_nibble(car.driftGauge) | (encode_rpm_nibble(car.rpm) << 4));

    int flags = 0;
    if (car.boostState != 0) flags |= (car.boostKind == 0) ? 0x80 : 0x40;
    if (car.reverseFlag == 1) flags |= 0x20;
    if (car.driftState == 0) flags |= 0x10;
    if (car.driftState == 1) flags |= 0x100;
    if (car.driftState == 2) flags |= 0x200;
    if (car.miniTurboStage == 1) flags |= 0x08;
    if (car.turnState == 1) flags |= 0x04;
    else if (car.turnState == 2) flags |= 0x02;
    g.flags = static_cast<uint16_t>(flags);
    g.inputMask = inputMask;

    car.ghost.samples[static_cast<size_t>(idx)] = g;
    car.ghost.writeIndex += 1;
    if (car.ghost.writeIndex >= wrap) {
        // 0x49fec4 the caller clears the finished state car 0xa7854 the index stays past the wrap
        car.ghost.readIndex = 0;
        car.ghost.tickCounter = 0;
        return true;
    }
    return false;
}

void car_ghost_sample_apply(RemoteCarState& car, bool isRaceMode0xD)
{
    int idx = car.ghost.readIndex % kGhostRingWrapMode0xD;
    const GhostSample cur = car.ghost.samples[static_cast<size_t>(idx)];
    float yawTarget = static_cast<float>(cur.yawByte) * kYawByteToDeg;
    int driftNibbleCur = cur.nibbles & 0xF;
    float gaugeTarget =
        static_cast<float>(driftNibbleCur) * (2.0f * kDriftGaugeCap * kRemoteGaugeNibbleUnit) - kDriftGaugeCap;

    if (idx == 0) {
        car.yawDeg = yawTarget;
        car.posX = cur.pos[0];
        car.posY = cur.pos[1];
        car.posZ = cur.pos[2];
        car.driftGauge = gaugeTarget;
    } else {
        const GhostSample prev = car.ghost.samples[static_cast<size_t>(idx - 1)];
        float prevYaw = static_cast<float>(prev.yawByte) * kYawByteToDeg;
        int prevDriftNibble = prev.nibbles & 0xF;
        float prevGauge =
            static_cast<float>(prevDriftNibble) * (2.0f * kDriftGaugeCap * kRemoteGaugeNibbleUnit) - kDriftGaugeCap;

        car.posX += (cur.pos[0] - prev.pos[0]) * kGhostBlendStep;
        car.posY += (cur.pos[1] - prev.pos[1]) * kGhostBlendStep;
        car.posZ += (cur.pos[2] - prev.pos[2]) * kGhostBlendStep;

        float yawDelta = yawTarget - prevYaw;
        wrap_angle_signed180(yawDelta);
        car.yawDeg += yawDelta * kGhostBlendStep;

        float gaugeDelta = gaugeTarget - prevGauge;
        wrap_angle_signed180(gaugeDelta); // ported as written the decompile reuses the angle wrap here
        car.driftGauge += gaugeDelta * kGhostBlendStep;
    }

    int rpmNibbleCur = (cur.nibbles >> 4) & 0xF;
    car.rpm += (static_cast<float>(rpmNibbleCur) * kRpmNibbleScale - car.rpm) * kEaseBlend;
    decode_boost_reverse_turn(car, static_cast<uint8_t>(cur.flags & 0xFF));
    if ((cur.flags & 0x10) != 0) car.driftState = 0;  // ghost flags one hot across bit4 8 and 9
    if ((cur.flags & 0x100) != 0) car.driftState = 1;
    if ((cur.flags & 0x200) != 0) car.driftState = 2;
    ease_turn_lean(car);

    car.ghost.tickCounter += 1;
    int threshold = isRaceMode0xD ? 0 : 10; // matches DAT 0x00b2360c equal 0xd collapsing to zero
    if (idx == 0 || car.ghost.tickCounter >= threshold) {
        car.ghost.tickCounter = 0;
        car.ghost.readIndex = idx + 1;
        if (car.ghost.writeIndex - 1 <= car.ghost.readIndex) {
            // 0x4a0350 car boost clear 0x496930 ends the replay boost then the ring restarts
            car.boostState = 0;
            car.ghost.readIndex = 0;
            car.ghost.tickCounter = 0;
        }
    }
}

}
