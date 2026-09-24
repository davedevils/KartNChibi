#pragma once
// Drift and mini turbo state machine car drift update 0x49AA90 see CLIENT PHYSICS MAP and CONSTANTS

#include "car_state.h"

namespace KnC::Kart::Client {

// car drift update 0x49AA90 drift gauge mini turbo and steering gains once per tick
void car_drift_update(GameState& game, int carIndex, int64_t nowMs);

// car drift state set 0x49A9E0 sets or clears the drift state the stamp is the tick clock
void car_drift_state_set(GameState& game, int carIndex, int newState, int64_t nowMs);

// car body set yaw 0x49A970 snaps the rigid body orientation to a yaw drift update only
void car_body_set_yaw(CarState& car, float yawDeg);

} // namespace KnC Kart Client
