#pragma once
// Loads a ghost car body plus its four wheels from the stock Car Body folder and its driver

#include "engine/render/map_scene.h"
#include "engine/render/nif_character_model.h"
#include "engine/render/nif_prop_model.h"
#include "tools/replay/ghost_replay.h"

#include <array>
#include <string>
#include <vector>

namespace KnC::Tools {

// Beside the nif first then its parent folder recursive same rule as track colours
std::string find_texture(const std::string& root, const std::string& wanted);
void resolve_textures(const std::string& root, KnC::Render::PropModel& model);
// Same walk for a character its textures sit in BODYSET beside the body nif
void resolve_textures(const std::string& root, KnC::Render::CharacterModel& model);

// One body plus its four wheels each with the matrix that places it at its corner
struct GhostCar {
    KnC::Render::PropModel body;
    std::vector<KnC::Render::PropModel> wheels;
    // Row 3 holds the translation same layout as bx mtxTranslate one per wheel
    std::vector<std::array<float, 16>> wheel_local;
    // Half the z extent of each wheel mesh the disc bakes around its own origin
    std::vector<float> wheel_radius;
    // O SM01 and O SM02 the exhaust dummies car 0x355C the smoke and the boost flame sit on them
    std::vector<std::array<float, 16>> smoke_local;
    // O ANT the antenna dummy car 0x3588 the kart item rides it false when the body has none
    std::array<float, 16> ant_local = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                                       0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    bool has_ant = false;
    // O NAME the number plate dummy the Car Parts nif of the plate rides it
    std::array<float, 16> name_local = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                                       0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    bool has_name = false;
};

// Steer and spin of the wheels the front pair turns every wheel rolls
struct GhostWheelState {
    // D3DX RotationZ degrees of the front pair positive turns the wheels to the left
    float steer_degrees = 0.f;
    // D3DX RotationY radians per wheel car 0x32A4 goes down while driving forward
    std::vector<float> spin_radians;
    // The ground shake of car visual update 0x48ED8F a lift per wheel added to its corner height
    std::vector<float> lift;
};

// The camber of car visual update 0x48EE62 8 degrees about the nose axis even wheels lean the other way
constexpr float kGhostWheelCamberDeg = 8.f;

// Reads BODY nif plus WHEEL1 to WHEEL4 on the O WHEEL dummies a paint names the BodyColor folder
bool load_ghost_car(const std::string& body_nif, GhostCar& out, std::string& error,
                    const std::string& paint = std::string());

// dummy world transform used by char panel for plate antenna
bool ghost_car_dummy(const std::string& body_nif, const char* name, float out[16]);

// kart part nif prop antenna binds sphere to bones
bool load_kart_part_model(const std::string& nif, const std::string& texture_dir,
                          KnC::Render::PropModel& out, std::string& error);

// The ground shake per frame rand 600 minus 300 times speed capped 100 times grip times 4e-6 on the ground
void ghost_wheels_shake(float speed, const float grip[4], bool on_ground, GhostWheelState& wheels);

// Appends the body then every wheel wheels 1 and 2 take the steer every wheel the spin
void ghost_car_instances(const GhostCar& car, size_t first_model_index, const float car_world[16],
                         const GhostWheelState& wheels, std::vector<KnC::Render::PropInstance>& out);

// The remote car rule of car visual update 0x48E7C4 spin by the distance steer eased to the lean side
void ghost_wheels_advance(const GhostCar& car, const float car_world[16], const float moved[3],
                          float dt, int turn_state, GhostWheelState& wheels);

// The local car rule of the tick 0x49D854 the spin per wheel and the front steer of the port
void ghost_wheels_from_physics(const GhostCar& car, const float spin_radians[4], float steer_radians,
                               GhostWheelState& wheels);

// KFM sequence ids of a driver body the body Anim h enum the client plays them by id
enum GhostDriverSequence {
    kDriverSeqIdle       = 0,
    kDriverSeqDriftLeft  = 1,
    kDriverSeqDriftRight = 2,
    kDriverSeqBack       = 3,
    kDriverSeqTurbo      = 4,
    kDriverSeqCrash      = 5,
    kDriverSeqDamage     = 6,
    kDriverSeqItemGet    = 7,
    kDriverSeqItemEmpty  = 8,
    kDriverSeqWin        = 9,
    kDriverSeqLose       = 10,
};

// The driver seated on the car one skinned body with its KFM clips placed by translation only
struct GhostDriver {
    KnC::Render::CharacterModel model;
    // Seat in car space row 3 holds the translation the car origin when no ini row names the chassis
    std::array<float, 16> seat_local = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
                                        0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    float seat[3] = {0.f, 0.f, 0.f};
    bool  seat_found = false;
    // Clip of MT IDLE inside the model minus one leaves the rest pose
    int idle_clip = -1;
    // Clip index by KFM sequence id minus one when the KFM has no such sequence
    std::vector<int> clip_of_sequence;
    // The BODYSET parts merged into the model when it was built the token of the registry
    std::string parts_token;

    int clip_of(int sequence_id) const;
};

// Seat from Define Driver driver pos ini above the body nif section chassis keys x y z
bool find_driver_seat(const std::string& body_nif, const std::string& chassis, float out_seat[3]);

// BODYSET attach nodes table 0x5EA910 by equip slot 2 to 6
const char* ghost_driver_slot_node(int equip_slot);

// worn BODYSET nifs merged into body FUN 0048C680
void ghost_driver_set_parts(const std::string& asset, const std::vector<std::string>& part_nifs);
// The parts registered for an asset empty when none were set
const std::vector<std::string>& ghost_driver_parts(const std::string& asset);
// One line of the registered parts a caller reloads its models when this changes
std::string ghost_driver_parts_token(const std::string& asset);

// Reads the body nif every clip its KFM names and the seat false when the body fails
bool load_ghost_driver(const std::string& body_nif, const std::string& chassis, GhostDriver& out,
                       std::string& error);

// Sequence rule of car visual update 0x48F515 back turbo lean side over 3 units per second idle
int ghost_driver_sequence(const GhostPose& pose, float speed);

// Same rule as a clip index of the driver model the idle clip when the wanted one is missing
int ghost_driver_clip(const GhostDriver& driver, const GhostPose& pose, float speed);

// The driver instance riding the car world matrix on one clip at the given clip time
void ghost_driver_instance(const GhostDriver& driver, size_t model_index,
                           const float car_world[16], int clip, float clip_seconds,
                           KnC::Render::CharacterInstance& out);

}
