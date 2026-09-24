#pragma once
// The tick entry points the 22 steps of car physics tick local from the attic decompile FUN 0049c0d0

#include "car_state.h"

namespace KnC::Kart::Client {

// cars frame update 0x00495330 loops the 30 car slots local car to the tick rest to remote
void cars_frame_update(GameState& game, const ColTrack& track, int64_t nowMs);

// car physics tick local 0x0049C0D0 the 22 steps of the map for one local car
void car_physics_tick_local(GameState& game, int carIndex, const ColTrack& track, int64_t nowMs);

// car substep collision response 0x00498960 reverts the substep and pushes a bounce and spin impulse
bool car_substep_collision_response(GameState& game, int carIndex, float impulseTime);

// car decode id5 0x0049BE20 decodes five denormal float encoded ints into a 5 char string
void car_decode_id5(char out[6], const int in[5]);

// car node name is 0x004A1350 true when the cell under the given wheel is named so case free
bool car_node_name_is(GameState& game, int carIndex, int wheelIndex, const char* name);

// standings local row index 0x4B4B50 the standings row of the local player id or minus one
int standings_local_row_index(const GameState& game);

// car ground flag set 0x49A920 on a car record the body flag the gear and the motion queue clear
void car_ground_flag_set_car(CarState& car, uint8_t groundValid);

// world car push apart 0x498800 on the game the overlap search the body nudge and the impact effect
void world_car_push_apart_tick(GameState& game, int carIndex);

} // namespace KnC Kart Client
