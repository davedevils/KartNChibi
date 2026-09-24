
#include "handlers/AntiCheatHandler.h"
#include "GameServer.h"
#include "packets/PacketBuilder.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/AutoBan.h"
#include <cmath>
#include <mutex>

namespace knc {

std::unordered_map<uint32_t, int> AntiCheatHandler::s_violationCounts;

static std::mutex s_mutex;

void AntiCheatHandler::reportViolation(Session::Ptr session, ViolationType type, 
                                       ViolationSeverity severity, const std::string& details) {
    uint32_t sessionId = session->id();
    int totalViolations = 0;
    {
        // the lock guards the counter only never the log the database or the kick
        std::lock_guard<std::mutex> lock(s_mutex);
        totalViolations = ++s_violationCounts[sessionId];
    }

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
    
    logViolation(session, type, severity, details);

    if (severity == ViolationSeverity::Critical ||
        totalViolations >= VIOLATION_THRESHOLD_BAN) {
        takeAction(session, ViolationSeverity::Critical);
    } else if (severity == ViolationSeverity::High || 
               totalViolations >= VIOLATION_THRESHOLD_KICK) {
        takeAction(session, ViolationSeverity::High);
    } else if (severity == ViolationSeverity::Medium) {
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
            LOG_WARN("ANTICHEAT", "Kicking player: char=" + std::to_string(session->characterId));
            session->send(PacketBuilder::displayMessage(u"MSG_KICKED_CHEAT", 2));
            session->stop();

            db.execute(
                "UPDATE anticheat_logs SET action_taken = 'kick' "
                "WHERE character_id = " + std::to_string(session->characterId) + 
                " ORDER BY id DESC LIMIT 1"
            );
            break;
            
        case ViolationSeverity::Critical: {
            int32_t earlier = 0;
            auto prior = db.query("SELECT COUNT(*) AS n FROM bans WHERE account_id = " +
                                  std::to_string(session->accountId) +
                                  " AND banned_by = 'SYSTEM' AND reason = 'Anti-cheat violation'");
            if (!prior.empty() && !prior[0]["n"].empty()) earlier = std::stoi(prior[0]["n"]);
            const std::string minutes = std::to_string(autoBanMinutes(earlier));
            LOG_WARN("ANTICHEAT", "Banning player: char=" + std::to_string(session->characterId) +
                     " for " + minutes + " minutes after " + std::to_string(earlier) + " earlier bans");

            db.execute(
                "INSERT INTO bans (account_id, ip_address, reason, banned_by, expires_at) VALUES (" +
                std::to_string(session->accountId) + ", '" + session->remoteAddress() + "', " +
                "'Anti-cheat violation', 'SYSTEM', DATE_ADD(NOW(), INTERVAL " + minutes + " MINUTE))"
            );

            db.execute(
                "UPDATE accounts SET is_banned = 1, ban_reason = 'Anti-cheat violation', "
                "ban_expires_at = DATE_ADD(NOW(), INTERVAL " + minutes + " MINUTE) "
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
        }

        default:
            break;
    }
}

} // namespace knc

