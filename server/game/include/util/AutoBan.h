#pragma once

#include <array>
#include <cstdint>

namespace knc {

// anti cheat ban grows per earlier ban 10 min 1 h 3 h 1 day 7 days 30 days
inline int32_t autoBanMinutes(int32_t earlierBans) {
    static constexpr std::array<int32_t, 6> kSteps = {10, 60, 180, 1440, 10080, 43200};
    if (earlierBans < 0) earlierBans = 0;
    const int32_t last = static_cast<int32_t>(kSteps.size()) - 1;
    return kSteps[static_cast<size_t>(earlierBans > last ? last : earlierBans)];
}

} // namespace knc
