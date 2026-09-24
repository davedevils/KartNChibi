/// anti cheat system detecting speedhacks teleports and exploits
#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include <memory>
#include <unordered_map>
#include <chrono>
#include <vector>

namespace knc {

class GameServer;

enum class ViolationType {
    SpeedHack,
    Teleport,
    ItemExploit,
    PacketFlood,
    InvalidData,
    MemoryEdit
};

// Low warns Medium kicks High bans 24 hours Critical bans for good
enum class ViolationSeverity {
    Low,
    Medium,
    High,
    Critical
};

/// movement judges are in MotionPackets motionStep and DriftBoostPackets checkSpeed packet rate is RateLimiter lap floor is in RaceHandler
class AntiCheatHandler {
public:
    static void reportViolation(Session::Ptr session, ViolationType type,
                                ViolationSeverity severity, const std::string& details);

    static int getViolationCount(uint32_t sessionId);
    static void clearViolations(uint32_t sessionId);

    static constexpr int VIOLATION_THRESHOLD_KICK = 3;
    static constexpr int VIOLATION_THRESHOLD_BAN = 10;

private:
    static std::unordered_map<uint32_t, int> s_violationCounts;
    
    static void logViolation(Session::Ptr session, ViolationType type, 
                             ViolationSeverity severity, const std::string& details);
    static void takeAction(Session::Ptr session, ViolationSeverity severity);
};

} // namespace knc

