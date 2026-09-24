/// garage skin slot and expiry rules the stock 0xBA and 0xB8 handlers expect of the owned rows

#pragma once

#include <cstdint>
#include <ctime>
#include <string>

namespace knc {

/// kart def 0x84 and 0x88 of every 0xC0 row the burst sends RED paint and the standard plate
constexpr uint32_t KART_DEFAULT_PAINT_KEY = 9007;
constexpr uint32_t KART_DEFAULT_PLATE_KEY = 9100;

/// owned row period modes sub 451C90 knows
constexpr uint32_t OWNED_PERIOD_DAYS  = 1;
constexpr uint32_t OWNED_PERIOD_COUNT = 2;

/// sub 484B10 puts kart def 0x84 0x88 or 0x8C back on a Remove so no zero paint or plate
inline uint32_t kartSkinDefaultKey(const std::string& column) {
    if (column == "skin_primary") return KART_DEFAULT_PAINT_KEY;
    if (column == "skin_secondary") return KART_DEFAULT_PLATE_KEY;
    return 0;
}

/// the client day count of sub 451C30 365 times tm year plus tm yday minus 365
inline int64_t clientDayNumber(const std::tm& t) {
    return 365LL * t.tm_year + t.tm_yday - 365LL;
}

/// today in the client day count on the server clock
inline int64_t clientDayNumberToday() {
    const std::time_t now = std::time(nullptr);
    std::tm parts{};
#if defined(_WIN32)
    localtime_s(&parts, &now);
#else
    localtime_r(&now, &parts);
#endif
    return clientDayNumber(parts);
}

/// sub 451C90 a day row runs out once today passes its value a count row at zero
inline bool ownedPeriodRunOut(uint32_t mode, int64_t value, int64_t today) {
    if (mode == OWNED_PERIOD_DAYS) return today > value;
    if (mode == OWNED_PERIOD_COUNT) return value <= 0;
    return false;
}

/// the client sends C2S 0xB8 only for a row it saw expire so the server takes the same view
inline bool ownedRowExpired(uint32_t activeFlag, uint32_t mode, int64_t value, int64_t today) {
    return activeFlag == 0 || ownedPeriodRunOut(mode, value, today);
}

}  // namespace knc
