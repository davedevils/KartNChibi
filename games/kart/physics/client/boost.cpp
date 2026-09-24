#include "boost.h"

#include <cmath>

#include "constants.h"
#include "gimmicks.h"
#include "math_helpers.h"

namespace KnC::Kart::Client {

namespace {
float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
} // namespace

void car_apply_engine_force(CarState& car, float x, float y, float z) {
    // car apply engine force 0x4968F0 z reaches body apply force untouched session flag game 0x1397384 gates tick runs when set
    body_apply_force(car.body, -x, -y, z);
}

void math_dir_from_heading_pitch(float headingDeg, float pitchDeg, Vec3& out) {
    // math dir from heading pitch 0x44DD50 D3DX rotate z by minus heading axis row 0 result row 1 of product
    float h = headingDeg * kDegToRad;
    float p = pitchDeg * kDegToRad;
    out.x = std::sin(h) * std::cos(p);
    out.y = std::cos(h) * std::cos(p);
    out.z = std::sin(p);
}

void car_boost_push(GameState& game, int carIndex, float headingDeg, float strength) {
    // car boost push 0x496B40 heading minus 90 and car 0x3224 give the direction z is a tenth of the strength
    CarState& car = game.cars[static_cast<size_t>(carIndex)];
    Vec3 dir;
    math_dir_from_heading_pitch(headingDeg - kAxisCorrection90Deg, car.pitchDeg, dir);
    float x = dir.x * strength;
    float y = dir.y * strength;
    float z = strength * kTickDeadzoneStep;
    if (game.sessionRunning != 0) body_apply_force(car.body, -x, -y, z);
}

void car_push_along_yaw(GameState& game, int carIndex, float strength) {
    // car push along yaw 0x497160 car boost push with the car yaw at car 0x3220
    car_boost_push(game, carIndex, game.cars[static_cast<size_t>(carIndex)].yawDeg, strength);
}

void car_boost_clear(GameState& game, int carIndex) {
    // car boost clear 0x496930 zeroes car 0x3300 while the session runs and a boost is active
    CarState& car = game.cars[static_cast<size_t>(carIndex)];
    if (game.sessionRunning != 0 && car.boostState != 0) car.boostState = 0;
}

void car_boost_start(GameState& game, int carIndex, int kind, int64_t nowMs) {
    CarState& car = game.cars[carIndex];
    if (game.sessionRunning == 0 || car.body.overValidGround == 0 || car.forceRespawnFlag == 1) return;
    if (car.boostState != 0) {
        // 0x496C34 the gate is world theme is special row 0x487230 there is no camera flag in this function
        bool special = game.themeSpecialRow != 0;
        if (special && kind == car.boostKind) return;
        if (!special && kind < car.boostKind) return;
    }

    if (game.hooks.playSound) game.hooks.playSound(game.hooks.user, carIndex, 0, 1);
    game.boostStartGlobalMs = nowMs; // DAT 005C8508
    car.boostKind = kind;
    car.boostStartMs = nowMs;
    car.boostState = 1;
    game.boostHudFlag = 1;
    game.boostHudMs = nowMs;
    game.boostHudStrength = (kind == 0) ? kBoostHudKind0 : kBoostHudOther;

    // 0x496CD0 the first equipped pet of kind 0x15 pet key 30 adds 0 04 to the decay strength
    float partBonus = (pet_equipped_kind(game.pets) == kPetKindBoostDecay) ? kBoostPartBonusScale : 0.0f;

    switch (kind) {
        case BOOST_KIND_CANCEL:
            car.boostDecayStrength = 1.0f;
            car.boostTargetKmh = 1.0f;
            break;
        case BOOST_KIND_MINI_TURBO: {
            // 0x496D2B wire stat 3 car 0x3454 times 0 2 plus one clamped 1 to 1 2 times 120 kmh
            float stat3 = stat_total(car.stats, KartStatIndex::MiniTurboTargetSpeed);
            float scale = clampf(stat3 * kGearSolverDtBlendA + kOne, kBoostMiniTurboSpeedScaleFloor,
                                  kBoostMiniTurboSpeedScaleCeil);
            car.boostDecayStrength = kBoostMiniTurboDecayStrength;
            car.boostTargetKmh = scale * kBoostSpeedGateKmh;
            break;
        }
        case 1:
            car.boostTargetKmh = kBoostKind1TargetKmh;
            car.boostDecayStrength = kOne + partBonus;
            break;
        case 2:
            car.boostTargetKmh = kBoostKind2TargetKmh;
            car.boostDecayStrength = kOne + partBonus;
            break;
        case 3:
        case 5:
        case 7:
            car.boostTargetKmh = kBoostItemTargetKmh;
            car.boostDecayStrength = kOne + partBonus;
            break;
        default:
            break; // kind 4 and every other kind leave the decay and the target untouched 0x496DBD
    }

    if (car.boostTargetKmh < car.speedKmh + kAirborneLandingGate) {
        car.boostTargetKmh = car.speedKmh + kAirborneLandingGate;
    }
}

void car_boost_update(GameState& game, int carIndex, int64_t nowMs) {
    if (game.sessionRunning == 0) return;
    CarState& car = game.cars[carIndex];
    int code = car.effect.activeCode;
    if (code == 500 || code == 600 || code == 900) {
        return; // 0x496E6E the update skips these codes the apply of each already cleared the boost
    }

    if (car.boostState == 0) {
        // 0x496E8C game 0x5C and 0x60 decay by 0 86 floor 0 under 0 1 then reach three scene nodes each
        for (float& fade : game.boostNodeFade) {
            fade *= kBoostSuspensionDecay;
            if (fade < kTickDeadzoneStep) fade = 0.0f;
        }
    }

    if (car.boostState == 1) {
        if (car.speedKmh < car.boostTargetKmh) {
            // 0x496FB0 push along yaw minus the smoothed gauge times 1 8 with the decay strength
            car_boost_push(game, carIndex, car.yawDeg - car.driftGaugeSmoothed * kBoostPushYawScale,
                           car.boostDecayStrength);
        }
        // 0x496FDB the first equipped pet of kind 0x16 pet key 40 adds 500 ms to kind 1 and 2
        int partBonusMs = (pet_equipped_kind(game.pets) == kPetKindBoostDuration) ? kBoostDurationPartBonusMs : 0;
        int durationMs = 0;
        switch (car.boostKind) {
            case BOOST_KIND_MINI_TURBO: {
                // 0x497065 wire stat 3 plus one clamped 1 to 2 times 400 ms truncated
                float scale = stat_total(car.stats, KartStatIndex::MiniTurboTargetSpeed) + kOne;
                if (scale < kOne) scale = kOne;
                if (scale > kStatClampCeilTwo) scale = kStatClampCeilTwo;
                durationMs = static_cast<int>(scale * kBoostDurationKind0ScaleMs);
                break;
            }
            case 1: durationMs = kBoostDurationKind1Ms + partBonusMs; break;
            case 2: durationMs = kBoostDurationKind2Ms + partBonusMs; break;
            case 3: durationMs = kBoostDurationKind3Ms; break;
            case 4: durationMs = kBoostDurationKind4Ms; break;
            case 5: durationMs = kBoostDurationKind5Ms; break;
            case 7: durationMs = kBoostDurationKind7Ms; break;
            case BOOST_KIND_CANCEL:
                durationMs = kBoostDurationKind6Ms;
                if (game.sessionRunning != 0 && car.boostState != 0) car.boostState = 0;
                break;
            default: durationMs = 0; break;
        }
        if (nowMs - car.boostStartMs > durationMs) {
            car.boostStartMs = nowMs;
            car.boostState = 2;
        }
    } else if (car.boostState == 2) {
        car.boostDecayStrength *= kBoostSuspensionDecay;
        if (car.boostDecayStrength <= kOne) { // 0x497176 the pair of flags reads at or under one
            car.boostStartMs = nowMs;
            car.boostState = 3;
        }
    } else if (car.boostState == 3) {
        car_boost_clear(game, carIndex); // 0x4971A9
    }
}

void car_boost_speed_gate(GameState& game, int carIndex, int64_t nowMs) {
    if (carIndex != game.localCarIndex) return; // local car only
    CarState& car = game.cars[carIndex];
    (void)nowMs;
    // HUD latch and camera shake state scene and UI calls out of scope hook stands in
    if (car.boostState != 0 && car.boostKind != 0 && car.speedKmh > kBoostSuspensionDecay) {
        if (game.hooks.playSound) game.hooks.playSound(game.hooks.user, carIndex, 1, 2);
    }
}

float car_draft_factor(GameState& game, int carIndex, float maxDistance, float coneHalfAngleDeg) {
    const CarState& self = game.cars[static_cast<size_t>(carIndex)];
    float bestDist = maxDistance;
    bool found = false;

    for (int i = 0; i < kCarSlotCount; ++i) {
        if (i == carIndex) continue;
        const CarState& other = game.cars[static_cast<size_t>(i)];
        if (other.slotOccupied == 0) continue;

        float dz = self.posZ - other.posZ;
        if (dz < kDraftMinZ || dz > -kDraftMinZ) continue;

        float bearing = math_atan2_deg(self.posX - other.posX, self.posY - other.posY);
        float angle = bearing - self.yawDeg - kAxisCorrection90Deg;
        math_wrap_angle_360(&angle);

        bool qualifies = (angle <= coneHalfAngleDeg) || (angle >= kCollisionAngleWrap360 - coneHalfAngleDeg);
        if (!qualifies) continue;

        float dx = self.posX - other.posX;
        float dy = self.posY - other.posY;
        float dist = math_hypot2d(dx, dy);
        if (dist < bestDist) {
            bestDist = dist;
            found = true;
        }
    }

    if (!found) return 0.0f;
    float factor = kOne - bestDist / maxDistance;
    return clampf(factor, kZero, kOne);
}

} // namespace KnC Kart Client
