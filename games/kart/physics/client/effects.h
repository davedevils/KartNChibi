#pragma once
// Item and gimmick effects on a car codes 100 to 1100 see EFFECTS AND GEAR

#include "car_state.h"

namespace KnC::Kart::Client {

// car effect apply 0x495C30 starts an effect refused while one is already active
void car_effect_apply(GameState& game, int carIndex, int code, int64_t nowMs);

// car effect update 0x4960A0 per tick dispatch outScaleX and outScaleY multiply the throttle force
void car_effect_update(GameState& game, int carIndex, float* outScaleX, float* outScaleY,
                        int64_t nowMs);

// car remote effect update 0x496600 the effect clock of a car the remote mover drives
void car_remote_effect_update(GameState& game, int carIndex);

// car effect lean update 0x49B3D0 once per tick roll wobble or cornering lean overlay
void car_effect_lean_update(GameState& game, int carIndex);

// car suspension shake 0x49A8A0 vehicle kind 2 only sums the four wheel compressions
float car_suspension_shake(const CarState& car);

// effect end 0x495BE0 stand in clears the active effect code and its scratch
void effect_end(CarState& car);

// car is slowed 0x4C31D0 16 entry table lookup quarters the drift gauge rate
bool car_is_slowed(const std::array<StatusIdSlot, STATUS_TABLE_SIZE>& table, int id);

// car is camera reversed 0x4BD8D0 16 entry table lookup swaps the drift stick slots
bool car_is_camera_reversed(const std::array<StatusIdSlot, STATUS_TABLE_SIZE>& table, int id);

// gimmick pool update 0x4C7ED0 the carry pool grab lock carry drop and release of every live slot
void gimmick_pool_update(GameState& game, const ColTrack& track, int64_t nowMs);

// gimmick pool update 0x4C7ED0 and 0x4BA380 one pool the carry grabs with 600 the blue rabbit pool with 900
void gimmick_pool_update_pool(GameState& game, std::array<GimmickPoolSlot, GIMMICK_POOL_LIVE_SLOTS>& pool,
                              int grabCode, const ColTrack& track, int64_t nowMs);

} // namespace KnC Kart Client
