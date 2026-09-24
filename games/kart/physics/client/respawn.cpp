#include "respawn.h"

#include "gimmicks.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "boost.h"
#include "constants.h"
#include "drift.h"
#include "effects.h"
#include "math_helpers.h"

namespace KnC::Kart::Client {

int car_name_match_count_tick(CarState& car, const char* tag) {
    // car name match count 0x4A1380 the wheel probe points whose cell name equals the tag
    float wx[4];
    float wy[4];
    for (int i = 0; i < 4; ++i) {
        wx[i] = car.wheelProbePoint[i].x;
        wy[i] = car.wheelProbePoint[i].y;
    }
    return world_wheel_surface_tag_count(car.body.groundQuery, wx, wy, tag);
}

bool respawn_checkpoint_nearest_on_list(const CheckpointList& lists, int listIndex, float x, float y,
                                         float z, int* outPointIndex, float* outDistance) {
    if (listIndex < 0 || listIndex >= 4) return false;
    const std::vector<CheckpointPoint>& pts = lists.lists[listIndex];
    if (pts.empty()) return false;
    int best = -1;
    float bestSq = 0.0f;
    for (size_t i = 0; i < pts.size(); ++i) {
        float dx = pts[i].x - x, dy = pts[i].y - y, dz = pts[i].z - z;
        float sq = dx * dx + dy * dy + dz * dz; // FUN 0044D950 squared distance guess
        if (best < 0 || sq < bestSq) {
            best = static_cast<int>(i);
            bestSq = sq;
        }
    }
    if (outPointIndex) *outPointIndex = best;
    if (outDistance) *outDistance = std::sqrt(bestSq);
    return best >= 0;
}

bool respawn_checkpoint_nearest_all(const CheckpointList& lists, float x, float y, float z,
                                     int* outListIndex, int* outPointIndex, float* outDistance) {
    int bestList = -1, bestPoint = -1;
    float bestDist = 0.0f;
    for (int list = 0; list < 4; ++list) {
        int point = -1;
        float dist = 0.0f;
        if (respawn_checkpoint_nearest_on_list(lists, list, x, y, z, &point, &dist)) {
            if (bestList < 0 || dist < bestDist) {
                bestList = list;
                bestPoint = point;
                bestDist = dist;
            }
        }
    }
    if (bestList < 0) return false;
    if (outListIndex) *outListIndex = bestList;
    if (outPointIndex) *outPointIndex = bestPoint;
    if (outDistance) *outDistance = bestDist;
    return true;
}

bool respawn_recovery_point_find(const CheckpointList& lists, float x, float y, float z,
                                  int* outListIndex, int* outPointIndex) {
    // FUN 00485970 extra validity filter not traced plain nearest stands in
    float dist = 0.0f;
    return respawn_checkpoint_nearest_all(lists, x, y, z, outListIndex, outPointIndex, &dist);
}

void respawn_crash_recovery_update(GameState& game, int carIndex, int64_t nowMs) {
    CarState& car = game.cars[carIndex];
    ProgressWatchdogState& w = game.progressWatchdog;

    if (game.checkpoints.lists[0].empty() && game.checkpoints.lists[1].empty() &&
        game.checkpoints.lists[2].empty() && game.checkpoints.lists[3].empty()) {
        return; // no checkpoint list loaded watchdog stays idle
    }

    if (w.state == 0) {
        int list = 0, point = 0;
        float dist = 0.0f;
        if (respawn_checkpoint_nearest_all(game.checkpoints, car.posX, car.posY, car.posZ, &list, &point,
                                            &dist)) {
            w.nearestListIndex = list;
            w.progressIndex = point;
            w.distance = dist;
            w.lastDirectionChangeMs = nowMs;
            w.state = 1;
        }
        return;
    }

    int point = 0;
    float dist = 0.0f;
    if (!respawn_checkpoint_nearest_on_list(game.checkpoints, w.nearestListIndex, car.posX, car.posY, car.posZ,
                                            &point, &dist)) {
        return;
    }
    w.distance = dist;
    // 0x4A0A60 a car over 80 from its list takes the nearest point of all four lists again
    if (dist > kRespawnReprobeDistance &&
        !respawn_checkpoint_nearest_all(game.checkpoints, car.posX, car.posY, car.posZ, &w.nearestListIndex,
                                        &point, &dist)) {
        return;
    }
    if (game.worldTrackId == kBattleTrackId) return;
    const int step = std::abs(w.progressIndex - point);
    // only a falling index counts a car that stands still or drives on never escalates
    const bool backward = step < kRespawnIndexStepLimit && point < w.progressIndex;
    const bool forward = step < kRespawnIndexStepLimit && point > w.progressIndex;
    if (w.state == 1) {
        if (backward) {
            w.state = 2;
            w.lastDirectionChangeMs = nowMs;
        }
    } else if (w.state == 2) {
        if (backward && nowMs - w.lastDirectionChangeMs >= kRespawnDirectionDebounceMs) {
            w.lastDirectionChangeMs = nowMs;
            const bool loopA = game.worldTrackId == kRespawnLoopTrackA && w.nearestListIndex == 0 &&
                               point >= kRespawnLoopTrackAFirst && point <= kRespawnLoopTrackALast;
            const bool loopB = game.worldTrackId == kRespawnLoopTrackB && w.nearestListIndex == 0 &&
                               point >= kRespawnLoopTrackBFirst && point <= kRespawnLoopTrackBLast;
            w.state = loopA || loopB ? 1 : 100;
        } else if (forward) {
            w.state = 1;
        }
    } else if (w.state == 100) {
        if (forward) {
            w.state = 1;
        } else if (backward && nowMs - w.lastDirectionChangeMs > kRespawnCommitTimeoutMs) {
            w.state = 0;
            car.recoveryPosX = car.posX;
            car.recoveryPosY = car.posY;
            car.recoveryPosZ = car.posZ;
            car.trackProgress = 100;
            car.checkpointTimestampMs = static_cast<uint64_t>(nowMs);
        }
    }
    w.progressIndex = point;
}

void car_respawn_state_machine(GameState& game, int carIndex, const ColTrack& track, int64_t nowMs) {
    CarState& car = game.cars[carIndex];
    if (game.sessionRunning == 0) return;
    switch (car.trackProgress) {
        case 0: {
            // 0x4A1520 the idle scan reads the cell under each wheel a BOOST NNN cell is a boost pad
            int code = car.effect.activeCode;
            if (code == 500 || code == 600 || code == 900) break;
            if (!car.body.groundQuery.active) break;
            for (int wheel = 0; wheel < 4; ++wheel) {
                const Vec3& p = car.wheelProbePoint[wheel];
                const char* cell = world_wheel_query_surface(car.body.groundQuery, p.x, p.y);
                if (cell == nullptr) continue;
                int padRow = world_cell_boost_pad_index(cell);
                if (padRow < 0) continue;
                // 0x4A3157 respawn checkpoint set with minus one keeps the cell name until no wheel touches it
                car.trackProgress = -1;
                car.checkpointTimestampMs = static_cast<uint64_t>(nowMs);
                std::strncpy(car.boostPadCellName, cell, sizeof(car.boostPadCellName) - 1);
                car.boostPadCellName[sizeof(car.boostPadCellName) - 1] = 0;
                if (padRow < static_cast<int>(game.boostRows.size())) {
                    const GimmickBoostRow& rowData = game.boostRows[static_cast<size_t>(padRow)];
                    if (rowData.kind == BOOST_KIND_LAUNCH_PAD) {
                        // 0x4A317F the launch pad drops the drift and the gauge then car launch pad kick
                        car_drift_state_set(game, carIndex, 0, nowMs);
                        car.driftGauge = 0.0f;
                        car.driftGaugeSmoothed = 0.0f;
                        car_launch_pad_kick(game, carIndex, rowData.field_1, rowData.field_2, rowData.field_3,
                                            rowData.field_4, 2, track, nowMs);
                    } else {
                        car_boost_start(game, carIndex, rowData.kind, nowMs); // 0x4A31D2
                    }
                }
                break;
            }
            break;
        }
        case -1: {
            // 0x4A14AD state minus one waits until no wheel sits on the pad cell then frees car 0x6BC
            if (car_name_match_count_tick(car, car.boostPadCellName) > 0) break;
            car.trackProgress = 0;
            car.checkpointTimestampMs = static_cast<uint64_t>(nowMs);
            car.boostPadCellName[0] = 0;
            break;
        }
        case 100:
            if (game.hooks.playSound) game.hooks.playSound(game.hooks.user, carIndex, 2, 5);
            car.trackProgress = 0x65;
            car.checkpointTimestampMs = static_cast<uint64_t>(nowMs);
            break;
        case 0x65: {
            int list = 0, point = 0;
            if (respawn_recovery_point_find(game.checkpoints, car.recoveryPosX, car.recoveryPosY,
                                             car.recoveryPosZ, &list, &point)) {
                size_t idx = static_cast<size_t>(list) * static_cast<size_t>(kRespawnGridRowWidth) +
                             static_cast<size_t>(point);
                if (idx < game.respawnGrid.size()) {
                    // 0x4A33ED to 0x4A3431 x y then z plus 3 then the yaw then 0x4A346C places the body
                    const RespawnGridEntry& e = game.respawnGrid[idx];
                    car.posX = e.x;
                    car.posY = e.y;
                    car.posZ = e.z + kRespawnTeleportHeightBias;
                    car.yawDeg = e.yawDeg;
                    body_place_and_probe(car.body, -car.posX, -car.posY, car.posZ, -car.yawDeg, track);
                }
            }
            car_drift_state_set(game, carIndex, 0, nowMs);
            car.trackProgress = 0x66;
            break;
        }
        case 0x66:
            car.trackProgress = 0;
            car.checkpointTimestampMs = 0;
            break;
        default:
            break; // not in the teleport sequence nothing to do
    }
}

bool respawn_follow_lists_load(GameState& game, const std::string& trackDir, std::string& error) {
    // gimmick load follow 0x489730 four files 400 rows each x y z yaw the grid is the same array
    game.respawnGrid.assign(static_cast<size_t>(4) * static_cast<size_t>(kRespawnGridRowWidth), RespawnGridEntry{});
    for (int list = 0; list < 4; ++list) {
        std::vector<GimmickFollowRow> rows;
        game.checkpoints.lists[list].clear();
        if (!gimmick_load_follow(trackDir, list + 1, rows, error)) return false;
        for (size_t i = 0; i < rows.size() && i < static_cast<size_t>(kRespawnGridRowWidth); ++i) {
            CheckpointPoint p;
            p.x = rows[i].field_0;
            p.y = rows[i].field_1;
            p.z = rows[i].field_2;
            p.yawDeg = rows[i].field_3;
            game.checkpoints.lists[list].push_back(p);
            RespawnGridEntry& e = game.respawnGrid[static_cast<size_t>(list) * static_cast<size_t>(kRespawnGridRowWidth) + i];
            e.x = p.x;
            e.y = p.y;
            e.z = p.z;
            e.yawDeg = p.yawDeg;
        }
    }
    return true;
}

void respawn_car_state_reset(CarState& car) {
    // caller sets position separately
    car.body.overValidGround = 0;
    car.forceRespawnFlag = 0;
    car.boostState = 0;
    car.boostDecayStrength = 0.0f;
    car.boostTargetKmh = 0.0f;
    car.boostKind = 0;
    car.driftState = 0;
    car.driftGauge = 0.0f;
    car.driftGaugeSmoothed = 0.0f;
    car.finishedOrSpectating = 0;
    car.slowFlag = 0;
    car.airborneFlag = 0.0f;
    car.airTimeScale = 0.0f;
    car.airPeakHeight = 0.0f;
    for (TickWheelState& w : car.wheelsTick) {
        w.steerAngle = 0.0f;
        w.grip = 0.0f;
    }
    car.bodyPitchOffset = 0.0f;
    effect_end(car);
    car.trackProgress = 0; // off track range per world on track check until the first checkpoint
}

int car_nearest_racing_car(const GameState& game, int carIndex, float maxDistance) {
    // car nearest racing car 0x499AA0 the closest other occupied car with no finish rank inside the range
    const CarState& self = game.cars[static_cast<size_t>(carIndex)];
    int best = -1;
    float bestDist = maxDistance;
    for (int i = 0; i < kCarSlotCount; ++i) {
        const CarState& other = game.cars[static_cast<size_t>(i)];
        if (other.slotOccupied == 0 || i == carIndex || other.finishRank >= 0) continue;
        float dist = math_hypot2d(self.posX - other.posX, self.posY - other.posY);
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

void car_rival_nearby_cue(GameState& game, int carIndex, int64_t nowMs) {
    // car rival nearby cue 0x49A130 local car every 3 s over 30 kmh a racing car inside 30
    CarState& car = game.cars[carIndex];
    if (car.playerOrGhostId != game.localPlayerId) return; // 0x49A14E car 0x744 against DAT 01A20658
    if (nowMs - game.rivalCueMs < kRivalCuePeriodMs) return;
    game.rivalCueMs = nowMs; // game 0x1397348
    if (car.speedKmh < kRivalCueSpeedKmh) return;
    if (car.driverAnimState == kDriverAnimHit || car.driverAnimState == kDriverAnimEleven) return;
    int other = car_nearest_racing_car(game, carIndex, kRivalCueRange);
    if (other < 0) return;
    const CarState& rival = game.cars[static_cast<size_t>(other)];
    float angle = (math_atan2_deg(car.posX - rival.posX, car.posY - rival.posY) - car.yawDeg) +
                  kCollisionAngleFold180;
    math_wrap_angle_360(&angle);
    if (angle > kRivalCueAngleLow && angle < kRivalCueAngleHigh) {
        if (game.hooks.playSound) game.hooks.playSound(game.hooks.user, carIndex, 3, 3);
        if (game.hooks.voiceCue) game.hooks.voiceCue(game.hooks.user, carIndex, 3); // FUN 004815F0
    }
}

void car_launch_pad_kick(GameState& game, int carIndex, float yawDeg, float pushStrength, float kickValue,
                         float kickDecay, int boostKind, const ColTrack& track, int64_t nowMs) {
    // car launch pad kick 0x497190 refused while a kick runs sets the yaw pushes arms the up kick and boosts
    if (game.engineKickActive == 1) return;
    CarState& car = game.cars[static_cast<size_t>(carIndex)];
    car_boost_clear(game, carIndex);
    car.yawDeg = yawDeg;
    body_place_and_probe(car.body, -car.posX, -car.posY, car.posZ, -yawDeg, track);
    car_boost_push(game, carIndex, car.yawDeg, pushStrength);
    game.engineKickValue = kickValue * kLaunchKickScale;
    game.engineKickDecay = kickDecay;
    game.engineKickActive = 1;
    game.engineKickYawDeg = yawDeg;
    car_boost_start(game, carIndex, boostKind, nowMs);
}

void car_mission_rally_update(GameState& game, int carIndex, int64_t nowMs) {
    if (carIndex != game.localCarIndex) return;
    CarState& car = game.cars[carIndex];
    (void)nowMs;
    int list = game.progressWatchdog.nearestListIndex;
    int point = 0;
    float dist = 0.0f;
    if (respawn_checkpoint_nearest_on_list(game.checkpoints, list, car.posX, car.posY, car.posZ, &point,
                                            &dist)) {
        if (point > car.lapCheckpointCounter) {
            car.lapCheckpointCounter = point;
        }
        const std::vector<CheckpointPoint>& pts = game.checkpoints.lists[list];
        if (!pts.empty() && point >= 0 && static_cast<size_t>(point) < pts.size()) {
            const CheckpointPoint& target = pts[static_cast<size_t>(point)];
            car.checkpointBearing = math_atan2_deg(car.posX - target.x, car.posY - target.y);
        }
    }
}

} // namespace KnC Kart Client
