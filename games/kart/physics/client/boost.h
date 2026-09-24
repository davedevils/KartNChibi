#pragma once
// Boost start and update the draft factor and the engine force wrapper see TICK HELPERS

#include "car_state.h"

namespace KnC::Kart::Client {

// car boost start 0x496BE0 starts a boost of the given kind refused while a higher kind runs
void car_boost_start(GameState& game, int carIndex, int kind, int64_t nowMs);

// car boost update 0x496E50 the boost state machine push decay end runs every tick
void car_boost_update(GameState& game, int carIndex, int64_t nowMs);

// car boost speed gate 0x49A390 local car only HUD latch and camera shake gate
void car_boost_speed_gate(GameState& game, int carIndex, int64_t nowMs);

// car draft factor 0x4998F0 slipstream behind a qualifying car ahead 0 to 1
float car_draft_factor(GameState& game, int carIndex, float maxDistance, float coneHalfAngleDeg);

// car apply engine force 0x4968F0 body apply force with x and y negated and z passed through
void car_apply_engine_force(CarState& car, float x, float y, float z);

// math dir from heading pitch 0x44DD50 sin h cos p cos h cos p sin p the tilted forward
void math_dir_from_heading_pitch(float headingDeg, float pitchDeg, Vec3& out);

// car boost push 0x496B40 pushes along the heading pitched by car 0x3224 with a tenth up
void car_boost_push(GameState& game, int carIndex, float headingDeg, float strength);

// car push along yaw 0x497160 car boost push along the car yaw the carry drop kicks 40 with it
void car_push_along_yaw(GameState& game, int carIndex, float strength);

// car boost clear 0x496930 zeroes car 0x3300 the effect apply the collision and state 3 call it
void car_boost_clear(GameState& game, int carIndex);

} // namespace KnC Kart Client
