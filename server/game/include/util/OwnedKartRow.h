/// one reader for an owned kart row so the garage the inventory and the race read the same kart

#pragma once

#include <map>
#include <string>

#include "packets/PacketBuilder.h"
#include "util/KartDurability.h"

namespace knc {

/// the select list every owned kart reader shares alias k is the row alias vt its catalogue row
inline std::string ownedKartSelect() {
    return "SELECT k.id AS id, k.base_key AS vehicle_type_id, " + kartDurabilityColumns() +
           kartStatColumns() +
           "CASE WHEN c.selected_kart_instance_id = k.id THEN 1 ELSE 0 END AS equipped "
           "FROM owned_kart k " + kartTemplateJoin() +
           "JOIN characters c ON c.id = k.character_id ";
}

/// turns one row of the select above into the struct the packet builders take
inline VehicleInfo ownedKartVehicleInfo(const std::map<std::string, std::string>& row) {
    auto num = [&row](const char* col, int32_t fallback) -> int32_t {
        auto it = row.find(col);
        if (it == row.end() || it->second.empty()) return fallback;
        try { return std::stoi(it->second); } catch (...) { return fallback; }
    };

    VehicleInfo v;
    v.id = num("id", 0);
    v.templateId = num("vehicle_type_id", 0);
    v.durability = num("durability", KART_DURABILITY_MAX);
    v.maxDurability = num("max_durability", KART_DURABILITY_MAX);

    static const char* const cols[KART_STAT_COUNT] = {
        "stat_speed", "stat_accel", "stat_handling", "stat_drift",
        "stat_boost", "stat_weight", "stat_special"};
    static const int32_t fallback[KART_STAT_COUNT] = {50, 50, 50, 40, 30, 50, 0};
    for (size_t i = 0; i < KART_STAT_COUNT; ++i) {
        v.stats[i] = num(cols[i], fallback[i]);
    }

    v.equipped = num("equipped", 0) != 0;
    return v;
}

}  // namespace knc
