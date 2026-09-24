#include "effects.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "boost.h"
#include "constants.h"
#include "drift.h"
#include "gimmicks.h"
#include "math_helpers.h"
#include "respawn.h"

namespace KnC::Kart::Client {

bool car_is_slowed(const std::array<StatusIdSlot, STATUS_TABLE_SIZE>& table, int id) {
    for (const StatusIdSlot& slot : table) {
        if (slot.active == 1 && slot.id == id) return true;
    }
    return false;
}

bool car_is_camera_reversed(const std::array<StatusIdSlot, STATUS_TABLE_SIZE>& table, int id) {
    for (const StatusIdSlot& slot : table) {
        if (slot.active == 1 && slot.id == id) return true;
    }
    return false;
}

void effect_end(CarState& car) {
    // effect end 0x495BE0 zeroes car 0x36C0 0x36C4 0x36C8 0x36A8 0x36AC and the elapsed at car 0x36B0
    car.effect.activeCode = 0;
    car.effect.wobbleAmplitude = 0.0f;
    car.effect.endCondition = 0.0f;
    car.effect.elapsedAccum = 0.0f;
}

namespace {

// 0x495C4C every code snapshots the yaw and the position first
void effect_snapshot(CarState& car) {
    car.effect.snapshotYaw = car.yawDeg;
    car.effect.snapshotX = car.posX;
    car.effect.snapshotY = car.posY;
    car.effect.snapshotZ = car.posZ;
}

// 0x495D3F codes 200 and 300 zero the scratch seed 36BC with 34 and add dt times 200000 to car 0x2604
void effect_seed_spin(CarState& car) {
    car.effect.endCondition = 0.0f;
    car.effect.quadraticInput = kEffect300ApplySeed;
    car.effect.elapsedAccum = 0.0f;
    car.effect.secondaryAccum += car.frameDt * kEffect300AccumRate;
}

// 0x496084 the common tail sound 6 then car boost clear then car drift state set 0
void effect_start_common(GameState& game, int carIndex, bool sound, int64_t nowMs) {
    CarState& car = game.cars[static_cast<size_t>(carIndex)];
    if (sound && game.hooks.playSound) game.hooks.playSound(game.hooks.user, carIndex, 0, 6);
    (void)car;
    car_boost_clear(game, carIndex);
    car_drift_state_set(game, carIndex, 0, nowMs);
}

} // namespace

void car_effect_apply(GameState& game, int carIndex, int code, int64_t nowMs) {
    CarState& car = game.cars[carIndex];
    if (car.effect.activeCode != 0) return; // refuses while one is already active 0x495C44 code lands before the switch an unknown code stays
    car.effect.activeCode = code;
    car.effect.wobbleAmplitude = 0.0f;

    switch (code) {
        case 100:
        case 700:
            effect_snapshot(car);
            effect_start_common(game, carIndex, true, nowMs);
            break;
        case 200:
            effect_snapshot(car);
            effect_start_common(game, carIndex, true, nowMs);
            effect_seed_spin(car);
            break;
        case 300:
            effect_snapshot(car);
            effect_start_common(game, carIndex, true, nowMs);
            car_gear_clamp(car.body, 1);
            car.input.accel = 0;
            car.input.brake = 0;
            car.input.steerRight = 0;
            car.input.steerLeft = 0;
            game.stoppedFlag = 0;
            car.input.stuck = 0;
            if (carIndex == game.localCarIndex) {
                car_boost_start(game, carIndex, BOOST_KIND_CANCEL, nowMs);
            }
            effect_seed_spin(car);
            game.crashRecovery.state = 1;
            game.crashRecovery.timestampMs = nowMs;
            break;
        case 400:
        case 500:
            effect_snapshot(car);
            effect_start_common(game, carIndex, true, nowMs);
            break;
        case 600:
        case 900:
            effect_snapshot(car);
            effect_start_common(game, carIndex, false, nowMs);
            break;
        case 800:
            effect_snapshot(car); // 0x495F80 the shield snapshots and nothing else
            break;
        case 1000:
            effect_snapshot(car);
            if (game.hooks.impactEffect) game.hooks.impactEffect(game.hooks.user, carIndex, 1); // 0x496066
            effect_start_common(game, carIndex, true, nowMs);
            break;
        case 0x44C:
            effect_snapshot(car);
            if (game.hooks.playSound) game.hooks.playSound(game.hooks.user, carIndex, 0, 6);
            if (game.hooks.cameraPan) game.hooks.cameraPan(game.hooks.user); // FUN 0043D7E0
            break;
        default:
            break;
    }
}

namespace {

// 0x4960C6 the idle dispatch starts a hit effect inline snapshot code sound 6 boost clear drift clear
void effect_start_from_hit(GameState& game, int carIndex, int code, int64_t nowMs) {
    CarState& car = game.cars[static_cast<size_t>(carIndex)];
    effect_snapshot(car);
    car.effect.activeCode = code;
    car.effect.wobbleAmplitude = 0.0f;
    if (code == 1000 && game.hooks.impactEffect) game.hooks.impactEffect(game.hooks.user, carIndex, 1);
    effect_start_common(game, carIndex, true, nowMs);
}

} // namespace

void car_effect_update(GameState& game, int carIndex, float* outScaleX, float* outScaleY,
                        int64_t nowMs) {
    CarState& car = game.cars[carIndex];
    float dt = car.frameDt;
    float scale = kOne;

    if (car.effect.activeCode == 0) {
        // 0x4960C6 the three hit tables in this order each only while the code is still 0
        if (effect_hazard_hit_lookup(game.hazardTable, carIndex) >= 0 && car.effect.activeCode == 0) {
            effect_start_from_hit(game, carIndex, 500, nowMs);
        }
        if (world_wheel_bump_slot(game.itembiteTable, carIndex) >= 0 && car.effect.activeCode == 0) {
            effect_start_from_hit(game, carIndex, 700, nowMs);
        }
        if (world_wheel_bump_slot(game.itemdrumTable, carIndex) >= 0 && car.effect.activeCode == 0) {
            effect_start_from_hit(game, carIndex, 1000, nowMs);
        }
    }

    int code = car.effect.activeCode;
    switch (code) {
        case 0:
            break;
        case 100:
            scale = kEffect100SpeedScale;
            car.effect.wobbleAmplitude += dt * kEffect100RampRateMs;
            if (car.effect.wobbleAmplitude > kEffect100EndCap) effect_end(car);
            break;
        case 200: {
            scale = kEffect200SpeedScale;
            car.effect.wobbleAmplitude -= dt * kEffect200DecayRateMs;
            // 0x4962A0 car wheels in air count 0x48D730 ends the spin once at most one wheel is in the air
            int wheelsInAir = body_wheels_on_ground_count(car.body.wheels);
            if (car.effect.wobbleAmplitude != 0.0f && wheelsInAir <= 1) effect_end(car);
            break;
        }
        case 300: {
            scale = kEffect300ZeroTerm;
            car.effect.secondaryAccum *= kEffect300SecondaryMul;
            // 0x49634C the end condition reads the elapsed before this tick adds dt
            float a = car.effect.elapsedAccum;
            car.effect.endCondition = car.effect.quadraticInput * a - a * a * kEffect300QuadraticTerm;
            car.effect.elapsedAccum += dt;
            car.effect.wobbleAmplitude -= dt * kEffect300RecoveryRate; // 0x49638A minus
            car_gear_clamp(car.body, 1);
            car.input.accel = 0;
            car.input.brake = 0;
            car.input.steerRight = 0;
            car.input.steerLeft = 0;
            game.stoppedFlag = 0;
            car.input.stuck = 0;
            if (car.effect.endCondition < 0.0f) effect_end(car);
            break;
        }
        case 400: {
            scale = kEffect400And1000SpeedScale;
            car.effect.wobbleAmplitude += dt * kEffect100RampRateMs;
            if (car.effect.wobbleAmplitude > kEffect100EndCap) car.effect.wobbleAmplitude = kEffect100EndCap;
            // 0x4964AB effect hive hit lookup 0x4CF020 on the table at 0x2F077F0 ends it when the slot is gone
            if (effect_hive_hit_lookup(game.hiveTable, carIndex) < 0) effect_end(car);
            break;
        }
        case 500:
            scale = kEffect500SpeedScale;
            if (effect_hazard_hit_lookup(game.hazardTable, carIndex) < 0) effect_end(car);
            break;
        case 600:
            // 0x4964CD gimmick pool slot lookup 0x4B9FE0 on the carry pool at 0x2EFC818 ends the grab lock
            if (gimmick_pool_slot_lookup(game.carryPool, carIndex) < 0) effect_end(car);
            break;
        case 700:
            // 0x496553 sound 6 and the scale run every tick not once
            if (game.hooks.playSound) game.hooks.playSound(game.hooks.user, carIndex, 0, 6);
            scale = kEffect700SpeedScale;
            if (world_wheel_bump_slot(game.itembiteTable, carIndex) < 0) effect_end(car);
            break;
        case 800:
            break; // shield passive an outside system clears it
        case 900:
            // 0x496512 the same lookup on the second pool at 0x2ECDA38 the item use dispatch fills it
            if (gimmick_pool_slot_lookup(game.effect900Pool, carIndex) < 0) effect_end(car);
            break;
        case 1000:
            // 0x4965A8 sound 6 and the scale run every tick not once
            if (game.hooks.playSound) game.hooks.playSound(game.hooks.user, carIndex, 0, 6);
            scale = kEffect400And1000SpeedScale;
            if (world_wheel_bump_slot(game.itemdrumTable, carIndex) < 0) effect_end(car);
            break;
        case 0x44C:
            // 0x4965F4 DAT 00D6E1D0 the cutscene busy flag ends the effect when it reads 0
            if (game.cutsceneBusy == 0) effect_end(car);
            break;
        default:
            break;
    }

    car.body.wheels.velocity.x *= scale;
    car.body.wheels.velocity.y *= scale;
    if (outScaleX) *outScaleX = scale;
    if (outScaleY) *outScaleY = scale;
}

void car_remote_effect_update(GameState& game, int carIndex) {
    CarState& car = game.cars[static_cast<size_t>(carIndex)];
    const float dt = car.frameDt;
    EffectState& e = car.effect;
    bool still = true;
    switch (e.activeCode) {
        case 100:
            e.wobbleAmplitude += dt * kRemoteEffect100Rate;
            still = e.wobbleAmplitude <= kRemoteEffect100End;
            break;
        case 200:
            e.endCondition = e.quadraticInput * e.elapsedAccum - e.elapsedAccum * e.elapsedAccum * kRemoteEffect200Gravity;
            e.elapsedAccum += dt;
            still = e.endCondition >= 0.0f;
            break;
        case 300:
            // 0x496664 the remote crash rises and falls on 29 4 then the height of the snapshot comes back
            e.endCondition = e.quadraticInput * e.elapsedAccum - e.elapsedAccum * e.elapsedAccum * kEffect300QuadraticTerm;
            e.elapsedAccum += dt;
            e.wobbleAmplitude -= dt * kEffect300RecoveryRate;
            if (e.endCondition < 0.0f) {
                effect_end(car);
                car.posZ = e.snapshotZ;
            }
            return;
        case 400:
            e.wobbleAmplitude = std::min(e.wobbleAmplitude + dt * kEffect100RampRateMs, kRemoteEffectWobbleCap);
            still = effect_hive_hit_lookup(game.hiveTable, carIndex) >= 0;
            break;
        case 500:
            still = effect_hazard_hit_lookup(game.hazardTable, carIndex) >= 0;
            break;
        case 600:
            still = gimmick_pool_slot_lookup(game.carryPool, carIndex) >= 0;
            break;
        case 700:
            e.wobbleAmplitude = std::min(e.wobbleAmplitude + dt * kEffect100RampRateMs, kRemoteEffectWobbleCap);
            still = world_wheel_bump_slot(game.itembiteTable, carIndex) >= 0;
            break;
        case 900:
            still = gimmick_pool_slot_lookup(game.effect900Pool, carIndex) >= 0;
            break;
        case 1000:
            e.wobbleAmplitude = std::min(e.wobbleAmplitude + dt * kEffect100RampRateMs, kRemoteEffectWobbleCap);
            still = world_wheel_bump_slot(game.itemdrumTable, carIndex) >= 0;
            break;
        default:
            return;
    }
    if (!still) effect_end(car);
}

float car_suspension_shake(const CarState& car) {
    if (car.vehicleKind != 2) return 0.0f;
    float sum = car.body.wheels.tireScratch[0].compression + car.body.wheels.tireScratch[1].compression +
                car.body.wheels.tireScratch[2].compression + car.body.wheels.tireScratch[3].compression;
    float shake = sum * kSuspensionShakeScale * kSuspensionShakeSign;
    if (shake < kSuspensionShakeClampFloor) shake = kSuspensionShakeClampFloor;
    if (shake > kCollisionHighSpeed) shake = kCollisionHighSpeed;
    return shake;
}

void car_effect_lean_update(GameState& game, int carIndex) {
    // car effect lean update 0x49B3D0 the model lean the drift slip turn and the body world matrix
    CarState& car = game.cars[carIndex];
    float leanY = 0.0f; // D3DX yaw about model y axis is the nose lift pitch about model x axis is the side lean
    float leanX = 0.0f;

    if (car.effect.activeCode == 200 || car.effect.activeCode == 300) {
        // 0x49B569 the spin effects turn the model about y by the wobble term D3DXMatrixRotationY
        leanY = car.effect.wobbleAmplitude;
    } else {
        // 0x49B488 drifting the lift is wire stat 6 times the gauge size the lean wire 7 times shake plus gauge
        float gauge = car.driftGaugeSmoothed;
        float shake = car_suspension_shake(car);
        float lift = 0.0f;
        float side = shake;
        if (car.driftState != 0) {
            lift = (gauge < kZero) ? -gauge : gauge;
            side = shake + gauge;
        } else if (car.boostState != 0) {
            // 0x49B4C0 a boost lifts the nose by wire stat 4 car 0x3458 times 30 clamped 0 to 30 a wheelie
            lift = stat_total(car.stats, KartStatIndex::BoostLeanLift) * kDriftLowSpeedSkip;
            if (lift < kZero) lift = kZero;
            else if (lift > kDriftLowSpeedSkip) lift = kDriftLowSpeedSkip;
        }
        // 0x49B54B wire stat 6 car 0x3460 and 0x49B528 wire stat 7 car 0x3464 with their bonus
        leanY = stat_total(car.stats, KartStatIndex::WheelSpin) * lift;
        leanX = -(stat_total(car.stats, KartStatIndex::WheelSteerAngle) * side);
    }

    // wobble states 1 to 9 stay a simplified stand in they scale the side lean the client scales car 0x36E4
    float wobble = leanX * kDegToRad;
    switch (car.effect.leanState) {
        case 0:
            break;
        case 1:
        case 2:
        case 3:
            if (wobble < kMiniTurboThresholdFloor) {
                car.effect.leanState = (car.effect.leanState + 1) % 4;
            } else {
                wobble *= kLeanWobbleGrowBack118;
                if (wobble > kOne) car.effect.leanState = 0;
            }
            break;
        case 4:
        case 5:
            wobble *= kLeanWobbleDecayPair;
            if (wobble < kMiniTurboThresholdFloor) car.effect.leanState = 0;
            break;
        case 7:
        case 8:
        case 9:
            wobble *= kLeanWobbleGrowBack;
            if (wobble > kCollisionSpinDurabilityGate) car.effect.leanState = 0;
            break;
        default:
            car.effect.leanState = 0;
            break;
    }
    if (car.effect.leanState != 0) {
        car.accumulatedRoll += kMiniTurboThresholdFloor; // 0x49B9AC car 0x3018 plus 0 2 in the wobble states
        leanX = wobble * kRadToDeg;
    }
    car.leanPitchDeg = leanY;
    car.leanRollDeg = leanX;

    // 0x49B591 car matrix lean times slip times body matrix column order F Rz Ry Rx F flips wire x y
    const Mat3& r = car.body.wheels.orientationR;
    Mat3 bodyWire;
    body_mat3_set(bodyWire, r.m[0][0], r.m[0][1], -r.m[0][2], r.m[1][0], r.m[1][1], -r.m[1][2], -r.m[2][0],
                  -r.m[2][1], r.m[2][2]);
    float slipRad = car.driftSlipDeg * kDegToRad;
    float cs = std::cos(slipRad);
    float ss = std::sin(slipRad);
    Mat3 slipZ;
    body_mat3_set(slipZ, cs, -ss, 0.0f, ss, cs, 0.0f, 0.0f, 0.0f, kOne);
    float cy = std::cos(leanY * kDegToRad);
    float sy = std::sin(leanY * kDegToRad);
    Mat3 liftY;
    body_mat3_set(liftY, cy, 0.0f, sy, 0.0f, kOne, 0.0f, -sy, 0.0f, cy);
    float cx = std::cos(leanX * kDegToRad);
    float sx = std::sin(leanX * kDegToRad);
    Mat3 leanXm;
    body_mat3_set(leanXm, kOne, 0.0f, 0.0f, 0.0f, cx, -sx, 0.0f, sx, cx);
    Mat3 lean;
    body_mat3_multiply(lean, liftY, leanXm);
    Mat3 slipLean;
    body_mat3_multiply(slipLean, slipZ, lean);
    body_mat3_multiply(car.carMatrix.rotation, bodyWire, slipLean);
    car.carMatrix.translation = Vec3{car.posX, car.posY, car.posZ};
}


namespace {

// math hypot3d 0x44D950 zero when all three are zero else the square root of the sum of squares
float math_hypot3d(float a, float b, float c) {
    if (a == 0.0f && b == 0.0f && c == 0.0f) return 0.0f;
    return std::sqrt(a * a + b * b + c * c);
}

// 0x4C8240 respawn follow advance point 0x489D50 steps the point when reached or receding
void follow_advance_point(const GameState& game, GimmickPoolSlot& slot) {
    const std::vector<CheckpointPoint>& pts = game.checkpoints.lists[slot.follow_list & 3];
    if (pts.empty()) return;
    if (slot.follow_point < 0 || slot.follow_point >= static_cast<int32_t>(pts.size())) slot.follow_point = 0;
    const CheckpointPoint& p = pts[static_cast<size_t>(slot.follow_point)];
    float dist = math_hypot3d(p.x - slot.x, p.y - slot.y, p.z - slot.z);
    if (dist > slot.last_dist || dist < kFollowAdvanceRadius) {
        slot.follow_point = (slot.follow_point + 1) % static_cast<int32_t>(pts.size());
        const CheckpointPoint& n = pts[static_cast<size_t>(slot.follow_point)];
        dist = math_hypot3d(n.x - slot.x, n.y - slot.y, n.z - slot.z);
    }
    slot.last_dist = dist;
}

// 0x4C8588 respawn follow next point 0x489860 steps to the next point wrapping at the count
void follow_next_point(const GameState& game, GimmickPoolSlot& slot) {
    const std::vector<CheckpointPoint>& pts = game.checkpoints.lists[slot.follow_list & 3];
    if (pts.empty()) return;
    slot.follow_point = (slot.follow_point + 1) % static_cast<int32_t>(pts.size());
}

const CheckpointPoint* follow_point_of(const GameState& game, const GimmickPoolSlot& slot) {
    const std::vector<CheckpointPoint>& pts = game.checkpoints.lists[slot.follow_list & 3];
    if (slot.follow_point < 0 || slot.follow_point >= static_cast<int32_t>(pts.size())) return nullptr;
    return &pts[static_cast<size_t>(slot.follow_point)];
}

} // namespace

void gimmick_pool_update(GameState& game, const ColTrack& track, int64_t nowMs) {
    gimmick_pool_update_pool(game, game.carryPool, kGimmickCarryGrabCode, track, nowMs);
}

void gimmick_pool_update_pool(GameState& game, std::array<GimmickPoolSlot, GIMMICK_POOL_LIVE_SLOTS>& pool,
                              int grabCode, const ColTrack& track, int64_t nowMs) {
    // gimmick pool update 0x4C7ED0 the eight carry slots at 0x2EFC818 states 0 1 100 200 201
    for (size_t i = 0; i < GIMMICK_POOL_LIVE_SLOTS; ++i) {
        GimmickPoolSlot& slot = pool[i];
        if (!slot.active || slot.car_index < 0 || slot.car_index >= kCarSlotCount) continue;
        CarState& car = game.cars[static_cast<size_t>(slot.car_index)];
        switch (slot.state) {
            case GimmickPoolState::Idle: {
                // 0x4C7F6E the grab lock a particle on the car then the timestamps 0x4BA380 grabs with 900
                car_effect_apply(game, slot.car_index, grabCode, nowMs);
                if (game.hooks.spawnParticle) {
                    game.hooks.spawnParticle(game.hooks.user, slot.car_index, 4, car.posX, car.posY, car.posZ);
                }
                slot.state_ms = nowMs;
                slot.land_ms = nowMs;
                slot.state = GimmickPoolState::GrabLock;
                slot.target_x = slot.x;
                slot.target_y = slot.y;
                slot.target_z = slot.z;
                if (slot.car_index == game.localCarIndex && game.hooks.freezeInput) {
                    game.hooks.freezeInput(game.hooks.user, slot.car_index); // FUN 0043ED70 7
                }
                break;
            }
            case GimmickPoolState::GrabLock:
                if (nowMs - slot.state_ms > kGimmickGrabLockMs) slot.state = GimmickPoolState::Carry;
                break;
            case GimmickPoolState::Carry: {
                // 0x4C80A5 the land effect after 1300 ms once then the carry window
                if (nowMs - slot.state_ms > kGimmickCarryLandMs && slot.land_played == 2) slot.land_played = 3;
                int64_t window = kGimmickCarryMs;
                if (game.raceMode == RACE_STATE_INPUT_GATE && game.licenceTestId == 1) window = kGimmickCarryLicenceMs;
                if (slot.car_index != game.localCarIndex) window += kGimmickCarryRemoteExtraMs;
                if (game.hooks.carHasAbility &&
                    game.hooks.carHasAbility(game.hooks.user, slot.car_index, kGimmickAbilityCarry)) {
                    window += kGimmickCarryAbilityExtraMs;
                }
                bool inWindow = nowMs - slot.land_ms <= window;
                bool release = !inWindow;
                if (inWindow) {
                    // 0x4C81B8 the local car presses the accel key to drop early
                    if (car.playerOrGhostId == game.localPlayerId && game.accelKeyPressed != 0) release = true;
                    if (game.raceMode == RACE_STATE_INPUT_GATE && game.itemCounters.at_0xbfc510 >= 4) release = true;
                }
                if (release) {
                    slot.state = GimmickPoolState::Drop;
                    break;
                }
                // 0x4C8240 the slot walks the follow polyline the car rides on the slot
                follow_advance_point(game, slot);
                const CheckpointPoint* p = follow_point_of(game, slot);
                if (p != nullptr) {
                    float bearing = math_atan2_deg(p->x - slot.x, p->y - slot.y) + kAxisCorrection90Deg;
                    float turn = bearing - slot.yaw_deg;
                    math_wrap_angle_signed180(&turn);
                    slot.yaw_deg += turn;
                    // 0x4C8350 the target advances 3 6 along the bearing then the slot eases a tenth toward it
                    float a = -(bearing - kAxisCorrection90Deg - kAxisCorrection90Deg) * kDegToRad;
                    slot.target_x += std::cos(a) * kGimmickCarryStep;
                    slot.target_y += std::sin(a) * kGimmickCarryStep;
                    slot.yaw_deg = bearing;
                }
                slot.x += (slot.target_x - slot.x) * kGimmickCarryEase;
                slot.y += (slot.target_y - slot.y) * kGimmickCarryEase;
                {
                    float ground[3] = {slot.x, slot.y, slot.z};
                    if (world_ground_height_at(car.body.groundQuery, ground)) {
                        slot.target_z = ground[2];
                    } else {
                        // 0x4C83E7 off the mesh the slot snaps to the nearest follow row
                        int list = 0, point = 0;
                        float dist = 0.0f;
                        if (respawn_checkpoint_nearest_all(game.checkpoints, slot.x, slot.y, slot.z, &list, &point,
                                                            &dist)) {
                            slot.follow_list = list;
                            slot.follow_point = point;
                            const CheckpointPoint& r = game.checkpoints.lists[list][static_cast<size_t>(point)];
                            slot.x = r.x;
                            slot.y = r.y;
                            slot.z = r.z;
                            slot.yaw_deg = math_atan2_deg(r.x - slot.x, r.y - slot.y);
                        }
                    }
                }
                slot.z += slot.target_z - slot.z;
                // 0x4C8480 the car takes the slot position and yaw through body set position
                car.posX = slot.x;
                car.posY = slot.y;
                car.posZ = slot.z;
                car.yawDeg = slot.yaw_deg;
                body_place_and_probe(car.body, -car.posX, -car.posY, car.posZ, -car.yawDeg, track);
                car.prevPosX = car.posX;
                car.prevPosY = car.posY;
                car.prevPosZ = car.posZ;
                car.prevSubstepYawDeg = car.yawDeg;
                break;
            }
            case GimmickPoolState::Drop: {
                // 0x4C8520 the drop places the car on the next follow row plus 5 and kicks it 40 along its yaw
                slot.state = GimmickPoolState::Released;
                slot.boost_armed = 1;
                if (slot.car_index == game.localCarIndex && game.hooks.freezeInput) {
                    game.hooks.freezeInput(game.hooks.user, slot.car_index); // FUN 0043ED70 1
                }
                int list = 0, point = 0;
                float dist = 0.0f;
                if (respawn_checkpoint_nearest_all(game.checkpoints, slot.x, slot.y, slot.z, &list, &point, &dist)) {
                    slot.follow_list = list;
                    slot.follow_point = point;
                    follow_next_point(game, slot);
                    const CheckpointPoint* r = follow_point_of(game, slot);
                    if (r != nullptr) {
                        car.yawDeg = math_atan2_deg(r->x - slot.x, r->y - slot.y) + kAxisCorrection90Deg;
                        body_place_and_probe(car.body, -car.posX, -car.posY, car.posZ + kGimmickDropHeightBias,
                                             -car.yawDeg, track);
                        car.posZ += kGimmickDropHeightBias;
                    }
                }
                car_push_along_yaw(game, slot.car_index, kGimmickDropPushStrength); // 0x4C8648
                if (game.hooks.spawnParticle) {
                    game.hooks.spawnParticle(game.hooks.user, slot.car_index, 5, car.posX, car.posY, car.posZ);
                }
                slot.state_ms = nowMs;
                break;
            }
            case GimmickPoolState::Released: {
                // 0x4C86E8 the accel key press inside 200 to 1000 ms gives a kind 0 boost once
                int64_t elapsed = nowMs - slot.state_ms;
                if (game.accelKeyPressed != 0) {
                    if (slot.boost_armed == 1 && elapsed > kGimmickReleaseBoostLowMs &&
                        elapsed < kGimmickReleaseBoostHighMs) {
                        car_boost_start(game, slot.car_index, BOOST_KIND_MINI_TURBO, nowMs);
                    }
                    slot.boost_armed = 0;
                }
                if (elapsed > kGimmickReleaseSlotMs) {
                    // FUN 004BA300 frees the slot
                    slot.active = false;
                    slot.car_index = -1;
                    slot.state = GimmickPoolState::Idle;
                }
                break;
            }
        }
    }
}

} // namespace KnC Kart Client
