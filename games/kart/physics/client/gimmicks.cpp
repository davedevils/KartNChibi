#include "gimmicks.h"

#include "boost.h"
#include "car_state.h"
#include "constants.h"
#include "effects.h"
#include "math_helpers.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace KnC::Kart::Client {

namespace {

FILE* open_gimmick_file(const std::string& track_dir, const char* file_name, std::string& error) {
    const std::string path = track_dir + "/" + file_name;
    FILE* file = std::fopen(path.c_str(), "r");
    if (!file) {
        error = "cannot open " + path;
    }
    return file;
}

} // namespace

bool gimmick_load_boost(const std::string& track_dir, std::vector<GimmickBoostRow>& out,
                         std::string& error) {
    out.clear();
    FILE* file = open_gimmick_file(track_dir, "boost.ini", error);
    if (!file) return false;

    GimmickBoostRow row;
    while (out.size() < GIMMICK_BOOST_MAX_LINES &&
           std::fscanf(file, "%d,%f,%f,%f,%f", &row.kind, &row.field_1, &row.field_2,
                        &row.field_3, &row.field_4) == 5) {
        out.push_back(row);
    }
    std::fclose(file);
    return true;
}

bool gimmick_load_itembox(const std::string& track_dir, std::vector<GimmickItemboxRow>& out,
                           std::string& error) {
    out.clear();
    FILE* file = open_gimmick_file(track_dir, "itembox.ini", error);
    if (!file) return false;

    GimmickItemboxRow row;
    while (out.size() < GIMMICK_ITEMBOX_MAX_LINES &&
           std::fscanf(file, "%f,%f,%f", &row.x, &row.y, &row.z) == 3) {
        out.push_back(row);
    }
    std::fclose(file);
    return true;
}

bool gimmick_load_itembite(const std::string& track_dir, std::vector<GimmickItembiteRow>& out,
                            std::string& error) {
    out.clear();
    FILE* file = open_gimmick_file(track_dir, "itembite.ini", error);
    if (!file) return false;

    GimmickItembiteRow row;
    while (out.size() < GIMMICK_ITEMBITE_MAX_LINES &&
           std::fscanf(file, "%f,%f,%f", &row.x, &row.y, &row.z) == 3) {
        out.push_back(row);
    }
    std::fclose(file);
    return true;
}

bool gimmick_load_itemdrum(const std::string& track_dir, std::vector<GimmickItemdrumRow>& out,
                            std::string& error) {
    out.clear();
    FILE* file = open_gimmick_file(track_dir, "itemdrum.ini", error);
    if (!file) return false;

    // 0x48af5f the loop leaves on EOF alone so a three column row keeps its yaw slot as it was
    GimmickItemdrumRow row;
    while (out.size() < GIMMICK_ITEMDRUM_MAX_LINES) {
        row.field_3 = 0.0f;
        if (std::fscanf(file, "%f,%f,%f,%f", &row.x, &row.y, &row.z, &row.field_3) == EOF) break;
        out.push_back(row);
    }
    std::fclose(file);
    return true;
}

bool gimmick_load_follow(const std::string& track_dir, int32_t index,
                          std::vector<GimmickFollowRow>& out, std::string& error) {
    out.clear();
    char file_name[32];
    std::snprintf(file_name, sizeof(file_name), "follow_%02d.ini", index);

    FILE* file = open_gimmick_file(track_dir, file_name, error);
    if (!file) return false;

    GimmickFollowRow row;
    while (out.size() < GIMMICK_FOLLOW_MAX_LINES &&
           std::fscanf(file, "%f,%f,%f,%f", &row.field_0, &row.field_1, &row.field_2,
                        &row.field_3) == 4) {
        out.push_back(row);
    }
    std::fclose(file);
    return true;
}

bool gimmick_load_start(const std::string& track_dir, std::vector<GimmickStartRow>& out,
                         std::string& error) {
    out.clear();
    FILE* file = open_gimmick_file(track_dir, "start.ini", error);
    if (!file) return false;

    GimmickStartRow row;
    while (out.size() < GIMMICK_START_MAX_LINES &&
           std::fscanf(file, "%f,%f,%f,%f", &row.x, &row.y, &row.z, &row.heading) == 4) {
        out.push_back(row);
    }
    std::fclose(file);
    return true;
}

GimmickDrumResult itemdrum_hit_test(const std::vector<GimmickItemdrumRow>& rows,
                                     const std::vector<uint8_t>& broken, float carX, float carY,
                                     float carYawDeg, float driftGaugeSmoothed, float speedKmh,
                                     bool boosting) {
    // itemdrum hit test 0x4bed40 the heading is the yaw less the smoothed gauge times 1 2 less 90
    GimmickDrumResult out;
    const float heading = (carYawDeg - driftGaugeSmoothed * GIMMICK_DRUM_GAUGE_TURN) - GIMMICK_DRUM_HEADING_TURN;
    // math point along heading 0x44dec0 turns the bearing into a plain cos and sin pair
    const float radians = -(heading - GIMMICK_DRUM_HEADING_TURN) * kDegToRad;
    const float dirX = std::cos(radians);
    const float dirY = std::sin(radians);
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i < broken.size() && broken[i] != 0) continue;
        const GimmickItemdrumRow& row = rows[i];
        if (math_hypot2d(carX - row.x, carY - row.y) > GIMMICK_DRUM_COARSE_REACH) continue;
        for (float t = GIMMICK_DRUM_SWEEP_FROM; t <= GIMMICK_DRUM_SWEEP_TO; t += GIMMICK_DRUM_SWEEP_STEP) {
            const float px = carX + dirX * t;
            const float py = carY + dirY * t;
            if (math_hypot2d(px - row.x, py - row.y) >= GIMMICK_DRUM_HIT_REACH) continue;
            out.row = static_cast<int32_t>(i);
            if (boosting) {
                // 0x4befd1 a boosting car loses the boost and keeps 0 7 of its velocity
                out.kind = GimmickDrumHit::BoostCancel;
            } else if (speedKmh > GIMMICK_DRUM_SLOW_KMH) {
                // 0x4bef58 the impact then a push 270 degrees off the bearing of the barrel
                out.kind = GimmickDrumHit::Bounce;
                out.pushHeadingDeg = math_atan2_deg(row.x - carX, row.y - carY) + GIMMICK_DRUM_PUSH_TURN;
                out.pushStrength = speedKmh * GIMMICK_DRUM_PUSH_PER_KMH + GIMMICK_DRUM_PUSH_FLOOR;
            } else {
                // 0x4befb5 under the gate the x and y velocity halve and the barrel still breaks
                out.kind = GimmickDrumHit::Slow;
            }
            return out;
        }
    }
    return out;
}

int32_t world_wheel_bump_slot(const std::array<GimmickBumpSlot, GIMMICK_BUMP_TABLE_SIZE>& table,
                               int32_t car_index) {
    for (size_t i = 0; i < GIMMICK_BUMP_TABLE_SIZE; ++i) {
        const GimmickBumpSlot& slot = table[i];
        if (slot.active == 1 && slot.payload == 0 && slot.car_index == car_index) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

int32_t effect_hazard_hit_lookup(const std::array<GimmickHazardSlot, GIMMICK_HAZARD_TABLE_SIZE>& table,
                                  int32_t car_index) {
    for (size_t i = 0; i < GIMMICK_HAZARD_TABLE_SIZE; ++i) {
        const GimmickHazardSlot& slot = table[i];
        if (slot.active == 1 && slot.car_index == car_index &&
            slot.countdown >= GIMMICK_HAZARD_COUNTDOWN_MIN &&
            slot.countdown <= GIMMICK_HAZARD_COUNTDOWN_MAX) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

int32_t gimmick_pool_slot_lookup(const std::array<GimmickPoolSlot, GIMMICK_POOL_LIVE_SLOTS>& pool,
                                  int32_t car_index) {
    // gimmick pool slot lookup 0x4B9FE0 active byte one the car index and a state under 200
    for (size_t i = 0; i < GIMMICK_POOL_LIVE_SLOTS; ++i) {
        const GimmickPoolSlot& slot = pool[i];
        if (slot.active && slot.car_index == car_index &&
            static_cast<int32_t>(slot.state) < GIMMICK_POOL_SCRIPTED_FLOOR) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

int32_t effect_hive_hit_lookup(const std::array<GimmickHiveSlot, GIMMICK_HIVE_TABLE_SIZE>& table,
                                int32_t car_index) {
    // effect hive hit lookup 0x4CF020 active byte one the car index and the state 0x69
    for (size_t i = 0; i < GIMMICK_HIVE_TABLE_SIZE; ++i) {
        const GimmickHiveSlot& slot = table[i];
        if (slot.active == 1 && slot.car_index == car_index && slot.state == GIMMICK_HIVE_STATE_HELD) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

int32_t world_cell_boost_pad_index(const char* cell_name) {
    // world cell boost pad index 0x486CF0 strstr BOOST then sscanf BOOST %03d minus one
    if (cell_name == nullptr) return -1;
    int value = -1;
    if (std::strstr(cell_name, "BOOST_") != nullptr) {
        std::sscanf(cell_name, "BOOST_%03d", &value);
    }
    return value - 1;
}

int32_t world_cell_warp_index(const char* cell_name) {
    // world cell warp index 0x486CA0 strstr WARP then sscanf WARP %03d minus one
    if (cell_name == nullptr) return -1;
    int value = -1;
    if (std::strstr(cell_name, "WARP_") != nullptr) {
        std::sscanf(cell_name, "WARP_%03d", &value);
    }
    return value - 1;
}

// world gimmick hit dispatch 0x4D3950 the order the sweep asks each class the first match wins
const int32_t GIMMICK_DISPATCH_ORDER[GIMMICK_CLASS_COUNT] = {
    2, 13, 8, 9, 10, 0, 3, 6, 7, 4, 5, 1, 11, 12, 14, 17, 18, 19, 21, 20, 15, 16
};

// the reach each class tests with read on its own hit test function
const float GIMMICK_CLASS_REACH[GIMMICK_CLASS_COUNT] = {
    8.0f,  // 0 Ant 0x4D4A2E 1 LavaMan 0x4D7EB0 is a stub that returns minus one so no reach exists
    0.0f,
    12.0f,  // 2 MushMan 0x4DB48A 3 Pierrot 0x4DC77D
    12.0f,
    16.0f,  // 4 Scorpion 0x4DCEA4 5 ToyBox 0x4DF1BC
    9.0f,
    12.0f,  // 6 CookieMan 0x4D5D5D 7 Chef 0x4D509D
    10.0f,
    6.0f,  // 8 TreeFairy 0x4DFD7D 9 TreeDoor 0x4DF77D
    6.0f,
    6.0f,  // 10 Mole 0x4DB1F6 11 Sheep 0x4DD4EC
    6.0f,
    20.0f,  // 12 Twister 0x4E0E9C 13 Spider 0x4DE4DC
    10.0f,
    6.0f,  // 14 Cobra 0x4D54DD 15 MissionEffect 0x4D976C
    9.0f,
    4.0f,  // 16 MissionMark 0x4DAB3C 17 Glass 0x4D793D
    30.0f,
    6.0f,  // 18 Turnstile 0x4E0440 and 19 Door 0x4D62B0 both sweep faces the reach stands in for the face test
    6.0f,
    6.0f,  // 20 Fountain 0x4D6BFC planar 21 Frame 0x4D72FD
    12.0f
};

namespace {

// math hypot3d 0x44D950 zero when all three are zero else the square root of the sum of squares
float gimmick_hypot3d(float a, float b, float c) {
    if (a == 0.0f && b == 0.0f && c == 0.0f) return 0.0f;
    return std::sqrt(a * a + b * b + c * c);
}

} // namespace

bool gimmick_row_touched(const GimmickInstance& row, float carX, float carY, float carZ,
                          const GimmickPoint* wheelPoints) {
    // 0x4D7EB0 the lava man test is a stub that returns minus one so the class never touches
    if (row.gimmickClass == static_cast<int32_t>(GimmickClass::LavaMan)) return false;
    if (row.gimmickClass < 0 || row.gimmickClass >= GIMMICK_CLASS_COUNT) return false;
    const float reach = row.radius > 0.0f ? row.radius
                                          : GIMMICK_CLASS_REACH[row.gimmickClass];
    if (reach <= 0.0f) return false;

    if (row.gimmickClass == static_cast<int32_t>(GimmickClass::Turnstile) ||
        row.gimmickClass == static_cast<int32_t>(GimmickClass::Door)) {
        // 0x4E0574 and 0x4D63E4 the four wheel probe points carry the test not the car centre
        if (wheelPoints == nullptr) return false;
        for (int w = 0; w < 4; ++w) {
            const GimmickPoint& p = wheelPoints[w];
            if (gimmick_hypot3d(p.x - row.x, p.y - row.y, p.z - row.z) < reach) return true;
        }
        return false;
    }
    if (row.gimmickClass == static_cast<int32_t>(GimmickClass::Fountain)) {
        // 0x4D6C0A a planar reach and the node has to sit above the car
        if (row.z <= carZ) return false;
        return math_hypot2d(carX - row.x, carY - row.y) < reach;
    }
    if (row.gimmickClass == static_cast<int32_t>(GimmickClass::Mole)) {
        // 0x4DB21A the mole only bites a car that drives under its node
        if (row.z <= carZ) return false;
    }
    return gimmick_hypot3d(carX - row.x, carY - row.y, carZ - row.z) < reach;
}

bool car_gimmick_hit(GameState& game, int carIndex, int32_t gimmickClass,
                      const GimmickPoint& refPoint, int64_t nowMs) {
    if (carIndex < 0 || carIndex >= static_cast<int>(game.cars.size())) return false;
    CarState& car = game.cars[static_cast<size_t>(carIndex)];
    // 0x4982E7 a car already under an effect takes no gimmick at all
    if (car.effect.activeCode != 0) return false;

    switch (gimmickClass) {
        case 0:
        case 3:
        case 6:
        case 7:
        case 8:
        case 11:
        case 12:
        case 14:
        case 21:
            // 0x498311 effect 100 the stumble then the packet 105 notice
            car_effect_apply(game, carIndex, GIMMICK_EFFECT_STUMBLE, nowMs);
            if (game.hooks.effectNotify) {
                game.hooks.effectNotify(game.hooks.user, carIndex, GIMMICK_EFFECT_STUMBLE);
            }
            return true;
        case 1:
        case 4:
        case 5:
        case 10:
        case 20:
            // 0x4983A4 effect 300 the spin out then the packet 105 notice
            car_effect_apply(game, carIndex, GIMMICK_EFFECT_SPINOUT, nowMs);
            if (game.hooks.effectNotify) {
                game.hooks.effectNotify(game.hooks.user, carIndex, GIMMICK_EFFECT_SPINOUT);
            }
            return true;
        case 9: {
            // 0x498338 the impact effect then one of two fixed bearings by the yaw window
            if (game.hooks.impactEffect) game.hooks.impactEffect(game.hooks.user, carIndex, 1);
            const bool outside = car.yawDeg <= GIMMICK_JUMP_YAW_LOW ||
                                 car.yawDeg >= GIMMICK_JUMP_YAW_HIGH;
            const float heading = outside ? GIMMICK_JUMP_HEADING_LOW : GIMMICK_JUMP_HEADING_HIGH;
            car_boost_push(game, carIndex, heading, GIMMICK_JUMP_STRENGTH);
            return true;
        }
        // Turnstile pushes toward the touched face point Door pushes away from it
        case 18:
        case 19: {
            // 0x4983D1 and 0x49845F the impact the boost cancel the push then the velocity cut
            if (game.hooks.impactEffect) game.hooks.impactEffect(game.hooks.user, carIndex, 1);
            car_boost_clear(game, carIndex);
            car_boost_start(game, carIndex, BOOST_KIND_CANCEL, nowMs);
            float dx = refPoint.x - car.posX;
            float dy = refPoint.y - car.posY;
            if (gimmickClass == static_cast<int32_t>(GimmickClass::Door)) {
                dx = -dx;
                dy = -dy;
            }
            car_boost_push(game, carIndex, math_atan2_deg(dx, dy), GIMMICK_BLOCK_STRENGTH);
            car.body.wheels.velocity.x *= GIMMICK_BLOCK_VELOCITY_XY;
            car.body.wheels.velocity.y *= GIMMICK_BLOCK_VELOCITY_XY;
            car.body.wheels.velocity.z *= GIMMICK_BLOCK_VELOCITY_Z;
            return true;
        }
        default:
            // 0x498527 classes 2 0xD 0xF 0x10 0x11 and anything over 0x15 change nothing
            return true;
    }
}

int32_t world_gimmick_hit_dispatch(GameState& game, int carIndex, GimmickWorld& world,
                                    int64_t nowMs) {
    // 0x4D3954 the manager byte then the effect gate and the local car gate
    if (world.loaded == 0) return -1;
    if (carIndex < 0 || carIndex >= static_cast<int>(game.cars.size())) return -1;
    CarState& car = game.cars[static_cast<size_t>(carIndex)];
    if (car.effect.activeCode != 0 || carIndex != game.localCarIndex) return -1;

    GimmickPoint wheels[4];
    for (int w = 0; w < 4; ++w) {
        wheels[w].x = car.wheelProbePoint[w].x;
        wheels[w].y = car.wheelProbePoint[w].y;
        wheels[w].z = car.wheelProbePoint[w].z;
    }

    for (int order = 0; order < GIMMICK_CLASS_COUNT; ++order) {
        const int32_t wanted = GIMMICK_DISPATCH_ORDER[order];
        for (size_t i = 0; i < world.rows.size(); ++i) {
            GimmickInstance& row = world.rows[i];
            if (row.gimmickClass != wanted || row.state != 0) continue;
            if (!gimmick_row_touched(row, car.posX, car.posY, car.posZ, wheels)) continue;
            // 0x4DB4A5 every test marks its row live and stamps the touch before the response
            row.state = 1;
            row.hitMs = nowMs;
            if (wanted == static_cast<int32_t>(GimmickClass::Turnstile) ||
                wanted == static_cast<int32_t>(GimmickClass::Door)) {
                // 0x4D64B6 and 0x4E06FE the touched face point lands in the car slot at 0x5F1958
                GimmickPoint& slot = world.refPoint[static_cast<size_t>(carIndex)];
                slot.x = row.x;
                slot.y = row.y;
                slot.z = row.z;
            }
            car_gimmick_hit(game, carIndex, wanted,
                            world.refPoint[static_cast<size_t>(carIndex)], nowMs);
            return static_cast<int32_t>(i);
        }
    }

    // 0x4D3C46 nothing was touched so every car position goes back into the car slots
    const size_t slots = world.refPoint.size() < game.cars.size() ? world.refPoint.size()
                                                                  : game.cars.size();
    for (size_t c = 0; c < slots; ++c) {
        const CarState& other = game.cars[c];
        GimmickPoint& slot = world.refPoint[c];
        if (slot.x != other.posX || slot.y != other.posY) {
            slot.x = other.posX;
            slot.y = other.posY;
            slot.z = other.posZ;
        }
    }
    return -1;
}

int32_t car_nearest_index(const GameState& game, float x, float y, float maxDistance,
                           int32_t excludeCarIndex) {
    // car nearest index 0x499B50 occupied slot still racing with no effect and the closest wins
    int32_t best = -1;
    float bestDistance = maxDistance;
    for (size_t i = 0; i < game.cars.size(); ++i) {
        const CarState& car = game.cars[i];
        if (car.slotOccupied == 0) continue;
        if (static_cast<int32_t>(i) == excludeCarIndex) continue;
        if (car.finishRank >= 0) continue;
        if (car.effect.activeCode != 0) continue;
        const float distance = math_hypot2d(x - car.posX, y - car.posY);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = static_cast<int32_t>(i);
        }
    }
    return best;
}

void gimmick_mushman_update(GameState& game, GimmickMushman& mush, int64_t nowMs) {
    switch (mush.state) {
        case GimmickMushState::Idle: {
            // 0x4DB56B the closest racing car inside 18 starts the chase
            const int32_t found = car_nearest_index(game, mush.x, mush.y, GIMMICK_MUSH_FIND_REACH, -1);
            if (found >= 0) {
                mush.carIndex = found;
                mush.state = GimmickMushState::Chase;
                mush.lastDist = GIMMICK_MUSH_DIST_SEED;
                if (found == game.localCarIndex && game.hooks.cameraShake) {
                    game.hooks.cameraShake(game.hooks.user, GIMMICK_MUSH_CAMERA_SHAKE);
                }
                if (game.hooks.spawnParticle) {
                    game.hooks.spawnParticle(game.hooks.user, found, 0, mush.x, mush.y, mush.z);
                }
            }
            break;
        }
        case GimmickMushState::Chase: {
            // 0x4DB5F9 the chase restarts the animation stamp every frame it runs
            mush.animMs = nowMs;
            if (mush.carIndex < 0 || mush.carIndex >= static_cast<int32_t>(game.cars.size())) break;
            const CarState& car = game.cars[static_cast<size_t>(mush.carIndex)];
            // 0x4DB65D the mush closes a quarter of the gap on all three axes
            mush.x += (car.posX - mush.x) * GIMMICK_MUSH_CHASE_LERP;
            mush.y += (car.posY - mush.y) * GIMMICK_MUSH_CHASE_LERP;
            mush.z += (car.posZ - mush.z) * GIMMICK_MUSH_CHASE_LERP;
            const float gap = math_hypot2d(car.posX - mush.x, car.posY - mush.y);
            if (gap >= mush.lastDist) {
                // 0x4DB6B7 the chase stopped closing so the mush catches and holds
                mush.state = GimmickMushState::Caught;
                mush.stateMs = nowMs;
                mush.shakeA = GIMMICK_MUSH_SHAKE_SEED;
                mush.shakeB = GIMMICK_MUSH_SHAKE_SEED;
                mush.shakeC = GIMMICK_MUSH_SHAKE_SEED;
                for (int k = 0; k < 6; ++k) {
                    mush.jitter[k] = std::rand() % GIMMICK_MUSH_JITTER_MOD[k] +
                                     GIMMICK_MUSH_JITTER_BIAS[k];
                }
            } else {
                mush.lastDist = gap;
            }
            break;
        }
        case GimmickMushState::Caught:
            // 0x4DB761 the hold of 3000 ms then the shake grows
            if (nowMs - mush.stateMs > GIMMICK_MUSH_HOLD_MS) mush.state = GimmickMushState::Grow;
            break;
        case GimmickMushState::Grow:
            // 0x4DB78D the three counters grow until the first passes 800
            mush.shakeA += GIMMICK_MUSH_GROW_A;
            mush.shakeB += GIMMICK_MUSH_GROW_B;
            mush.shakeC += GIMMICK_MUSH_GROW_B;
            if (mush.shakeA > GIMMICK_MUSH_GROW_CAP) {
                mush.state = GimmickMushState::Cooldown;
                mush.stateMs = nowMs;
            }
            break;
        case GimmickMushState::Cooldown:
            // 0x4DB7D1 the second hold of 3000 ms then the mush walks home facing 180
            if (nowMs - mush.stateMs > GIMMICK_MUSH_HOLD_MS) {
                mush.state = GimmickMushState::Idle;
                mush.x = mush.homeX;
                mush.y = mush.homeY;
                mush.z = mush.homeZ;
                mush.yawDeg = GIMMICK_MUSH_HOME_YAW;
            }
            break;
    }
    // 0x4DB822 the idle animation restarts every 2800 ms whatever the state
    if (nowMs - mush.animMs > GIMMICK_MUSH_ANIM_MS) mush.animMs = nowMs;
}

} // namespace KnC Kart Client
