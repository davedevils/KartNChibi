#include "input.h"

#include "constants.h"

namespace KnC::Kart::Client {

int32_t input_key_binding_lookup(const InputBindingTable& table, int32_t slot, int32_t alt_index) {
    if (slot < 0 || slot >= static_cast<int32_t>(INPUT_ACTION_SLOT_COUNT)) {
        return 0; // input key binding lookup 0x45AF30 returns zero when the slot is out of range
    }
    return alt_index == 1 ? table.secondary[static_cast<size_t>(slot)]
                           : table.primary[static_cast<size_t>(slot)];
}

bool input_key_down(const InputKeyState& key_state, int32_t key_code) {
    if (key_code < 0 || key_code >= static_cast<int32_t>(INPUT_KEY_STATE_SIZE)) {
        return false;
    }
    return key_state[static_cast<size_t>(key_code)] != 0;
}

bool input_key_pressed(const InputPressedState& pressed_state, int32_t key_code) {
    // input key pressed 0x44B590 this plus 0x104 plus code times 4 equal to 1 every caller passes a key code
    if (key_code < 0 || key_code >= static_cast<int32_t>(INPUT_PRESSED_STATE_SIZE)) {
        return false;
    }
    return pressed_state[static_cast<size_t>(key_code)] == 1;
}

void input_key_synthesize_down(InputKeyState& key_state, int32_t key_code) {
    if (key_code >= 0 && key_code < static_cast<int32_t>(INPUT_KEY_STATE_SIZE)) {
        key_state[static_cast<size_t>(key_code)] = 1; // FUN 0044B550 writes the held byte
    }
}

uint8_t input_ghost_mask_from_flags(const InputFlags& flags, bool driftHeld, bool itemHeld, bool item2Held) {
    uint8_t mask = 0;
    if (flags.accel != 0) mask |= GHOST_MASK_SLOT0_ACCELERATE;
    if (flags.brake != 0) mask |= GHOST_MASK_SLOT1_BRAKE;
    if (flags.steerLeft != 0) mask |= GHOST_MASK_SLOT2_STEER;
    if (flags.steerRight != 0) mask |= GHOST_MASK_SLOT3_STEER;
    if (driftHeld) mask |= GHOST_MASK_SLOT5_DRIFT;
    if (itemHeld) mask |= GHOST_MASK_SLOT4_ITEM;
    if (item2Held) mask |= GHOST_MASK_SLOT7_ITEM2;
    return mask;
}

void input_flags_from_ghost_mask(uint8_t mask, InputFlags& flags) {
    flags.accel = (mask & GHOST_MASK_SLOT0_ACCELERATE) ? 1 : 0;
    flags.brake = (mask & GHOST_MASK_SLOT1_BRAKE) ? 1 : 0;
    flags.steerLeft = (mask & GHOST_MASK_SLOT2_STEER) ? 1 : 0;
    flags.steerRight = (mask & GHOST_MASK_SLOT3_STEER) ? 1 : 0;
}

void car_gear_shift_up(int32_t& gear_index, int32_t max_gear) {
    if (gear_index < max_gear) {
        ++gear_index; // car gear shift up 0x4990A0 capped by car 0x2F40
    }
}

void car_gear_shift_down(int32_t& gear_index) {
    if (gear_index > -1) {
        --gear_index; // car gear shift down 0x4990D0 floored at minus one
    }
}

KeyboardPollResult input_poll_keyboard(InputFlags& flags, const InputBindingTable& bindings,
                                       const InputKeyState& key_state, const InputPressedState& pressed,
                                       const KeyboardPollState& state, int32_t& gear_index, int32_t max_gear) {
    KeyboardPollResult out;

    // 0x497ACD the special theme turns the slot 4 key into a kind 5 boost while DAT 02EB4828 is clear
    if (state.theme_special_row) {
        const int32_t code4 = input_key_binding_lookup(bindings, static_cast<int32_t>(InputActionSlot::ItemUse), -1);
        if (input_key_down(key_state, code4) && !state.theme_boost_blocked) {
            out.boost_kind5 = 1;
        } else if (state.boost_active && state.boost_kind == 5) {
            out.boost_kind5 = -1; // car boost clear 0x496930
        }
    }

    const int32_t code2 = input_key_binding_lookup(bindings, static_cast<int32_t>(InputActionSlot::SteerStickA), -1);
    const int32_t code3 = input_key_binding_lookup(bindings, static_cast<int32_t>(InputActionSlot::SteerStickB), -1);
    const bool held2 = input_key_down(key_state, code2);
    const bool held3 = input_key_down(key_state, code3);

    // 0x497B5C camera reversed swaps the pair slot 2 lands on game 0x20 and slot 3 on game 0x24
    if (state.camera_reversed) {
        flags.steerRight = held2 ? 1 : 0;
        flags.steerLeft = held3 ? 1 : 0;
    } else {
        flags.steerLeft = held2 ? 1 : 0;
        flags.steerRight = held3 ? 1 : 0;
    }

    const int32_t code0 = input_key_binding_lookup(bindings, static_cast<int32_t>(InputActionSlot::Accelerate), -1);
    flags.accel = input_key_down(key_state, code0) ? 1 : 0;

    const int32_t code1 = input_key_binding_lookup(bindings, static_cast<int32_t>(InputActionSlot::BrakeReverse), -1);
    if (!state.substep_input_flag) {
        // 0x497C50 the debug path car 0x9D4 zero the brake key is a plain brake and A Z step the gear
        flags.brake = input_key_down(key_state, code1) ? 1 : 0;
        if (input_key_pressed(pressed, VK_GEAR_UP)) {
            car_gear_shift_up(gear_index, max_gear);
        } else if (input_key_pressed(pressed, VK_GEAR_DOWN)) {
            car_gear_shift_down(gear_index);
        }
    } else {
        // 0x497C7B the race path the brake key brakes while rolling forward and reverses when slow
        bool braking = false;
        if (input_key_down(key_state, code1)) {
            if (state.gear_mirror_non_negative && state.speed >= kReverseBrakeSpeed && state.rpm > kReverseBrakeRpm) {
                braking = true;
            } else {
                flags.stuck = 1; // game 0x2C the reverse channel of the body
                flags.accel = 0;
            }
        }
        if (!braking && input_key_down(key_state, code1)) {
            flags.brake = 0;
        } else {
            flags.stuck = 0;
            if (braking) {
                flags.brake = 1;
                flags.accel = 0;
            } else {
                flags.brake = 0;
            }
        }
    }

    if (state.wheel_bump_active) {
        flags.accel = 0;
        flags.brake = 0;
        flags.stuck = 0;
    }

    if (state.stopped_flag || state.wheels_over_three_in_air) {
        flags.accel = 0; // 0x497D28 game 0x28 or all four wheels in the air
    }

    if (flags.brake == 1) {
        out.brake_latch = true; // car 0x332C
    }

    // 0x497D50 race mode 9 with no accel no reverse and neither steer key brakes the body velocity
    if (state.race_mode == 9 && flags.accel == 0 && flags.stuck == 0 && !held3 && !held2) {
        out.coast_brake = true;
    }

    if (state.driver_slot >= 0) {
        // 0x497DA4 the turn state car 0xA78E4 one on game 0x24 two on game 0x20 alone
        if (flags.steerLeft == 0) {
            out.turn_state = (flags.steerRight != 0) ? 2 : 0;
        } else {
            out.turn_state = 1;
        }
        // 0x497DC1 the engine sound state reverse 3 boost 4 turning 1 or 2 rolling 0 stopped none
        if (flags.stuck == 0) {
            if (!state.boost_active) {
                if (state.speed <= kAirborneLandingGate) {
                    out.engine_sound_state = 0;
                } else if (flags.steerLeft == 0) {
                    out.engine_sound_state = (flags.steerRight == 0) ? 0 : 2;
                } else {
                    out.engine_sound_state = 1;
                }
            } else {
                out.engine_sound_state = 4;
            }
        } else if (state.speed > kAirborneLandingGate) {
            out.engine_sound_state = 3;
        }
    }
    // 0x497E19 DAT 02EB0650 latches one once the accel key is seen a HUD flag not ported
    return out;
}

namespace {

// 0x497360 the pad pressed latch per button held sets it released clears it 100 ms later
void pad_pressed_latch_update(const PadDeviceState& pad, AnalogInputScratch& scratch, int64_t nowMs) {
    for (size_t i = 0; i < 128; ++i) {
        if (pad.buttonHeld[i] == 0) {
            if (nowMs - scratch.padPressedMs[i] > kPadPressedHoldMs) scratch.padPressedLatch[i] = 0;
        } else {
            scratch.padPressedLatch[i] = 1;
            scratch.padPressedMs[i] = nowMs;
        }
    }
}

// 0x4973EB the steer axis over the thresholds synthesises slot 2 and slot 3 swapped by the camera
void pad_steer_synthesize(InputKeyState& key_state, const InputBindingTable& bindings, float axis,
                          bool camera_reversed) {
    const int32_t code2 = input_key_binding_lookup(bindings, static_cast<int32_t>(InputActionSlot::SteerStickA), -1);
    const int32_t code3 = input_key_binding_lookup(bindings, static_cast<int32_t>(InputActionSlot::SteerStickB), -1);
    if (camera_reversed) {
        if (axis > kPadSteerRightThreshold) input_key_synthesize_down(key_state, code2);
        if (axis < kPadSteerLeftThreshold) input_key_synthesize_down(key_state, code3);
    } else {
        if (axis < kPadSteerLeftThreshold) input_key_synthesize_down(key_state, code2);
        if (axis > kPadSteerRightThreshold) input_key_synthesize_down(key_state, code3);
    }
}

bool pad_button_held(const PadDeviceState& pad, int32_t index) {
    // FUN 0044B0B0 the held byte at 0xE52048 plus 0xA4 plus index
    return index >= 0 && index < 128 && pad.buttonHeld[static_cast<size_t>(index)] != 0;
}

bool pad_button_pressed(const PadDeviceState& pad, int32_t index) {
    // FUN 0044B0C0 the pressed int at 0xE52048 plus 0x1A4 plus index times 4 equal to 1
    return index >= 0 && index < 128 && pad.buttonPressed[static_cast<size_t>(index)] == 1;
}

} // namespace

void input_poll_device2(InputKeyState& key_state, const InputBindingTable& bindings, const PadDeviceState& pad,
                         AnalogInputScratch& scratch, bool camera_reversed, int64_t nowMs) {
    // input poll device2 0x497270 the analog block then the synthesised keys
    if (pad.deviceReset) {
        scratch.steerAxis = 0.0f;
        scratch.throttle = 0.0f;
        scratch.brake = 0.0f;
        scratch.driftButton = 0;
        scratch.reverseButton = 0;
    }
    // 0x4972B0 the x axis minus 0x8000 times one over 32768 is the steer axis
    scratch.steerAxis = static_cast<float>(static_cast<int32_t>(pad.axisX) - 0x8000) * kPadAxisScale;

    // 0x4972C1 the pedal is 0 with the accel button 65536 with the brake button else the centre
    float pedal = kPadAnalogCenter;
    const int32_t button0 = input_key_binding_lookup(bindings, static_cast<int32_t>(InputActionSlot::Accelerate), 1);
    const int32_t button1 = input_key_binding_lookup(bindings, static_cast<int32_t>(InputActionSlot::BrakeReverse), 1);
    if (pad_button_held(pad, button0)) {
        pedal = kZero;
    } else if (pad_button_held(pad, button1)) {
        pedal = kPadBrakeAnalog;
    }
    if (scratch.reverseButton != 0) pedal = -pedal;
    if (pedal >= kZero) {
        scratch.brake = pedal;
        scratch.throttle = 0.0f;
    } else {
        scratch.brake = 0.0f;
        scratch.throttle = -pedal;
    }

    // 0x497330 ten edge flags at DAT 02EB0440 set on the first held sample cleared on release
    for (size_t i = 0; i < 10; ++i) {
        if (pad.buttonHeld[i] == 0) {
            scratch.padEdge[i] = 0;
        } else if (scratch.padEdge[i] == 0) {
            scratch.padEdge[i] = 1;
        }
    }
    pad_pressed_latch_update(pad, scratch, nowMs);

    pad_steer_synthesize(key_state, bindings, scratch.steerAxis, camera_reversed);

    // 0x497484 held buttons synthesise slots 0 1 5 6 pressed buttons synthesise slots 4 and 7
    const int32_t heldSlots[4] = {0, 1, 5, 6};
    for (int32_t slot : heldSlots) {
        if (pad_button_held(pad, input_key_binding_lookup(bindings, slot, 1))) {
            input_key_synthesize_down(key_state, input_key_binding_lookup(bindings, slot, -1));
        }
    }
    const int32_t pressedSlots[2] = {4, 7};
    for (int32_t slot : pressedSlots) {
        if (pad_button_pressed(pad, input_key_binding_lookup(bindings, slot, 1))) {
            input_key_synthesize_down(key_state, input_key_binding_lookup(bindings, slot, -1));
        }
    }
}

void input_poll_device1(const InputKeyState& key_state, const InputPressedState& pressed, int32_t mouse_delta_x,
                         AnalogInputScratch& scratch, bool substep_input_flag, int32_t& gear_index,
                         int32_t max_gear) {
    // input poll device1 0x497940 the mouse x delta integrates into game 0x98 which nothing reads
    if (mouse_delta_x == 0) {
        scratch.steerAxis -= scratch.steerAxis * kMouseSteerDecay;
    } else {
        scratch.steerAxis += static_cast<float>(mouse_delta_x) * kMouseSteerGain;
    }
    float pedal = input_key_down(key_state, VK_MOUSE_LEFT) ? kOne : kZero;
    scratch.reverseButton = input_key_down(key_state, VK_MOUSE_RIGHT) ? 1 : 0;
    if (scratch.reverseButton != 0) pedal = -pedal;
    if (pedal < kZero) {
        scratch.throttle = 0.0f;
        scratch.brake = -pedal;
    } else {
        scratch.throttle = pedal;
        scratch.brake = 0.0f;
    }
    if (!substep_input_flag) {
        if (input_key_pressed(pressed, VK_GEAR_UP)) {
            car_gear_shift_up(gear_index, max_gear);
        } else if (input_key_pressed(pressed, VK_GEAR_DOWN)) {
            car_gear_shift_down(gear_index);
        }
    }
}

void input_poll_device3(InputKeyState& key_state, const InputBindingTable& bindings, const PadDeviceState& pad,
                         AnalogInputScratch& scratch, bool camera_reversed, int64_t nowMs) {
    // input poll device3 0x4975E0 the dev pad both axes and buttons 0 and 1 the rest are debug spawns
    if (pad.deviceReset) {
        scratch.steerAxis = 0.0f;
        scratch.throttle = 0.0f;
        scratch.brake = 0.0f;
        scratch.driftButton = 0;
        scratch.reverseButton = 0;
    }
    scratch.steerAxis = static_cast<float>(static_cast<int32_t>(pad.axisX) - 0x8000) * kPadAxisScale;
    if (pad.buttonHeld[0] != 0) scratch.driftButton = 1;
    if (pad.buttonHeld[1] != 0) scratch.reverseButton = 1;
    float pedal = static_cast<float>(static_cast<int32_t>(pad.axisY) - 0x8000) * kPadAxisScale;
    if (scratch.reverseButton != 0) pedal = -pedal;
    if (pedal >= kZero) {
        scratch.brake = pedal;
        scratch.throttle = 0.0f;
    } else {
        scratch.brake = 0.0f;
        scratch.throttle = -pedal;
    }
    // 0x4976A0 the edge flags button 1 synthesises control buttons 2 to 8 call the debug spawns not ported
    for (size_t i = 0; i < 10; ++i) {
        if (pad.buttonHeld[i] == 0) {
            scratch.padEdge[i] = 0;
        } else if (scratch.padEdge[i] == 0) {
            scratch.padEdge[i] = 1;
            if (i == 1) input_key_synthesize_down(key_state, VK_CONTROL_SYNTH);
        }
    }
    pad_steer_synthesize(key_state, bindings, scratch.steerAxis, camera_reversed);
    // 0x497839 throttle over 0 2 is slot 0 brake over 0 2 is slot 1 button 0 is slot 5
    if (scratch.throttle > kPadSteerRightThreshold) {
        input_key_synthesize_down(key_state, input_key_binding_lookup(bindings, 0, -1));
    }
    if (scratch.brake > kPadSteerRightThreshold) {
        input_key_synthesize_down(key_state, input_key_binding_lookup(bindings, 1, -1));
    }
    if (pad.buttonHeld[0] != 0) {
        input_key_synthesize_down(key_state, input_key_binding_lookup(bindings, 5, -1));
    }
    pad_pressed_latch_update(pad, scratch, nowMs);
}

bool input_dispatch_gate_pass(int32_t race_state, const InputDispatchCounters& counters) {
    if (race_state != RACE_STATE_INPUT_GATE) {
        return true; // every race state except 0xD always runs the dispatch
    }
    return counters.at_0xbfc3b0 > 1 || counters.at_0xbfc510 > 1 || counters.at_0xbfc830 > 1 ||
           counters.at_0xbfc8d0 > 1 || counters.at_0xbfcd2c > 1 || counters.at_0xbfd458 > 1 ||
           counters.at_0xbfd6d0 > 1 || counters.at_0xbfd780 > 1 || counters.at_0xbfd824 > 1 ||
           counters.at_0xbfd8c8 > 1 || counters.at_0xbfd978 > 1 || counters.at_0xbfda1c > 1;
}

DispatchResult car_input_poll_dispatch(InputFlags& flags, const InputDispatchInputs& in,
                                        const KeyboardPollState& poll_state, int32_t& gear_index,
                                        int32_t max_gear, bool& brake_latch, bool& reverse_flag) {
    DispatchResult result;
    brake_latch = false;  // car 0x332C and 0x332D cleared every call
    reverse_flag = false;
    if (in.bindings == nullptr || in.key_state == nullptr || in.pressed == nullptr) return result;

    switch (in.device_index) {
        case InputDeviceIndex::Keyboard:
            result.keyboard = input_poll_keyboard(flags, *in.bindings, *in.key_state, *in.pressed, poll_state,
                                                  gear_index, max_gear);
            result.handled = true;
            break;
        case InputDeviceIndex::Pad:
            if (in.pad != nullptr && in.scratch != nullptr) {
                input_poll_device2(*in.key_state, *in.bindings, *in.pad, *in.scratch, poll_state.camera_reversed,
                                   in.nowMs);
            }
            result.keyboard = input_poll_keyboard(flags, *in.bindings, *in.key_state, *in.pressed, poll_state,
                                                  gear_index, max_gear);
            result.handled = true;
            break;
        case InputDeviceIndex::Device1:
            // 0x497E7C the mouse path never reaches game 0x18 proved the scratch it writes has no reader
            if (in.scratch != nullptr) {
                input_poll_device1(*in.key_state, *in.pressed, in.mouse_delta_x, *in.scratch,
                                   poll_state.substep_input_flag, gear_index, max_gear);
            }
            break;
        case InputDeviceIndex::Device3:
            // 0x497E9C the dev pad synthesises keys the drift update and the coast check read not the flags
            if (in.pad != nullptr && in.scratch != nullptr) {
                input_poll_device3(*in.key_state, *in.bindings, *in.pad, *in.scratch, poll_state.camera_reversed,
                                   in.nowMs);
            }
            break;
    }
    brake_latch = result.keyboard.brake_latch;

    if (gear_index < 0) {
        reverse_flag = true; // car 0x332D set when car 0x2DE8 is negative
    }

    return result;
}

} // namespace KnC Kart Client
