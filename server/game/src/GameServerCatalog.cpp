/// catalogue query helpers the dispatcher the login burst and the room builders all share

#include <cctype>
#include "GameServerInternal.h"
#include "handlers/InventoryHandler.h"
#include "db/Database.h"
#include "logging/Logger.h"

#include <cstdlib>
#include <string>
#include <unordered_map>

namespace knc {

// maps drivers name to the DevClient Driver Body High Asset folder the client renders
std::string driverBodyAsset(const std::string& dbName) {
    static const std::unordered_map<std::string, std::string> kMap = {
        {"cosmo", "Cosmo"}, {"moriko", "Moriko"}, {"prince", "Prince"},
        {"princess", "Princess"}, {"pumpkin", "Pumpkin"}, {"witch", "Witch"},
        {"wolf", "Wolf"}, {"monster", "Monster"}, {"yuk", "Yuk"},
        {"mummy", "Mummy"}, {"racer_mummy", "Mummy"},
    };
    std::string key = dbName;
    for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto it = kMap.find(key);
    if (it != kMap.end()) return it->second;
    return "Cosmo";  // racer basic pro speed drift lack body asset fall back visible
}

// only BLUE GREEN PURPLE RED YELLOW body colors exist a missing color loads no texture and fails SetBody
const int32_t kDefaultPaintKey = 9007;
// NAMEBOX NORMAL is the stock plate the twenty numbered ones are shop items
const int32_t kDefaultPlateKey = 9100;

// KNC KART STATS 0 puts the 0xC0 stat block back to all zero for testing
bool kartStatsEnabled() {
    const char* e = std::getenv("KNC_KART_STATS");
    return !e || std::string(e) != "0";
}

// the login burst and the race anti cheat must read one block or the thresholds drift off the client
std::array<float, 17> kartWireStats(const std::map<std::string, std::string>& t) {
    auto statCol = [&t](const char* col, float dflt) {
        auto it = t.find(col);
        return it == t.end() || it->second.empty()
                   ? dflt : static_cast<float>(std::atof(it->second.c_str()));
    };
    std::array<float, 17> stats{};
    stats[0]  = statCol("stat00", 0.52f);
    stats[1]  = statCol("stat01", 0.52f);
    stats[2]  = statCol("stat02", 0.52f);
    stats[3]  = statCol("stat03", 0.52f);
    stats[4]  = statCol("stat04", 0.30f);
    stats[5]  = statCol("stat05", 0.52f);
    stats[6]  = statCol("stat06", 0.30f);
    stats[7]  = statCol("stat07", 0.30f);
    stats[8]  = statCol("stat08", 0.52f);
    stats[9]  = statCol("stat09", 0.52f);
    stats[10] = statCol("stat10", 0.52f);
    stats[11] = statCol("stat11", 0.70f);
    stats[12] = statCol("stat12", 0.52f);
    stats[13] = statCol("stat13", 0.0f);
    // 14 15 16 are the chase camera distance pitch and look height camera update 0x43F529
    stats[14] = statCol("stat14", 9.0f);   if (stats[14] == 0.0f) stats[14] = 9.0f;
    stats[15] = statCol("stat15", 37.0f);  if (stats[15] == 0.0f) stats[15] = 37.0f;
    stats[16] = statCol("stat16", 3.5f);   if (stats[16] == 0.0f) stats[16] = 3.5f;
    // chibikart karts all share the same 17 stat floats scale ours by bar over the starter bar within safe range
    if (kartStatsEnabled()) {
        auto bar = [&t](const char* col, float base) {
            auto it = t.find(col);
            const float v = (it == t.end() || it->second.empty())
                                ? base : static_cast<float>(std::atof(it->second.c_str()));
            const float scaled = 0.52f * (v / base);
            return scaled < 0.30f ? 0.30f : (scaled > 0.90f ? 0.90f : scaled);
        };
        stats[1] = bar("stat_speed", 50.0f);
        stats[2] = bar("stat_accel", 50.0f);
        stats[3] = bar("stat_boost", 30.0f);
        stats[5] = bar("stat_handling", 50.0f);
        stats[8] = bar("stat_drift", 40.0f);
    }
    return stats;
}

bool kartWireStatsForTemplate(int32_t templateId, std::array<float, 17>& out, bool& factory) {
    out.fill(0.0f);
    factory = false;
    auto rows = Database::instance().queryPrepared(
        "SELECT id, COALESCE(is_factory_car, 0) AS is_factory_car, "
        "COALESCE(stat_speed,50) AS stat_speed, COALESCE(stat_accel,50) AS stat_accel, "
        "COALESCE(stat_handling,50) AS stat_handling, COALESCE(stat_drift,40) AS stat_drift, "
        "COALESCE(stat_boost,30) AS stat_boost "
        "FROM vehicle_templates WHERE id = ? LIMIT 1", {templateId});
    if (rows.empty()) {
        LOG_WARN("GAME", "vehicle_templates miss for kart " + std::to_string(templateId));
        return false;
    }
    out = kartWireStats(rows[0]);
    factory = factoryFlag(rows[0]);
    return true;
}

RoomTrackChoice defaultRoomTrack(int32_t wantedMapId) {
    auto& db = Database::instance();
    if (wantedMapId > 0) {
        auto hit = db.queryPrepared(
            "SELECT track_id, map_id FROM track_catalog WHERE map_id = ? LIMIT 1",
            {wantedMapId});
        if (!hit.empty()) {
            return { std::stoi(hit[0].at("track_id")), std::stoi(hit[0].at("map_id")) };
        }
    }
    // ids under ten are license tracks a race room needs a real one with a map link
    auto rows = db.queryPrepared(
        "SELECT track_id, map_id FROM track_catalog "
        "WHERE track_id >= 10 AND map_id IS NOT NULL ORDER BY track_id LIMIT 1", {});
    if (rows.empty()) {
        LOG_ERROR("ROOM", "track_catalog has no race track with a map link so the room "
                          "screen faults on a null record and the grid cannot load");
        return {};
    }
    return { std::stoi(rows[0].at("track_id")), std::stoi(rows[0].at("map_id")) };
}

// room row keeps the loadout the grid packet reads a zero kart key drops the client on the first 0x3E
void applyLoadout(RoomPlayer* rp, int32_t charId) {
    if (!rp) return;
    InventoryPackets::KartRow kr;
    if (InventoryHandler::selectedKartRow(charId, kr) && kr.baseKey != 0) {
        rp->vehicleTemplateId = static_cast<int32_t>(kr.baseKey);
    }
    InventoryPackets::CharacterRow cr;
    if (InventoryHandler::selectedCharacterRow(charId, cr) && cr.baseKey != 0) {
        rp->driverId = static_cast<int32_t>(cr.baseKey);
    }
}

bool factoryFlag(const std::map<std::string, std::string>& row) {
    auto it = row.find("is_factory_car");
    return it != row.end() && it->second != "0";
}

}  // namespace knc
