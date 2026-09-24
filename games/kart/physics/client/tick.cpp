#include "tick.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "boost.h"
#include "constants.h"
#include "drift.h"
#include "effects.h"
#include "input.h"
#include "math_helpers.h"
#include "motion_packet.h"
#include "remote_car.h"
#include "respawn.h"
#include "stats.h"

namespace KnC::Kart::Client {

namespace {
float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

bool strcmpi_ascii(const char* a, const char* b) {
    while (*a && *b) {
        char ca = static_cast<char>(std::toupper(static_cast<unsigned char>(*a)));
        char cb = static_cast<char>(std::toupper(static_cast<unsigned char>(*b)));
        if (ca != cb) return false;
        ++a;
        ++b;
    }
    return *a == *b;
}

// math dir from heading 0x44DDD0 x y of the magnitude along minus heading minus 90 in radians
void math_dir_from_heading(float magnitude, float headingDeg, float* outX, float* outY) {
    float a = -(headingDeg - kAxisCorrection90Deg) * kDegToRad;
    *outX = std::cos(a) * magnitude;
    *outY = std::sin(a) * magnitude;
}

// car name match count 0x4A1380 counts the wheel probe points whose cell name equals the tag
int car_name_match_count(CarState& car, const char* tag) {
    float wx[4];
    float wy[4];
    for (int i = 0; i < 4; ++i) {
        wx[i] = car.wheelProbePoint[i].x;
        wy[i] = car.wheelProbePoint[i].y;
    }
    return world_wheel_surface_tag_count(car.body.groundQuery, wx, wy, tag);
}

// the ghost recorder reads the remote record the local tick mirrors its fields first
void ghost_sync_from_local(CarState& car) {
    car.remote.posX = car.posX;
    car.remote.posY = car.posY;
    car.remote.posZ = car.posZ;
    car.remote.yawDeg = car.yawDeg;
    car.remote.rpm = car.rpm;
    car.remote.driftGauge = car.driftGauge;
    car.remote.driftGaugeSmoothed = car.driftGaugeSmoothed;
    car.remote.driftState = car.driftState;
    car.remote.miniTurboStage = car.miniTurboStage;
    car.remote.boostState = car.boostState;
    car.remote.boostKind = car.boostKind;
    car.remote.reverseFlag = car.reverseFlag;
    car.remote.turnState = car.turnState;
}

// the pose the car pair functions read built from one car record
CarPose car_pose_of(const CarState& car) {
    CarPose p;
    p.x = car.posX;
    p.y = car.posY;
    p.z = car.posZ;
    p.speed = car.speed;
    p.yaw = car.yawDeg;
    p.extentY = car.body.catalogue.chassisExtent[1];
    p.extentZ = car.body.catalogue.chassisExtent[2];
    p.slotOccupied = car.slotOccupied;
    p.finished = car.finishedOrSpectating == 2 ? 1 : 0;
    return p;
}
} // namespace

void car_decode_id5(char out[6], const int in[5]) {
    // car decode id5 0x49BE20 division by 21 idiom decodes to REGEN for the traced call site
    for (int i = 0; i < 5; ++i) {
        int v = in[i];
        int q = v / 21; // truncating division matches the traced positive inputs exactly
        out[i] = static_cast<char>(q - (i + 1));
    }
    out[5] = '\0';
}

bool car_node_name_is(GameState& game, int carIndex, int wheelIndex, const char* name) {
    // car node name is 0x4A1350 world wheel query surface 0x4A0680 gives the cell name under one wheel
    if (wheelIndex < 0 || wheelIndex > 3) return false;
    CarState& car = game.cars[static_cast<size_t>(carIndex)];
    if (!car.body.groundQuery.active) return false;
    const Vec3& p = car.wheelProbePoint[wheelIndex];
    const char* cell = world_wheel_query_surface(car.body.groundQuery, p.x, p.y);
    if (!cell) return false;
    return strcmpi_ascii(cell, name);
}

int standings_local_row_index(const GameState& game) {
    // standings local row index 0x4B4B50 the row of the standings table whose player id is the local one
    for (int i = 0; i < 16; ++i) {
        if (game.standingsPlayerIds[static_cast<size_t>(i)] == game.localPlayerId) return i;
    }
    return -1;
}

void car_ground_flag_set_car(CarState& car, uint8_t groundValid) {
    // car ground flag set 0x49A920 the body flag and the gear then 0x49A942 net motion queue clear
    car_ground_flag_set(car.body, groundValid);
    if (groundValid == 1) net_motion_queue_clear(car.remote.mailbox);
}

void world_car_push_apart_tick(GameState& game, int carIndex) {
    // world car push apart 0x498800 the self body velocity scales and takes the unit delta the other car nudges
    CarPose poses[kCarSlotCount];
    for (int i = 0; i < kCarSlotCount; ++i) poses[i] = car_pose_of(game.cars[static_cast<size_t>(i)]);
    int other = world_car_find_overlap(poses, kCarSlotCount, carIndex);
    if (other < 0) return;
    float dir[3] = {0.0f, 0.0f, 0.0f};
    int effectCar = -1;
    int effectTier = -1;
    if (!world_car_push_apart(poses[carIndex], poses[other], dir, &effectCar, &effectTier)) return;
    CarState& self = game.cars[static_cast<size_t>(carIndex)];
    CarState& otherCar = game.cars[static_cast<size_t>(other)];
    // 0x498887 car 0x25FC scales by 0 994 then adds 0 8 dx 0 8 dy and dz
    body_vec3_scale(self.body.wheels.velocity, kPushApartVelocityScale);
    Vec3 nudge;
    body_vec3_set(nudge, dir[0] * kPushApartVelocityDirScale, dir[1] * kPushApartVelocityDirScale, dir[2]);
    body_vec3_add(self.body.wheels.velocity, nudge);
    // 0x4988BC the other car position moves by the nudge times 0 06
    otherCar.posX = poses[other].x;
    otherCar.posY = poses[other].y;
    if (effectTier >= 0 && game.hooks.impactEffect) {
        game.hooks.impactEffect(game.hooks.user, effectCar == 0 ? carIndex : other, effectTier);
    }
}

void cars_frame_update(GameState& game, const ColTrack& track, int64_t nowMs) {
    // cars frame update 0x00495330 local car to the tick every other car to the remote mover
    for (int i = 0; i < kCarSlotCount; ++i) {
        CarState& car = game.cars[static_cast<size_t>(i)];
        if (car.slotOccupied == 0) continue;
        if (i == game.localCarIndex) {
            car_physics_tick_local(game, i, track, nowMs);
        } else {
            car.remote.frameDt = car.frameDt;
            car.remote.effectCode = car.effect.activeCode;
            car.remote.effectWobble = car.effect.wobbleAmplitude;
            car.remote.groundQuery = &car.body.groundQuery;
            car.remote.track = &track;
            car_remote_update_tick(car.remote, nowMs);
        }
    }
}

void car_physics_tick_local(GameState& game, int carIndex, const ColTrack& track, int64_t nowMs) {
    CarState& car = game.cars[static_cast<size_t>(carIndex)];

    // car physics setup 0x494D50 arms the probe through world place probe local the port has no setup call
    if (!car.body.groundQuery.active) {
        world_place_probe_local(car.body.groundQuery, track, car.posX, car.posY, car.posZ, nullptr,
                                game.worldReady != 0);
    }
    // car 0x32DC and 0x32E0 take the catalogue grips 0x130 0x134 at setup the friction pair scales them
    car.gripBaseFront = car.body.catalogue.tireGripBase[0];
    car.gripBaseRear = car.body.catalogue.tireGripBase[1];

    // step 1 the four ground probes then body ground probe response the wheel probe sits in the body module
    if (game.worldReady != 0) {
        body_ground_probe_response(car.body, car.body.contactCallback, track);
    }

    // step 2 crash recovery timer game 0x13972F4 states 1 2 3 windows 800 2001 1000 200 1000 ms
    {
        CrashRecoveryState& cr = game.crashRecovery;
        int64_t elapsed = nowMs - cr.timestampMs;
        // body wheels on ground count is the number of wheels in the air byte 1 at car 0x2DF0
        int wheelsInAir = body_wheels_on_ground_count(car.body.wheels);
        // 0x49C22E and 0x49C339 the recover key is the raw slot 0 accelerate key not the game 0x18 flag
        bool recoverHeld = game.accelKeyRawHeld != 0;

        if (cr.state == 1) {
            if (elapsed >= kCrashRecoveryLandWindowLowMs && elapsed <= kCrashRecoveryLandWindowHighMs) {
                if (wheelsInAir > 2) { // 0x49C2C2 more than two wheels without load
                    if (!recoverHeld) {
                        cr.timestampMs = nowMs;
                        cr.state = 2;
                    }
                }
            } else if (elapsed > kCrashRecoveryLandWindowHighMs) {
                cr.state = 0;
            }
        } else if (cr.state == 2) {
            if (elapsed >= kCrashRecoverySettleMs || wheelsInAir <= 2) {
                cr.timestampMs = nowMs;
                cr.state = 3;
            }
        } else if (cr.state == 3) {
            if (elapsed <= kCrashRecoveryKeyWindowMs) {
                if (recoverHeld) {
                    car_boost_start(game, carIndex, BOOST_KIND_MINI_TURBO, nowMs);
                    cr.state = 0;
                }
            } else if (elapsed >= kCrashRecoveryTimeoutMs) {
                cr.state = 0;
            }
        }

        // drift changed latch decay or growth game 0x13971a4 0x13971a8
        if (game.driftChangedLatch == 0) {
            game.driftChangedValue -= kCrashRecoveryDecayState0;
            if (game.driftChangedValue < kZero) game.driftChangedValue = 0.0f;
        } else {
            game.driftChangedValue += kTickDeadzoneStep;
            if (game.driftChangedValue > kCrashRecoveryGrowthCap) {
                game.driftChangedValue = kCrashRecoveryGrowthCap;
                game.driftChangedLatch = 0;
            }
        }
    }

    // step 3 0x49C3A3 green light state 4 or 5 keys 1 2 3 pressed start boost kind 0 1 2
    if (game.startLightState == 4 || game.startLightState == 5) {
        if (input_key_pressed(game.keyPressed, VK_DEBUG_BOOST_KIND0)) {
            car_boost_start(game, carIndex, BOOST_KIND_MINI_TURBO, nowMs);
        } else if (input_key_pressed(game.keyPressed, VK_DEBUG_BOOST_KIND1)) {
            car_boost_start(game, carIndex, 1, nowMs);
        } else if (input_key_pressed(game.keyPressed, VK_DEBUG_BOOST_KIND2)) {
            car_boost_start(game, carIndex, 2, nowMs);
        }
    }

    // step 4 finished or spectating returns early
    int finishedState = car.finishedOrSpectating;
    if (finishedState == 2) {
        car_ghost_sample_apply(car.remote, game.raceMode == RACE_STATE_INPUT_GATE);
        // car visual update 0x48E6A0 out of scope the render hooks stand in for it
        return;
    }
    if (finishedState == 1) {
        ghost_sync_from_local(car);
        uint8_t mask = input_ghost_mask_from_flags(car.input, game.driftKeyHeld != 0, false, false);
        if (car_ghost_sample_record(car.remote, game.raceMode == RACE_STATE_INPUT_GATE, mask)) {
            car.finishedOrSpectating = 0; // 0x49FEC4 the wrap clears car 0xA7854
        }
    }

    // step 5 car input poll dispatch 0x497E40 gated in race state 0xD by the twelve item counters
    bool dispatchGate = input_dispatch_gate_pass(game.raceMode, game.itemCounters);
    if (dispatchGate) {
        car.brakeLatch = 0;
        car.reverseFlag = 0;
        if (game.inputHost.armed && game.sessionRunning != 0 && game.inputHost.controlsEnabled != 0) {
            // the host arms the input manager 0x5DEAF0 and the pad 0xE52048 else it writes the six flags itself
            KeyboardPollState poll;
            poll.camera_reversed = car_is_camera_reversed(game.cameraReversedTable, carIndex);
            poll.substep_input_flag = car.substepInputFlag != 0;
            poll.gear_mirror_non_negative = !std::signbit(car.gearMirror);
            poll.speed = car.speed;
            poll.rpm = car.rpm;
            poll.wheel_bump_active = world_wheel_bump_slot(game.itembiteTable, carIndex) >= 0;
            poll.stopped_flag = game.stoppedFlag == 1;
            poll.wheels_over_three_in_air = body_wheels_on_ground_count(car.body.wheels) > 3;
            poll.theme_special_row = game.themeSpecialRow != 0;
            poll.theme_boost_blocked = game.themeBoostBlocked != 0;
            poll.boost_active = car.boostState != 0;
            poll.boost_kind = car.boostKind;
            poll.race_mode = game.raceMode;
            poll.driver_slot = car.partSlotType;
            InputDispatchInputs in;
            in.device_index = game.inputHost.device;
            in.bindings = &game.inputHost.bindings;
            in.key_state = &game.inputHost.keyState;
            in.pressed = &game.keyPressed;
            in.pad = &game.inputHost.pad;
            in.scratch = &game.inputHost.scratch;
            in.mouse_delta_x = game.inputHost.mouseDeltaX;
            in.nowMs = nowMs;
            bool brakeLatch = false;
            bool reverse = false;
            DispatchResult r = car_input_poll_dispatch(car.input, in, poll, car.body.wheels.currentGear,
                                                       car.body.catalogue.maxGearCeiling, brakeLatch, reverse);
            car.brakeLatch = brakeLatch ? 1 : 0;
            car.reverseFlag = reverse ? 1 : 0;
            if (r.handled) {
                if (r.keyboard.boost_kind5 > 0) car_boost_start(game, carIndex, 5, nowMs);
                if (r.keyboard.boost_kind5 < 0) car_boost_clear(game, carIndex);
                if (r.keyboard.coast_brake) body_vec3_scale(car.body.wheels.velocity, kCoastBrakeMode9);
                if (car.partSlotType >= 0) car.turnState = r.keyboard.turn_state;
                if (r.keyboard.engine_sound_state >= 0 && game.hooks.playSound) {
                    game.hooks.playSound(game.hooks.user, carIndex, 4, r.keyboard.engine_sound_state);
                }
            }
        } else if (car.body.wheels.currentGear < 0) {
            car.reverseFlag = 1; // 0x497EB0 car 0x332D follows a negative gear index
        }
    }

    // step 6 world on track check clears input and forces the brake when off track
    bool onTrack = world_on_track_check(car.trackProgress);
    if (!onTrack) {
        car.input.brake = 1;
        car.input.accel = 0;
        car.input.stuck = 0;
    }
    if (car.body.overValidGround == 0) {
        car.input.brake = 1;
    }

    // step 7 substep count ceil of frame time over the fixed step
    int substeps = static_cast<int>(std::ceil(car.frameDt / car.fixedStepSeconds));
    if (substeps < 0) substeps = 0;
    float substepDt = (substeps > 0) ? car.frameDt / static_cast<float>(substeps) : car.frameDt;
    int wheelsInAir = body_wheels_on_ground_count(car.body.wheels); // local 794 wheels without load local 7a8 speedCappedKmh capped below by max speed stat
    float speedCappedKmh = car.speedKmh;

    // step 8 the launch kick else the airborne flag arms with four wheels in the air else the airborne branch
    if (game.engineKickActive == 1) {
        // 0x49C54B car launch pad kick 0x497190 armed it the up push decays until under 0 1
        car_apply_engine_force(car, 0.0f, 0.0f, game.engineKickValue);
        game.engineKickValue = game.engineKickDecay * game.engineKickValue;
        if (game.engineKickValue < kEngineKickFloor) game.engineKickActive = 0;
    } else if (car.airborneFlag == 0.0f) {
        bool arm = false;
        if (wheelsInAir >= 4 && car.driftState == 0) { // 0x49C58E all four wheels in the air and no drift
            char regeTag[5];
            world_decode_surface_tag_bytes(regeTag);
            char lavaTag[5];
            world_decode_surface_tag_digits(lavaTag);
            if (car_name_match_count(car, regeTag) <= 0 && car_name_match_count(car, lavaTag) <= 0) arm = true;
        }
        if (arm) {
            car.airborneFlag = 1.0f;
            car.airTimeScale = 1.0f;
            game.airKickRamp = 0.0f;
            car.airPeakHeight = car.posZ; // car 0x371C takes car 0x324C the height
        }
    } else {
        // 0x49C69A the special theme ramps a downward push while no effect runs
        if (game.themeSpecialRow != 0 && car.effect.activeCode == 0) {
            game.airKickRamp += kAirKickStep;
            bool push = false;
            if (game.airKickRamp > kAirKickCap) {
                game.airKickRamp = kAirKickCap;
                push = true;
            } else if (game.airKickRamp > kAirKickPushGate) {
                push = true;
            }
            if (push) car_apply_engine_force(car, 0.0f, 0.0f, -game.airKickRamp);
        }
        if (car.airPeakHeight < car.posZ) car.airPeakHeight = car.posZ;
        car.airTimeScale *= kAirTimeScaleDecay;
        if (wheelsInAir < 2) { // 0x49C743 landed at most one wheel without load
            float drop = car.airPeakHeight - car.posZ;
            if (drop < kZero) drop = -car.airPeakHeight - car.posZ;
            float clampedDrop = kAirborneLandingGate;
            bool fire = false;
            if (kAirborneLandingGate < drop) {
                fire = true;
            } else {
                clampedDrop = drop;
                if (kOne < drop) fire = true;
            }
            if (fire) {
                if (game.hooks.spawnParticle) {
                    float z = (kLandingHeightScale - clampedDrop * kAirborneLandingGate) + car.posZ;
                    game.hooks.spawnParticle(game.hooks.user, carIndex, 3, car.posX, car.posY, z);
                }
                car.landedFlag = 1;
                car.landedAtMs = nowMs;
            }
            car.airborneFlag = 0.0f;
            game.airKickRamp = 0.0f;
        }
    }

    // 0x49C85E the coast flag stays clear only with nothing pressed under two wheels in the air no boost no drift
    bool coastFlag = true;
    if (car.input.accel == 0 && car.input.stuck == 0 && car.input.steerLeft == 0 && car.input.steerRight == 0 &&
        wheelsInAir < 2 && car.boostState == 0 && car.driftState == 0) {
        coastFlag = game.accelKeyRawHeld != 0; // 0x49C884 the slot 0 key itself still counts
    }

    // step 9 per wheel surface index 0x49C8A4 the probe points at car 0x3274 feed the query
    int wheelSurfaceIndex[4];
    for (int i = 0; i < 4; ++i) {
        wheelSurfaceIndex[i] = world_wheel_surface_index(car.body.groundQuery, car.wheelProbePoint[i].x,
                                                           car.wheelProbePoint[i].y, game.worldReady != 0);
    }

    // 0x49C8BB the friction pair front wheels times the front grip base rear wheels times the rear grip base
    float frictionPair[2];
    for (int pair = 0; pair < 2; ++pair) {
        float fa = world_surface_friction(wheelSurfaceIndex[pair * 2]);
        float fb = world_surface_friction(wheelSurfaceIndex[pair * 2 + 1]);
        float extra = car.frictionExtra;
        float gripBase = (pair == 0) ? car.gripBaseFront : car.gripBaseRear;
        frictionPair[pair] = (extra + extra + fb + fa) * kFrictionPairHalf * gripBase;
    }

    // step 10 drift update
    car_drift_update(game, carIndex, nowMs);

    if (car.driftState != 0) {
        frictionPair[0] *= (car.input.accel == 0) ? kFrictionPairDriftOffGas : kFrictionPairDriftGas;
    }
    if (car.slowFlag == 1) {
        frictionPair[0] *= kFrictionPairSlow;
        frictionPair[1] *= kFrictionPairSlow;
    }
    if (game.viewMode == 2) {
        frictionPair[0] *= kFrictionPairRearView;
        frictionPair[1] *= kFrictionPairRearView;
    }
    car_effect_update(game, carIndex, nullptr, nullptr, nowMs);
    // 0x49C9AA car apply force if valid 0x499000 is body set tire grip 0x4EC130 with the pair every tick
    body_set_tire_grip(car.body, frictionPair[0], frictionPair[1]);

    // 0x49C9D6 per wheel grip wire stat 12 car 0x3478 times the surface grip plus the extra times 0 2
    for (int i = 0; i < 4; ++i) {
        if (wheelSurfaceIndex[i] >= 0 && wheelSurfaceIndex[i] < 9) {
            float grip = world_surface_grip(wheelSurfaceIndex[i]);
            car.wheelsTick[static_cast<size_t>(i)].grip =
                stat_total(car.stats, KartStatIndex::Grip) * (grip + car.gripExtra) * kGripExtraScale;
        }
    }

    // 0x49CA05 the surface contact drag per loaded wheel byte 0 then the standings row then the item slot
    game.stoppedFlag = 0;
    float throttle = kOne; // local 7a4
    for (int i = 0; i < 4; ++i) {
        if (car.body.wheels.wheelOnGround[static_cast<size_t>(i)] == 0) {
            float drag = world_surface_contact_drag(wheelSurfaceIndex[i]);
            throttle -= (drag + car.contactDragExtra) * kSmoothQuarter;
        }
    }
    int row = standings_local_row_index(game); // 0x49CA45 the standings table 0x2EBD6A0 the local row
    if (row >= 0 && row < 30) {
        throttle *= standings_rank_throttle_multiplier(row);
    }
    {
        const ItemSlot* slot = item_slot_lookup(car.items, 0); // 0x49CA6B this is car 0x348C index 0
        if (slot != nullptr && slot->kind == 3 && slot->count < 1) throttle *= kEngineItemPenalty;
    }

    // step 11 max speed cap turn force draft factor the loss above the cap boosts slow drift taper
    float statMax = stat_total(car.stats, KartStatIndex::MaxSpeed) + kOne;
    float maxSpeedCap = clampf(statMax, kOne, kStatClampCeilTwo) * kMaxSpeedToKmh; // local 780
    if (maxSpeedCap < speedCappedKmh) speedCappedKmh = maxSpeedCap;

    float rollAbs = std::fabs(car.rollDeg);
    float rollTurn = std::min(rollAbs * kTurnRateClamp, kRollTurnForceClampCeil);

    float driftMul = (car.driftState != 0) ? kDriftTurnForceMul : kOne;
    // 0x49CB45 wire stat 5 car 0x345C plus one clamped 1 to 2 over 2 snaps to the drift mul
    float statTurn = stat_total(car.stats, KartStatIndex::TurnForce) + kOne;
    float turnClamped = kOne;
    if (kOne <= statTurn) turnClamped = (statTurn > kStatClampCeilTwo) ? driftMul : statTurn;
    float turnForce = (turnClamped + driftMul) * rollTurn;
    game.draftFactor = turnForce; // game 0x1397364 turn force copy local 784 turnForceBase
    float turnForceBase = turnForce * kDriftChargeFloorScale + game.tuning.tuning_2;

    if (speedCappedKmh > kDriftExtraTorqueSpeedKmh && car.driftState != 0) {
        turnForceBase += game.tuning.tuning_2 * kTickDeadzoneStep * (kOne - game.driftStartBoost);
    }

    // 0x49CBDB the literals are 0x5EB710 150 and 0x5EB714 30 read only in this call
    float draft = car_draft_factor(game, carIndex, kDraftMaxDistance, kDraftConeHalfAngleDeg);
    game.draftFactor = draft; // game 0x1397360
    float cap = turnForceBase;
    if (draft <= kZero || speedCappedKmh <= kRespawnReprobeDistance || car.reverseFlag != 0 ||
        game.raceMode == 0xf || game.raceMode == 0x11) {
        car.draftActiveFlag = 0;
    } else {
        car.draftActiveFlag = 1;
        cap = game.tuning.tuning_2 * kDriftChargeFloorScale * draft + turnForceBase;
    }
    if (maxSpeedCap < cap) cap = maxSpeedCap;
    if (cap < speedCappedKmh) throttle -= (speedCappedKmh - cap) * kThrottleSpeedGapScale; // 0x49CC50

    if (wheelsInAir > 2) { // 0x49CC9A more than two wheels in the air
        if (car.boostState == 0) {
            throttle *= kEngineDragAirborneNoBoost;
        } else if (car.boostKind != 0) {
            throttle *= kEngineDecayAirborneBoost;
        }
    }
    if (car.slowFlag == 1) throttle *= kTurnForceSlowFlagMul;

    if (speedCappedKmh > kDriftExtraTorqueSpeedKmh && car.driftState != 0) {
        float g = std::fabs(car.driftGaugeSmoothed);
        float capFrac = std::min(g / kDriftGaugeCap * kEngineDriftStartCapScale, kEngineDriftStartCapScale);
        throttle = (kOne - capFrac * game.driftStartBoost) * throttle;
        game.driftStartBoost *= kDriftStartBoostDecay;
    }

    // 0x49CD40 coasting airborne tapers the throttle or stops the car under speed 1
    if (!coastFlag) {
        if (car.speed >= kOne) {
            throttle *= (car.speed >= kSteerScaleThree) ? kEngineTaperHighSpeed : kEngineTaperMidSpeed;
        } else {
            game.stoppedFlag = 1;
            throttle = 0.0f;
        }
    }

    throttle *= car.throttleJitter;
    float newJitter = (car.frameDt * kGearSolverDtBlendA + kOne) * car.throttleJitter;
    car.throttleJitter = (newJitter > kOne) ? 1.0f : newJitter;
    car.throttleFactorLast = throttle;

    // 0x49CDDA the velocity at car 0x25FC scales by wire stat 0 car 0x3448 gain clamped 1 to 1 01
    {
        float gain = stat_total(car.stats, KartStatIndex::BodySetupInput) * kVelocityScaleStat + kOne;
        if (gain < kOne) gain = kOne;
        else if (gain > kVelocityScaleCeil) gain = kVelocityScaleCeil;
        body_vec3_scale(car.body.wheels.velocity, gain * throttle);
    }

    // 0x49CE34 four wheels in the air not drifting and turning pushes 0 3 along the heading or its reverse
    if (wheelsInAir > 3 && car.driftState == 0 &&
        (car.yawRateBody > kYawRateDeadzoneHigh || car.yawRateBody < kYawRateDeadzoneLow)) {
        float heading = car.yawDeg - kAxisCorrection90Deg;
        heading += (car.yawRateBody >= kZero) ? kAxisCorrection90Deg : -kAxisCorrection90Deg;
        float px = 0.0f;
        float py = 0.0f;
        math_dir_from_heading(kLateralPushMagnitude, heading, &px, &py);
        car_apply_engine_force(car, px, py, 0.0f);
    }

    // step 12 the speed curve scales the gravity then the substep push while no effect runs local 7ac curveTerm
    float curveTerm = kOne;
    {
        float gravityScale = 0.0f;
        if (car.effect.activeCode == 0) {
            int idx = speed_curve_index(speedCappedKmh); // 0x49CEEC kmh minus 30 times half
            gravityScale = body_durability_curve_value(idx) * kDurabilitySoundScale;
        }
        body_apply_durability_scale(car.body.wheels, gravityScale);
        if (car.effect.activeCode == 0) {
            // 0x49CF48 the raw capped kmh truncated and clamped 0 to 99 no minus 30 no half here
            int idx = static_cast<int>(speedCappedKmh);
            if (idx < 0) idx = 0;
            if (idx > 99) idx = 99;
            curveTerm = kOne - body_durability_curve_value(idx);
        }
    }

    // step 13 substep loop gravity the local z push body integrate collision gate body step world
    for (int sub = 0; sub < substeps; ++sub) {
        // 0x49CF90 world z push tuning 0x5EB6F0 times minus 0 16 through car apply engine force
        car_apply_engine_force(car, 0.0f, 0.0f, game.tuning.tuning_0 * kGravityPerSubstepScale);

        // 0x49CFB3 minus the third column of the body R at car 0x2608 times the curve term scaled
        {
            float scale = curveTerm * game.tuning.tuning_1 * kEngineForcePrepC;
            const Mat3& r = car.body.wheels.orientationR;
            body_apply_force(car.body, -(r.m[0][2] * scale), -(r.m[1][2] * scale), -(r.m[2][2] * scale));
        }

        body_integrate(car.body, track);

        if (car.body.collisionHappened == 1) {
            // 0x49D073 the cell under wheel 2 named REGEN skips the response
            const int rawIds[5] = {1743, 1491, 1554, 1533, 1743};
            char decoded[6];
            car_decode_id5(decoded, rawIds);
            bool touchesRegen = car_node_name_is(game, carIndex, 2, decoded);
            if (!touchesRegen) {
                car_substep_collision_response(game, carIndex, substepDt);
            }
        }

        // 0x49D0A5 z copied x and y negated from the body position at car 0x21E0
        car.posZ = car.body.position.z;
        car.posX = -car.body.position.x;
        car.posY = -car.body.position.y;

        if (game.raceMode != 0xf && game.raceMode != 0x11) {
            world_car_push_apart_tick(game, carIndex); // 0x49D0CB the result is dropped
        }

        if (car.body.overValidGround == 0 || car.forceRespawnFlag == 1) {
            car_gear_clamp(car.body, 0);
            if (car.forceRespawnFlag == 1) game.stoppedFlag = 1;
        }

        // 0x49D12D the six flags at game 0x18 as floats and car 0x9D4 as the flag zero off valid ground
        {
            // 0x49D12E the pointer is game 0x18 so channel 2 is 0x20 the right turn and channel 3 is 0x24
            float in[6] = {static_cast<float>(car.input.accel),      static_cast<float>(car.input.brake),
                           static_cast<float>(car.input.steerRight), static_cast<float>(car.input.steerLeft),
                           static_cast<float>(game.stoppedFlag),     static_cast<float>(car.input.stuck)};
            int flag = (car.body.overValidGround == 0 || car.forceRespawnFlag == 1) ? 0 : car.substepInputFlag;
            body_step_world(car.body, substepDt, in, flag);
        }
    }

    // step 14 stuck probe on x y and the yaw then finalize once per tick
    bool stuckProbe = world_stuck_probe(car.body.groundQuery, car.posX, car.posY, car.yawDeg);
    if (stuckProbe) {
        if (game.stuckWatch.phase == 0) {
            game.stuckWatch.phase = 1;
            game.stuckWatch.armedAtMs = nowMs;
        } else if (game.stuckWatch.phase == 1 && nowMs - game.stuckWatch.armedAtMs > 100) {
            game.stuckWatch.stuckFlag = 1;
            game.stuckWatch.phase = 2;
        }
    } else {
        game.stuckWatch.phase = 0;
        game.stuckWatch.stuckFlag = 0;
    }
    body_finalize_wheels(car.body);

    float bumpAccum = 0.0f;
    for (int i = 0; i < 4; ++i) {
        float compression = car.body.wheels.tireScratch[static_cast<size_t>(i)].compression *
                             kSuspensionCompressionScale;
        float signed_ = (kSuspensionCompressionFloor <= compression) ? std::min(compression, kOne) : -1.0f;
        bumpAccum += signed_ * kSuspensionBumpWeight;
    }

    // step 15 0x49D3C9 the drift slip turn car 0x35B0 is wire stat 9 car 0x346C times the smoothed gauge
    {
        float driftSteer = stat_total(car.stats, KartStatIndex::DriftSteer) * kDriftSteerScale + kDriftSteerClampFloor;
        driftSteer = clampf(driftSteer, kDriftSteerClampFloor, kDriftSteerClampCeil);
        float slip = driftSteer * car.driftGaugeSmoothed;
        // 0x49D41D the effects 100 400 700 take the wobble term off the slip turn
        int code = car.effect.activeCode;
        if (code == 100 || code == 400 || code == 700) slip -= car.effect.wobbleAmplitude;
        car.driftSlipDeg = slip;
    }
    // the lean update builds the car matrix from the body R the slip turn and the lean
    Vec3 up{0.0f, 0.0f, 1.0f};
    car_effect_lean_update(game, carIndex);

    // 0x49D4D4 car 0x32E4 takes wheel set 0x520 the mean front steer tangent the lateral push reads it
    car.yawRateBody = car.body.wheels.steerAverage;

    // 0x49D4F5 the visual spin angle per wheel minus the body spin a quarter of it on a loaded wheel
    if (car.landedFlag == 1 || car.speed > kOne) {
        for (int i = 0; i < 4; ++i) {
            float spin = -car.body.wheels.wheel[static_cast<size_t>(i)].spinAngle;
            if (car.body.wheels.wheelOnGround[static_cast<size_t>(i)] == 0) spin *= kSmoothQuarter;
            car.wheelsTick[static_cast<size_t>(i)].spinAngle = spin;
        }
    }

    // 0x49D5A0 the body pitch offset car 0x3738 eases a quarter toward the tyre peak sum term
    if (car.attachedNodeCount < 5) {
        float peakSum = car.body.wheels.tireForceResult[0][0] + car.body.wheels.tireForceResult[1][0] +
                        car.body.wheels.tireForceResult[2][0] + car.body.wheels.tireForceResult[3][0];
        float target = (kTickDeadzoneStep - peakSum * kShakeSumScale) * kAirKickCap;
        target = clampf(target, kZero, kCrashRecoveryGrowthCap);
        car.bodyPitchOffset += (target - car.bodyPitchOffset) * kSmoothQuarter;

        // step 16 wheel matrices 0x49D640 the node position at car 0x3528 the bump the steer and the spin
        for (int i = 0; i < 4; ++i) {
            TickWheelState& w = car.wheelsTick[static_cast<size_t>(i)];
            float bump = 0.0f;
            if (wheelsInAir < 2 && car.speedKmh > kAirborneLandingGate &&
                world_wheel_bump_slot(game.itembiteTable, carIndex) < 0) {
                // 0x49D6B3 rand mod 600 minus 300 times the kmh capped 100 times the wheel grip times 4e minus 6
                float kmh = (car.speedKmh > kWheelBumpCap) ? kWheelBumpCap : car.speedKmh;
                bump = static_cast<float>(std::rand() % 600 - 300) * kmh * w.grip * kWheelBumpFinalScale;
            }
            float squash = kOne;
            if (bump > kZero && car.driftState == 0) squash = bump * kSmoothQuarter + kOne;
            // 0x49D854 the front wheels steer by the front steer tangent times 6 twice while drifting clamp 30 deg
            float steer = 0.0f;
            if (i < 2) {
                steer = car.yawRateBody * kYawRateToWheelAngle;
                if (car.driftState != 0) steer *= kYawRateToWheelAngle;
                steer = clampf(steer, kWheelAngleClampFloor, kWheelAngleClampCeil);
            }
            w.steerAngle = steer;
            w.squash = squash;
            Quat steerQuat = body_quat_from_axis_angle(up, steer);
            Vec3 axle{0.0f, 1.0f, 0.0f};
            Quat spinQuat = body_quat_from_axis_angle(axle, w.spinAngle);
            Quat local = body_quat_multiply(steerQuat, spinQuat);
            Mat3 localRot;
            body_quat_to_matrix(localRot, local);
            // world is the car matrix times the local the translation is the node plus the bump minus the offset
            const Vec3& node = car.wheelNodePos[i];
            Vec3 localPos{node.x, node.y, node.z + bump - car.bodyPitchOffset};
            Vec3 worldPos;
            body_vec3_transform(worldPos, localPos, car.carMatrix.rotation, car.carMatrix.translation);
            body_mat3_multiply(w.matrix.rotation, car.carMatrix.rotation, localRot);
            w.matrix.translation = worldPos;
            if (game.hooks.setNodeTransform) {
                game.hooks.setNodeTransform(game.hooks.user, carIndex, i, w.matrix.rotation, w.matrix.translation);
            }
        }
    }
    // step 20 scene node transforms folded into the hook calls above and below
    if (game.hooks.setNodeTransform) {
        game.hooks.setNodeTransform(game.hooks.user, carIndex, -1, car.carMatrix.rotation,
                                     car.carMatrix.translation);
    }

    // step 17 velocity acceleration speed and km per hour the probe points copied from the body
    float invDt = (car.frameDt > 0.0f) ? kOne / car.frameDt : 0.0f;
    car.yawRateOut = (car.yawDeg - car.prevSubstepYawDeg) * invDt;
    float newVelX = (car.posX - car.prevPosX) * invDt;
    float newVelY = (car.posY - car.prevPosY) * invDt;
    float newVelZ = (car.posZ - car.prevPosZ) * invDt;
    car.accelX = (newVelX - car.prevVelX) * invDt;
    car.accelY = (newVelY - car.prevVelY) * invDt;
    car.accelZ = (newVelZ - car.prevVelZSnapshot) * invDt;
    car.velX = newVelX;
    car.velY = newVelY;
    car.velZ = newVelZ;
    car.speed = std::sqrt(car.velX * car.velX + car.velY * car.velY + car.velZ * car.velZ);
    float kmh = car.speed * kSpeedToKmh;
    car.speedKmh = (kmh < 0.0f) ? 0.0f : kmh;
    car.distanceTraveled += car.speed * car.frameDt;
    // the slip angle is the yaw minus the motion angle yaw A drives toward minus cos A sin A
    if (car.speed > kOne) {
        float motionDeg = std::atan2(car.velY, -car.velX) * kRadToDeg;
        float slip = car.yawDeg - motionDeg;
        math_wrap_angle_signed180(&slip);
        car.slipAngleDeg = slip;
    } else {
        car.slipAngleDeg = 0.0f;
    }

    // 0x49DD1C car 0x3274 stride 0xC takes body 0x2120 with x and y negated z kept
    for (int i = 0; i < 4; ++i) {
        const Vec3& a = car.body.hubWorld[i];
        car.wheelProbePoint[i] = Vec3{-a.x, -a.y, a.z};
    }

    // step 18 0x49DE10 the unit square corners through the body R give yaw pitch and roll
    {
        const Mat3& r = car.body.wheels.orientationR;
        const float sx[4] = {1.0f, 1.0f, -1.0f, -1.0f};
        const float sy[4] = {1.0f, -1.0f, 1.0f, -1.0f};
        Vec3 corner[4];
        Vec3 unitZ;
        body_vec3_set(unitZ, 0.0f, 0.0f, 1.0f);
        Vec3 zero;
        body_vec3_set(zero, 0.0f, 0.0f, 0.0f);
        for (int i = 0; i < 4; ++i) {
            Vec3 sign;
            body_vec3_set(sign, sx[i], sy[i], 0.0f);
            Vec3 v;
            body_vec3_madd(v, sign, unitZ, 0.0f);
            body_vec3_transform(corner[i], v, r, zero);
        }
        float frontX = (corner[1].x + corner[0].x) * kHalf;
        float frontY = (corner[1].y + corner[0].y) * kHalf;
        float frontZ = (corner[1].z + corner[0].z) * kHalf;
        float rearX = (corner[3].x + corner[2].x) * kHalf;
        float rearY = (corner[3].y + corner[2].y) * kHalf;
        float rearZ = (corner[3].z + corner[2].z) * kHalf;
        float sidePlusZ = (corner[2].z + corner[0].z) * kHalf;
        float sideMinusZ = (corner[3].z + corner[1].z) * kHalf;
        float dy = rearY - frontY;
        float dx = rearX - frontX;

        // 0x49DFC7 dy is pushed first so dx is the first argument the yaw follows by a quarter
        float yawDelta = (math_atan2_deg(dx, dy) + kAxisCorrection90Deg) - car.yawDeg;
        math_wrap_angle_signed180(&yawDelta);
        if (kTickDeadzoneStep < yawDelta || yawDelta < kYawPitchRollDeadzoneLow) {
            car.yawDeg = yawDelta * kSmoothQuarter + car.yawDeg;
            math_wrap_angle_360(&car.yawDeg);
        }
        float base = math_hypot2d(dx, dy);
        float pitchDelta = (math_atan2_deg(base, rearZ - frontZ) - kAxisCorrection90Deg) - car.pitchDeg;
        math_wrap_angle_signed180(&pitchDelta);
        if (kTickDeadzoneStep < pitchDelta || pitchDelta < kYawPitchRollDeadzoneLow) {
            car.pitchDeg = pitchDelta * kPitchRollSmoothBlend + car.pitchDeg;
        }
        float rollDelta = (math_atan2_deg(base, sideMinusZ - sidePlusZ) - kAxisCorrection90Deg) - car.rollDeg;
        math_wrap_angle_signed180(&rollDelta);
        if (kTickDeadzoneStep < rollDelta || rollDelta < kYawPitchRollDeadzoneLow) {
            car.rollDeg = rollDelta * kPitchRollSmoothBlend + car.rollDeg;
        }
    }

    float rpmSource = car.body.wheels.gearRatio * kRadPerSecToRpm;
    car.rpm = clampf(rpmSource, kRpmClampFloor, kRpmClampCeil);
    car.gearMirror = static_cast<float>(car.body.wheels.currentGear);
    car.rpmMirrorA = car.body.rpmRatio;
    car.rpmMirrorB = car.body.finalLerp;

    // step 19 motion send 0x40 in stages 9 and 0xB
    if (game.raceMode == 0xb || game.raceMode == 9) {
        MotionSendInputs in;
        in.pos[0] = car.posX;
        in.pos[1] = car.posY;
        in.pos[2] = car.posZ;
        in.vel[0] = car.velX;
        in.vel[1] = car.velY;
        in.vel[2] = car.velZ;
        in.yawDeg = car.yawDeg;
        in.driftGaugeSmoothed = car.driftGaugeSmoothed;
        in.driftGauge = car.driftGauge;
        in.rpm = car.rpm;
        in.frameDt = car.frameDt;
        in.miniTurboBoost = car.boostState != 0 && car.boostKind == 0;
        in.itemBoost = car.boostState != 0 && car.boostKind != 0;
        in.reverse = car.reverseFlag == 1;
        in.drift = car.driftState != 0;
        in.miniTurboStage1 = car.miniTurboStage == 1;
        in.turnState = car.turnState;
        MotionSend0x40 packet = motion_send_0x40(in); // socket send is a host concern out of scope
        (void)packet;
    }

    // step 21 camera and shake sums then boost respawn mission and rival updates
    car.shakeSumX = bumpAccum * kShakeSumScale;
    car.shakeSumY = bumpAccum * kShakeSumScale;
    car.shakeSumZ = bumpAccum * kShakeSumScale;

    car_boost_update(game, carIndex, nowMs);
    car_respawn_state_machine(game, carIndex, track, nowMs);
    respawn_crash_recovery_update(game, carIndex, nowMs);
    if (game.raceMode != 0xb && game.raceMode != 0xf && game.raceMode != 0x11) {
        car_mission_rally_update(game, carIndex, nowMs);
    }
    car_boost_speed_gate(game, carIndex, nowMs);
    car_rival_nearby_cue(game, carIndex, nowMs); // 0x49E4C7 the lean update ran once after the car matrix

    // step 22 vehicle kind 5 spins its extra part
    if (car.vehicleKind == 5) {
        float delta = car.frameDt * car.speedKmh * kWheelBumpFinalScale;
        if (car.reverseFlag == 1) {
            car.spinAccumulator -= delta;
            if (car.spinAccumulator < 0.0f) car.spinAccumulator += car.spinPeriod;
        } else {
            car.spinAccumulator += delta;
            if (car.spinPeriod <= car.spinAccumulator) car.spinAccumulator -= car.spinPeriod;
        }
    }

    // save previous state for the next tick's substep restore and derivative math
    car.prevPosX = car.posX;
    car.prevPosY = car.posY;
    car.prevPosZ = car.posZ;
    car.prevVelX = car.velX;
    car.prevVelY = car.velY;
    car.prevVelZSnapshot = car.velZ;
    car.prevSubstepYawDeg = car.yawDeg;
}

} // namespace KnC Kart Client
