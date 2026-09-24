#include "drift.h"

#include <cmath>
#include <cstdlib>

#include "boost.h"
#include "constants.h"
#include "effects.h"
#include "gimmicks.h"
#include "math_helpers.h"
#include "tick.h"

namespace KnC::Kart::Client {

namespace {
float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
} // namespace

void car_body_set_yaw(CarState& car, float yawDeg) {
    // car body set yaw 0x49A970 R is Rz 360 minus yaw times Rx pitch times the axes transposed
    float pitchRad = car.pitchDeg * kDegToRad;
    float yawRad = (kCollisionAngleWrap360 - yawDeg) * kDegToRad;
    float cz = std::cos(yawRad);
    float sz = std::sin(yawRad);
    float cx = std::cos(pitchRad);
    float sx = std::sin(pitchRad);
    Mat3 rz;
    body_mat3_set(rz, cz, -sz, 0.0f, sz, cz, 0.0f, 0.0f, 0.0f, kOne);
    Mat3 rx;
    body_mat3_set(rx, kOne, 0.0f, 0.0f, 0.0f, cx, -sx, 0.0f, sx, cx);
    Mat3 euler;
    body_mat3_multiply(euler, rz, rx); // body mat3 from euler zyx 0x4ED2B0 y turn zero orientation matrix 0x4F1A10 R is principal axes transposed Q follows R
    WheelSet& w = car.body.wheels;
    Mat3 axesT;
    body_mat3_transpose_copy(axesT, w.principalAxes);
    body_mat3_multiply(w.orientationR, euler, axesT);
    body_mat3_to_quat(w.orientationQ, w.orientationR);
    car.body.referenceOrientation = w.orientationQ;
}

void car_drift_state_set(GameState& game, int carIndex, int newState, int64_t nowMs) {
    CarState& car = game.cars[carIndex];
    if (newState == 0) {
        car.driftState = 0;
        car.miniTurboStage = 0;
        // car 0x3600 is only ever written 0 so this release boost never fires the mini turbo is stage 3
        if (car.gaugeThresholdCount > 1 && car.boostState == 0) {
            car_boost_start(game, carIndex, BOOST_KIND_MINI_TURBO, nowMs);
        }
        return;
    }
    // 0x49AA20 car 0x6BC must be 0 free driving no respawn 100 to 201 no boost pad hold minus 1
    if (car.trackProgress != 0) return;
    bool changed = car.driftState != newState;
    car.driftState = newState;
    car.miniTurboStage = 0;
    car.gaugeThresholdCount = 0;
    // 0x49AA53 time now ms the same clock the update reads the port passes the tick clock for both
    car.miniTurboTimestampMs = nowMs;
    if (changed) {
        // pokes a scene node effect and the drift changed latch game 0x13971a4
        if (game.hooks.playSound) game.hooks.playSound(game.hooks.user, carIndex, 0, 0);
        game.driftChangedLatch = 1;
    }
}

void car_drift_update(GameState& game, int carIndex, int64_t nowMs) {
    // car drift update 0x49AA90 the gauge the unwinding turn the mini turbo and the steering gains
    CarState& car = game.cars[carIndex];
    float dt = car.frameDt;
    float kmh = car.speedKmh;             // car 0x32F4 speed car 0x332D reverse flag
    uint8_t reverse = car.reverseFlag;
    float tickScale = dt * kDriftGaugeTickScale;

    // 0x49AAC0 the drift key on slot 5 or the game 0xB0 latch arms the drift
    bool armed = game.driftKeyHeld != 0 || game.driftArmLatch != 0;
    // 0x49AB0A the cell under wheel 0 named FLY at 0x5A6AA4 disarms the drift so does an active effect
    if (car_node_name_is(game, carIndex, 0, "FLY")) armed = false;
    if (car.effect.activeCode != 0) armed = false;

    // 0x49AB3F raw slot 2 is 0x25 slot 3 is 0x27 swap at 0x49ABA6 poll agrees game 0x24 is left key
    int steerKey = 0;
    if (car.input.steerLeft != 0) steerKey = VK_STEER_LEFT_DIRECT;
    else if (car.input.steerRight != 0) steerKey = VK_STEER_RIGHT_DIRECT;

    float slowedScale = car_is_slowed(game.slowedTable, carIndex) ? kSmoothQuarter : kOne;

    // 0x49AC11 gauge rate is wire stat 8 car 0x3468 times half plus 0 3 clamped 0 3 to 0 8
    float rate = stat_total(car.stats, KartStatIndex::DriftChargeRate) * kHalf + kDriftChargeFloorScale;
    if (rate < kDriftChargeFloorScale) rate = kDriftChargeFloorScale;
    else if (rate > kStatGaugeNormalize) rate = kStatGaugeNormalize;

    bool unwind = false;
    if (reverse == 0) {
        if (kmh > kDriftMinSpeed && armed) {
            int state = car.driftState;
            if (state == 0) {
                // 0x49ACAA zeroes car 0x3604 which nothing reads the state set zeroes car 0x3600 itself
                if (steerKey == VK_STEER_LEFT_DIRECT) {
                    car_drift_state_set(game, carIndex, 1, nowMs);
                    game.driftStartBoost = kOne;
                } else if (steerKey == VK_STEER_RIGHT_DIRECT) {
                    car_drift_state_set(game, carIndex, 2, nowMs);
                    game.driftStartBoost = kOne;
                }
            } else if (state == 1) {
                if (steerKey == VK_STEER_LEFT_DIRECT) {
                    car.driftGauge += rate * slowedScale * tickScale;
                    if (car.driftGauge > kDriftGaugeCap) car.driftGauge = kDriftGaugeCap;
                } else {
                    car.driftGauge -= tickScale * kDriftGaugeReleaseDecay;
                    if (car.driftGauge < kZero) car_drift_state_set(game, carIndex, 0, nowMs);
                }
            } else if (state == 2) {
                if (steerKey == VK_STEER_RIGHT_DIRECT) {
                    car.driftGauge -= rate * slowedScale * tickScale;
                    if (car.driftGauge < -kDriftGaugeCap) car.driftGauge = -kDriftGaugeCap;
                } else {
                    car.driftGauge += tickScale * kDriftGaugeReleaseDecay;
                    if (car.driftGauge > kZero) car_drift_state_set(game, carIndex, 0, nowMs);
                }
            }
        }
        // 0x49ADB6 the unwinding runs under 30 kmh or when the drift is not armed
        unwind = !(kmh >= kDriftLowSpeedSkip && armed);
    } else if (reverse == 1) {
        unwind = true;
    } else {
        unwind = !(kmh >= kDriftLowSpeedSkip && armed);
    }

    if (unwind) {
        // 0x49ADD6 the gauge unwinds at 120 per second and turns the yaw with the rate
        if (car.driftGauge >= kAirborneLandingGate) {
            if (car.driftGauge > kDriftLowSpeedGaugeFloor) {
                car.driftGauge -= tickScale * kTurnRateClamp;
                if (car.driftGauge < kZero) {
                    car.driftGauge = 0.0f;
                    car_drift_state_set(game, carIndex, 0, nowMs);
                } else {
                    car.yawDeg = car.yawDeg - rate * tickScale * kTurnRateClamp;
                    car_body_set_yaw(car, car.yawDeg);
                }
            }
        } else {
            car.driftGauge += tickScale * kTurnRateClamp;
            if (car.driftGauge > kZero) {
                car.driftGauge = 0.0f;
                car.driftState = 0;
                car.miniTurboStage = 0;
                if (car.gaugeThresholdCount > 1 && car.boostState == 0) {
                    car_boost_start(game, carIndex, BOOST_KIND_MINI_TURBO, nowMs);
                }
            } else {
                car.yawDeg = rate * tickScale * kTurnRateClamp + car.yawDeg;
                car_body_set_yaw(car, car.yawDeg);
            }
        }
    }

    // 0x49AEF5 0x49AF47 stage 1 thresholds one minus wire stat 10 and 11 times 0 8 clamped 0 2 to 1
    if (car.miniTurboStage == 0) {
        float frac12 = kOne - stat_total(car.stats, KartStatIndex::MiniTurboThreshold) * kMiniTurboStatToFraction;
        frac12 = clampf(frac12, kMiniTurboThresholdFloor, kMiniTurboThresholdCeil);
        float frac13 = kOne - stat_total(car.stats, KartStatIndex::MiniTurboHoldTime) * kMiniTurboStatToFraction;
        frac13 = clampf(frac13, kMiniTurboThresholdFloor, kMiniTurboThresholdCeil);
        float gaugeThreshold = frac12 * kMiniTurboStage1GaugeMul;
        float heldMs = static_cast<float>(nowMs - car.miniTurboTimestampMs);
        if (car.driftState != 0 && (car.driftGauge > gaugeThreshold || car.driftGauge < -gaugeThreshold) && armed &&
            frac13 * kMiniTurboStage1HoldMulMs < heldMs) {
            car.miniTurboStage = 1;
        }
    }
    if (car.miniTurboStage == 1 && !armed) {
        car.miniTurboStage = 2;
        car.miniTurboTimestampMs = nowMs;
    }
    if (car.miniTurboStage == 2) {
        float sinceMs = static_cast<float>(nowMs - car.miniTurboTimestampMs);
        if (game.accelKeyPressed != 0 && sinceMs < kMiniTurboStage3WindowMs) {
            car.miniTurboStage = 3;
            if (car.boostState == 0) {
                car_drift_state_set(game, carIndex, 0, nowMs);
                // 0x49B0B0 the first equipped pet of kind 0x14 rolls rand mod 100 under 3 for a kind 1 boost
                bool chai = false;
                if (pet_equipped_kind(game.pets) == kPetKindChai) {
                    int roll = std::rand() % 100;
                    chai = roll >= 0 && roll < kPetChaiChancePercent;
                }
                car_boost_start(game, carIndex, chai ? 1 : BOOST_KIND_MINI_TURBO, nowMs);
            }
        }
    }

    // 0x49B0F1 the steering gains at car 0x2974 0x2A78 0x2A7C and the smoothed gauge
    int st = car.driftState;
    float gainScale = kOne;
    float sideScale = kOne;
    float tau = kDriftSmoothTauIdle;
    if (st == 0) {
        if (reverse == 1) gainScale = kHalf;
    } else {
        tau = kDriftSmoothTauActive;
        if (st == 1 && steerKey == VK_STEER_RIGHT_DIRECT) sideScale = kHalf;
        else if (st == 2 && steerKey == VK_STEER_LEFT_DIRECT) sideScale = kHalf;
        else {
            sideScale = kStatClampCeilTwo;
            gainScale = kStatClampCeilTwo;
        }
    }
    // 0x49B1B2 wire stat 2 car 0x3450 times 3 plus one clamped 1 to 4 the steering gain
    float steerStat = stat_total(car.stats, KartStatIndex::SteeringGain) * kSteerScaleThree + kOne;
    steerStat = clampf(steerStat, kOne, kSteerGainClampCeil);
    // car 0x2F34 is the catalogue max steer degrees 0x2974 is wheel set 0x518 the max steer radians
    car.steerAngleDeg = car.body.catalogue.maxSteerDeg;
    car.steeringGainYaw = car.steerAngleDeg * kDegToRadFactorA * kDegToRadFactorB * gainScale * sideScale;
    car.body.wheels.maxSteerRad = car.steeringGainYaw;
    float gain = steerStat * car.steeringScale * gainScale * slowedScale;
    car.steeringGainB = gain;
    car.steeringGainA = gain;
    // car 0x2A78 and 0x2A7C are the steer pair of the material rates the spring channels 2 and 3 read
    car.body.wheels.materialPairRates[2] = gain;
    car.body.wheels.materialPairRates[3] = gain;
    car.driftGaugeSmoothed = (car.driftGauge - car.driftGaugeSmoothed) / tau + car.driftGaugeSmoothed;
    if (st != 0) {
        // 0x49B24D wire stat 9 car 0x346C times 0 6 plus 1 2 clamped 1 2 to 1 8
        float driftSteer = stat_total(car.stats, KartStatIndex::DriftSteer) * kDriftSteerScale + kDriftSteerClampFloor;
        driftSteer = clampf(driftSteer, kDriftSteerClampFloor, kDriftSteerClampCeil);
        Vec3 steerVec;
        body_vec3_set(steerVec, 0.0f, 0.0f, driftSteer * car.driftGauge * slowedScale * kDegToRad);
        // 0x49B2B3 the this is car 0x263C wheel set 0x1E0 the spin in the principal frame a z spin per tick
        body_vec3_add(car.body.wheels.angularVelocity, steerVec);
    }

    // 0x49B1D0 a released drift near zero or effects 500 600 900 clear the drift in place
    bool released = game.driftKeyHeld == 0 && st != 0 && car.driftGauge < kHalf && car.driftGauge > kDriftResidualGaugeFloor;
    int code = car.effect.activeCode;
    if (released || code == 500 || code == 600 || code == 900) {
        car.driftState = 0;
        car.miniTurboStage = 0;
        if (car.gaugeThresholdCount > 1 && car.boostState == 0) {
            car_boost_start(game, carIndex, BOOST_KIND_MINI_TURBO, nowMs);
        }
    }

    // 0x49B270 the stuck flag turns the yaw with the gains game 0x20 up to the right 0x24 down
    if (game.stuckWatch.stuckFlag == 1) {
        float yaw = car.yawDeg;
        if (car.input.steerLeft == 0) {
            if (car.input.steerRight == 0) return;
            yaw = dt * kStuckYawTurnRate * car.steeringGainA + yaw;
        } else {
            yaw = yaw - dt * kStuckYawTurnRate * car.steeringGainB;
        }
        car.yawDeg = yaw;
        car_body_set_yaw(car, yaw);
    }
}

} // namespace KnC Kart Client
