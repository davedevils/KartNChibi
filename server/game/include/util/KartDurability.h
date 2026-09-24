/// kart durability rules the wire never states the client only draws the owned kart 0x30

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace knc {

/// owned kart record 0x2C value that makes 0x30 a durability
constexpr uint32_t KART_PERIOD_DURABILITY = 3;

/// kart durability bar draw 0x42AD20 clamps the owned kart 0x30 here the burst names no other max
constexpr int32_t KART_DURABILITY_MAX = 500;

/// one finished race takes this much the real rule never reached the wire so this is the emulator default
constexpr int32_t KART_DURABILITY_WEAR_PER_RACE = 1;

/// a repair scroll whose price row has no unit type 3 amount restores a full bar
constexpr int32_t KART_REPAIR_DEFAULT_AMOUNT = KART_DURABILITY_MAX;

/// owned scroll period mode a scroll bought with a unit type 3 row is one use of the scroll
constexpr uint32_t KART_SCROLL_PERIOD_USES = 2;

/// 0 to the client max
inline int32_t clampKartDurability(int64_t v) {
    if (v < 0) return 0;
    if (v > KART_DURABILITY_MAX) return KART_DURABILITY_MAX;
    return static_cast<int32_t>(v);
}

/// period pair of one owned row
struct KartPeriod {
    uint32_t mode  = 0;
    int64_t  value = 0;
};

/// unit type 3 price row on a kart a mode 3 row adds capped any other row becomes mode 3
inline KartPeriod kartPeriodAfterDurabilityGrant(uint32_t oldMode, int64_t oldValue,
                                                 uint32_t unitAmount) {
    KartPeriod p;
    p.mode = KART_PERIOD_DURABILITY;
    const int64_t held = (oldMode == KART_PERIOD_DURABILITY && oldValue > 0) ? oldValue : 0;
    p.value = clampKartDurability(held + static_cast<int64_t>(unitAmount));
    return p;
}

/// a scroll bought with a unit type 3 row stacks one use the durability sold is read at use time
inline KartPeriod scrollPeriodAfterGrant(uint32_t oldMode, int64_t oldValue) {
    KartPeriod p;
    p.mode = KART_SCROLL_PERIOD_USES;
    const int64_t held = (oldMode == KART_SCROLL_PERIOD_USES && oldValue > 0) ? oldValue : 0;
    p.value = held + 1;
    return p;
}

/// wear floors at zero false when the kart is not on mode 3 so nothing changes
inline bool kartDurabilityAfterWear(uint32_t mode, int64_t value, int32_t amount, int32_t& out) {
    if (mode != KART_PERIOD_DURABILITY) return false;
    out = clampKartDurability(value - static_cast<int64_t>(amount > 0 ? amount : 0));
    return true;
}

/// repair adds capped at the max false when the kart is not on mode 3
inline bool kartDurabilityAfterRepair(uint32_t mode, int64_t value, int32_t amount, int32_t& out) {
    if (mode != KART_PERIOD_DURABILITY) return false;
    const int64_t held = clampKartDurability(value);
    out = clampKartDurability(held + static_cast<int64_t>(amount > 0 ? amount : 0));
    return true;
}

/// the seven garage stats in wire order speed accel handling drift boost weight special
constexpr size_t KART_STAT_COUNT = 7;

/// owned kart upgrade columns migration 064 holds a delta over the catalogue not a copy
inline const char* const* kartUpgradeColumns() {
    static const char* const cols[KART_STAT_COUNT] = {
        "up_speed", "up_accel", "up_handling", "up_drift",
        "up_boost", "up_weight", "up_special"};
    return cols;
}

/// what the garage shows the catalogue value of the kart plus the upgrades bought on that row
inline int32_t effectiveKartStat(int32_t templateStat, int32_t upgradeSteps) {
    const int64_t steps = upgradeSteps > 0 ? upgradeSteps : 0;
    const int64_t v = static_cast<int64_t>(templateStat) + steps;
    return v < 0 ? 0 : static_cast<int32_t>(v);
}

/// select piece the durability of the owned kart alias k mode 3 carries it any other mode is full
inline std::string kartDurabilityColumns() {
    const std::string max = std::to_string(KART_DURABILITY_MAX);
    return "COALESCE(CASE WHEN k.period_mode = 3 THEN k.period_value END, " + max +
           ") AS durability, " + max + " AS max_durability, ";
}

/// select piece the seven stats of the owned kart alias k over its catalogue row alias vt
inline std::string kartStatColumns() {
    static const char* const tpl[KART_STAT_COUNT] = {
        "stat_speed", "stat_accel", "stat_handling", "stat_drift",
        "stat_boost", "stat_weight", "stat_special"};
    static const int32_t fallback[KART_STAT_COUNT] = {50, 50, 50, 40, 30, 50, 0};
    const char* const* up = kartUpgradeColumns();
    std::string out;
    for (size_t i = 0; i < KART_STAT_COUNT; ++i) {
        out += "COALESCE(vt." + std::string(tpl[i]) + ", " +
               std::to_string(fallback[i]) + ") + k." + up[i] +
               " AS " + tpl[i] + ", ";
    }
    return out;
}

/// join piece the owned kart alias k to the catalogue row alias vt that names its stats
inline std::string kartTemplateJoin() {
    return "LEFT JOIN vehicle_templates vt ON vt.id = k.base_key ";
}

}  // namespace knc
