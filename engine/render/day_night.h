#pragma once
#include <array>

namespace KnC::Render {

// Client clock runs 20 game seconds per real second
constexpr float kGameSecondsPerRealSecond = 20.0f;
// Each hour change cross fades linearly over real seconds
constexpr float kHourFadeSeconds = 2.0f;
constexpr int kHoursPerDay = 24;

// One hourly colour table 0 1 per channel
struct HourColour {
    float red = 1.f;
    float green = 1.f;
    float blue = 1.f;
};

// Default backdrop colour tile node viewer cannot find
constexpr HourColour kDefaultBackdrop{0.078f, 0.102f, 0.125f};

// Hourly ambient colours 0 23 hourly fog colours 24 47
struct DayNightTable {
    std::array<HourColour, kHoursPerDay> ambient{};
    std::array<HourColour, kHoursPerDay> fog{};
};

// One colour all day node terrain ramp zero flag
DayNightTable flat_day_night(const HourColour& fog);

// Clock CxDayNight retail never sends time packet free runs
class DayNightCycle {
public:
    // Zone in CxDayNight ForceHourSnap no cross fade
    void reset(const DayNightTable& table, int hour);
    void advance(float real_seconds);
    // Every hour change after zone in cross fades
    void step_hour(int delta);
    void toggle_running() { running_ = !running_; }

    bool running() const { return running_; }
    int  hour() const { return hour_; }
    const HourColour& ambient() const { return ambient_now_; }
    const HourColour& fog() const { return fog_now_; }

private:
    void start_fade(int hour);
    void advance_fade(float real_seconds);

    DayNightTable table_;
    float         game_seconds_ = 0.f;
    int           hour_ = 0;
    HourColour    ambient_from_;
    HourColour    ambient_to_;
    HourColour    ambient_now_;
    HourColour    fog_from_;
    HourColour    fog_to_;
    HourColour    fog_now_;
    float         fade_left_ = 0.f;
    bool          running_ = true;
};

}
