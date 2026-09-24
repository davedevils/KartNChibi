/// hooks sub 42CF50 which calls sub 44CC50 then re registers buttons via sub 44C580 every stage init

#pragma once

namespace questhook {

struct Config {
    bool enabled = false;

    /// registers Quest command 15 at 449 4
    bool quest = true;

    /// registers Room Craft command 9 at 606 731 needs an empty S2C 0x010E reply
    bool roomCraft = true;

    /// 0x0042CC8B JZ to JMP else both buttons get the grey 03 overlay
    bool suppressGreyOverlay = true;

    /// 23 byte NOP at 0x0042CB21 pattern is derived not quoted so off by default
    bool suppressLowLevelGrey = false;
};

/// must be called before install
void configure(const Config& cfg);

/// verifies the image base and every original byte refuses install on any mismatch returns true when disabled
bool install(const char* logPath);

/// restores every byte written safe to call when install refused
void shutdown();

/// short reason for the last refusal empty when nothing refused
const char* lastError();

}  // namespace questhook
