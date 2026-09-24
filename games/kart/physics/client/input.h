#pragma once
// Client input flags key binding table poll dispatch from INPUT AND STATS

#include <array>
#include <cstdint>

namespace KnC::Kart::Client {

// shared input flags 0x18 0x1C 0x20 0x24 0x2C game offsets steer sense proved by ghost and body 0x20 is right

struct InputFlags {
    int32_t accel = 0;  // game 0x18 accelerate held game 0x1C brake held the brake key while moving forward
    int32_t brake = 0;
    int32_t steerRight = 0;  // game 0x20 slot 3 and 0x24 slot 2 held camera not reversed drift key tags 0x27 up 0x25 down
    int32_t steerLeft = 0;
    int32_t stuck = 0;      // game 0x2C reverse the brake key while slow or in reverse gear channel 5 of the body
};

// 8 action slots key binding lookup 0x45AF30 proved by car replay
enum class InputActionSlot : int32_t {
    Accelerate = 0,
    BrakeReverse = 1,
    SteerStickA = 2,
    SteerStickB = 3,
    ItemUse = 4,
    Drift = 5,
    CosmeticKey = 6,
    SecondaryItem = 7
};

constexpr size_t INPUT_ACTION_SLOT_COUNT = 8; // input key binding lookup 0x45AF30 slot 0 to 7

// input key binding lookup 0x45AF30 table this plus 0x100A0 stride 0x20 bytes per alt index
struct InputBindingTable {
    std::array<int32_t, INPUT_ACTION_SLOT_COUNT> primary{};  // alt index negative one or zero alt index one the pad button index
    std::array<int32_t, INPUT_ACTION_SLOT_COUNT> secondary{};
};

constexpr int32_t VK_STEER_LEFT_DIRECT = 0x25;  // car drift update 0x49AA90 tag of slot 2 left turn and slot 3 right turn
constexpr int32_t VK_STEER_RIGHT_DIRECT = 0x27;
constexpr int32_t VK_GEAR_UP = 0x41;  // input poll keyboard 0x497CB5 letter A pressed steps gear up input poll keyboard 0x497CD0 letter Z pressed steps gear down
constexpr int32_t VK_GEAR_DOWN = 0x5A;
constexpr int32_t VK_MOUSE_LEFT = 1;  // input poll device1 0x4979F0 mouse button 1 is the throttle input poll device1 0x497A02 mouse button 2 is the reverse
constexpr int32_t VK_MOUSE_RIGHT = 2;
constexpr int32_t VK_CONTROL_SYNTH = 0x11;      // input poll device3 0x4976E5 pad button 1 synthesises control

constexpr size_t INPUT_KEY_STATE_SIZE = 256;  // input key down 0x44B580 indexed by raw key code input key pressed 0x44B590 this plus 0x104 by raw key code
constexpr size_t INPUT_PRESSED_STATE_SIZE = 256;

using InputKeyState = std::array<uint8_t, INPUT_KEY_STATE_SIZE>;
using InputPressedState = std::array<int32_t, INPUT_PRESSED_STATE_SIZE>;

// input key binding lookup 0x45AF30 alt index negative one or zero is primary one is secondary
int32_t input_key_binding_lookup(const InputBindingTable& table, int32_t slot, int32_t alt_index);

// input key down 0x44B580 reads the held state byte the host fills for one raw key code
bool input_key_down(const InputKeyState& key_state, int32_t key_code);

// input key pressed 0x44B590 reads the pressed this frame int for one raw key code the callers pass codes
bool input_key_pressed(const InputPressedState& pressed_state, int32_t key_code);

// FUN 0044B550 writes the held byte of one raw key code the pad paths synthesise keys with it
void input_key_synthesize_down(InputKeyState& key_state, int32_t key_code);

// car gear shift up 0x4990A0 steps the gear index up capped at the max gear
void car_gear_shift_up(int32_t& gear_index, int32_t max_gear);

// car gear shift down 0x4990D0 steps the gear index down floored at minus one
void car_gear_shift_down(int32_t& gear_index);

// the pad device object 0xE52048 the pad paths read the host fills it
struct PadDeviceState {
    uint16_t axisX = 0x8000;  // 0xE520E0 raw x axis centre 0x8000 0xE520E8 raw y axis centre 0x8000
    uint16_t axisY = 0x8000;
    std::array<uint8_t, 128> buttonHeld{};  // 0xE520EC byte per button FUN 0044B0B0 reads it 0xE52048 plus 0x1A4 int per button FUN 0044B0C0 reads it
    std::array<int32_t, 128> buttonPressed{};
    bool deviceReset = false;                 // FUN 0044B0E0 true once after a device reset clears the scratch
};

// game 0x98 to 0xA8 the analog scratch the device paths write nothing else in the exe reads it
struct AnalogInputScratch {
    float steerAxis = 0.0f;  // game 0x98 minus one to plus one game 0x9C
    float throttle = 0.0f;
    float brake = 0.0f;  // game 0xA0 game 0xA4 device 3 pad button 0
    int32_t driftButton = 0;
    int32_t reverseButton = 0;  // game 0xA8 flips the pedal sign game 0xAC one int per pad button held or 100 ms after
    std::array<int32_t, 128> padPressedLatch{};
    std::array<int64_t, 128> padPressedMs{};  // game 0x2B0 the time of the last held sample per button DAT 02EB0440 ten edge flags one per pad button
    std::array<int32_t, 10> padEdge{};
};

// the extra per tick state input poll keyboard 0x497AB0 reads beyond keys and bindings
struct KeyboardPollState {
    bool camera_reversed = false;  // car is camera reversed 0x4BD8D0 swaps binding 2 and 3 car 0x9D4 race path zero is debug gear key path
    bool substep_input_flag = true;
    bool gear_mirror_non_negative = true;  // car 0x32FC sign bit clear read as an int at 0x497C8A car 0x3234
    float speed = 0.0f;
    float rpm = 0.0f;  // car 0x32F8 world wheel bump slot matched forces accel brake stuck to zero
    bool wheel_bump_active = false;
    bool stopped_flag = false;  // game 0x28 the stopped flag forces accel off body wheels on ground count over three forces accel off
    bool wheels_over_three_in_air = false;
    bool theme_special_row = false;  // world theme is special row 0x487230 slot 4 held is kind 5 boost DAT 02EB4828 nonzero blocks kind 5 boost
    bool theme_boost_blocked = false;
    bool boost_active = false;  // car 0x3300 nonzero car 0x3304
    int32_t boost_kind = 0;
    int32_t race_mode = 0;  // DAT 00B2360C mode 9 has the coast brake car 0x3520 negative skips the turn state and the sound
    int32_t driver_slot = 0;
};

// what input poll keyboard 0x497AB0 writes beside the flags the caller applies them to the car
struct KeyboardPollResult {
    bool brake_latch = false;  // car 0x332C one while game 0x1C is one starts kind 5 boost minus one clears kind 5 boost
    int32_t boost_kind5 = 0;
    bool coast_brake = false;  // body velocity times 0 85 in race mode 9 with nothing pressed car 0xA78E4 one left two right zero none
    int32_t turn_state = 0;
    int32_t engine_sound_state = -1;  // FUN 0048B460 state 0 to 4 minus one means no call
};

// input poll keyboard 0x497AB0 device 0 writes the five flags from the bindings
KeyboardPollResult input_poll_keyboard(InputFlags& flags, const InputBindingTable& bindings,
                                       const InputKeyState& key_state, const InputPressedState& pressed,
                                       const KeyboardPollState& state, int32_t& gear_index, int32_t max_gear);

// input poll device2 0x497270 the pad axis and buttons become synthesised key downs the keyboard poll reads
void input_poll_device2(InputKeyState& key_state, const InputBindingTable& bindings, const PadDeviceState& pad,
                         AnalogInputScratch& scratch, bool camera_reversed, int64_t nowMs);

// input poll device1 0x497940 the mouse writes only the analog scratch and the debug gear keys never the flags
void input_poll_device1(const InputKeyState& key_state, const InputPressedState& pressed, int32_t mouse_delta_x,
                         AnalogInputScratch& scratch, bool substep_input_flag, int32_t& gear_index,
                         int32_t max_gear);

// input poll device3 0x4975E0 the dev pad synthesises slots 0 1 2 3 5 and never calls the keyboard poll
void input_poll_device3(InputKeyState& key_state, const InputBindingTable& bindings, const PadDeviceState& pad,
                         AnalogInputScratch& scratch, bool camera_reversed, int64_t nowMs);

constexpr int32_t RACE_STATE_INPUT_GATE = 0xD; // car physics tick local 0x49C0D0 the gated race state

// ghost sample input mask ring entry 0x18 car ghost sample record 0x49FAD0 one bit per action slot slot 0 accelerate
constexpr uint8_t GHOST_MASK_SLOT0_ACCELERATE = 0x80;
constexpr uint8_t GHOST_MASK_SLOT1_BRAKE = 0x40;  // slot 1 brake or reverse slot 2 writes game 0x24 the left turn camera not reversed
constexpr uint8_t GHOST_MASK_SLOT2_STEER = 0x20;
constexpr uint8_t GHOST_MASK_SLOT3_STEER = 0x10;  // slot 3 writes game 0x20 the right turn camera not reversed slot 5 drift
constexpr uint8_t GHOST_MASK_SLOT5_DRIFT = 0x08;
constexpr uint8_t GHOST_MASK_SLOT4_ITEM = 0x04;  // slot 4 item use slot 7 second item action slot 6 is never recorded
constexpr uint8_t GHOST_MASK_SLOT7_ITEM2 = 0x02;

// builds the ring mask from the game flags slot 2 is 0x24 left slot 3 is 0x20 right
uint8_t input_ghost_mask_from_flags(const InputFlags& flags, bool driftHeld, bool itemHeld, bool item2Held);

// replays the ring mask into the game flags the inverse of the builder
void input_flags_from_ghost_mask(uint8_t mask, InputFlags& flags);

// 12 item counters 0xBFC3B0 to 0xBFDA1C gate input dispatch
struct InputDispatchCounters {
    int32_t at_0xbfc3b0 = 0;
    int32_t at_0xbfc510 = 0;
    int32_t at_0xbfc830 = 0;
    int32_t at_0xbfc8d0 = 0;
    int32_t at_0xbfcd2c = 0;
    int32_t at_0xbfd458 = 0;
    int32_t at_0xbfd6d0 = 0;
    int32_t at_0xbfd780 = 0;
    int32_t at_0xbfd824 = 0;
    int32_t at_0xbfd8c8 = 0;
    int32_t at_0xbfd978 = 0;
    int32_t at_0xbfda1c = 0;
};

// car physics tick local 0x49C0D0 line near 257 the gate before calling car input poll dispatch
bool input_dispatch_gate_pass(int32_t race_state, const InputDispatchCounters& counters);

// game 0x94 device index car input poll dispatch 0x497E40 reads to pick the sub handler
enum class InputDeviceIndex : int32_t {
    Keyboard = 0,
    Device1 = 1,
    Pad = 2,
    Device3 = 3
};

// true when the keyboard path ran and actually wrote the shared flags this tick
struct DispatchResult {
    bool handled = false;
    KeyboardPollResult keyboard; // filled when handled
};

// everything car input poll dispatch 0x497E40 hands to the four device paths
struct InputDispatchInputs {
    InputDeviceIndex device_index = InputDeviceIndex::Keyboard; // game 0x94
    const InputBindingTable* bindings = nullptr;
    InputKeyState* key_state = nullptr;
    const InputPressedState* pressed = nullptr;
    const PadDeviceState* pad = nullptr;
    AnalogInputScratch* scratch = nullptr;
    int32_t mouse_delta_x = 0; // FUN 0044B830 the mouse x since the last poll
    int64_t nowMs = 0;
};

// car input poll dispatch 0x497E40 four devices keyboard mouse pad dev pad only devices 0 and 2 reach keyboard poll

DispatchResult car_input_poll_dispatch(InputFlags& flags, const InputDispatchInputs& in,
                                        const KeyboardPollState& poll_state, int32_t& gear_index,
                                        int32_t max_gear, bool& brake_latch, bool& reverse_flag);

} // namespace KnC Kart Client
