/// the level ladder math the stored level and the 0x000A bar bounds both come from these two

#pragma once

#include <cstdint>
#include <vector>

namespace knc {

/// the highest level whose cumulative exp the player reached rows sorted by level and cumExp
template <class Row>
inline int32_t levelOnCurve(const std::vector<Row>& curve, int64_t exp, int32_t fallback) {
    if (curve.empty()) return fallback;
    int32_t best = curve.front().level;
    for (const auto& row : curve) {
        if (row.cumExp > exp) break;
        best = row.level;
    }
    return best;
}

/// floor is the row of the level next the row after it the top row gets floor plus one
template <class Row>
inline bool boundsOnCurve(const std::vector<Row>& curve, int32_t level, int32_t& floorOut,
                          int32_t& nextOut, int32_t expMax) {
    floorOut = 0;
    nextOut = 1;
    if (curve.empty()) return false;
    size_t idx = 0;
    bool found = false;
    for (size_t i = 0; i < curve.size(); ++i) {
        if (curve[i].level == level) {
            idx = i;
            found = true;
            break;
        }
    }
    // off the ladder so pin to the nearest end and keep the divisor alive
    if (!found) idx = (level < curve.front().level) ? 0 : curve.size() - 1;
    floorOut = curve[idx].cumExp;
    if (idx + 1 < curve.size()) nextOut = curve[idx + 1].cumExp;
    else nextOut = (floorOut < expMax) ? (floorOut + 1) : expMax;
    return found;
}

}  // namespace knc
