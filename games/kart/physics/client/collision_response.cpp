#include "tick.h"

#include <algorithm>
#include <cmath>

#include "boost.h"
#include "constants.h"
#include "math_helpers.h"

namespace KnC::Kart::Client {

bool car_substep_collision_response(GameState& game, int carIndex, float substepDt) {
    // car substep collision response 0x498960 reverts the substep and pushes a bounce impulse
    CarState& car = game.cars[carIndex];

    car_boost_clear(game, carIndex); // 0x498983 car boost clear 0x496930

    car.yawDeg = car.prevSubstepYawDeg;
    car.posX = car.prevPosX;
    car.posY = car.prevPosY;
    car.posZ = car.prevPosZ;

    // 0x4989C3 heading from collision normal into game 0x6FC when not stuck car 0x2E34 and 0x2E38 normal x and y
    float normalX = car.body.collisionNormal.x;
    float normalY = car.body.collisionNormal.y;
    if (game.stuckWatch.stuckFlag == 0) {
        float bearing = math_atan2_deg(normalX, normalY);
        game.deflectHeadingDeg = (car.yawDeg - car.driftGaugeSmoothed * kDriftSteerClampFloor) - bearing -
                                 kAxisCorrection90Deg;
        math_wrap_angle_360(&game.deflectHeadingDeg);
    }

    // 0x498A06 to 0x498B22 the impact angle index into the curve at game 0x1396FE0
    int angleIndex = impact_angle_index(game.deflectHeadingDeg);
    // index 100 reads one float past the table in the client the port clamps to 99
    float curve = body_durability_curve_value(angleIndex);

    float restitution = kCollisionRestitutionNormal;
    if (game.stuckWatch.stuckFlag != 0) {
        restitution = kCollisionRestitutionStuck;
    } else if (angleIndex < 20) {
        restitution = curve * kCollisionRestitutionDurability + kTurnForceSlowFlagMul;
    }
    // 0x498B68 body vec3 scale on the velocity at car 0x25FC
    body_vec3_scale(car.body.wheels.velocity, restitution);

    float impulseScale = (car.speed - kCollisionSpeedFifteen) * kDriftChargeFloorScale * curve;
    if (impulseScale < kCollisionImpulseFloor) impulseScale = kCollisionImpulseFloor;
    else if (impulseScale > kCollisionImpulseCeil) impulseScale = kCollisionImpulseCeil;

    float spinTerm = 0.0f;
    if (game.stuckWatch.stuckFlag != 0) {
        impulseScale = kOne;
        spinTerm = 0.0f;
    } else {
        if (angleIndex > 50) {
            spinTerm = (kOne - curve) * car.speedKmh * kCollisionSpinKmhScale;
            if (spinTerm > kCollisionSpinCeil) spinTerm = kCollisionSpinCeil;
            if (car.speed > kCollisionHighSpeed) car.throttleJitter = kTurnForceSlowFlagMul;
            if (game.raceMode != 9) {
                if (car.speed <= kLowSpeedParticle) {
                    if (game.hooks.spawnParticle) {
                        game.hooks.spawnParticle(game.hooks.user, carIndex, 0, car.body.fallbackContactPoint.x,
                                                 car.body.fallbackContactPoint.y, car.body.fallbackContactPoint.z);
                    }
                } else {
                    if (game.hooks.spawnParticle) {
                        game.hooks.spawnParticle(game.hooks.user, carIndex, 1, car.body.fallbackContactPoint.x,
                                                 car.body.fallbackContactPoint.y, car.body.fallbackContactPoint.z);
                    }
                    if (game.hooks.playSound) game.hooks.playSound(game.hooks.user, carIndex, 0, 5);
                }
            }
        }
        if (car.speed > kCollisionSpeedFifteen && game.hooks.spawnParticle) {
            game.hooks.spawnParticle(game.hooks.user, carIndex, 2, car.body.fallbackContactPoint.x,
                                     car.body.fallbackContactPoint.y, car.body.fallbackContactPoint.z);
        }
        // 0x498D39 car impact effect play 0x4981B0 tier 1 over angle index 20 else tier 0
        if (car.speed > kCollisionSpeedFifteen && game.hooks.impactEffect) {
            game.hooks.impactEffect(game.hooks.user, carIndex, angleIndex > 20 ? 1 : 0);
        }
    }

    float frictionFloor = kHalf; // 0x498D46 0 8 with no drift and at most two wheels in the air else 0 5
    if (car.driftState == 0 && body_wheels_on_ground_count(car.body.wheels) < 3) {
        frictionFloor = kCollisionFrictionFloorLow;
    }

    // 0x498D6C the impulse x from car 0x2E34 y from car 0x2E38 z from the spin term
    Vec3 impulse;
    impulse.x = normalX * frictionFloor * impulseScale * substepDt;
    impulse.y = frictionFloor * impulseScale * normalY * substepDt;
    impulse.z = spinTerm * substepDt * kSmoothQuarter;
    body_vec3_add(car.body.wheels.velocity, impulse);

    // 0x498DB2 licence test 0x17 counts the collisions DAT 00BFDAC4 is the test id DAT 00BFDABC the count
    if (game.raceMode == RACE_STATE_INPUT_GATE && game.licenceTestId == kLicenceTestCollisionCount) {
        game.licenceTestCollisionCount += 1;
    }
    // the flag car 0x2E20 is not written here 0x4ECB35 clears it at the next integrate
    return true;
}

} // namespace KnC Kart Client
