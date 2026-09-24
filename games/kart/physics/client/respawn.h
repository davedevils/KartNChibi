#pragma once
// Crash recovery watchdog checkpoint lookup and the teleport state machine see SUSPENSION AND TELEPORT

#include <string>

#include "car_state.h"

namespace KnC::Kart::Client {

// respawn crash recovery update 0x4A0970 wrong way watchdog on the checkpoint polylines
void respawn_crash_recovery_update(GameState& game, int carIndex, int64_t nowMs);

// respawn checkpoint nearest all 0x489970 closest point across all 4 lists arms the watchdog
bool respawn_checkpoint_nearest_all(const CheckpointList& lists, float x, float y, float z,
                                     int* outListIndex, int* outPointIndex, float* outDistance);

// respawn checkpoint nearest on list 0x489890 nearest point on the one chosen list
bool respawn_checkpoint_nearest_on_list(const CheckpointList& lists, int listIndex, float x, float y,
                                         float z, int* outPointIndex, float* outDistance);

// respawn recovery point find 0x489B40 extra validity filter over the 4 lists
bool respawn_recovery_point_find(const CheckpointList& lists, float x, float y, float z,
                                  int* outListIndex, int* outPointIndex);

// car respawn state machine 0x4A1420 the fade states 100 0x65 0x66 writes the teleport
void car_respawn_state_machine(GameState& game, int carIndex, const ColTrack& track, int64_t nowMs);

// gimmick load follow 0x489730 fills the 4 lists and the grid from follow 01 ini to follow 04 ini
bool respawn_follow_lists_load(GameState& game, const std::string& trackDir, std::string& error);

// respawn car state reset 0x48DB30 per car reset at spawn does not touch position
void respawn_car_state_reset(CarState& car);

// car rival nearby cue 0x49A130 was car stuck surface check a voice cue when a racing car is close
void car_rival_nearby_cue(GameState& game, int carIndex, int64_t nowMs);

// car nearest racing car 0x499AA0 the closest occupied unfinished other car inside the range or minus one
int car_nearest_racing_car(const GameState& game, int carIndex, float maxDistance);

// car launch pad kick 0x497190 a kind 3 boost ini row yaw push up kick decay then a boost
void car_launch_pad_kick(GameState& game, int carIndex, float yawDeg, float pushStrength, float kickValue,
                         float kickDecay, int boostKind, const ColTrack& track, int64_t nowMs);

// car name match count 0x4A1380 counts the wheels whose cell name equals the tag
int car_name_match_count_tick(CarState& car, const char* tag);

// car mission rally update 0x4A3BD0 local car only lap and checkpoint bookkeeping
void car_mission_rally_update(GameState& game, int carIndex, int64_t nowMs);

} // namespace KnC Kart Client
