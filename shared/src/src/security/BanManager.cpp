/**
 * @file BanManager.cpp
 * @brief IP and account ban management
 */

#include "security/BanManager.h"
#include "logging/Logger.h"

namespace knc {

bool BanManager::isBanned(const std::string& ip) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_bannedIPs.find(ip) != m_bannedIPs.end();
}

bool BanManager::isBanned(uint32_t accountId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_bannedAccounts.find(accountId) != m_bannedAccounts.end();
}

void BanManager::banIP(const std::string& ip, const std::string& reason, 
                       const std::string& by, int durationMinutes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_bannedIPs.insert(ip);
    LOG_INFO("BAN", "Banned IP: " + ip + " by " + by + " reason: " + reason);
    
    // Note: DB persistence would add expiration tracking
    // For now, bans are in-memory and cleared on restart
    (void)durationMinutes;
}

void BanManager::banAccount(uint32_t accountId, const std::string& reason,
                            const std::string& by, int durationMinutes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_bannedAccounts.insert(accountId);
    LOG_INFO("BAN", "Banned account: " + std::to_string(accountId) + " by " + by + " reason: " + reason);
    
    // Note: DB persistence would add expiration tracking
    (void)durationMinutes;
}

void BanManager::unbanIP(const std::string& ip) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_bannedIPs.erase(ip);
    LOG_INFO("BAN", "Unbanned IP: " + ip);
}

void BanManager::unbanAccount(uint32_t accountId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_bannedAccounts.erase(accountId);
    LOG_INFO("BAN", "Unbanned account: " + std::to_string(accountId));
}

void BanManager::loadFromDB() {
    // Note: Would load from 'bans' table - currently in-memory only
    LOG_INFO("BAN", "Ban manager initialized (in-memory)");
}

void BanManager::saveToDB() {
    // Note: Would persist to 'bans' table - currently in-memory only
    LOG_DEBUG("BAN", "Ban save requested (in-memory mode)");
}

} // namespace knc
