/**
 * @file AntiCheatHandler.cpp
 * @brief Anti-cheat system implementation
 */

#include "handlers/AntiCheatHandler.h"
#include "GameServer.h"
#include "packets/PacketBuilder.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include <cmath>
#include <mutex>

namespace knc {

// Static member definitions
std::unordered_map<uint32_t, std::vector<PositionRecord>> AntiCheatHandler::s_positionHistory;
std::unordered_map<uint32_t, int> AntiCheatHandler::s_violationCounts;
std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> AntiCheatHandler::s_lastPacketTime;
std::unordered_map<uint32_t, int> AntiCheatHandler::s_packetCounts;
std::unordered_map<int, float> AntiCheatHandler::s_maxSpeeds;
std::unordered_map<int, int> AntiCheatHandler::s_minLapTimes;

static std::mutex s_mutex;

bool AntiCheatHandler::validatePosition(Session::Ptr session, float x, float y, float z) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    uint32_t sessionId = session->id();
    auto now = std::chrono::steady_clock::now();
    
    PositionRecord currentPos{x, y, z, now};
    
    auto& history = s_positionHistory[sessionId];
    
    if (!history.empty()) {
        const auto& lastPos = history.back();
        
        // Calculate time delta
        auto timeDelta = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastPos.timestamp
        ).count();
        
        if (timeDelta > 0) {
            // Calculate distance
            float distance = calculateDistance(lastPos.x, lastPos.y, lastPos.z, x, y, z);
            
            // Check for teleport (instant large movement)
            if (distance > TELEPORT_THRESHOLD && timeDelta < 100) {
                reportViolation(session, ViolationType::Teleport, ViolationSeverity::High,
                    "Teleport detected: " + std::to_string(distance) + " units in " + 
                    std::to_string(timeDelta) + "ms");
                return false;
            }
            
            // Calculate speed (units per second)
            float speed = (distance / timeDelta) * 1000.0f;
            
            // Check for speedhack
            if (speed > DEFAULT_MAX_SPEED) {
                reportViolation(session, ViolationType::SpeedHack, ViolationSeverity::Medium,
                    "Speed violation: " + std::to_string(speed) + " units/s (max: " + 
                    std::to_string(DEFAULT_MAX_SPEED) + ")");
                return false;
            }
        }
    }
    
    // Store position (keep last 60 positions = ~1 second at 60fps)
    history.push_back(currentPos);
    if (history.size() > 60) {
        history.erase(history.begin());
    }
    
    return true;
}

bool AntiCheatHandler::validateSpeed(Session::Ptr session, float speed, int mapId) {
    float maxSpeed = DEFAULT_MAX_SPEED;
    
    auto it = s_maxSpeeds.find(mapId);
    if (it != s_maxSpeeds.end()) {
        maxSpeed = it->second;
    }
    
    // Allow 20% tolerance for network lag and boost items
    if (speed > maxSpeed * 1.2f) {
        reportViolation(session, ViolationType::SpeedHack, ViolationSeverity::Medium,
            "Speed violation on map " + std::to_string(mapId) + ": " + 
            std::to_string(speed) + " (max: " + std::to_string(maxSpeed) + ")");
        return false;
    }
    
    return true;
}

bool AntiCheatHandler::validateLapTime(Session::Ptr session, int mapId, int lapTimeMs) {
    int minTime = MIN_LAP_TIME_MS;
    
    auto it = s_minLapTimes.find(mapId);
    if (it != s_minLapTimes.end()) {
        minTime = it->second;
    }
    
    // Impossibly fast lap
    if (lapTimeMs < minTime) {
        reportViolation(session, ViolationType::SpeedHack, ViolationSeverity::High,
            "Impossible lap time on map " + std::to_string(mapId) + ": " + 
            std::to_string(lapTimeMs) + "ms (min: " + std::to_string(minTime) + "ms)");
        
        // Log to database for analysis
        auto& db = Database::instance();
        db.execute(
            "INSERT INTO speed_records (character_id, map_id, lap_time, total_time, max_speed, avg_speed, is_valid) "
            "VALUES (" + std::to_string(session->characterId) + ", " + 
            std::to_string(mapId) + ", " + std::to_string(lapTimeMs) + ", " +
            std::to_string(lapTimeMs) + ", 0, 0, 0)"
        );
        
        return false;
    }
    
    return true;
}

bool AntiCheatHandler::validateItemUse(Session::Ptr session, int itemId, int targetId) {
    // Check for item exploit patterns
    // using items too quickly, using items player doesn't own
    
    // For now, just log and allow
    LOG_DEBUG("ANTICHEAT", "Item use: session=" + std::to_string(session->id()) + 
              " item=" + std::to_string(itemId) + " target=" + std::to_string(targetId));
    
    return true;
}

bool AntiCheatHandler::validatePacketRate(Session::Ptr session) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    uint32_t sessionId = session->id();
    auto now = std::chrono::steady_clock::now();
    
    auto& lastTime = s_lastPacketTime[sessionId];
    auto& count = s_packetCounts[sessionId];
    
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime).count();
    
    if (elapsed >= 1) {
        // Reset counter every second
        count = 1;
        lastTime = now;
    } else {
        count++;
        
        if (count > PACKET_FLOOD_THRESHOLD) {
            reportViolation(session, ViolationType::PacketFlood, ViolationSeverity::Medium,
                "Packet flood: " + std::to_string(count) + " packets/sec");
            return false;
        }
    }
    
    return true;
}

void AntiCheatHandler::reportViolation(Session::Ptr session, ViolationType type, 
                                       ViolationSeverity severity, const std::string& details) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    uint32_t sessionId = session->id();
    s_violationCounts[sessionId]++;
    
    // Log to file
    std::string typeStr;
    switch (type) {
        case ViolationType::SpeedHack: typeStr = "SPEEDHACK"; break;
        case ViolationType::Teleport: typeStr = "TELEPORT"; break;
        case ViolationType::ItemExploit: typeStr = "ITEM_EXPLOIT"; break;
        case ViolationType::PacketFlood: typeStr = "PACKET_FLOOD"; break;
        case ViolationType::InvalidData: typeStr = "INVALID_DATA"; break;
        case ViolationType::MemoryEdit: typeStr = "MEMORY_EDIT"; break;
    }
    
    std::string severityStr;
    switch (severity) {
        case ViolationSeverity::Low: severityStr = "LOW"; break;
        case ViolationSeverity::Medium: severityStr = "MEDIUM"; break;
        case ViolationSeverity::High: severityStr = "HIGH"; break;
        case ViolationSeverity::Critical: severityStr = "CRITICAL"; break;
    }
    
    LOG_WARN("ANTICHEAT", "[" + severityStr + "] " + typeStr + " - " + details + 
             " (session=" + std::to_string(sessionId) + 
             ", char=" + std::to_string(session->characterId) + 
             ", ip=" + session->remoteAddress() + ")");
    
    // Log to database
    logViolation(session, type, severity, details);
    
    // Take action based on severity and violation count
    int totalViolations = s_violationCounts[sessionId];
    
    if (severity == ViolationSeverity::Critical || 
        totalViolations >= VIOLATION_THRESHOLD_BAN) {
        takeAction(session, ViolationSeverity::Critical);
    } else if (severity == ViolationSeverity::High || 
               totalViolations >= VIOLATION_THRESHOLD_KICK) {
        takeAction(session, ViolationSeverity::High);
    } else if (severity == ViolationSeverity::Medium) {
        // Warning - send message to client
        session->send(PacketBuilder::displayMessage(u"MSG_CHEAT_WARNING", 2));
    }
}

int AntiCheatHandler::getViolationCount(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_violationCounts.find(sessionId);
    return (it != s_violationCounts.end()) ? it->second : 0;
}

void AntiCheatHandler::clearViolations(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_violationCounts.erase(sessionId);
    s_positionHistory.erase(sessionId);
    s_lastPacketTime.erase(sessionId);
    s_packetCounts.erase(sessionId);
}

void AntiCheatHandler::setMaxSpeed(int mapId, float maxSpeed) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_maxSpeeds[mapId] = maxSpeed;
}

void AntiCheatHandler::setMinLapTime(int mapId, int minTimeMs) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_minLapTimes[mapId] = minTimeMs;
}

float AntiCheatHandler::calculateSpeed(const PositionRecord& prev, const PositionRecord& curr) {
    float distance = calculateDistance(prev.x, prev.y, prev.z, curr.x, curr.y, curr.z);
    auto timeDelta = std::chrono::duration_cast<std::chrono::milliseconds>(
        curr.timestamp - prev.timestamp
    ).count();
    
    if (timeDelta <= 0) return 0.0f;
    
    return (distance / timeDelta) * 1000.0f;  // Units per second
}

float AntiCheatHandler::calculateDistance(float x1, float y1, float z1, float x2, float y2, float z2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

void AntiCheatHandler::logViolation(Session::Ptr session, ViolationType type, 
                                    ViolationSeverity severity, const std::string& details) {
    auto& db = Database::instance();
    
    std::string typeStr;
    switch (type) {
        case ViolationType::SpeedHack: typeStr = "speedhack"; break;
        case ViolationType::Teleport: typeStr = "teleport"; break;
        case ViolationType::ItemExploit: typeStr = "item_exploit"; break;
        case ViolationType::PacketFlood: typeStr = "packet_flood"; break;
        case ViolationType::InvalidData: typeStr = "invalid_data"; break;
        case ViolationType::MemoryEdit: typeStr = "memory_edit"; break;
    }
    
    std::string severityStr;
    switch (severity) {
        case ViolationSeverity::Low: severityStr = "low"; break;
        case ViolationSeverity::Medium: severityStr = "medium"; break;
        case ViolationSeverity::High: severityStr = "high"; break;
        case ViolationSeverity::Critical: severityStr = "critical"; break;
    }
    
    // Escape details for SQL
    std::string escapedDetails = details;
    size_t pos = 0;
    while ((pos = escapedDetails.find("'", pos)) != std::string::npos) {
        escapedDetails.replace(pos, 1, "''");
        pos += 2;
    }
    
    try {
        db.execute(
            "INSERT INTO anticheat_logs (character_id, account_id, ip_address, violation_type, severity, details) "
            "VALUES (" + 
            std::to_string(session->characterId) + ", " +
            std::to_string(session->accountId) + ", '" +
            session->remoteAddress() + "', '" +
            typeStr + "', '" +
            severityStr + "', '" +
            escapedDetails + "')"
        );
    } catch (const std::exception& e) {
        LOG_ERROR("ANTICHEAT", "Failed to log violation: " + std::string(e.what()));
    }
}

void AntiCheatHandler::takeAction(Session::Ptr session, ViolationSeverity severity) {
    auto& db = Database::instance();
    
    switch (severity) {
        case ViolationSeverity::High:
            // Kick player
            LOG_WARN("ANTICHEAT", "Kicking player: char=" + std::to_string(session->characterId));
            session->send(PacketBuilder::displayMessage(u"MSG_KICKED_CHEAT", 2));
            session->stop();
            
            // Update log with action
            db.execute(
                "UPDATE anticheat_logs SET action_taken = 'kick' "
                "WHERE character_id = " + std::to_string(session->characterId) + 
                " ORDER BY id DESC LIMIT 1"
            );
            break;
            
        case ViolationSeverity::Critical:
            // Temp ban (24 hours)
            LOG_WARN("ANTICHEAT", "Banning player: char=" + std::to_string(session->characterId));
            
            db.execute(
                "INSERT INTO bans (account_id, ip_address, reason, banned_by, expires_at) VALUES (" +
                std::to_string(session->accountId) + ", '" +
                session->remoteAddress() + "', " +
                "'Anti-cheat violation', 'SYSTEM', DATE_ADD(NOW(), INTERVAL 24 HOUR))"
            );
            
            db.execute(
                "UPDATE accounts SET is_banned = 1, ban_reason = 'Anti-cheat violation', "
                "ban_expires_at = DATE_ADD(NOW(), INTERVAL 24 HOUR) "
                "WHERE id = " + std::to_string(session->accountId)
            );
            
            session->send(PacketBuilder::displayMessage(u"MSG_BANNED_CHEAT", 2));
            session->stop();
            
            db.execute(
                "UPDATE anticheat_logs SET action_taken = 'temp_ban' "
                "WHERE character_id = " + std::to_string(session->characterId) + 
                " ORDER BY id DESC LIMIT 1"
            );
            break;
            
        default:
            break;
    }
}

} // namespace knc

