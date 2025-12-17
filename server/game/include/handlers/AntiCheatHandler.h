/**
 * @file AntiCheatHandler.h
 * @brief Anti-cheat system for detecting speedhacks, teleports, and exploits
 */
#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include <memory>
#include <unordered_map>
#include <chrono>
#include <vector>

namespace knc {

class GameServer;

// Position with timestamp for movement validation
struct PositionRecord {
    float x, y, z;
    std::chrono::steady_clock::time_point timestamp;
};

// Violation types
enum class ViolationType {
    SpeedHack,
    Teleport,
    ItemExploit,
    PacketFlood,
    InvalidData,
    MemoryEdit
};

// Violation severity
enum class ViolationSeverity {
    Low,      // Warning only
    Medium,   // Kick
    High,     // Temp ban (24h)
    Critical  // Perm ban
};

class AntiCheatHandler {
public:
    // Validation functions (called by other handlers)
    static bool validatePosition(Session::Ptr session, float x, float y, float z);
    static bool validateSpeed(Session::Ptr session, float speed, int mapId);
    static bool validateLapTime(Session::Ptr session, int mapId, int lapTimeMs);
    static bool validateItemUse(Session::Ptr session, int itemId, int targetId);
    static bool validatePacketRate(Session::Ptr session);
    
    // Violation handling
    static void reportViolation(Session::Ptr session, ViolationType type, 
                                ViolationSeverity severity, const std::string& details);
    
    // Statistics
    static int getViolationCount(uint32_t sessionId);
    static void clearViolations(uint32_t sessionId);
    
    // Configuration
    static void setMaxSpeed(int mapId, float maxSpeed);
    static void setMinLapTime(int mapId, int minTimeMs);
    
    // Constants
    static constexpr float DEFAULT_MAX_SPEED = 500.0f;       // Units per second
    static constexpr float TELEPORT_THRESHOLD = 100.0f;      // Units (instant movement)
    static constexpr int PACKET_FLOOD_THRESHOLD = 100;       // Packets per second
    static constexpr int MIN_LAP_TIME_MS = 15000;            // 15 seconds minimum lap
    static constexpr int VIOLATION_THRESHOLD_KICK = 3;       // Violations before kick
    static constexpr int VIOLATION_THRESHOLD_BAN = 10;       // Violations before temp ban
    
private:
    // Per-session tracking
    static std::unordered_map<uint32_t, std::vector<PositionRecord>> s_positionHistory;
    static std::unordered_map<uint32_t, int> s_violationCounts;
    static std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> s_lastPacketTime;
    static std::unordered_map<uint32_t, int> s_packetCounts;
    
    // Map-specific limits
    static std::unordered_map<int, float> s_maxSpeeds;
    static std::unordered_map<int, int> s_minLapTimes;
    
    // Helper functions
    static float calculateSpeed(const PositionRecord& prev, const PositionRecord& curr);
    static float calculateDistance(float x1, float y1, float z1, float x2, float y2, float z2);
    static void logViolation(Session::Ptr session, ViolationType type, 
                             ViolationSeverity severity, const std::string& details);
    static void takeAction(Session::Ptr session, ViolationSeverity severity);
};

} // namespace knc

