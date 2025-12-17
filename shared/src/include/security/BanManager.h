/**
 * @file BanManager.h
 * @brief IP and account ban management
 */

#pragma once
#include <string>
#include <chrono>
#include <unordered_set>
#include <mutex>

namespace knc {

struct Ban {
    std::string accountId;
    std::string ipAddress;
    std::string reason;
    std::string bannedBy;
    std::chrono::system_clock::time_point expiresAt;
    bool isPermanent = false;
};

class BanManager {
public:
    static BanManager& instance() {
        static BanManager inst;
        return inst;
    }
    
    // Check if banned
    bool isBanned(const std::string& ip) const;
    bool isBanned(uint32_t accountId) const;
    
    // Add ban
    void banIP(const std::string& ip, const std::string& reason, 
               const std::string& by, int durationMinutes = 0);
    void banAccount(uint32_t accountId, const std::string& reason,
                    const std::string& by, int durationMinutes = 0);
    
    // Remove ban
    void unbanIP(const std::string& ip);
    void unbanAccount(uint32_t accountId);
    
    // Load/save from database
    void loadFromDB();
    void saveToDB();

private:
    BanManager() = default;
    mutable std::mutex m_mutex;
    std::unordered_set<std::string> m_bannedIPs;
    std::unordered_set<uint32_t> m_bannedAccounts;
};

} // namespace knc

