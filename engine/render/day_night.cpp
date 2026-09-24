#include "engine/render/day_night.h"

#include <algorithm>
#include <cmath>

namespace KnC::Render {

namespace {

constexpr float kSecondsPerHour = 3600.f;
constexpr float kSecondsPerDay = kSecondsPerHour * static_cast<float>(kHoursPerDay);

int wrapped_hour(int hour) { return ((hour % kHoursPerDay) + kHoursPerDay) % kHoursPerDay; }

HourColour blend(const HourColour& from, const HourColour& to, float done) {
    return {from.red + (to.red - from.red) * done,
            from.green + (to.green - from.green) * done,
            from.blue + (to.blue - from.blue) * done};
}

} // namespace

DayNightTable flat_day_night(const HourColour& fog) {
    DayNightTable table;
    table.fog.fill(fog);
    return table;
}

void DayNightCycle::reset(const DayNightTable& table, int hour) {
    table_ = table;
    hour_ = wrapped_hour(hour);
    game_seconds_ = static_cast<float>(hour_) * kSecondsPerHour;
    ambient_from_ = ambient_now_ = table_.ambient[hour_];
    fog_from_ = fog_now_ = table_.fog[hour_];
    fade_left_ = 0.f;
}

void DayNightCycle::advance(float real_seconds) {
    if (real_seconds <= 0.f) return;
    if (running_) {
        game_seconds_ = std::fmod(game_seconds_ + real_seconds * kGameSecondsPerRealSecond,
                                  kSecondsPerDay);
        const int hour = static_cast<int>(game_seconds_ / kSecondsPerHour);
        if (hour != hour_) start_fade(hour);
    }
    advance_fade(real_seconds);
}

void DayNightCycle::step_hour(int delta) {
    const int hour = wrapped_hour(hour_ + delta);
    game_seconds_ = static_cast<float>(hour) * kSecondsPerHour;
    start_fade(hour);
}

void DayNightCycle::start_fade(int hour) {
    hour_ = hour;
    ambient_from_ = ambient_now_;
    fog_from_ = fog_now_;
    ambient_to_ = table_.ambient[hour];
    fog_to_ = table_.fog[hour];
    fade_left_ = kHourFadeSeconds;
}

// Outside two seconds client pushes nothing colours hold
void DayNightCycle::advance_fade(float real_seconds) {
    if (fade_left_ <= 0.f) return;
    fade_left_ = std::max(0.f, fade_left_ - real_seconds);
    const float done = 1.f - fade_left_ / kHourFadeSeconds;
    ambient_now_ = blend(ambient_from_, ambient_to_, done);
    fog_now_ = blend(fog_from_, fog_to_, done);
}

}
